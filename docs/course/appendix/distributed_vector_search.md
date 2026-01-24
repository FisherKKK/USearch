# 分布式向量检索 - 深度指南
## Distributed Vector Search Architecture and Implementation

---

## 🎯 学习目标

- 理解分布式向量搜索的核心挑战
- 掌握主流分布式架构设计模式
- 学习数据分片和复制策略
- 实现分布式 USearch 集群
- 优化分布式环境下的性能

---

## 1. 分布式向量搜索概述

### 1.1 为什么需要分布式？

**单节点限制**：

```cpp
// 单节点内存限制
constexpr std::size_t max_vectors = 100'000'000;  // 1亿向量
constexpr std::size_t dimensions = 768;           // 768维
constexpr std::size_t bytes_per_vector = dimensions * sizeof(float);

// 总内存需求
constexpr std::size_t total_memory = max_vectors * bytes_per_vector;
// ~73 GB (仅向量数据，不含索引结构)
```

**分布式优势**：

| 维度 | 单节点 | 分布式集群 |
|------|--------|-----------|
| 数据规模 | < 1亿向量 | 10亿+ 向量 |
| 吞吐量 | ~1000 QPS | 100,000+ QPS |
| 可用性 | 单点故障 | 高可用 |
| 扩展性 | 垂直扩展 | 水平扩展 |

### 1.2 核心挑战

1. **数据分片（Sharding）**
   - 如何均匀分配数据？
   - 如何处理数据倾斜？
   - 如何支持动态扩容？

2. **查询路由（Query Routing）**
   - 如何快速定位目标节点？
   - 如何减少网络跳数？
   - 如何处理跨节点查询？

3. **一致性（Consistency）**
   - 如何保证数据一致性？
   - 如何处理节点故障？
   - 如何同步索引更新？

4. **性能优化**
   - 如何减少网络延迟？
   - 如何平衡负载？
   - 如何优化通信协议？

---

## 2. 分布式架构模式

### 2.1 架构分类

#### 模式1：无状态代理架构（Stateless Proxy）

```
┌─────────────────────────────────────────┐
│           Load Balancer                 │
└─────────────────┬───────────────────────┘
                  │
    ┌─────────────┴─────────────┐
    │                          │
┌───▼────┐              ┌──────▼──────┐
│ Proxy  │              │   Proxy     │
│  Node  │              │   Node      │
└───┬────┘              └──────┬──────┘
    │                          │
    └──────────┬───────────────┘
               │
    ┌──────────┴──────────┐
    │                     │
┌───▼────┐          ┌────▼─────┐  ┌─────────┐
│Shard 1 │          │ Shard 2  │  │ Shard 3 │
│Node A  │          │ Node B   │  │ Node C  │
└────────┘          └──────────┘  └─────────┘
```

**优点**：
- 无状态，易于扩展
- 故障隔离
- 负载均衡灵活

**缺点**：
- 代理层成为瓶颈
- 增加一跳延迟

**实现**（Milvus 架构）：

```cpp
// 代理节点 - 协调节点
class CoordinatorNode {
    std::vector<std::unique_ptr<ShardClient>> shards_;

public:
    // 添加分片客户端
    void add_shard(std::string const& address) {
        shards_.push_back(std::make_unique<ShardClient>(address));
    }

    // 路由查询到相关分片
    std::vector<result_t> search(
        float const* query,
        std::size_t k,
        std::size_t n_probe  // 探查分片数
    ) {
        std::vector<result_t> all_results;

        // 1. 确定需要查询的分片
        auto target_shards = select_shards(query, n_probe);

        // 2. 并行查询所有分片
        std::vector<std::future<std::vector<result_t>>> futures;
        for (auto& shard : target_shards) {
            futures.push_back(shard->async_search(query, k));
        }

        // 3. 聚合结果
        for (auto& future : futures) {
            auto results = future.get();
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 4. 全局排序
        std::partial_sort(
            all_results.begin(),
            all_results.begin() + k,
            all_results.end(),
            [](auto const& a, auto const& b) { return a.distance < b.distance; }
        );

        all_results.resize(k);
        return all_results;
    }

private:
    // 选择分片策略
    std::vector<ShardClient*> select_shards(float const* query, std::size_t n_probe) {
        // 策略1：随机选择
        std::vector<ShardClient*> selected;
        std::sample(shards_.begin(), shards_.end(),
                    std::back_inserter(selected),
                    std::min(n_probe, shards_.size()),
                    std::mt19937{std::random_device{}()});
        return selected;

        // 策略2：基于 locality-sensitive hashing (LSH)
        // auto shard_id = hash_vector(query) % shards_.size();
        // return {shards_[shard_id].get()};
    }
};
```

#### 模式2：去中心化 P2P 架构

