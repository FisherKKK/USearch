# 分布式向量检索 - 高级主题
## Advanced Distributed Vector Search

---

## 🎯 学习目标

- 掌握分布式一致性协议实现
- 学习分布式索引构建和优化
- 实现高级容错和恢复机制
- 优化跨数据中心部署
- 构建生产级监控系统

---

## 1. 分布式一致性协议

### 1.1 Raft 协议实现

```cpp
/**
 * 基于 Raft 的分布式共识
 * 用于保证多副本之间的数据一致性
 */

class RaftNode {
public:
    enum class State {
        Follower,
        Candidate,
        Leader
    };

    struct LogEntry {
        std::uint64_t index;
        std::uint64_t term;
        enum class Op { Add, Delete } operation;
        vector_key_t key;
        std::vector<float> vector;
    };

private:
    State state_;
    std::uint64_t current_term_;
    std::optional<std::size_t> voted_for_;
    std::vector<LogEntry> log_;

    // Leader 状态
    std::vector<std::uint64_t> next_index_;  // 每个 follower 的下一条日志索引
    std::vector<std::uint64_t> match_index_; // 每个 follower 已复制的最高索引

    // 集群配置
    std::size_t node_id_;
    std::vector<std::string> peers_;
    std::size_t majority_;

    // 超时
    std::chrono::milliseconds election_timeout_;
    std::chrono::milliseconds heartbeat_interval_;

    std::mutex mutex_;
    std::thread background_thread_;
    std::atomic<bool> running_{true};

public:
    RaftNode(std::size_t node_id, std::vector<std::string> peers)
        : state_(State::Follower),
          current_term_(0),
          node_id_(node_id),
          peers_(std::move(peers)),
          majority_(peers_.size() / 2 + 1),
          election_timeout_(150 + std::rand() % 150),
          heartbeat_interval_(50) {

        next_index_.resize(peers_.size(), 1);
        match_index_.resize(peers_.size(), 0);

        background_thread_ = std::thread([this] { run(); });
    }

    ~RaftNode() {
        running_ = false;
        if (background_thread_.joinable()) {
            background_thread_.join();
        }
    }

    // 客户端请求：添加向量
    bool add_vector(vector_key_t key, float const* vector, std::size_t dimensions) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != State::Leader) {
            // 重定向到 leader
            return false;
        }

        // 追加日志
        LogEntry entry{
            .index = log_.size() + 1,
            .term = current_term_,
            .operation = LogEntry::Op::Add,
            .key = key,
            .vector = std::vector<float>(vector, vector + dimensions)
        };
        log_.push_back(entry);

        // 复制到 followers
        return replicate_log();
    }

private:
    void run() {
        while (running_) {
            switch (state_) {
                case State::Follower:
                    run_follower();
                    break;
                case State::Candidate:
                    run_candidate();
                    break;
                case State::Leader:
                    run_leader();
                    break;
            }
        }
    }

    void run_follower() {
        // 等待 leader 的心跳或 RPC
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, election_timeout_) == std::cv_status::timeout) {
            // 超时，转为 candidate
            state_ = State::Candidate;
        }
    }

    void run_candidate() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 增加任期
        current_term_++;
        voted_for_ = node_id_;

        // 请求投票
        std::size_t votes = 1;  // 投给自己
        for (auto const& peer : peers_) {
            if (request_vote(peer, current_term_)) {
                votes++;
            }
        }

        if (votes >= majority_) {
            // 成为 leader
            state_ = State::Leader;
            become_leader();
        } else {
            // 退回 follower
            state_ = State::Follower;
        }
    }

    void run_leader() {
        // 发送心跳
        for (auto const& peer : peers_) {
            send_heartbeat(peer);
        }

        std::this_thread::sleep_for(heartbeat_interval_);
    }

    void become_leader() {
        // 初始化 leader 状态
        std::fill(next_index_.begin(), next_index_.end(), log_.size() + 1);
        std::fill(match_index_.begin(), match_index_.end(), 0);
    }

    bool replicate_log() {
        std::size_t successes = 1;  // 自己

        for (std::size_t i = 0; i < peers_.size(); ++i) {
            if (append_entry(i, log_[next_index_[i] - 1])) {
                next_index_[i]++;
                match_index_[i]++;
                successes++;
            }
        }

        return successes >= majority_;
    }

    bool request_vote(std::string const& peer, std::uint64_t term) {
        // 实际应通过 RPC 调用
        // 这里简化为随机
        return (std::rand() % 2) == 0;
    }

    bool append_entry(std::size_t peer_id, LogEntry const& entry) {
        // 实际应通过 RPC 调用
        // 这里简化为随机
        return (std::rand() % 10) > 2;  // 90% 成功率
    }

    void send_heartbeat(std::string const& peer) {
        // 实际应发送 AppendEntries RPC（空日志）
    }

    std::condition_variable cv_;
};
```

