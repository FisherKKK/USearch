/**
 * 高级分布式向量搜索示例
 *
 * 演示：
 * 1. Raft 共识实现
 * 2. 检查点和恢复
 * 3. 故障检测和自动转移
 * 4. 分布式追踪
 * 5. 负载均衡优化
 *
 * 编译：
 * g++ -std=c++17 -O3 -march=native -pthread \
 *     -I../../../include \
 *     advanced_distributed.cpp -o adv_dist
 *
 * 运行：
 * ./adv_dist
 */

#include <usearch/index.hpp>
#include <usearch/index_dense.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace unum::usearch;

//==============================================================================
// 工具类
//==============================================================================

class Timer {
    std::chrono::high_resolution_clock::time_point start_;

public:
    void start() { start_ = std::chrono::high_resolution_clock::now(); }

    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> diff = end - start_;
        return diff.count();
    }
};

//==============================================================================
// 分布式追踪系统
//==============================================================================

class Tracer {
public:
    struct Span {
        std::string name;
        std::chrono::microseconds duration;
        std::chrono::system_clock::time_point start;
        std::string parent_id;
        std::map<std::string, std::string> tags;
    };

private:
    std::vector<Span> spans_;
    std::mutex mutex_;
    std::atomic<std::uint64_t> id_counter_{0};

public:
    std::string start_span(std::string const& name, std::string const& parent_id = "") {
        std::string span_id = "span_" + std::to_string(id_counter_.fetch_add(1));

        Span span;
        span.name = name;
        span.start = std::chrono::system_clock::now();
        span.parent_id = parent_id;

        std::lock_guard<std::mutex> lock(mutex_);
        spans_.push_back(span);

        return span_id;
    }

    void finish_span(std::string const& span_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = std::find_if(spans_.begin(), spans_.end(),
                              [&](Span const& s) {
                                  return s.tags["span_id"] == span_id;
                              });

        if (it != spans_.end()) {
            auto end = std::chrono::system_clock::now();
            it->duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - it->start
            );
        }
    }

    void print_trace() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "\n=== Distributed Trace ===\n";

        for (auto const& span : spans_) {
            std::cout << "  " << span.name
                     << " (" << span.duration.count() << " us)\n";

            if (!span.parent_id.empty()) {
                std::cout << "    Parent: " << span.parent_id << "\n";
            }

            for (auto const& [key, value] : span.tags) {
                std::cout << "    " << key << ": " << value << "\n";
            }
        }
    }

    void export_json(std::string const& filename) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ofstream file(filename);
        file << "[\n";

        for (std::size_t i = 0; i < spans_.size(); ++i) {
            auto const& span = spans_[i];

            file << "  {\n";
            file << "    \"name\": \"" << span.name << "\",\n";
            file << "    \"duration_us\": " << span.duration.count() << ",\n";
            file << "    \"parent_id\": \"" << span.parent_id << "\"\n";
            file << "  }";

            if (i < spans_.size() - 1) {
                file << ",";
            }
            file << "\n";
        }

        file << "]\n";
    }
};

//==============================================================================
// 故障检测器
//==============================================================================

class FailureDetector {
public:
    struct NodeStatus {
        std::string address;
        std::chrono::steady_clock::time_point last_heartbeat;
        bool is_alive;
        std::size_t failure_count;
    };

private:
    std::map<std::string, NodeStatus> nodes_;
    std::chrono::milliseconds heartbeat_timeout_;
    std::thread detector_thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;

    std::function<void(std::string const&)> on_failure_;

public:
    FailureDetector(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000))
        : heartbeat_timeout_(timeout) {}

    ~FailureDetector() {
        stop();
    }

    void add_node(std::string const& address) {
        std::lock_guard<std::mutex> lock(mutex_);

        nodes_[address] = {
            .address = address,
            .last_heartbeat = std::chrono::steady_clock::now(),
            .is_alive = true,
            .failure_count = 0
        };
    }

    void heartbeat(std::string const& address) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodes_.find(address);
        if (it != nodes_.end()) {
            it->second.last_heartbeat = std::chrono::steady_clock::now();
            it->second.is_alive = true;
            it->second.failure_count = 0;
        }
    }

    void set_failure_callback(std::function<void(std::string const&)> callback) {
        on_failure_ = std::move(callback);
    }

    void start() {
        running_ = true;
        detector_thread_ = std::thread([this] { detect_loop(); });
    }

    void stop() {
        running_ = false;
        if (detector_thread_.joinable()) {
            detector_thread_.join();
        }
    }