```
    ┌─────┐
    │Node1│────┐
    └──┬──┘    │
       │       │
    ┌──▼──┐    │    ┌─────┐
    │Node2│────┼────│Node4│
    └──┬──┘    │    └──┬──┘
       │       │       │
    ┌──▼──┐    │    ┌──▼──┐
    │Node3│────┘    │Node5│
    └─────┘        └─────┘
```

**优点**：
- 无单点故障
- 高可扩展性
- 低延迟（就近访问）

**缺点**：
- 一致性难以保证
- 路由复杂度高

**实现**（基于 Chord 协议）：

```cpp
// Chord 环分布式哈希表
template <typename Key, typename Value>
class ChordRing {
    struct NodeInfo {
        std::string address;
        std::uint64_t id;  // Hash(IP)
        std::unique_ptr<index_dense_gt<float, std::uint32_t>> index;
    };

    std::map<std::uint64_t, NodeInfo> ring_;

public:
    // 查找负责 key 的节点
    NodeInfo* find_node(Key const& key) {
        std::uint64_t key_hash = hash_key(key);

        // 查找第一个 >= key_hash 的节点
        auto it = ring_.lower_bound(key_hash);
        if (it == ring_.end()) {
            it = ring_.begin();  // 环绕
        }

        return &it->second;
    }

    // 添加向量到对应节点
    void add(vector_key_t key, float const* vector) {
        auto* node = find_node(key);
        node->index->add(key, vector);
    }

    // 搜索（可能需要多个节点）
    std::vector<result_t> search(float const* query, std::size_t k) {
        // 简化版：查询所有节点
        std::vector<result_t> all_results;

        for (auto& [id, node] : ring_) {
            auto results = node.index->search(query, k);
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 全局 Top-K
        std::partial_sort(all_results.begin(), all_results.begin() + k,
                         all_results.end());
        all_results.resize(k);
        return all_results;
    }
};
```

#### 模式3：分层架构（Hierarchical）

```
┌─────────────────────────────────────┐
│         Coordinator Layer           │
│    (Query Routing & Aggregation)    │
└──────────────┬──────────────────────┘
               │
    ┌──────────┴──────────┐
    │                     │
┌───▼────────┐     ┌─────▼──────┐
│ Region 1   │     │ Region 2   │
│ (US-East)  │     │ (EU-West)  │
└───┬────────┘     └─────┬──────┘
    │                    │
    │        ┌───────────┤
    │        │           │
┌───▼───┐ ┌─▼────┐ ┌───▼────┐
│Shard 1│ │Shard 2│ │Shard 3 │
└───────┘ └───────┘ └────────┘
```

**优点**：
- 地理分布优化
- 多租户隔离
- 灵活的访问控制

**缺点**：
- 架构复杂
- 跨区域延迟

---

## 3. 数据分片策略

### 3.1 哈希分片（Hash-Based Sharding）

```cpp
class HashShardingStrategy {
    std::size_t num_shards_;

public:
    // 简单哈希
    std::size_t shard_id(vector_key_t key) const {
        return std::hash<vector_key_t>{}(key) % num_shards_;
    }

    // 一致性哈希（虚拟节点）
    class ConsistentHash {
        struct VirtualNode {
            std::uint64_t hash;
            std::size_t shard_id;
        };

        std::vector<VirtualNode> ring_;
        std::size_t virtual_nodes_per_shard_;

    public:
        ConsistentHash(std::size_t num_shards,
                      std::size_t virtual_nodes = 100)
            : virtual_nodes_per_shard_(virtual_nodes) {

            std::mt19937_64 rng(42);

            // 为每个分片创建虚拟节点
            for (std::size_t shard = 0; shard < num_shards; ++shard) {
                for (std::size_t i = 0; i < virtual_nodes; ++i) {
                    std::uint64_t hash = rng();
                    ring_.push_back({hash, shard});
                }
            }

            // 排序
            std::sort(ring_.begin(), ring_.end(),
                     [](auto const& a, auto const& b) {
                         return a.hash < b.hash;
                     });
        }

        std::size_t find_shard(vector_key_t key) const {
            std::uint64_t key_hash = std::hash<vector_key_t>{}(key);

            // 二分查找
            auto it = std::lower_bound(ring_.begin(), ring_.end(), key_hash,
                                     [](auto const& node, std::uint64_t hash) {
                                         return node.hash < hash;
                                     });

            if (it == ring_.end()) {
                it = ring_.begin();  // 环绕
            }

            return it->shard_id;
        }

        // 添加分片（最小化数据迁移）
        void add_shard(std::size_t new_shard_id) {
            std::mt19937_64 rng(std::random_device{}());

            for (std::size_t i = 0; i < virtual_nodes_per_shard_; ++i) {
                std::uint64_t hash = rng();
                ring_.push_back({hash, new_shard_id});
            }

            std::sort(ring_.begin(), ring_.end(),
                     [](auto const& a, auto const& b) {
                         return a.hash < b.hash;
                     });
        }
    };
};
```