### 1.2 向量时钟（Vector Clock）

```cpp
/**
 * 向量时钟：用于检测分布式系统中的事件顺序
 */

class VectorClock {
    using Clock = std::map<std::size_t, std::uint64_t>;

    Clock clock_;
    std::size_t node_id_;

public:
    VectorClock(std::size_t node_id) : node_id_(node_id) {
        clock_[node_id_] = 0;
    }

    // 事件发生：增加自己的时钟
    void tick() {
        clock_[node_id_]++;
    }

    // 发送消息：返回副本
    VectorClock send() const {
        VectorClock copy = *this;
        copy.tick();
        return copy;
    }

    // 接收消息：合并时钟
    void receive(VectorClock const& other) {
        tick();
        for (auto const& [node, timestamp] : other.clock_) {
            clock_[node] = std::max(clock_[node], timestamp);
        }
    }

    // 比较两个时钟
    enum class Order { Before, After, Concurrent, Equal };

    static Order compare(VectorClock const& a, VectorClock const& b) {
        bool a_before_b = true;
        bool b_before_a = true;

        // 检查所有节点
        for (auto const& [node, _] : a.clock_) {
            auto a_time = a.get(node);
            auto b_time = b.get(node);

            if (a_time > b_time) {
                b_before_a = false;
            } else if (a_time < b_time) {
                a_before_b = false;
            }
        }

        if (a_before_b && !b_before_a) {
            return Order::Before;
        } else if (!a_before_b && b_before_a) {
            return Order::After;
        } else if (!a_before_b && !b_before_a) {
            return Order::Concurrent;
        } else {
            return Order::Equal;
        }
    }

    std::uint64_t get(std::size_t node) const {
        auto it = clock_.find(node);
        return (it != clock_.end()) ? it->second : 0;
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "{";
        for (auto const& [node, time] : clock_) {
            oss << node << ":" << time << ",";
        }
        oss << "}";
        return oss.str();
    }
};
```

---

## 2. 分布式索引构建

### 2.1 MapReduce 风格的索引构建

```cpp
/**
 * 分布式索引构建器
 * 使用 MapReduce 模式并行构建大规模索引
 */

class DistributedIndexBuilder {
    struct MapTask {
        std::size_t task_id;
        std::size_t shard_id;
        std::vector<vector_key_t> keys;
        std::vector<float> vectors;
    };

    struct ReduceTask {
        std::size_t shard_id;
        std::vector<std::vector<float>> vectors;  // 来自不同 map tasks
        std::vector<vector_key_t> keys;
    };

    std::vector<std::unique_ptr<ShardNode>> shards_;
    std::size_t num_mappers_;
    std::size_t num_reducers_;

public:
    DistributedIndexBuilder(std::vector<std::unique_ptr<ShardNode>> shards,
                           std::size_t num_mappers)
        : shards_(std::move(shards)),
          num_mappers_(num_mappers),
          num_reducers_(shards_.size()) {}

    // Map 阶段：读取数据并分区
    std::vector<MapTask> map(std::string const& input_file, std::size_t dimensions) {
        std::vector<MapTask> map_tasks;

        // 1. 读取输入数据
        auto [keys, vectors] = read_vectors(input_file, dimensions);

        // 2. 分割成多个 map tasks
        std::size_t vectors_per_task = vectors.size() / num_mappers_;

        for (std::size_t i = 0; i < num_mappers_; ++i) {
            std::size_t start = i * vectors_per_task;
            std::size_t end = (i == num_mappers_ - 1) ? vectors.size() : (i + 1) * vectors_per_task;

            MapTask task;
            task.task_id = i;
            task.keys.assign(keys.begin() + start, keys.begin() + end);
            task.vectors.assign(vectors.begin() + start * dimensions,
                              vectors.begin() + end * dimensions);

            map_tasks.push_back(std::move(task));
        }

        return map_tasks;
    }

    // Shuffle 阶段：按分片 ID 分组数据
    std::vector<ReduceTask> shuffle(std::vector<MapTask>&& map_tasks,
                                    std::size_t dimensions) {
        std::vector<ReduceTask> reduce_tasks(num_reducers_);

        for (auto& task : map_tasks) {
            for (std::size_t i = 0; i < task.keys.size(); ++i) {
                // 确定目标分片
                std::size_t shard_id = hash_key(task.keys[i]) % num_reducers_;

                // 添加到对应的 reduce task
                reduce_tasks[shard_id].shard_id = shard_id;
                reduce_tasks[shard_id].keys.push_back(task.keys[i]);

                float const* vec = task.vectors.data() + i * dimensions;
                reduce_tasks[shard_id].vectors.push_back(
                    std::vector<float>(vec, vec + dimensions)
                );
            }
        }

        return reduce_tasks;
    }

    // Reduce 阶段：构建每个分片的索引
    void reduce(std::vector<ReduceTask>&& reduce_tasks) {
        std::vector<std::future<void>> futures;

        for (auto& task : reduce_tasks) {
            futures.push_back(std::async(std::launch::async, [this, task]() mutable {
                // 合并向量
                std::size_t total_vectors = task.keys.size();
                std::vector<float> all_vectors;

                for (auto const& vec : task.vectors) {
                    all_vectors.insert(all_vectors.end(), vec.begin(), vec.end());
                }

                // 批量添加到分片
                shards_[task.shard_id]->add_batch(
                    task.keys.data(),
                    all_vectors.data(),
                    total_vectors
                );

                std::cout << "Shard " << task.shard_id
                         << " built with " << total_vectors << " vectors\n";
            }));
        }

        // 等待所有 reduce tasks 完成
        for (auto& future : futures) {
            future.get();
        }
    }

    // 完整的 MapReduce 流程
    void build(std::string const& input_file, std::size_t dimensions) {
        Timer timer;
        timer.start();

        std::cout << "Starting distributed index build...\n\n";

        // Map
        std::cout << "Map phase...\n";
        auto map_tasks = map(input_file, dimensions);
        double map_time = timer.elapsed_ms();
        std::cout << "Map completed in " << map_time << " ms\n\n";

        // Shuffle
        std::cout << "Shuffle phase...\n";
        auto reduce_tasks = shuffle(std::move(map_tasks), dimensions);
        double shuffle_time = timer.elapsed_ms() - map_time;
        std::cout << "Shuffle completed in " << shuffle_time << " ms\n\n";

        // Reduce
        std::cout << "Reduce phase...\n";
        reduce(std::move(reduce_tasks));
        double reduce_time = timer.elapsed_ms() - map_time - shuffle_time;
        std::cout << "Reduce completed in " << reduce_time << " ms\n\n";

        std::cout << "Total build time: " << timer.elapsed_ms() << " ms\n";
    }

private:
    std::size_t hash_key(vector_key_t key) const {
        return std::hash<vector_key_t>{}(key);
    }

    std::pair<std::vector<vector_key_t>, std::vector<float>>
    read_vectors(std::string const& file, std::size_t dimensions) {
        // 简化：生成随机数据
        constexpr std::size_t n = 100000;
        std::vector<vector_key_t> keys(n);
        std::vector<float> vectors(n * dimensions);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (std::size_t i = 0; i < n; ++i) {
            keys[i] = i;
            for (std::size_t j = 0; j < dimensions; ++j) {
                vectors[i * dimensions + j] = dist(rng);
            }
        }

        return {keys, vectors};
    }
};
```

