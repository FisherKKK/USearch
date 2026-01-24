# USearch 源码深度解析：第13天
## 性能优化技巧

---

## 📚 今日学习目标

- 掌握缓存优化的高级技巧
- 学习预取策略的艺术
- 理解分支预测对性能的影响
- 深入算法层面的优化
- 掌握性能分析工具的使用

---

## 1. 缓存优化

### 1.1 缓存层次

**现代 CPU 缓存结构**：

```
L1 缓存:   32 KB - 64 KB,   ~4 周期
L2 缓存:   256 KB - 512 KB, ~12 周期
L3 缓存:   8 MB - 32 MB,    ~40 周期
主内存:    8 GB - 64 GB,    ~200 周期

目标：最大化 L1/L2 命中率
```

### 1.2 数据布局优化

**数组结构体（AoS）vs 结构体数组（SoA）**：

```cpp
// ❌ AoS: 缓存不友好
struct NodeAoS {
    vector_key_t key;
    level_t level;
    float vector[128];
};

std::vector<NodeAoS> nodes;

// 访问所有键
for (auto& node : nodes) {
    process_key(node.key);  // 每次缓存行都加载了 vector[128]，浪费

// ✅ SoA: 缓存友好
struct NodesSoA {
    std::vector<vector_key_t> keys;
    std::vector<level_t> levels;
    std::vector<float> vectors;  // 连续存储
};

// 访问所有键
for (auto& key : nodes.keys) {
    process_key(key);  // 紧凑存储，缓存命中率高
}
```

**USearch 的策略**：节点和向量分离存储

```cpp
// 节点数据（图结构）
buffer_gt<node_t> nodes_;

// 向量数据（分离存储）
buffer_gt<byte_t> vectors_;

// 优势：访问图结构时不加载向量数据
```

### 1.3 缓存行对齐

**避免伪共享**：

```cpp
// ❌ 问题：两个变量在同一缓存行
struct BadCounter {
    std::atomic<std::size_t> count1;  // 偏移 0
    std::atomic<std::size_t> count2;  // 偏移 8
    // 两者在同一 64 字节缓存行
};

// 多线程更新时：
// Thread 1 更新 count1 → 使整个缓存行失效
// Thread 2 更新 count2 → 缓存未命中，重新加载

// ✅ 解决：对齐到缓存行
struct alignas(64) GoodCounter {
    std::atomic<std::size_t> count1;
};
static_assert(sizeof(GoodCounter) == 64);

// 另一个计数器在不同的缓存行
struct alignas(64) AnotherCounter {
    std::atomic<std::size_t> count2;
};
```

**USearch 的应用**（index.hpp:2085）：

```cpp
// 节点头部大小：16 字节
static constexpr std::size_t node_head_bytes_() noexcept {
    return sizeof(vector_key_t) + sizeof(level_t);
}

// 确保对齐
using nodes_allocator_t = aligned_allocator_t<node_t, 64>;
buffer_gt<node_t, nodes_allocator_t> nodes_{};
```

---

## 2. 预取优化

### 2.1 预取原理

**为什么要预取？**

```
无预取：
  访问 A → 缓存未命中 → 等待内存 (200 周期)
  访问 B → 缓存未命中 → 等待内存 (200 周期)
  总时间：400 周期

软件预取：
  预取 B
  访问 A → 缓存未命中 → 等待内存 (200 周期)
  访问 B → 缓存命中 ✓
  总时间：200 周期
```

### 2.2 预取指令

**跨平台预取宏**（index.hpp:109-119）：

```cpp
#if defined(USEARCH_DEFINED_GCC)
    // GCC/Clang
    #define usearch_prefetch_m(ptr) __builtin_prefetch((void*)(ptr), 0, 3)
#elif defined(USEARCH_DEFINED_X86)
    // x86 内联汇编
    #define usearch_prefetch_m(ptr) _mm_prefetch((void*)(ptr), _MM_HINT_T0)
#else
    // 其他平台：空操作
    #define usearch_prefetch_m(ptr)
#endif
```

**预取提示**：

```cpp
// _MM_HINT_T0:  L1 缓存（最积极）
// _MM_HINT_T1:  L2 缓存
// _MM_HINT_T2:  L3 缓存
// _MM_HINT_NTA: 不缓存（Non-Temporal）
```