**优点**：
- 数据分布均匀
- 路由简单高效

**缺点**：
- 范围查询需要查询所有分片
- 扩容需要数据迁移

### 3.2 范围分片（Range-Based Sharding）

```cpp
class RangeShardingStrategy {
    struct Range {
        vector_key_t min_key;
        vector_key_t max_key;
        std::size_t shard_id;
    };

    std::vector<Range> ranges_;

public:
    RangeShardingStrategy(std::vector<std::pair<vector_key_t, vector_key_t>> const& ranges) {
        for (std::size_t i = 0; i < ranges.size(); ++i) {
            ranges_.push_back({ranges[i].first, ranges[i].second, i});
        }
    }

    std::size_t shard_id(vector_key_t key) const {
        for (auto const& range : ranges_) {
            if (key >= range.min_key && key < range.max_key) {
                return range.shard_id;
            }
        }
        throw std::runtime_error("Key out of range");
    }

    // 支持范围查询优化
    std::vector<std::size_t> shards_for_range(vector_key_t min, vector_key_t max) const {
        std::vector<std::size_t> shards;
        for (auto const& range : ranges_) {
            if (!(max < range.min_key || min > range.max_key)) {
                shards.push_back(range.shard_id);
            }
        }
        return shards;
    }

    // 动态分裂分片（处理热点）
    void split_shard(std::size_t shard_id, vector_key_t split_point) {
        auto it = std::find_if(ranges_.begin(), ranges_.end(),
                              [shard_id](auto const& r) {
                                  return r.shard_id == shard_id;
                              });

        if (it != ranges_.end()) {
            vector_key_t old_max = it->max_key;

            // 修改原分片范围
            it->max_key = split_point;

            // 添加新分片
            std::size_t new_shard_id = ranges_.size();
            ranges_.push_back({split_point, old_max, new_shard_id});
        }
    }
};
```

**优点**：
- 范围查询高效
- 数据局部性好

**缺点**：
- 数据分布可能不均
- 热点问题

### 3.3 向量内容分片（Content-Based Sharding）

```cpp
// 基于聚类的分片
class ClusterBasedSharding {
    struct ClusterInfo {
        std::size_t shard_id;
        std::vector<float> centroid;  // 聚类中心
        index_dense_gt<float, std::uint32_t> index;
    };

    std::vector<ClusterInfo> clusters_;
    std::size_t dimensions_;

public:
    // 训练聚类（K-Means）
    void train_clusters(
        std::vector<float> const& vectors,
        std::size_t k,
        std::size_t dimensions
    ) {
        dimensions_ = dimensions;
        clusters_.resize(k);

        // 初始化聚类中心
        for (std::size_t i = 0; i < k; ++i) {
            clusters_[i].shard_id = i;
            clusters_[i].centroid.assign(vectors.begin() + i * dimensions,
                                        vectors.begin() + (i + 1) * dimensions);
            clusters_[i].index.init(dimensions, metric_kind_t::cos_k);
        }

        // 简化的 K-Means 迭代
        for (int iter = 0; iter < 10; ++iter) {
            // 1. 分配向量到最近的聚类
            // 2. 更新聚类中心
        }
    }

    // 查找目标分片
    std::size_t find_shard(float const* vector) const {
        std::size_t best_shard = 0;
        float min_distance = std::numeric_limits<float>::max();

        for (auto const& cluster : clusters_) {
            float dist = cosine_distance(vector, cluster.centroid.data());
            if (dist < min_distance) {
                min_distance = dist;
                best_shard = cluster.shard_id;
            }
        }

        return best_shard;
    }

    // 搜索（只查询最近的聚类）
    std::vector<result_t> search(float const* query, std::size_t k,
                                 std::size_t n_probe) {
        // 1. 找到最近的 n_probe 个聚类
        struct ShardDistance {
            std::size_t shard_id;
            float distance;
        };

        std::vector<ShardDistance> shard_distances;
        for (auto const& cluster : clusters_) {
            float dist = cosine_distance(query, cluster.centroid.data());
            shard_distances.push_back({cluster.shard_id, dist});
        }

        std::partial_sort(shard_distances.begin(),
                         shard_distances.begin() + n_probe,
                         shard_distances.end(),
                         [](auto const& a, auto const& b) {
                             return a.distance < b.distance;
                         });

        // 2. 查询这些分片
        std::vector<result_t> all_results;
        for (std::size_t i = 0; i < n_probe && i < shard_distances.size(); ++i) {
            auto& cluster = clusters_[shard_distances[i].shard_id];
            auto results = cluster.index.search(query, k);
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 3. 全局 Top-K
        std::partial_sort(all_results.begin(), all_results.begin() + k,
                         all_results.end());
        all_results.resize(k);
        return all_results;
    }

private:
    float cosine_distance(float const* a, float const* b) const {
        float ab = 0, a2 = 0, b2 = 0;
        for (std::size_t i = 0; i < dimensions_; ++i) {
            ab += a[i] * b[i];
            a2 += a[i] * a[i];
            b2 += b[i] * b[i];
        }
        return 1.0f - ab / (std::sqrt(a2) * std::sqrt(b2));
    }
};
```