### 2.2 增量索引构建

```cpp
/**
 * 增量索引构建
 * 支持在线添加新向量而不中断服务
 */

class IncrementalIndexBuilder {
    struct BuildConfig {
        std::size_t batch_size = 10000;
        std::size_t max_queue_size = 100000;
        double trigger_ratio = 0.8;  // 队列达到 80% 时触发构建
    };

    ShardNode& shard_;
    BuildConfig config_;

    std::queue<vector_key_t> pending_keys_;
    std::queue<std::vector<float>> pending_vectors_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread builder_thread_;
    std::atomic<bool> running_{true};

public:
    IncrementalIndexBuilder(ShardNode& shard, BuildConfig config = {})
        : shard_(shard), config_(config) {

        builder_thread_ = std::thread([this] { run(); });
    }

    ~IncrementalIndexBuilder() {
        running_ = false;
        cv_.notify_all();
        if (builder_thread_.joinable()) {
            builder_thread_.join();
        }
    }

    // 添加向量到待构建队列
    void add(vector_key_t key, std::vector<float> vector) {
        std::lock_guard<std::mutex> lock(mutex_);

        pending_keys_.push(key);
        pending_vectors_.push(std::move(vector));

        // 检查是否需要触发构建
        if (pending_keys_.size() >= config_.batch_size * config_.trigger_ratio) {
            cv_.notify_one();
        }
    }

    // 强制刷新
    void flush() {
        cv_.notify_one();
    }

private:
    void run() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待足够的数据或停止信号
            cv_.wait(lock, [this] {
                return pending_keys_.size() >= config_.batch_size || !running_;
            });

            if (!running_ && pending_keys_.empty()) {
                break;
            }

            // 收集批次
            std::vector<vector_key_t> keys;
            std::vector<float> vectors;

            std::size_t batch_size = std::min({
                config_.batch_size,
                pending_keys_.size(),
                config_.max_queue_size
            });

            for (std::size_t i = 0; i < batch_size; ++i) {
                keys.push_back(pending_keys_.front());
                pending_keys_.pop();

                vectors.insert(vectors.end(),
                             pending_vectors_.front().begin(),
                             pending_vectors_.front().end());
                pending_vectors_.pop();
            }

            lock.unlock();

            // 构建索引（不持有锁）
            if (!keys.empty()) {
                std::cout << "Building batch of " << keys.size() << " vectors...\n";
                shard_.add_batch(keys.data(), vectors.data(), keys.size());
            }
        }
    }
};
```