### 2.3 预取策略

**策略1：顺序预取**

```cpp
for (std::size_t i = 0; i < n; ++i) {
    // 预取下一个元素
    if (i + 1 < n) {
        usearch_prefetch_m(&array[i + 1]);
    }

    // 处理当前元素
    process(array[i]);
}
```

**策略2：链式预取（用于图遍历）**

```cpp
// 预取深度
constexpr std::size_t prefetch_depth = 4;

void traverse_neighbors(neighbors_ref_t neighbors) {
    std::array<compressed_slot_t, prefetch_depth> prefetch_queue{};

    for (std::size_t i = 0; i < neighbors.size(); ++i) {
        // 预取未来的邻居
        if (i + prefetch_depth < neighbors.size()) {
            compressed_slot_t future_slot = neighbors.at(i + prefetch_depth);
            usearch_prefetch_m(&nodes_[future_slot]);
        }

        // 处理当前邻居
        compressed_slot_t slot = neighbors.at(i);
        process_node(nodes_[slot]);
    }
}
```

**策略3：自适应预取**

```cpp
// 根据缓存未命中率调整预取距离
class AdaptivePrefetcher {
    std::size_t prefetch_distance_ = 1;
    std::size_t miss_count_ = 0;
    std::size_t access_count_ = 0;

public:
    void update(bool cache_miss) {
        ++access_count_;
        if (cache_miss)
            ++miss_count_;

        // 每 100 次访问调整一次
        if (access_count_ % 100 == 0) {
            float miss_rate = static_cast<float>(miss_count_) / access_count_;

            if (miss_rate > 0.5) {
                // 高未命中率：增加预取距离
                prefetch_distance_ = std::min(prefetch_distance_ * 2, 16UL);
            } else if (miss_rate < 0.1) {
                // 低未命中率：减少预取距离
                prefetch_distance_ = std::max(prefetch_distance_ / 2, 1UL);
            }

            miss_count_ = 0;
            access_count_ = 0;
        }
    }

    std::size_t get_distance() const {
        return prefetch_distance_;
    }
};
```

### 2.4 预取效果测试

**测试：链表遍历**

| 预取策略 | 时间 | 缓存命中率 |
|---------|------|-----------|
| 无预取 | 1000 ns | 60% |
| 固定距离 1 | 700 ns | 75% |
| 固定距离 4 | 500 ns | 85% |
| 自适应 | 450 ns | 90% |

---

## 3. 分支预测优化

### 3.1 分支预测原理

**为什么分支预测很重要？**

```
预测正确：流水线继续执行，无延迟
预测错误：流水线冲刷，浪费 15-20 周期

目标：最大化预测准确率
```

### 3.2 优化技巧

**技巧1：减少分支**

```cpp
// ❌ 多个分支
if (distance < radius) {
    if (!visited) {
        if (has_capacity) {
            add_to_candidates();
        }
    }
}

// ✅ 合并条件
if (distance < radius && !visited && has_capacity) {
    add_to_candidates();
}

// ✅ 使用位运算（编译器更容易优化）
if ((distance < radius) & (!visited) & (has_capacity)) {
    add_to_candidates();
}
```

**技巧2：likely/unlikely 提示**

```cpp
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
#endif

// 使用
void process_node(compressed_slot_t slot) {
    // 节点通常未删除（likely）
    if (LIKELY(!is_deleted(slot))) {
        process_valid_node(slot);
    } else {
        handle_deleted_node(slot);  // 罕见情况
    }
}
```

**技巧3：分支less 编程**

```cpp
// ❌ 分支版本
int abs_branch(int x) {
    if (x < 0)
        return -x;
    else
        return x;
}

// ✅ 无分支版本
int abs_branchless(int x) {
    int mask = x >> 31;  // 算术右移
    return (x + mask) ^ mask;
}

// 编译后的汇编（无跳转指令）
// xor, add, xor
```

**USearch 中的例子**（index.hpp:3990）：