**优点**：
- 查询效率高（局部性）
- 支持近似搜索

**缺点**：
- 需要训练聚类
- 数据分布变化时需重新平衡

---

## 4. 数据复制策略

### 4.1 主从复制（Master-Slave）

```cpp
class MasterSlaveReplication {
    struct SlaveNode {
        std::string address;
        std::unique_ptr<RPCClient> client;
        bool is_online;
    };

    index_dense_gt<float, std::uint32_t> master_index_;
    std::vector<SlaveNode> slaves_;

public:
    // 主节点写入
    void add(vector_key_t key, float const* vector) {
        // 1. 写入主节点
        master_index_.add(key, vector);

        // 2. 异步复制到从节点
        for (auto& slave : slaves_) {
            if (slave.is_online) {
                std::thread([&, key] {
                    try {
                        slave.client->async_add(key, vector);
                    } catch (...) {
                        // 记录失败，稍后重试
                    }
                }).detach();
            }
        }
    }

    // 从节点读取（负载均衡）
    std::vector<result_t> search(float const* query, std::size_t k) {
        // 轮询选择从节点
        static std::atomic<std::size_t> counter{0};
        std::size_t slave_idx = counter++ % slaves_.size();

        if (slaves_[slave_idx].is_online) {
            try {
                return slaves_[slave_idx].client->search(query, k);
            } catch (...) {
                // 降级到主节点
            }
        }

        // 降级到主节点
        return master_index_.search(query, k);
    }
};
```

### 4.2 多主复制（Multi-Master）

```cpp
class MultiMasterReplication {
    struct ReplicaNode {
        std::string address;
        std::unique_ptr<RPCClient> client;
        std::uint64_t last_vector_clock;
    };

    std::vector<ReplicaNode> replicas_;
    std::atomic<std::uint64_t> local_clock_;

    struct Operation {
        enum Type { Add, Delete };
        Type type;
        vector_key_t key;
        std::vector<float> vector;
        std::uint64_t timestamp;
        std::uint64_t node_id;
    };

    std::queue<Operation> operation_log_;

public:
    // 写入（任一副本）
    void add(vector_key_t key, float const* vector) {
        // 1. 本地写入
        local_index_.add(key, vector);
        local_clock_.fetch_add(1, std::memory_order_relaxed);

        // 2. 记录操作日志
        operation_log_.push({
            Operation::Add,
            key,
            std::vector<float>(vector, vector + dimensions_),
            local_clock_.load(),
            node_id_
        });

        // 3. 异步传播到其他副本
        propagate_operation(operation_log_.back());
    }

    // 冲突解决（Last-Writer-Wins）
    void resolve_conflict(Operation const& op) {
        auto existing_clock = get_vector_clock(op.key);

        if (op.timestamp > existing_clock) {
            // 应用操作
            if (op.type == Operation::Add) {
                local_index_.add(op.key, op.vector.data());
            }
            update_vector_clock(op.key, op.timestamp);
        }
    }

private:
    void propagate_operation(Operation const& op) {
        for (auto& replica : replicas_) {
            std::thread([&, op] {
                try {
                    replica.client->send_operation(op);
                } catch (...) {
                    // 重试队列
                }
            }).detach();
        }
    }
};
```

### 4.3 Quorum 机制

```cpp
class QuorumReplication {
    std::size_t replication_factor_;  // 例如 N = 3
    std::size_t quorum_size_;         // 例如 W + R > N, W=2, R=2

public:
    QuorumReplication(std::size_t n, std::size_t w, std::size_t r)
        : replication_factor_(n), quorum_size_(w + r) {}

    // 写入（需要 W 个确认）
    bool add(vector_key_t key, float const* vector, std::size_t w) {
        std::atomic<std::size_t> ack_count{0};

        // 并行写入所有副本
        std::vector<std::future<bool>> futures;
        for (auto& replica : replicas_) {
            futures.push_back(std::async(std::launch::async, [&] {
                return replica.client->add(key, vector);
            }));
        }

        // 等待 W 个确认
        for (auto& future : futures) {
            if (future.get()) {
                ack_count.fetch_add(1);
                if (ack_count >= w) {
                    return true;  // 写成功
                }
            }
        }

        return ack_count >= w;
    }

    // 读取（需要 R 个响应）
    std::vector<result_t> search(float const* query, std::size_t k,
                                 std::size_t r) {
        std::vector<std::future<std::vector<result_t>>> futures;

        // 并行查询 R 个副本
        for (std::size_t i = 0; i < r; ++i) {
            futures.push_back(std::async(std::launch::async, [&] {
                return replicas_[i].client->search(query, k);
            }));
        }

        // 合并结果（选择最新的）
        std::map<vector_key_t, result_t> merged;
        for (auto& future : futures) {
            auto results = future.get();
            for (auto& result : results) {
                auto it = merged.find(result.key);
                if (it == merged.end() ||
                    result.timestamp > it->second.timestamp) {
                    merged[result.key] = result;
                }
            }
        }

        // 提取 Top-K
        std::vector<result_t> final_results(merged.begin(), merged.end());
        std::partial_sort(final_results.begin(), final_results.begin() + k,
                         final_results.end());
        final_results.resize(k);
        return final_results;
    }
};
```