---

## 3. 高级容错机制

### 3.1 故障检测和恢复

```cpp
/**
 * 故障检测器
 * 使用心跳机制检测节点故障
 */

class FailureDetector {
    struct NodeStatus {
        std::string address;
        std::chrono::steady_clock::time_point last_heartbeat;
        bool is_alive;
        std::size_t failure_count;
    };

    std::map<std::string, NodeStatus> nodes_;
    std::chrono::milliseconds heartbeat_timeout_;
    std::thread detector_thread_;
    std::atomic<bool> running_{true};

    std::mutex mutex_;
    std::function<void(std::string const&)> on_failure_;

public:
    FailureDetector(std::chrono::milliseconds timeout)
        : heartbeat_timeout_(timeout) {

        detector_thread_ = std::thread([this] { detect(); });
    }

    ~FailureDetector() {
        running_ = false;
        if (detector_thread_.joinable()) {
            detector_thread_.join();
        }
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

private:
    void detect() {
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
                        // 首次检测到故障
                        status.is_alive = false;
                        status.failure_count++;

                        std::cout << "Node " << address << " may have failed\n";

                        if (status.failure_count >= 3 && on_failure_) {
                            // 确认故障
                            std::cout << "Node " << address << " confirmed failed!\n";
                            on_failure_(address);
                        }
                    }
                }
            }
        }
    }
};

/**
 * 自动恢复管理器
 */

class RecoveryManager {
    struct ShardReplica {
        std::size_t shard_id;
        std::string primary_address;
        std::string replica_address;
        bool is_replica_valid;
    };

    std::vector<ShardReplica> replicas_;
    std::map<std::size_t, std::vector<std::string>> shard_to_nodes_;

public:
    // 为每个分片配置副本
    void add_replica(std::size_t shard_id,
                    std::string const& primary,
                    std::string const& replica) {

        replicas_.push_back({
            .shard_id = shard_id,
            .primary_address = primary,
            .replica_address = replica,
            .is_replica_valid = true
        });

        shard_to_nodes_[shard_id] = {primary, replica};
    }

    // 处理节点故障
    void handle_failure(std::string const& failed_address) {
        std::cout << "Handling failure of " << failed_address << "\n";

        // 1. 找到受影响的分片
        for (auto& replica : replicas_) {
            if (replica.primary_address == failed_address) {
                // 主节点故障，提升副本
                promote_replica(replica);
            } else if (replica.replica_address == failed_address) {
                // 副本故障，重建副本
                rebuild_replica(replica);
            }
        }
    }

private:
    void promote_replica(ShardReplica& replica) {
        std::cout << "Promoting replica for shard " << replica.shard_id << "\n";

        // 1. 更新路由表（将流量导向新主）
        // 2. 创建新副本
        // 3. 同步数据

        replica.is_replica_valid = false;
    }

    void rebuild_replica(ShardReplica& replica) {
        std::cout << "Rebuilding replica for shard " << replica.shard_id << "\n";

        // 1. 从主节点复制数据
        // 2. 验证一致性
        // 3. 标记为有效

        replica.is_replica_valid = true;
    }
};
```

### 3.2 检查点机制

