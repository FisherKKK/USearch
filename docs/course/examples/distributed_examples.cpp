/**
 * USearch 分布式向量搜索示例
 *
 * 演示分布式向量搜索引擎的实现
 *
 * 编译：
 * g++ -std=c++17 -O3 -march=native \
 *     -I../../../include \
 *     -pthread \
 *     distributed_examples.cpp -o dist_examples
 *
 * 运行：
 * ./dist_examples
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

using namespace unum::usearch;

//==============================================================================
// 工具类：计时器
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
// 分片节点实现
//==============================================================================

class ShardNode {
    index_dense_gt<float, std::uint32_t> index_;
    std::size_t shard_id_;
    std::size_t dimensions_;
    mutable std::mutex mutex_;
    std::atomic<std::size_t> query_count_{0};

public:
    ShardNode(std::size_t shard_id, std::size_t dimensions)
        : shard_id_(shard_id), dimensions_(dimensions) {
        index_.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k,
                   index_config_t{}.connectivity = 16,
                   index_config_t{}.expansion = 64);
    }

    // 添加单个向量
    bool add(vector_key_t key, float const* vector) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.add(key, vector);
        return true;
    }

    // 批量添加
    bool add_batch(vector_key_t const* keys, float const* vectors, std::size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.add(keys, vectors, count);
        return true;
    }

    // 搜索
    std::vector<result_t> search(float const* query, std::size_t k) {
        query_count_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.search(query, k);
    }

    // 获取统计信息
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.size();
    }

    std::size_t shard_id() const { return shard_id_; }
    std::size_t query_count() const { return query_count_.load(); }

    // 保存索引
    bool save(std::string const& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.save(path.c_str()).error == error_t::success_k;
    }

    // 加载索引
    bool load(std::string const& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.load(path.c_str()).error == error_t::success_k;
    }
};

//==============================================================================
// 分片策略
//==============================================================================

class ShardingStrategy {
public:
    virtual ~ShardingStrategy() = default;

    virtual std::size_t shard_id(vector_key_t key) const = 0;
    virtual std::vector<std::size_t> target_shards(float const* query,
                                                   std::size_t n_probe) const = 0;
    virtual std::string name() const = 0;
};

// 哈希分片
class HashSharding : public ShardingStrategy {
    std::size_t num_shards_;

public:
    HashSharding(std::size_t num_shards) : num_shards_(num_shards) {}

    std::size_t shard_id(vector_key_t key) const override {
        return std::hash<vector_key_t>{}(key) % num_shards_;
    }

    std::vector<std::size_t> target_shards(float const*,
                                           std::size_t n_probe) const override {
        std::vector<std::size_t> shards;
        for (std::size_t i = 0; i < std::min(n_probe, num_shards_); ++i) {
            shards.push_back(i);
        }
        return shards;
    }

    std::string name() const override { return "Hash Sharding"; }
};

// 轮询分片（用于写入均衡）
class RoundRobinSharding : public ShardingStrategy {
    std::size_t num_shards_;
    mutable std::atomic<std::size_t> counter_{0};

public:
    RoundRobinSharding(std::size_t num_shards) : num_shards_(num_shards) {}

    std::size_t shard_id(vector_key_t) const override {
        return counter_.fetch_add(1, std::memory_order_relaxed) % num_shards_;
    }

    std::vector<std::size_t> target_shards(float const*,
                                           std::size_t n_probe) const override {
        std::vector<std::size_t> shards;
        for (std::size_t i = 0; i < std::min(n_probe, num_shards_); ++i) {
            shards.push_back(i);
        }
        return shards;
    }

    std::string name() const override { return "Round-Robin Sharding"; }
};

// 范围分片
class RangeSharding : public ShardingStrategy {
    struct Range {
        vector_key_t min_key;
        vector_key_t max_key;
        std::size_t shard_id;
    };

    std::vector<Range> ranges_;

public:
    RangeSharding(std::size_t num_shards, std::size_t total_keys) {
        vector_key_t keys_per_shard = total_keys / num_shards;

        for (std::size_t i = 0; i < num_shards; ++i) {
            vector_key_t min_key = i * keys_per_shard;
            vector_key_t max_key = (i == num_shards - 1) ? total_keys : (i + 1) * keys_per_shard;
            ranges_.push_back({min_key, max_key, i});
        }
    }

    std::size_t shard_id(vector_key_t key) const override {
        for (auto const& range : ranges_) {
            if (key >= range.min_key && key < range.max_key) {
                return range.shard_id;
            }
        }
        return ranges_.back().shard_id;
    }

    std::vector<std::size_t> target_shards(float const*,
                                           std::size_t n_probe) const override {
        std::vector<std::size_t> shards;
        for (std::size_t i = 0; i < std::min(n_probe, ranges_.size()); ++i) {
            shards.push_back(i);
        }
        return shards;
    }

    std::string name() const override { return "Range Sharding"; }
};

//==============================================================================
// 分布式索引
//==============================================================================

class DistributedIndex {
    std::vector<std::unique_ptr<ShardNode>> shards_;
    std::unique_ptr<ShardingStrategy> strategy_;
    std::size_t dimensions_;
    std::atomic<std::size_t> total_queries_{0};

public:
    DistributedIndex(std::unique_ptr<ShardingStrategy> strategy,
                    std::size_t num_shards,
                    std::size_t dimensions)
        : strategy_(std::move(strategy)),
          dimensions_(dimensions) {

        for (std::size_t i = 0; i < num_shards; ++i) {
            shards_.push_back(std::make_unique<ShardNode>(i, dimensions));
        }

        std::cout << "Created distributed index with " << num_shards
                  << " shards (" << strategy_->name() << ")\n";
    }

    // 添加单个向量
    bool add(vector_key_t key, float const* vector) {
        std::size_t shard_id = strategy_->shard_id(key);
        return shards_[shard_id]->add(key, vector);
    }

    // 批量添加（自动分片）
    bool add_batch(vector_key_t const* keys, float const* vectors, std::size_t count) {
        Timer timer;
        timer.start();

        // 按分片分组
        std::vector<std::vector<std::pair<vector_key_t, float const*>>> groups(shards_.size());

        for (std::size_t i = 0; i < count; ++i) {
            std::size_t shard_id = strategy_->shard_id(keys[i]);
            groups[shard_id].push_back({keys[i], vectors + i * dimensions_});
        }

        // 并行添加到每个分片
        std::vector<std::future<bool>> futures;
        for (std::size_t shard_id = 0; shard_id < shards_.size(); ++shard_id) {
            if (!groups[shard_id].empty()) {
                futures.push_back(std::async(std::launch::async, [this, shard_id, &groups]() {
                    // 准备批量数据
                    std::vector<vector_key_t> shard_keys;
                    std::vector<float> shard_vectors;

                    for (auto& [key, vec] : groups[shard_id]) {
                        shard_keys.push_back(key);
                        shard_vectors.insert(shard_vectors.end(),
                                           vec, vec + dimensions_);
                    }

                    return shards_[shard_id]->add_batch(
                        shard_keys.data(),
                        shard_vectors.data(),
                        shard_keys.size()
                    );
                }));
            }
        }

        // 等待所有完成
        for (auto& future : futures) {
            future.get();
        }

        double elapsed = timer.elapsed_ms();
        std::cout << "Added " << count << " vectors in " << elapsed << " ms ("
                  << (count * 1000 / elapsed) << " vectors/sec)\n";

        return true;
    }

    // 搜索（并行查询多个分片）
    std::vector<result_t> search(float const* query, std::size_t k,
                                std::size_t n_probe = 0) {
        total_queries_.fetch_add(1, std::memory_order_relaxed);

        if (n_probe == 0 || n_probe > shards_.size()) {
            n_probe = shards_.size();
        }

        Timer timer;
        timer.start();

        // 确定要查询的分片
        auto target_shards = strategy_->target_shards(query, n_probe);

        // 并行查询
        std::vector<std::future<std::vector<result_t>>> futures;
        for (auto shard_id : target_shards) {
            futures.push_back(std::async(std::launch::async, [&]() {
                return shards_[shard_id]->search(query, k);
            }));
        }

        // 聚合结果
        std::vector<result_t> all_results;
        for (auto& future : futures) {
            auto results = future.get();
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 去重（按 key）
        std::sort(all_results.begin(), all_results.end(),
                 [](auto const& a, auto const& b) { return a.key < b.key; });
        all_results.erase(
            std::unique(all_results.begin(), all_results.end(),
                       [](auto const& a, auto const& b) { return a.key == b.key; }),
            all_results.end()
        );

        // 全局 Top-K
        std::partial_sort(all_results.begin(), all_results.begin() + k,
                         all_results.end(),
                         [](auto const& a, auto const& b) {
                             return a.distance < b.distance;
                         });

        if (all_results.size() > k) {
            all_results.resize(k);
        }

        double elapsed = timer.elapsed_ms();
        std::cout << "Search completed in " << elapsed << " ms ("
                  << "queried " << target_shards.size() << " shards)\n";

        return all_results;
    }

    // 获取集群统计
    void print_stats() const {
        std::cout << "\n=== Cluster Statistics ===\n";
        std::cout << "Total queries: " << total_queries_.load() << "\n\n";

        std::cout << "Shard distribution:\n";
        for (auto const& shard : shards_) {
            std::cout << "  Shard " << shard->shard_id()
                      << ": " << shard->size() << " vectors, "
                      << shard->query_count() << " queries\n";
        }
        std::cout << "\n";
    }

    // 保存所有分片
    bool save_all(std::string const& base_path) const {
        for (auto const& shard : shards_) {
            std::string path = base_path + "_shard_" +
                             std::to_string(shard->shard_id()) + ".usearch";
            if (!shard->save(path)) {
                std::cerr << "Failed to save shard " << shard->shard_id() << "\n";
                return false;
            }
        }
        return true;
    }

    // 加载所有分片
    bool load_all(std::string const& base_path) {
        for (auto const& shard : shards_) {
            std::string path = base_path + "_shard_" +
                             std::to_string(shard->shard_id()) + ".usearch";
            if (!shard->load(path)) {
                std::cerr << "Failed to load shard " << shard->shard_id() << "\n";
                return false;
            }
        }
        return true;
    }
};

//==============================================================================
// 示例 1：基础分布式搜索
//==============================================================================

void example_01_basic_distributed_search() {
    std::cout << "\n=== Example 1: Basic Distributed Search ===\n";

    constexpr std::size_t num_shards = 4;
    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;
    constexpr std::size_t n_queries = 10;

    // 创建分布式索引
    auto strategy = std::make_unique<HashSharding>(num_shards);
    DistributedIndex cluster(std::move(strategy), num_shards, dimensions);

    // 生成数据
    std::cout << "\nGenerating " << n_vectors << " random vectors...\n";

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

    // 添加向量
    std::cout << "\nAdding vectors to cluster...\n";
    cluster.add_batch(keys.data(), vectors.data(), n_vectors);

    // 打印统计
    cluster.print_stats();

    // 搜索
    std::cout << "\nPerforming " << n_queries << " searches...\n";

    std::vector<float> query(dimensions);
    for (std::size_t q = 0; q < n_queries; ++q) {
        // 生成查询
        for (auto& val : query) {
            val = dist(rng);
        }

        // 搜索
        auto results = cluster.search(query.data(), 10, 2);  // 只查询2个分片

        std::cout << "\nQuery " << (q + 1) << " - Top 5 results:\n";
        for (std::size_t i = 0; i < std::min(results.size(), size_t(5)); ++i) {
            std::cout << "  " << (i + 1) << ". Key: " << results[i].key
                      << ", Distance: " << std::fixed << std::setprecision(4)
                      << results[i].distance << "\n";
        }
    }
}

//==============================================================================
// 示例 2：分片策略对比
//==============================================================================

void example_02_sharding_strategies() {
    std::cout << "\n=== Example 2: Sharding Strategy Comparison ===\n";

    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 5000;
    constexpr std::size_t num_shards = 4;

    // 生成数据
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

    // 测试不同策略
    std::vector<std::unique_ptr<ShardingStrategy>> strategies;
    strategies.push_back(std::make_unique<HashSharding>(num_shards));
    strategies.push_back(std::make_unique<RoundRobinSharding>(num_shards));
    strategies.push_back(std::make_unique<RangeSharding>(num_shards, n_vectors));

    for (auto& strategy : strategies) {
        std::cout << "\n--- " << strategy->name() << " ---\n";

        // 创建集群
        DistributedIndex cluster(
            std::make_unique<HashSharding>(num_shards),  // 复制策略用于测试
            num_shards,
            dimensions
        );

        // 添加数据
        cluster.add_batch(keys.data(), vectors.data(), n_vectors);

        // 搜索
        std::vector<float> query(dimensions);
        for (auto& val : query) {
            val = dist(rng);
        }

        auto results = cluster.search(query.data(), 10);
        std::cout << "Found " << results.size() << " results\n";
    }
}

//==============================================================================
// 示例 3：查询优化（n_probe 参数）
//==============================================================================

void example_03_query_optimization() {
    std::cout << "\n=== Example 3: Query Optimization (n_probe) ===\n";

    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;
    constexpr std::size_t num_shards = 8;

    // 创建集群
    DistributedIndex cluster(
        std::make_unique<HashSharding>(num_shards),
        num_shards,
        dimensions
    );

    // 添加数据
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

    cluster.add_batch(keys.data(), vectors.data(), n_vectors);

    // 测试不同 n_probe 值
    std::vector<float> query(dimensions);
    for (auto& val : query) {
        val = dist(rng);
    }

    std::cout << "\nTesting different n_probe values:\n";
    std::cout << std::left << std::setw(15) << "n_probe"
              << std::setw(15) << "Latency (ms)"
              << std::setw(15) << "Recall"
              << "\n";
    std::cout << std::string(45, '-') << "\n";

    // 基准：查询所有分片
    auto baseline_results = cluster.search(query.data(), 10, num_shards);

    for (std::size_t n_probe : {1, 2, 4, 8}) {
        Timer timer;
        timer.start();

        auto results = cluster.search(query.data(), 10, n_probe);
        double latency = timer.elapsed_ms();

        // 计算召回率（与基准的重叠度）
        std::unordered_set<vector_key_t> baseline_keys;
        for (auto& r : baseline_results) {
            baseline_keys.insert(r.key);
        }

        std::size_t overlap = 0;
        for (auto& r : results) {
            if (baseline_keys.count(r.key)) {
                overlap++;
            }
        }

        double recall = static_cast<double>(overlap) / baseline_results.size();

        std::cout << std::left << std::setw(15) << n_probe
                  << std::setw(15) << std::fixed << std::setprecision(2) << latency
                  << std::setw(15) << std::fixed << std::setprecision(2) << recall
                  << "\n";
    }
}

//==============================================================================
// 示例 4：并发搜索测试
//==============================================================================

void example_04_concurrent_search() {
    std::cout << "\n=== Example 4: Concurrent Search ===\n";

    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;
    constexpr std::size_t num_shards = 4;
    constexpr std::size_t n_concurrent_queries = 100;

    // 创建集群
    DistributedIndex cluster(
        std::make_unique<HashSharding>(num_shards),
        num_shards,
        dimensions
    );

    // 添加数据
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

    cluster.add_batch(keys.data(), vectors.data(), n_vectors);

    // 并发搜索
    std::cout << "\nRunning " << n_concurrent_queries << " concurrent searches...\n";

    std::vector<std::future<std::vector<result_t>>> futures;

    Timer timer;
    timer.start();

    for (std::size_t i = 0; i < n_concurrent_queries; ++i) {
        futures.push_back(std::async(std::launch::async, [&]() {
            std::vector<float> query(dimensions);
            for (auto& val : query) {
                val = dist(rng);
            }
            return cluster.search(query.data(), 10);
        }));
    }

    // 等待所有查询完成
    std::size_t total_results = 0;
    for (auto& future : futures) {
        auto results = future.get();
        total_results += results.size();
    }

    double elapsed = timer.elapsed_ms();

    std::cout << "Completed in " << elapsed << " ms\n";
    std::cout << "Throughput: " << (n_concurrent_queries * 1000 / elapsed) << " QPS\n";
    std::cout << "Average latency: " << (elapsed / n_concurrent_queries) << " ms\n";
}

//==============================================================================
// 示例 5：持久化和加载
//==============================================================================

void example_05_persistence() {
    std::cout << "\n=== Example 5: Persistence ===\n";

    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 1000;
    constexpr std::size_t num_shards = 2;

    // 创建并填充集群
    {
        DistributedIndex cluster(
            std::make_unique<HashSharding>(num_shards),
            num_shards,
            dimensions
        );

        // 添加数据
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

        cluster.add_batch(keys.data(), vectors.data(), n_vectors);

        // 保存
        std::cout << "Saving cluster to disk...\n";
        if (cluster.save_all("/tmp/distributed_index")) {
            std::cout << "Saved successfully!\n";
        }
    }

    // 从磁盘加载
    {
        std::cout << "\nLoading cluster from disk...\n";
        DistributedIndex cluster(
            std::make_unique<HashSharding>(num_shards),
            num_shards,
            dimensions
        );

        if (cluster.load_all("/tmp/distributed_index")) {
            std::cout << "Loaded successfully!\n";
            cluster.print_stats();
        }
    }
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  USearch Distributed Examples\n";
    std::cout << "========================================\n";

    example_01_basic_distributed_search();
    example_02_sharding_strategies();
    example_03_query_optimization();
    example_04_concurrent_search();
    example_05_persistence();

    std::cout << "\n========================================\n";
    std::cout << "  All examples completed!\n";
    std::cout << "========================================\n";

    return 0;
}