---

## 5. 查询路由优化

### 5.1 智能路由

```cpp
class QueryRouter {
    struct ShardStats {
        std::string address;
        std::size_t load;           // 当前负载
        double avg_latency;         // 平均延迟
        std::size_t queue_depth;    // 队列深度
    };

    std::vector<ShardStats> shards_;

public:
    // 基于负载的路由
    ShardStats* select_shard_by_load() {
        auto it = std::min_element(shards_.begin(), shards_.end(),
                                  [](auto const& a, auto const& b) {
                                      return a.load < b.load;
                                  });
        return &(*it);
    }

    // 基于延迟的路由
    ShardStats* select_shard_by_latency() {
        // 加权：负载 + 延迟
        auto it = std::min_element(shards_.begin(), shards_.end(),
                                  [](auto const& a, auto const& b) {
                                      double score_a = a.load * 0.3 + a.avg_latency * 0.7;
                                      double score_b = b.load * 0.3 + b.avg_latency * 0.7;
                                      return score_a < score_b;
                                  });
        return &(*it);
    }

    // 基于数据的路由（内容感知）
    std::vector<ShardStats*> select_shards_by_data(
        float const* query,
        std::size_t n_probe
    ) {
        // 1. 计算查询与每个分片的相似度
        struct ShardSimilarity {
            ShardStats* shard;
            float similarity;
        };

        std::vector<ShardSimilarity> similarities;
        for (auto& shard : shards_) {
            float sim = calculate_query_shard_similarity(query, shard);
            similarities.push_back({&shard, sim});
        }

        // 2. 选择最相似的 n_probe 个分片
        std::partial_sort(similarities.begin(),
                         similarities.begin() + n_probe,
                         similarities.end(),
                         [](auto const& a, auto const& b) {
                             return a.similarity > b.similarity;
                         });

        // 3. 提取分片指针
        std::vector<ShardStats*> selected;
        for (std::size_t i = 0; i < n_probe && i < similarities.size(); ++i) {
            selected.push_back(similarities[i].shard);
        }

        return selected;
    }

private:
    float calculate_query_shard_similarity(float const* query,
                                          ShardStats const& shard) {
        // 使用分片的元数据（如聚类中心）计算相似度
        // 简化版：随机返回
        return static_cast<float>(rand()) / RAND_MAX;
    }
};
```

### 5.2 缓存优化

```cpp
class DistributedCache {
    struct CacheEntry {
        std::vector<float> query_hash;
        std::vector<result_t> results;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::vector<CacheEntry> cache_;
    std::size_t max_size_;

public:
    // 查询缓存
    std::optional<std::vector<result_t>> get(float const* query,
                                             std::size_t dimensions) {
        auto hash = hash_query(query, dimensions);

        for (auto const& entry : cache_) {
            if (entry.query_hash == hash) {
                // 检查是否过期
                auto age = std::chrono::steady_clock::now() - entry.timestamp;
                if (age < std::chrono::minutes(5)) {
                    return entry.results;
                }
            }
        }

        return std::nullopt;
    }

    void put(float const* query, std::size_t dimensions,
             std::vector<result_t> const& results) {
        auto hash = hash_query(query, dimensions);

        // 查找或创建条目
        auto it = std::find_if(cache_.begin(), cache_.end(),
                              [&](auto const& e) {
                                  return e.query_hash == hash;
                              });

        if (it != cache_.end()) {
            // 更新
            it->results = results;
            it->timestamp = std::chrono::steady_clock::now();
        } else {
            // 添加（LRU 淘汰）
            if (cache_.size() >= max_size_) {
                auto oldest = std::min_element(cache_.begin(), cache_.end(),
                                              [](auto const& a, auto const& b) {
                                                  return a.timestamp < b.timestamp;
                                              });
                cache_.erase(oldest);
            }

            cache_.push_back({
                std::vector<float>(query, query + dimensions),
                results,
                std::chrono::steady_clock::now()
            });
        }
    }

private:
    std::vector<float> hash_query(float const* query, std::size_t dimensions) {
        // 简化的哈希：实际应使用更稳健的哈希函数
        return std::vector<float>(query, query + std::min(dimensions, size_t(8)));
    }
};
```

---

## 6. 实战：构建分布式 USearch 集群

### 6.1 完整实现