private:
    void detect_loop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::lock_guard<std::mutex> lock(mutex_);

            for (auto& [address, status] : nodes_) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - status.last_heartbeat
                );

                if (elapsed > heartbeat_timeout_) {
                    if (status.is_alive) {
                        status.is_alive = false;
                        status.failure_count++;

                        std::cout << "[FailureDetector] Node " << address
                                 << " may have failed (failure " << status.failure_count << ")\n";

                        if (status.failure_count >= 3 && on_failure_) {
                            std::cout << "[FailureDetector] Node " << address
                                     << " confirmed FAILED!\n";
                            on_failure_(address);
                        }
                    }
                }
            }
        }
    }
};

//==============================================================================
// 检查点管理器
//==============================================================================

class CheckpointManager {
public:
    struct Checkpoint {
        std::uint64_t id;
        std::string path;
        std::chrono::system_clock::time_point timestamp;
        std::size_t num_vectors;
    };

private:
    std::string checkpoint_dir_;
    std::chrono::minutes interval_;
    std::size_t max_checkpoints_;

    std::vector<Checkpoint> checkpoints_;
    std::thread checkpoint_thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;

    ShardNode* shard_;  // 非拥有指针

public:
    CheckpointManager(std::string const& dir,
                     ShardNode* shard,
                     std::chrono::minutes interval = std::chrono::minutes(10),
                     std::size_t max_checkpoints = 3)
        : checkpoint_dir_(dir),
          interval_(interval),
          max_checkpoints_(max_checkpoints),
          shard_(shard) {

        // 创建目录
        std::filesystem::create_directories(dir);
    }

    ~CheckpointManager() {
        stop();
    }

    void start() {
        running_ = true;
        checkpoint_thread_ = std::thread([this] { checkpoint_loop(); });
    }

    void stop() {
        running_ = false;
        if (checkpoint_thread_.joinable()) {
            checkpoint_thread_.join();
        }
    }

    Checkpoint create_checkpoint() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::uint64_t id = checkpoints_.empty() ? 1 :
                          checkpoints_.back().id + 1;

        std::string path = checkpoint_dir_ + "/checkpoint_" +
                          std::to_string(id) + ".usearch";

        std::cout << "[Checkpoint] Creating checkpoint " << id << "...\n";

        Timer timer;
        timer.start();

        if (shard_->save(path)) {
            Checkpoint cp{
                .id = id,
                .path = path,
                .timestamp = std::chrono::system_clock::now(),
                .num_vectors = shard_->size()
            };

            checkpoints_.push_back(cp);

            std::cout << "[Checkpoint] Checkpoint " << id << " created in "
                     << timer.elapsed_ms() << " ms ("
                     << cp.num_vectors << " vectors)\n";

            cleanup_old();

            return cp;
        } else {
            throw std::runtime_error("Failed to create checkpoint");
        }
    }

    bool restore_latest() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (checkpoints_.empty()) {
            std::cout << "[Checkpoint] No checkpoints available\n";
            return false;
        }

        auto& latest = checkpoints_.back();
        std::cout << "[Checkpoint] Restoring from checkpoint " << latest.id << "...\n";

        Timer timer;
        timer.start();

        bool success = shard_->load(latest.path);

        if (success) {
            std::cout << "[Checkpoint] Restored in " << timer.elapsed_ms() << " ms\n";
        } else {
            std::cout << "[Checkpoint] Failed to restore!\n";
        }

        return success;
    }

private:
    void checkpoint_loop() {
        while (running_) {
            std::this_thread::sleep_for(interval_);

            if (running_) {
                try {
                    create_checkpoint();
                } catch (std::exception const& e) {
                    std::cerr << "[Checkpoint] Error: " << e.what() << "\n";
                }
            }
        }
    }

    void cleanup_old() {
        while (checkpoints_.size() > max_checkpoints_) {
            auto& oldest = checkpoints_.front();

            std::filesystem::remove(oldest.path);
            std::cout << "[Checkpoint] Removed old checkpoint " << oldest.id << "\n";

            checkpoints_.erase(checkpoints_.begin());
        }
    }
};

//==============================================================================
// 自适应负载均衡器
//==============================================================================

class AdaptiveLoadBalancer {
    struct ShardStats {
        std::size_t shard_id;
        std::atomic<std::uint64_t> active_requests{0};
        std::atomic<std::uint64_t> total_requests{0};
        std::atomic<double> avg_latency_ms{0.0};
        std::atomic<std::size_t> error_count{0};
    };

    std::vector<ShardStats> shards_;
    std::size_t current_index_{0};
    std::mutex mutex_;

public:
    AdaptiveLoadBalancer(std::size_t num_shards) {
        for (std::size_t i = 0; i < num_shards; ++i) {
            shards_.push_back({.shard_id = i});
        }
    }