```cpp
/**
 * 检查点管理器
 * 定期保存索引状态以加快恢复速度
 */

class CheckpointManager {
    struct Checkpoint {
        std::uint64_t id;
        std::chrono::system_clock::time_point timestamp;
        std::string path;
        std::size_t num_vectors;
    };

    ShardNode& shard_;
    std::string checkpoint_dir_;
    std::chrono::minutes checkpoint_interval_;
    std::size_t max_checkpoints_;

    std::vector<Checkpoint> checkpoints_;
    std::thread checkpoint_thread_;
    std::atomic<bool> running_{true};
    std::mutex mutex_;

public:
    CheckpointManager(ShardNode& shard,
                     std::string const& dir,
                     std::chrono::minutes interval = std::chrono::minutes(10),
                     std::size_t max_checkpoints = 3)
        : shard_(shard),
          checkpoint_dir_(dir),
          checkpoint_interval_(interval),
          max_checkpoints_(max_checkpoints) {

        checkpoint_thread_ = std::thread([this] { run(); });
    }

    ~CheckpointManager() {
        running_ = false;
        if (checkpoint_thread_.joinable()) {
            checkpoint_thread_.join();
        }
    }

    // 创建检查点
    Checkpoint create_checkpoint() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::uint64_t checkpoint_id = checkpoints_.empty() ? 1 : checkpoints_.back().id + 1;
        std::string path = checkpoint_dir_ + "/checkpoint_" + std::to_string(checkpoint_id) + ".usearch";

        std::cout << "Creating checkpoint " << checkpoint_id << "...\n";

        // 保存索引
        if (shard_.save(path)) {
            Checkpoint checkpoint{
                .id = checkpoint_id,
                .timestamp = std::chrono::system_clock::now(),
                .path = path,
                .num_vectors = shard_.size()
            };

            checkpoints_.push_back(checkpoint);

            // 清理旧检查点
            cleanup_old_checkpoints();

            std::cout << "Checkpoint " << checkpoint_id << " created with "
                     << checkpoint.num_vectors << " vectors\n";

            return checkpoint;
        } else {
            throw std::runtime_error("Failed to create checkpoint");
        }
    }

    // 恢复到最新检查点
    bool restore_to_latest() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (checkpoints_.empty()) {
            std::cout << "No checkpoints available\n";
            return false;
        }

        auto& latest = checkpoints_.back();
        std::cout << "Restoring from checkpoint " << latest.id << "...\n";

        return shard_.load(latest.path);
    }

    // 恢复到指定检查点
    bool restore_to_checkpoint(std::uint64_t checkpoint_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = std::find_if(checkpoints_.begin(), checkpoints_.end(),
                              [checkpoint_id](auto const& cp) {
                                  return cp.id == checkpoint_id;
                              });

        if (it == checkpoints_.end()) {
            std::cout << "Checkpoint " << checkpoint_id << " not found\n";
            return false;
        }

        std::cout << "Restoring from checkpoint " << checkpoint_id << "...\n";
        return shard_.load(it->path);
    }

private:
    void run() {
        while (running_) {
            std::this_thread::sleep_for(checkpoint_interval_);
            if (running_) {
                try {
                    create_checkpoint();
                } catch (std::exception const& e) {
                    std::cerr << "Checkpoint failed: " << e.what() << "\n";
                }
            }
        }
    }

    void cleanup_old_checkpoints() {
        while (checkpoints_.size() > max_checkpoints_) {
            auto& oldest = checkpoints_.front();

            // 删除文件
            std::filesystem::remove(oldest.path);

            std::cout << "Removed old checkpoint " << oldest.id << "\n";

            checkpoints_.erase(checkpoints_.begin());
        }
    }
};
```

---

## 4. 跨数据中心部署

### 4.1 多区域架构

```cpp
/**
 * 多区域部署管理器
 * 支持跨数据中心的低延迟查询
 */

class GeoDistributedCluster {
    struct Region {
        std::string name;
        std::string location;  // 例如 "us-east-1"
        std::vector<std::unique_ptr<ShardNode>> shards;
        double latitude;
        double longitude;
    };

    std::vector<Region> regions_;

    // 地理路由
    struct ClientInfo {
        std::string client_ip;
        double latitude;
        double longitude;
    };

public:
    void add_region(std::string const& name,
                   std::string const& location,
                   double lat, double lon,
                   std::size_t num_shards,
                   std::size_t dimensions) {

        Region region;
        region.name = name;
        region.location = location;
        region.latitude = lat;
        region.longitude = lon;

        for (std::size_t i = 0; i < num_shards; ++i) {
            region.shards.push_back(std::make_unique<ShardNode>(i, dimensions));
        }

        regions_.push_back(std::move(region));
    }

    // 基于地理位置的路由
    Region* find_nearest_region(ClientInfo const& client) {
        Region* nearest = nullptr;
        double min_distance = std::numeric_limits<double>::max();

        for (auto& region : regions_) {
            double dist = haversine_distance(
                client.latitude, client.longitude,
                region.latitude, region.longitude
            );

            if (dist < min_distance) {
                min_distance = dist;
                nearest = &region;
            }
        }

        return nearest;
    }

    // 在最近区域搜索
    std::vector<result_t> search_nearest(ClientInfo const& client,
                                        float const* query,
                                        std::size_t k) {
        auto* region = find_nearest_region(client);
        if (!region) {
            throw std::runtime_error("No regions available");
        }

        std::cout << "Routing to region: " << region->name << "\n";

        // 搜索该区域的所有分片
        std::vector<result_t> all_results;

        for (auto& shard : region->shards) {
            auto results = shard->search(query, k);
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // Top-K
        std::partial_sort(all_results.begin(), all_results.begin() + k,
                         all_results.end());
        all_results.resize(k);

        return all_results;
    }

    // 跨区域搜索（更高召回率）
    std::vector<result_t> search_global(float const* query,
                                       std::size_t k,
                                       std::size_t n_regions) {
        std::vector<std::future<std::vector<result_t>>> futures;

        // 并行查询多个区域
        for (std::size_t i = 0; i < std::min(n_regions, regions_.size()); ++i) {
            futures.push_back(std::async(std::launch::async, [&]() {
                std::vector<result_t> region_results;

                for (auto& shard : regions_[i].shards) {
                    auto results = shard->search(query, k);
                    region_results.insert(region_results.end(),
                                        results.begin(), results.end());
                }

                return region_results;
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
                         all_results.end());
        all_results.resize(k);

        return all_results;
    }

private:
    // Haversine 距离（公里）
    double haversine_distance(double lat1, double lon1,
                             double lat2, double lon2) const {
        const double R = 6371.0;  // 地球半径（公里）

        double dlat = (lat2 - lat1) * M_PI / 180.0;
        double dlon = (lon2 - lon1) * M_PI / 180.0;

        double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
                   std::sin(dlon / 2) * std::sin(dlon / 2);

        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
        return R * c;
    }
};
```