```cpp
/**
 * Distributed USearch Cluster
 * 支持：分片、复制、查询路由
 */

#include <usearch/index.hpp>
#include <usearch/index_dense.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <grpc/grpc.h>
#include <.grpcpp/server.h>
#include <grpcpp/server_builder.h>

using namespace unum::usearch;

//==============================================================================
// RPC 定义（简化版）
//==============================================================================

// 实际应使用 protobuf 定义
struct SearchRequest {
    std::vector<float> query;
    std::size_t k;
};

struct SearchResponse {
    std::vector<result_t> results;
};

struct AddRequest {
    vector_key_t key;
    std::vector<float> vector;
};

//==============================================================================
// 分片节点
//==============================================================================

class ShardNode {
    index_dense_gt<float, std::uint32_t> index_;
    std::size_t shard_id_;
    std::size_t dimensions_;
    std::mutex mutex_;

public:
    ShardNode(std::size_t shard_id, std::size_t dimensions)
        : shard_id_(shard_id), dimensions_(dimensions) {
        index_.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k);
    }

    // 添加向量
    bool add(vector_key_t key, float const* vector) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.add(key, vector);
        return true;
    }

    // 批量添加
    bool add_batch(vector_key_t const* keys, float const* vectors,
                   std::size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.add(keys, vectors, count);
        return true;
    }

    // 搜索
    std::vector<result_t> search(float const* query, std::size_t k) {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.search(query, k);
    }

    // 获取统计信息
    struct Stats {
        std::size_t num_vectors;
        std::size_t memory_usage;
    };

    Stats get_stats() const {
        Stats stats;
        stats.num_vectors = index_.size();
        // 内存使用估算
        stats.memory_usage = stats.num_vectors * dimensions_ * sizeof(float);
        return stats;
    }

    // 序列化索引
    std::vector<char> save() const {
        // 简化版：实际应使用 index_.save()
        return {};
    }

    // 加载索引
    bool load(std::vector<char> const& data) {
        // 简化版：实际应使用 index_.load()
        return true;
    }
};

//==============================================================================
// 分片策略
//==============================================================================

class ShardingStrategy {
public:
    virtual ~ShardingStrategy() = default;

    virtual std::size_t shard_id(vector_key_t key) const = 0;
    virtual std::vector<std::size_t> shard_ids_for_search(
        float const* query,
        std::size_t n_probe
    ) const = 0;
};

// 哈希分片
class HashSharding : public ShardingStrategy {
    std::size_t num_shards_;

public:
    HashSharding(std::size_t num_shards) : num_shards_(num_shards) {}

    std::size_t shard_id(vector_key_t key) const override {
        return std::hash<vector_key_t>{}(key) % num_shards_;
    }

    std::vector<std::size_t> shard_ids_for_search(
        float const* query,
        std::size_t n_probe
    ) const override {
        // 哈希分片需要查询所有分片
        std::vector<std::size_t> all_shards;
        for (std::size_t i = 0; i < std::min(n_probe, num_shards_); ++i) {
            all_shards.push_back(i);
        }
        return all_shards;
    }
};

//==============================================================================
// 分布式索引
//==============================================================================

class DistributedIndex {
    std::vector<std::unique_ptr<ShardNode>> shards_;
    std::unique_ptr<ShardingStrategy> strategy_;
    std::size_t dimensions_;
    std::size_t replication_factor_;

public:
    DistributedIndex(std::size_t num_shards,
                    std::size_t dimensions,
                    std::size_t replication_factor = 1)
        : dimensions_(dimensions),
          replication_factor_(replication_factor) {

        strategy_ = std::make_unique<HashSharding>(num_shards);

        // 创建分片
        for (std::size_t i = 0; i < num_shards; ++i) {
            shards_.push_back(std::make_unique<ShardNode>(i, dimensions));
        }
    }

    // 添加向量
    bool add(vector_key_t key, float const* vector) {
        // 1. 确定主分片
        std::size_t primary_shard = strategy_->shard_id(key);

        // 2. 添加到主分片
        shards_[primary_shard]->add(key, vector);

        // 3. 添加到副本（如果有）
        for (std::size_t i = 1; i < replication_factor_; ++i) {
            std::size_t replica_shard = (primary_shard + i) % shards_.size();
            shards_[replica_shard]->add(key, vector);
        }

        return true;
    }

    // 批量添加
    bool add_batch(vector_key_t const* keys, float const* vectors,
                   std::size_t count) {
        // 按分片分组
        std::vector<std::vector<std::pair<vector_key_t, float const*>>> groups(shards_.size());

        for (std::size_t i = 0; i < count; ++i) {
            std::size_t shard_id = strategy_->shard_id(keys[i]);
            groups[shard_id].push_back({keys[i], vectors + i * dimensions_});
        }

        // 批量添加到每个分片
        for (std::size_t shard_id = 0; shard_id < shards_.size(); ++shard_id) {
            if (!groups[shard_id].empty()) {
                std::vector<vector_key_t> shard_keys;
                std::vector<float> shard_vectors;

                for (auto& [key, vec] : groups[shard_id]) {
                    shard_keys.push_back(key);
                    shard_vectors.insert(shard_vectors.end(),
                                       vec, vec + dimensions_);
                }

                shards_[shard_id]->add_batch(
                    shard_keys.data(),
                    shard_vectors.data(),
                    shard_keys.size()
                );
            }
        }

        return true;
    }

    // 搜索
    std::vector<result_t> search(float const* query, std::size_t k,
                                 std::size_t n_probe = 0) {
        if (n_probe == 0) {
            n_probe = shards_.size();
        }

        // 1. 确定要查询的分片
        auto shard_ids = strategy_->shard_ids_for_search(query, n_probe);

        // 2. 并行查询
        std::vector<std::future<std::vector<result_t>>> futures;
        for (auto shard_id : shard_ids) {
            futures.push_back(std::async(std::launch::async, [&] {
                return shards_[shard_id]->search(query, k);
            }));
        }

        // 3. 聚合结果
        std::vector<result_t> all_results;
        for (auto& future : futures) {
            auto results = future.get();
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 4. 去重（处理副本）
        std::sort(all_results.begin(), all_results.end(),
                 [](auto const& a, auto const& b) {
                     return a.key < b.key;
                 });
        all_results.erase(
            std::unique(all_results.begin(), all_results.end(),
                       [](auto const& a, auto const& b) {
                           return a.key == b.key;
                       }),
            all_results.end()
        );

        // 5. 全局 Top-K
        std::partial_sort(all_results.begin(), all_results.begin() + k,
                         all_results.end(),
                         [](auto const& a, auto const& b) {
                             return a.distance < b.distance;
                         });

        if (all_results.size() > k) {
            all_results.resize(k);
        }

        return all_results;
    }

    // 获取集群统计
    struct ClusterStats {
        std::size_t total_vectors;
        std::size_t total_memory;
        std::vector<std::size_t> vectors_per_shard;
    };

    ClusterStats get_stats() const {
        ClusterStats stats;
        for (auto const& shard : shards_) {
            auto shard_stats = shard->get_stats();
            stats.total_vectors += shard_stats.num_vectors;
            stats.total_memory += shard_stats.memory_usage;
            stats.vectors_per_shard.push_back(shard_stats.num_vectors);
        }
        return stats;
    }
};

//==============================================================================
// 使用示例
//==============================================================================

void example_distributed_cluster() {
    std::cout << "=== Distributed USearch Cluster ===\n\n";

    // 1. 创建分布式索引
    constexpr std::size_t num_shards = 4;
    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;

    DistributedIndex cluster(num_shards, dimensions, 1);

    std::cout << "Created cluster with " << num_shards << " shards\n\n";

    // 2. 添加向量
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

    cluster.add_batch(keys.data(), vectors.data(), n_vectors);

    std::cout << "Done.\n\n";

    // 3. 显示统计信息
    auto stats = cluster.get_stats();
    std::cout << "Cluster Statistics:\n";
    std::cout << "  Total vectors: " << stats.total_vectors << "\n";
    std::cout << "  Total memory: " << (stats.total_memory / 1024 / 1024) << " MB\n";
    std::cout << "  Vectors per shard:\n";
    for (std::size_t i = 0; i < stats.vectors_per_shard.size(); ++i) {
        std::cout << "    Shard " << i << ": " << stats.vectors_per_shard[i] << "\n";
    }
    std::cout << "\n";

    // 4. 搜索
    std::cout << "Searching...\n";

    std::vector<float> query(dimensions);
    for (auto& val : query) {
        val = dist(rng);
    }

    auto results = cluster.search(query.data(), 10);

    std::cout << "Top 10 results:\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        std::cout << "  " << (i + 1) << ". Key: " << results[i].key
                  << ", Distance: " << results[i].distance << "\n";
    }
}

int main() {
    example_distributed_cluster();
    return 0;
}
```