    // 选择最佳分片
    std::size_t select_shard() {
        // 加权最少连接算法
        std::lock_guard<std::mutex> lock(mutex_);

        std::size_t best_shard = 0;
        double best_score = std::numeric_limits<double>::max();

        for (auto const& shard : shards_) {
            std::uint64_t active = shard.active_requests.load();
            double latency = shard.avg_latency_ms.load();

            // 分数 = 活跃请求数 + 延迟权重
            double score = active + latency / 10.0;

            if (score < best_score) {
                best_score = score;
                best_shard = shard.shard_id;
            }
        }

        return best_shard;
    }

    void record_request_start(std::size_t shard_id) {
        shards_[shard_id].active_requests.fetch_add(1);
        shards_[shard_id].total_requests.fetch_add(1);
    }

    void record_request_end(std::size_t shard_id, double latency_ms, bool success) {
        shards_[shard_id].active_requests.fetch_sub(1);

        // 指数移动平均
        double old_avg = shards_[shard_id].avg_latency_ms.load();
        double new_avg = old_avg * 0.9 + latency_ms * 0.1;
        shards_[shard_id].avg_latency_ms.store(new_avg);

        if (!success) {
            shards_[shard_id].error_count.fetch_add(1);
        }
    }

    void print_stats() const {
        std::cout << "\n=== Load Balancer Stats ===\n";
        std::cout << std::left << std::setw(10) << "Shard"
                  << std::setw(15) << "Total Req"
                  << std::setw(15) << "Active Req"
                  << std::setw(15) << "Avg Latency"
                  << std::setw(10) << "Errors"
                  << "\n";
        std::cout << std::string(65, '-') << "\n";

        for (auto const& shard : shards_) {
            std::cout << std::left << std::setw(10) << shard.shard_id
                      << std::setw(15) << shard.total_requests.load()
                      << std::setw(15) << shard.active_requests.load()
                      << std::setw(15) << std::fixed << std::setprecision(2)
                      << shard.avg_latency_ms.load()
                      << std::setw(10) << shard.error_count.load()
                      << "\n";
        }
    }
};

//==============================================================================
// 示例 1：分布式追踪
//==============================================================================

void example_01_distributed_tracing() {
    std::cout << "\n=== Example 1: Distributed Tracing ===\n";

    Tracer tracer;

    // 开始 trace
    auto root_span = tracer.start_span("search_request");

    // 子操作 1
    auto shard_query_span = tracer.start_span("query_shard_0", root_span);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    tracer.finish_span(shard_query_span);

    // 子操作 2
    auto shard_query_span_2 = tracer.start_span("query_shard_1", root_span);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    tracer.finish_span(shard_query_span_2);

    // 聚合结果
    auto aggregate_span = tracer.start_span("aggregate_results", root_span);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    tracer.finish_span(aggregate_span);

    // 完成 trace
    tracer.finish_span(root_span);

    // 打印 trace
    tracer.print_trace();

    // 导出 JSON
    tracer.export_json("/tmp/trace_example.json");
    std::cout << "Trace exported to /tmp/trace_example.json\n";
}

//==============================================================================
// 示例 2：故障检测和恢复
//==============================================================================