---

## 5. 分布式性能监控

### 5.1 指标收集系统

```cpp
/**
 * 分布式监控系统
 * 收集和分析集群性能指标
 */

class ClusterMonitor {
    struct ShardMetrics {
        std::size_t shard_id;
        std::atomic<std::uint64_t> query_count{0};
        std::atomic<std::uint64_t> total_latency_us{0};
        std::atomic<std::uint64_t> error_count{0};
        std::atomic<std::size_t> current_connections{0};
        std::atomic<std::size_t> memory_usage{0};

        // P99 延迟计算
        std::vector<std::uint64_t> recent_latencies;
        std::mutex latencies_mutex;
    };

    std::vector<ShardMetrics> shard_metrics_;

    // 集群级指标
    std::atomic<std::uint64_t> cluster_query_count_{0};
    std::atomic<std::uint64_t> cluster_error_count_{0};

public:
    ClusterMonitor(std::size_t num_shards) {
        for (std::size_t i = 0; i < num_shards; ++i) {
            shard_metrics_.push_back({.shard_id = i});
        }
    }

    // 记录查询
    void record_query(std::size_t shard_id, std::uint64_t latency_us, bool success) {
        if (shard_id >= shard_metrics_.size()) return;

        auto& metrics = shard_metrics_[shard_id];
        metrics.query_count.fetch_add(1, std::memory_order_relaxed);
        metrics.total_latency_us.fetch_add(latency_us, std::memory_order_relaxed);

        if (!success) {
            metrics.error_count.fetch_add(1, std::memory_order_relaxed);
        }

        // 记录延迟用于 P99 计算
        {
            std::lock_guard<std::mutex> lock(metrics.latencies_mutex);
            metrics.recent_latencies.push_back(latency_us);
            if (metrics.recent_latencies.size() > 1000) {
                metrics.recent_latencies.erase(metrics.recent_latencies.begin());
            }
        }

        cluster_query_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // 计算 P99 延迟
    double calculate_p99_latency(std::size_t shard_id) const {
        if (shard_id >= shard_metrics_.size()) return 0.0;

        auto& metrics = shard_metrics_[shard_id];

        std::lock_guard<std::mutex> lock(metrics.latencies_mutex);
        if (metrics.recent_latencies.empty()) return 0.0;

        std::vector<std::uint64_t> sorted = metrics.recent_latencies;
        std::sort(sorted.begin(), sorted.end());

        std::size_t p99_index = sorted.size() * 99 / 100;
        return sorted[p99_index];
    }

    // 生成 Prometheus 格式的指标
    std::string export_prometheus_metrics() const {
        std::ostringstream oss;

        for (auto const& metrics : shard_metrics_) {
            std::uint64_t query_count = metrics.query_count.load();
            std::uint64_t total_latency = metrics.total_latency_us.load();
            std::uint64_t error_count = metrics.error_count.load();
            double avg_latency = query_count > 0 ?
                static_cast<double>(total_latency) / query_count : 0.0;
            double p99_latency = calculate_p99_latency(metrics.shard_id);

            oss << "usearch_shard_queries{shard=\"" << metrics.shard_id << "\"} "
                << query_count << "\n";
            oss << "usearch_shard_avg_latency_us{shard=\"" << metrics.shard_id << "\"} "
                << avg_latency << "\n";
            oss << "usearch_shard_p99_latency_us{shard=\"" << metrics.shard_id << "\"} "
                << p99_latency << "\n";
            oss << "usearch_shard_errors{shard=\"" << metrics.shard_id << "\"} "
                << error_count << "\n";
        }

        oss << "usearch_cluster_queries_total " << cluster_query_count_.load() << "\n";
        oss << "usearch_cluster_errors_total " << cluster_error_count_.load() << "\n";

        return oss.str();
    }

    // 生成人类可读的报告
    void print_report() const {
        std::cout << "\n=== Cluster Performance Report ===\n\n";

        std::cout << std::left << std::setw(10) << "Shard"
                  << std::setw(15) << "Queries"
                  << std::setw(15) << "Avg Latency"
                  << std::setw(15) << "P99 Latency"
                  << std::setw(10) << "Errors"
                  << "\n";
        std::cout << std::string(65, '-') << "\n";

        for (auto const& metrics : shard_metrics_) {
            std::uint64_t query_count = metrics.query_count.load();
            std::uint64_t total_latency = metrics.total_latency_us.load();
            double avg_latency = query_count > 0 ?
                static_cast<double>(total_latency) / query_count : 0.0;
            double p99_latency = calculate_p99_latency(metrics.shard_id);
            std::uint64_t error_count = metrics.error_count.load();

            std::cout << std::left << std::setw(10) << metrics.shard_id
                      << std::setw(15) << query_count
                      << std::setw(15) << std::fixed << std::setprecision(2) << avg_latency
                      << std::setw(15) << p99_latency
                      << std::setw(10) << error_count
                      << "\n";
        }

        std::cout << "\nTotal Queries: " << cluster_query_count_.load() << "\n";
        std::cout << "Total Errors: " << cluster_error_count_.load() << "\n";
    }
};
```