---

## 7. 性能优化技巧

### 7.1 减少网络往返

```cpp
// 批量查询
class BatchQueryOptimizer {
    struct QueryBatch {
        std::vector<float> queries;
        std::vector<std::size_t> ks;
    };

public:
    std::vector<std::vector<result_t>> batch_search(
        std::vector<float> const& queries,
        std::vector<std::size_t> const& ks
    ) {
        // 1. 打包所有查询
        QueryBatch batch{queries, ks};

        // 2. 发送到所有分片
        std::map<std::size_t, std::vector<result_t>> shard_results;

        for (std::size_t shard_id = 0; shard_id < shards_.size(); ++shard_id) {
            auto results = shards_[shard_id]->batch_search(batch);
            shard_results[shard_id] = results;
        }

        // 3. 合并结果
        std::vector<std::vector<result_t>> final_results(queries.size() / dimensions_);

        for (std::size_t q = 0; q < final_results.size(); ++q) {
            // 从所有分片收集这个查询的结果
            for (auto& [shard_id, results] : shard_results) {
                // 合并逻辑...
            }
        }

        return final_results;
    }
};
```

### 7.2 使用 RDMA（远程直接内存访问）

```cpp
// RDMA 优化（概念）
class RDMAShardClient {
    // 使用 RDMA 避免内核拷贝
    void* rdma_memory_region_;

public:
    // 零拷贝搜索
    std::vector<result_t> search_rdma(float const* query, std::size_t k) {
        // 1. 直接写入远程内存
        rdma_write(remote_addr, query, query_size);

        // 2. 等待完成（通过轮询而非中断）
        while (!rdma_poll_completion()) {
            _mm_pause();  // 降低 CPU 占用
        }

        // 3. 直接读取远程结果
        std::vector<result_t> results(k);
        rdma_read(remote_result_addr, results.data(), k * sizeof(result_t));

        return results;
    }
};
```