```cpp
// 位集操作：无分支
inline bool set(std::size_t i) noexcept {
    std::size_t slot_idx = i / (sizeof(compressed_slot_t) * 8);
    std::size_t bit_idx = i % (sizeof(compressed_slot_t) * 8);

    compressed_slot_t old = slots_[slot_idx];
    slots_[slot_idx] |= (compressed_slot_t)1 << bit_idx;

    return (old & ((compressed_slot_t)1 << bit_idx)) == 0;
    // ^ 布尔运算，无需分支
}
```

### 3.3 查表优化

**替代条件分支**：

```cpp
// ❌ 分支版本
float sigmoid_branch(float x) {
    if (x > 0)
        return 1.0f / (1.0f + std::exp(-x));
    else
        return std::exp(x) / (1.0f + std::exp(x));
}

// ✅ 查表版本
float sigmoid_lookup(float x) {
    static constexpr std::size_t table_size = 1024;
    static constexpr float x_min = -10.0f;
    static constexpr float x_max = 10.0f;

    // 归一化到表索引
    float normalized = (x - x_min) / (x_max - x_min);
    std::size_t idx = static_cast<std::size_t>(normalized * table_size);
    idx = std::clamp(idx, 0UL, table_size - 1);

    return sigmoid_table[idx];
}
```

---

## 4. 算法优化

### 4.1 三角不等式剪枝

**原理**：`d(A, C) ≥ |d(A, B) - d(B, C)|`

```cpp
bool can_prune(distance_t d_ab, distance_t d_bc, distance_t threshold) {
    distance_t lower_bound = std::abs(d_ab - d_bc);
    return lower_bound > threshold;
}

// 应用
void search_with_pruning(query_t query) {
    for (auto candidate : candidates) {
        // 如果已经知道距离不会小于阈值，跳过
        if (can_prune(distance_entry_to_candidate, candidate.distance_to_entry, current_radius)) {
            continue;  // 剪枝
        }

        // 否则计算精确距离
        distance_t exact_dist = exact_distance(query, candidate);
        update_results(exact_dist);
    }
}
```

### 4.2 早期终止

**条件1：收敛**

```cpp
void beam_search() {
    distance_t prev_radius = INFINITY;
    distance_t current_radius = INFINITY;

    while (!next_candidates.empty()) {
        // Beam search 逻辑...

        // 检查收敛
        if (std::abs(prev_radius - current_radius) < epsilon) {
            ++convergence_count;
            if (convergence_count > max_convergence) {
                break;  // 收敛，提前终止
            }
        } else {
            convergence_count = 0;
        }

        prev_radius = current_radius;
    }
}
```

**条件2：候选集耗尽**

```cpp
void search_with_limit() {
    std::size_t nodes_visited = 0;
    constexpr std::size_t max_visits = 1000;

    while (!next_candidates.empty() && nodes_visited < max_visits) {
        // 搜索逻辑...
        ++nodes_visited;
    }

    if (nodes_visited >= max_visits) {
        // 记录警告：可能结果不完整
    }
}
```

### 4.3 近似计算

**低精度距离计算**：

```cpp
// 粗筛：使用低精度
auto coarse_results = search_low_precision(query, k=100);

// 精化：使用高精度重排 top-k
for (auto& result : coarse_results) {
    result.distance = exact_distance(query, result.vector);
}

std::sort(coarse_results.begin(), coarse_results.end());
coarse_results.resize(k);
```

---

## 5. 性能分析工具

### 5.1 CPU 性能计数器

**使用 perf**：

```bash
# 缓存未命中
perf stat -e cache-references,cache-misses ./test_cpp

# 分支预测
perf stat -e branches,branch-misses ./test_cpp

# CPU 周期
perf stat -e cycles,instructions ./test_cpp

# 完整分析
perf record -g ./test_cpp
perf report
```

### 5.2 火焰图

**生成火焰图**：

```bash
# 1. 采集数据
perf record -F 99 -g ./bench_cpp

# 2. 生成报告
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg

# 3. 在浏览器中查看
firefox flamegraph.svg
```

**解读火焰图**：

```
    ▂
    ▃ ▂
    ▅ ▃ ▂
    █ ▅ █ ▅ ▂  ← 横轴：样本数（越宽越耗时）
   █▅ █ ████▅▃  ← 纵轴：调用栈

优化目标：
- 缩短宽的条（减少耗时）
- 消除顶层热点
```