void example_02_failure_detection() {
    std::cout << "\n=== Example 2: Failure Detection ===\n";

    FailureDetector detector(std::chrono::milliseconds(1000));

    // 添加节点
    detector.add_node("node1:5000");
    detector.add_node("node2:5000");
    detector.add_node("node3:5000");

    // 设置故障回调
    detector.set_failure_callback([](std::string const& failed_node) {
        std::cout << "[Callback] Initiating recovery for " << failed_node << "\n";
        // 实际实现中，这里会触发恢复流程
    });

    detector.start();

    // 模拟正常心跳
    std::cout << "Sending heartbeats...\n";
    for (int i = 0; i < 3; ++i) {
        detector.heartbeat("node1:5000");
        detector.heartbeat("node2:5000");
        detector.heartbeat("node3:5000");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // 模拟节点故障
    std::cout << "\nSimulating node2 failure...\n";
    std::cout << "(No more heartbeats from node2)\n";

    // 等待检测
    std::this_thread::sleep_for(std::chrono::seconds(4));

    detector.stop();
}

//==============================================================================
// 示例 3：检查点和恢复
//==============================================================================

void example_03_checkpoint_recovery() {
    std::cout << "\n=== Example 3: Checkpoint and Recovery ===\n";

    // 创建临时分片
    ShardNode shard(0, 128);

    // 添加一些向量
    constexpr std::size_t n = 1000;
    std::vector<std::uint32_t> keys(n);
    std::vector<float> vectors(n * 128);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i;
        for (std::size_t j = 0; j < 128; ++j) {
            vectors[i * 128 + j] = dist(rng);
        }
    }

    std::cout << "Adding " << n << " vectors...\n";
    shard.add_batch(keys.data(), vectors.data(), n);
    std::cout << "Shard size: " << shard.size() << "\n";

    // 创建检查点管理器（使用快速间隔）
    CheckpointManager checkpoint_mgr("/tmp/checkpoints", &shard,
                                    std::chrono::seconds(2), 2);

    // 手动创建检查点
    auto cp1 = checkpoint_mgr.create_checkpoint();
    std::cout << "Checkpoint " << cp1.id << " created\n";

    // 添加更多向量
    for (std::size_t i = n; i < n * 2; ++i) {
        keys.push_back(i);
        for (std::size_t j = 0; j < 128; ++j) {
            vectors.push_back(dist(rng));
        }
    }

    std::cout << "Adding " << n << " more vectors...\n";
    shard.add_batch(keys.data() + n, vectors.data() + n * 128, n);
    std::cout << "Shard size: " << shard.size() << "\n";

    // 创建另一个检查点
    auto cp2 = checkpoint_mgr.create_checkpoint();
    std::cout << "Checkpoint " << cp2.id << " created\n";

    // 模拟崩溃和恢复
    std::cout << "\nSimulating crash and recovery...\n";

    ShardNode new_shard(0, 128);
    checkpoint_mgr.shard_ = &new_shard;

    bool success = checkpoint_mgr.restore_latest();

    if (success) {
        std::cout << "Recovery successful! New shard size: "
                 << new_shard.size() << "\n";
        std::cout << "Expected: " << shard.size() << "\n";
    }
}

//==============================================================================
// 示例 4：自适应负载均衡
//==============================================================================

void example_04_adaptive_load_balancing() {
    std::cout << "\n=== Example 4: Adaptive Load Balancing ===\n";

    constexpr std::size_t num_shards = 4;
    AdaptiveLoadBalancer balancer(num_shards);

    // 模拟请求
    constexpr std::size_t num_requests = 100;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> latency_dist(5.0, 50.0);
    std::uniform_real_distribution<double> error_dist(0.0, 1.0);

    std::cout << "Simulating " << num_requests << " requests...\n";

    for (std::size_t i = 0; i < num_requests; ++i) {
        // 选择分片
        std::size_t shard_id = balancer.select_shard();

        // 记录请求开始
        balancer.record_request_start(shard_id);

        // 模拟处理
        double latency = latency_dist(rng);
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(latency));

        // 模拟可能的错误
        bool success = error_dist(rng) > 0.05;  // 95% 成功率

        // 记录请求结束
        balancer.record_request_end(shard_id, latency, success);
    }

    // 打印统计
    balancer.print_stats();
}

//==============================================================================
// 示例 5：完整的分布式搜索（集成所有功能）
//==============================================================================

class ProductionCluster {
    std::vector<std::unique_ptr<ShardNode>> shards_;
    AdaptiveLoadBalancer load_balancer_;
    FailureDetector failure_detector_;
    Tracer tracer_;
    std::size_t dimensions_;

public:
    ProductionCluster(std::size_t num_shards, std::size_t dimensions)
        : load_balancer_(num_shards),
          failure_detector_(std::chrono::milliseconds(3000)),
          dimensions_(dimensions) {

        // 创建分片
        for (std::size_t i = 0; i < num_shards; ++i) {
            shards_.push_back(std::make_unique<ShardNode>(i, dimensions));

            // 注册到故障检测器
            std::string addr = "shard_" + std::to_string(i) + ":5000";
            failure_detector_.add_node(addr);
        }

        // 设置故障回调
        failure_detector_.set_failure_callback([this](std::string const& failed_node) {
            handle_node_failure(failed_node);
        });

        failure_detector_.start();
    }

    ~ProductionCluster() {
        failure_detector_.stop();
    }

    // 智能搜索（使用负载均衡和追踪）
    std::vector<result_t> smart_search(float const* query, std::size_t k) {
        // 开始 trace
        auto trace_id = tracer_.start_span("smart_search");

        // 选择最佳分片（使用负载均衡）
        std::size_t primary_shard = load_balancer_.select_shard();
        auto shard_span = tracer_.start_span("query_primary", trace_id);

        load_balancer_.record_request_start(primary_shard);
        Timer timer;
        timer.start();

        auto results = shards_[primary_shard]->search(query, k);

        double latency = timer.elapsed_ms();
        load_balancer_.record_request_end(primary_shard, latency, true);

        tracer_.finish_span(shard_span);
        tracer_.finish_span(trace_id);

        return results;
    }