---

## 8. 生产环境最佳实践

### 8.1 监控和告警

```cpp
class ClusterMonitor {
    struct Metrics {
        std::atomic<std::size_t> queries_per_second{0};
        std::atomic<std::size_t> avg_latency_us{0};
        std::atomic<std::size_t> error_rate{0};
        std::atomic<std::size_t> cpu_usage{0};
        std::atomic<std::size_t> memory_usage{0};
    } metrics_;

public:
    void record_query(std::size_t latency_us) {
        metrics_.queries_per_second.fetch_add(1, std::memory_order_relaxed);

        // 指数移动平均
        std::size_t old_avg = metrics_.avg_latency_us.load();
        std::size_t new_avg = (old_avg * 9 + latency_us) / 10;
        metrics_.avg_latency_us.store(new_avg);
    }

    void export_prometheus() {
        std::cout << "usearch_queries_per_second " << metrics_.queries_per_second << "\n";
        std::cout << "usearch_avg_latency_us " << metrics_.avg_latency_us << "\n";
        std::cout << "usearch_error_rate " << metrics_.error_rate << "\n";
    }
};
```

### 8.2 故障恢复

```cpp
class FailureRecovery {
public:
    // 心跳检测
    void heartbeat_loop() {
        while (running_) {
            for (auto& shard : shards_) {
                if (!shard->check_heartbeat()) {
                    handle_shard_failure(shard.get());
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void handle_shard_failure(ShardNode* failed_shard) {
        std::cout << "Shard " << failed_shard->id() << " failed!\n";

        // 1. 标记为不可用
        failed_shard->mark_unavailable();

        // 2. 从副本恢复数据
        for (auto& shard : shards_) {
            if (shard->is_available() && shard->has_replica_of(failed_shard)) {
                // 复制数据到新节点
                replicate_data(shard.get(), failed_shard);
                break;
            }
        }
    }
};
```

---

## 9. 性能基准测试

### 9.1 可扩展性测试

| 集群规模 | 向量数量 | 分片数 | QPS | P99 延迟 |
|---------|---------|--------|-----|----------|
| 1 节点  | 1000万  | 1      | 1000 | 10 ms    |
| 4 节点  | 4000万  | 4      | 3500 | 12 ms    |
| 16 节点 | 1.6亿   | 16     | 12000| 15 ms    |

### 9.2 优化前后对比

| 指标 | 优化前 | 优化后 | 提升 |
|-----|--------|--------|------|
| QPS | 5000   | 12000  | 2.4x |
| P99 延迟 | 25 ms | 15 ms  | 1.67x |
| 网络流量 | 10 GB/s | 4 GB/s | 2.5x |

---

## 10. 总结

### 关键要点

1. **分片策略选择**
   - 小规模（< 1000万）：哈希分片
   - 中等规模（1000万-1亿）：范围分片
   - 大规模（> 1亿）：聚类分片

2. **复制策略选择**
   - 读多写少：主从复制
   - 写多读少：多主复制
   - 强一致性：Quorum 机制

3. **性能优化重点**
   - 减少网络往返（批量查询）
   - 使用 RDMA（如果可用）
   - 智能路由（基于数据和负载）
   - 本地缓存热点查询

4. **生产环境考虑**
   - 完善的监控
   - 自动故障恢复
   - 数据备份
   - 容量规划

### 学习路径

1. **基础**：实现单机 USearch
2. **进阶**：添加哈希分片
3. **高级**：实现复制和故障恢复
4. **专家**：优化网络和 RDMA

---

**下一步**：实现你自己的分布式向量搜索引擎！

**参考资源**：
- Milvus: https://github.com/milvus-io/milvus
- Qdrant: https://github.com/qdrant/qdrant
- Weaviate: https://github.com/weaviate/weaviate
- Vespa: https://github.com/vespa-engine/vespa