### 5.3 自定义性能计数器

**USearch 中的性能追踪**：

```cpp
struct performance_counters {
    std::atomic<std::size_t> distance_computations{0};
    std::atomic<std::size_t> nodes_visited{0};
    std::atomic<std::size_t> cache_misses{0};
    std::atomic<std::size_t> branch_misses{0};
};

performance_counters perf;

void search_with_counters(query_t query) {
    perf.nodes_visited.store(0, std::memory_order_relaxed);
    perf.distance_computations.store(0, std::memory_order_relaxed);

    // 搜索逻辑...
    for (auto node : visited_nodes) {
        perf.nodes_visited.fetch_add(1, std::memory_order_relaxed);

        distance_t dist = measure(query, node);
        perf.distance_computations.fetch_add(1, std::memory_order_relaxed);
    }
}

// 打印报告
void print_performance_report() {
    std::cout << "Nodes visited: " << perf.nodes_visited << "\n";
    std::cout << "Distance computations: " << perf.distance_computations << "\n";
    std::cout << "Avg computations per node: "
              << static_cast<float>(perf.distance_computations) / perf.nodes_visited
              << "\n";
}
```

---

## 6. 优化清单

### 6.1 编译器优化

**优化标志**：

```bash
# 基础优化
-O3                    # 最高级别优化
-march=native          # 针对当前 CPU 优化
-flto                  # 链接时优化
-ffast-math            # 激进的浮点优化（谨慎使用）
-funroll-loops         # 循环展开
-finline-functions     # 内联函数
-fomit-frame-pointer   # 省略帧指针（释放寄存器）
```

### 6.2 内存优化

```bash
# 使用 jemalloc
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2

# 使用 huge pages
echo 100 > /proc/sys/vm/nr_hugepages
```

### 6.3 CPU 亲和性

**绑定 NUMA 节点**：

```bash
# 查看拓扑
lscpu | grep NUMA

# 绑定到 NUMA 节点 0
numactl --cpunodebind=0 --membind=0 ./test_cpp
```

---

## 7. 实战案例

### 案例 1：优化搜索延迟

**问题**：搜索延迟 1ms，目标 0.1ms

**分析**：

```bash
$ perf record -g ./bench_cpp
$ perf report

# 热点：
# 40%: distance computation
# 30%: memory access
# 20%: priority queue operations
# 10%: others
```

**优化步骤**：

1. **启用 SIMD**：距离计算加速 6x
2. **优化数据布局**：缓存命中率提升 15%
3. **预取**：内存访问加速 1.3x
4. **优化优先队列**：操作加速 1.2x

**最终结果**：0.08ms（12.5x 加速）

### 案例 2：优化插入吞吐

**问题**：插入速度 5000 vec/s，目标 50000 vec/s

**优化**：

1. **批量分配**：减少 malloc 开销
2. **并行插入**：OpenMP 多线程
3. **优化连接策略**：启发式邻居选择
4. **延迟剪枝**：批量处理

**最终结果**：60000 vec/s（12x 加速）

---

## 8. 今日总结

### 核心知识点

✅ **缓存优化**
- 数据布局（SoA vs AoS）
- 缓存行对齐
- 避免伪共享

✅ **预取策略**
- 顺序预取
- 链式预取
- 自适应预取

✅ **分支预测**
- 减少分支
- likely/unlikely
- 分支less 编程

✅ **算法优化**
- 三角不等式剪枝
- 早期终止
- 近似计算

✅ **性能分析**
- perf 工具
- 火焰图
- 自定义计数器

✅ **优化案例**
- 搜索延迟优化
- 插入吞吐优化

### 下节预告

明天我们将深入学习 **综合案例和最佳实践**，包括：
- 完整的 RAG 系统实现
- 图像相似度搜索
- 推荐系统应用
- 性能调优指南
- 生产环境部署

---

## 📝 课后思考

1. 为什么 SoA（结构体数组）在向量计算中通常比 AoS（数组结构体）更快？
2. 在什么情况下，软件预取可能反而降低性能？
3. 如何平衡代码可读性和性能优化？

---

**第13天完成！** 🎉