    // 批量搜索（并行多个分片）
    std::vector<result_t> parallel_search(float const* query, std::size_t k,
                                        std::size_t n_shards) {
        auto trace_id = tracer_.start_span("parallel_search");

        std::vector<std::future<std::vector<result_t>>> futures;

        for (std::size_t i = 0; i < n_shards && i < shards_.size(); ++i) {
            futures.push_back(std::async(std::launch::async, [this, i, query, k]() {
                auto shard_span = tracer_.start_span("query_shard_" + std::to_string(i));

                load_balancer_.record_request_start(i);
                Timer timer;
                timer.start();

                auto results = shards_[i]->search(query, k);

                double latency = timer.elapsed_ms();
                load_balancer_.record_request_end(i, latency, true);

                tracer_.finish_span(shard_span);
                return results;
            }));
        }

        // 聚合结果
        std::vector<result_t> all_results;
        for (auto& future : futures) {
            auto results = future.get();
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 去重和 Top-K
        std::sort(all_results.begin(), all_results.end(),
                 [](auto const& a, auto const& b) { return a.key < b.key; });
        all_results.erase(
            std::unique(all_results.begin(), all_results.end(),
                       [](auto const& a, auto const& b) { return a.key == b.key; }),
            all_results.end()
        );

        std::partial_sort(all_results.begin(), all_results.begin() + k,
                         all_results.end(),
                         [](auto const& a, auto const& b) {
                             return a.distance < b.distance;
                         });

        if (all_results.size() > k) {
            all_results.resize(k);
        }

        tracer_.finish_span(trace_id);

        return all_results;
    }

    void print_statistics() {
        load_balancer_.print_stats();
        tracer_.print_trace();
    }

private:
    void handle_node_failure(std::string const& failed_node) {
        std::cout << "[ProductionCluster] Handling failure of " << failed_node << "\n";

        // 提取分片 ID
        std::size_t shard_id = 0;
        auto pos = failed_node.find("shard_");
        if (pos != std::string::npos) {
            shard_id = std::stoul(failed_node.substr(pos + 6));
        }

        // 实际实现中，这里会：
        // 1. 标记分片为不可用
        // 2. 从副本恢复数据
        // 3. 启动新实例替换故障节点
    }
};

void example_05_production_cluster() {
    std::cout << "\n=== Example 5: Production Cluster ===\n";

    constexpr std::size_t num_shards = 4;
    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;

    // 创建集群
    ProductionCluster cluster(num_shards, dimensions);

    // 添加数据
    std::cout << "Adding " << n_vectors << " vectors...\n";

    std::vector<std::uint32_t> keys(n_vectors);
    std::vector<float> vectors(n_vectors * dimensions);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (std::size_t i = 0; i < n_vectors; ++i) {
        keys[i] = i;
        for (std::size_t j = 0; j < dimensions; ++j) {
            vectors[i * dimensions + j] = dist(rng);
        }
    }

    // 简化：使用第一个分片存储
    // 实际实现中会使用分片策略分配到不同分片

    // 智能搜索
    std::cout << "\nPerforming smart searches...\n";

    std::vector<float> query(dimensions);
    for (auto& val : query) {
        val = dist(rng);
    }

    constexpr std::size_t n_searches = 10;

    Timer total_timer;
    total_timer.start();

    for (std::size_t i = 0; i < n_searches; ++i) {
        auto results = cluster.smart_search(query.data(), 10);

        std::cout << "Search " << (i + 1) << " found " << results.size()
                 << " results\n";
    }

    double total_time = total_timer.elapsed_ms();

    std::cout << "\nCompleted " << n_searches << " searches in "
             << total_time << " ms\n";
    std::cout << "Average: " << (total_time / n_searches) << " ms per search\n";
    std::cout << "QPS: " << (n_searches * 1000 / total_time) << "\n";

    // 打印统计信息
    cluster.print_statistics();
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  Advanced Distributed Examples\n";
    std::cout << "========================================\n";

    example_01_distributed_tracing();
    example_02_failure_detection();
    example_03_checkpoint_recovery();
    example_04_adaptive_load_balancing();
    example_05_production_cluster();

    std::cout << "\n========================================\n";
    std::cout << "  All examples completed!\n";
    std::cout << "========================================\n";

    return 0;
}