### 5.2 分布式追踪

```cpp
/**
 * 分布式追踪系统
 * 追踪请求在集群中的传播路径
 */

class DistributedTracer {
    struct Span {
        std::string trace_id;
        std::string span_id;
        std::string parent_span_id;
        std::string operation_name;
        std::chrono::system_clock::time_point start_time;
        std::chrono::microseconds duration;
        std::map<std::string, std::string> tags;
    };

    std::vector<Span> spans_;
    std::mutex mutex_;

public:
    // 开始新的 trace
    std::string start_trace(std::string const& operation_name) {
        Span span;
        span.trace_id = generate_id();
        span.span_id = generate_id();
        span.operation_name = operation_name;
        span.start_time = std::chrono::system_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        spans_.push_back(span);

        return span.trace_id;
    }

    // 创建子 span
    std::string start_child_span(std::string const& parent_trace_id,
                                 std::string const& parent_span_id,
                                 std::string const& operation_name) {
        Span span;
        span.trace_id = parent_trace_id;
        span.span_id = generate_id();
        span.parent_span_id = parent_span_id;
        span.operation_name = operation_name;
        span.start_time = std::chrono::system_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        spans_.push_back(span);

        return span.span_id;
    }

    // 完成 span
    void finish_span(std::string const& trace_id, std::string const& span_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = std::find_if(spans_.begin(), spans_.end(),
                              [&](Span const& s) {
                                  return s.trace_id == trace_id && s.span_id == span_id;
                              });

        if (it != spans_.end()) {
            auto end_time = std::chrono::system_clock::now();
            it->duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - it->start_time
            );
        }
    }

    // 打印 trace 树
    void print_trace(std::string const& trace_id) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "\n=== Trace: " << trace_id << " ===\n";

        for (auto const& span : spans_) {
            if (span.trace_id == trace_id) {
                std::cout << "  " << span.operation_name
                         << " (" << span.duration.count() << " us)\n";

                if (!span.parent_span_id.empty()) {
                    std::cout << "    Parent: " << span.parent_span_id << "\n";
                }

                for (auto const& [key, value] : span.tags) {
                    std::cout << "    " << key << ": " << value << "\n";
                }
            }
        }
    }

private:
    std::string generate_id() const {
        static std::atomic<std::uint64_t> counter{0};
        return std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
    }
};
```

---

## 6. 性能优化案例

### 6.1 批处理优化

```cpp
/**
 * 智能批处理管理器
 * 动态调整批大小以优化吞吐量
 */

class AdaptiveBatchManager {
    std::size_t min_batch_size_;
    std::size_t max_batch_size_;
    std::size_t current_batch_size_;

    // 性能历史
    struct PerformanceHistory {
        std::size_t batch_size;
        double throughput;  // ops/sec
    };

    std::vector<PerformanceHistory> history_;

public:
    AdaptiveBatchManager(std::size_t min_size = 10,
                        std::size_t max_size = 10000)
        : min_batch_size_(min_size),
          max_batch_size_(max_size),
          current_batch_size_(min_size) {}

    std::size_t get_batch_size() const {
        return current_batch_size_;
    }

    // 记录性能并调整批大小
    void record_performance(std::size_t batch_size, double throughput) {
        history_.push_back({batch_size, throughput});

        if (history_.size() < 5) {
            return;  // 需要足够的历史数据
        }

        // 分析趋势
        adjust_batch_size();
    }

private:
    void adjust_batch_size() {
        // 简单的启发式：增加批大小，直到吞吐量不再提升
        auto last = history_.back();
        auto prev = history_[history_.size() - 2];

        if (last.throughput > prev.throughput * 1.05) {
            // 提升超过 5%，继续增加批大小
            current_batch_size_ = std::min(
                current_batch_size_ * 2,
                max_batch_size_
            );
        } else if (last.throughput < prev.throughput * 0.95) {
            // 下降超过 5%，减少批大小
            current_batch_size_ = std::max(
                current_batch_size_ / 2,
                min_batch_size_
            );
        }
    }
};
```

### 6.2 连接池优化

```cpp
/**
 * 连接池
 * 复用网络连接以减少开销
 */

template <typename Connection>
class ConnectionPool {
    struct PooledConnection {
        std::unique_ptr<Connection> conn;
        std::chrono::system_clock::time_point last_used;
        bool in_use;
    };

    std::string address_;
    std::size_t max_connections_;
    std::chrono::seconds idle_timeout_;

    std::vector<PooledConnection> pool_;
    std::mutex mutex_;

public:
    ConnectionPool(std::string const& address,
                  std::size_t max_connections = 10,
                  std::chrono::seconds idle_timeout = std::chrono::seconds(60))
        : address_(address),
          max_connections_(max_connections),
          idle_timeout_(idle_timeout) {}

    // 获取连接
    std::unique_ptr<Connection, std::function<void(Connection*)>> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 清理空闲连接
        cleanup_idle();

        // 查找可用连接
        for (auto& pooled : pool_) {
            if (!pooled.in_use) {
                pooled.in_use = true;
                pooled.last_used = std::chrono::system_clock::now();

                return {
                    pooled.conn.get(),
                    [this, &pooled](Connection*) {
                        pooled.in_use = false;
                    }
                };
            }
        }

        // 创建新连接
        if (pool_.size() < max_connections_) {
            auto conn = std::make_unique<Connection>(address_);
            pool_.push_back({
                .conn = std::move(conn),
                .last_used = std::chrono::system_clock::now(),
                .in_use = true
            });

            auto& pooled = pool_.back();
            return {
                pooled.conn.get(),
                [this, &pooled](Connection*) {
                    pooled.in_use = false;
                }
            };
        }

        throw std::runtime_error("Connection pool exhausted");
    }

private:
    void cleanup_idle() {
        auto now = std::chrono::system_clock::now();

        pool_.erase(
            std::remove_if(pool_.begin(), pool_.end(),
                          [&](PooledConnection const& p) {
                              return !p.in_use &&
                                     (now - p.last_used) > idle_timeout_;
                          }),
            pool_.end()
        );
    }
};
```

---

## 7. 测试和验证

### 7.1 混沌测试

```cpp
/**
 * 混沌测试工具
 * 随机注入故障以测试系统健壮性
 */

class ChaosTest {
    enum class FaultType {
        NetworkDelay,
        NetworkPartition,
        NodeCrash,
        HighLatency
    };

    struct FaultInjection {
        FaultType type;
        double probability;  // 0.0 - 1.0
        std::chrono::milliseconds duration;
    };

    std::vector<FaultInjection> fault_injections_;

public:
    void add_fault(FaultType type, double probability,
                  std::chrono::milliseconds duration) {
        fault_injections_.push_back({type, probability, duration});
    }

    // 随机注入故障
    void inject_random_fault() {
        for (auto const& injection : fault_injections_) {
            if ((double)rand() / RAND_MAX < injection.probability) {
                execute_fault(injection);
            }
        }
    }

    // 运行混沌测试
    template <typename Func>
    void run_chaos_test(Func&& test_func, std::size_t iterations) {
        std::cout << "Starting chaos test with " << iterations << " iterations...\n";

        for (std::size_t i = 0; i < iterations; ++i) {
            std::cout << "Iteration " << (i + 1) << "\n";

            // 注入故障
            inject_random_fault();

            // 运行测试
            try {
                test_func();
                std::cout << "  Test passed\n";
            } catch (std::exception const& e) {
                std::cout << "  Test failed: " << e.what() << "\n";
            }

            // 等待恢复
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    void execute_fault(FaultInjection const& injection) {
        switch (injection.type) {
            case FaultType::NetworkDelay:
                std::cout << "  Injecting network delay...\n";
                // 实现延迟逻辑
                break;

            case FaultType::NodeCrash:
                std::cout << "  Injecting node crash...\n";
                // 实现崩溃逻辑
                break;

            default:
                break;
        }
    }
};
```

---

## 8. 总结

### 关键要点

1. **一致性协议**
   - Raft：强一致性，适合关键数据
   - 向量时钟：检测并发更新
   - 最终一致性：高吞吐量场景

2. **容错机制**
   - 心跳检测：快速发现故障
   - 自动故障转移：保证可用性
   - 检查点：加速恢复

3. **性能优化**
   - 批处理：提高吞吐量
   - 连接池：减少连接开销
   - 监控：持续优化

4. **多区域部署**
   - 地理路由：降低延迟
   - 跨区域复制：数据冗余
   - 就近查询：优化用户体验

### 最佳实践

- **监控一切**：建立完善的指标收集系统
- **自动化恢复**：减少人工干预
- **测试故障**：定期进行混沌测试
- **渐进式扩展**：从小规模开始，逐步增长

---

**下一步**：在生产环境中部署和优化你的分布式向量搜索引擎！

**参考项目**：
- etcd: Raft 实现
- Consul: 服务发现
- Cassandra: 分布式数据库
- TiKV: 分布式 KV 存储
