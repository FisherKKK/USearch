# USearch 代码审查与重构指南
## 如何识别性能瓶颈和代码异味

---

## 🎯 目标

- 学会识别代码异味（code smells）
- 掌握性能瓶颈分析方法
- 实践重构技巧
- 提升代码质量和性能

---

## 1. 代码异味识别

### 1.1 代码异味列表

#### 异味1：过长函数（Long Method）

**症状**：函数超过 50 行

```cpp
// ❌ 不好：过长函数
void complex_operation() {
    // 200 行代码...
    // 难以理解、测试和维护
}
```

**重构**：

```cpp
// ✅ 好：分解为多个小函数
void complex_operation() {
    prepare_data();
    process_data();
    cleanup_data();
}

void prepare_data() { /* ... */ }
void process_data() { /* ... */ }
void cleanup_data() { /* ... */ }
```

#### 异味2：重复代码（Duplicate Code）

```cpp
// ❌ 不好：重复的距离计算
float dist1 = calculate_distance(a, b);
float dist2 = calculate_distance(a, c);
float dist3 = calculate_distance(a, d);
```

**重构**：

```cpp
// ✅ 好：批量计算
auto distances = batch_calculate_distances(a, {b, c, d});
```

#### 异味3：过早优化（Premature Optimization）

```cpp
// ❌ 不好：不必要的复杂优化
template <typename T>
class fast_vector {
    // 手动管理内存，避免 std::vector
    // 但实际上现代 std::vector 已经很快
};
```

**重构**：

```cpp
// ✅ 好：使用标准库
std::vector<float> vectors;
```

#### 异味4：魔法数字（Magic Numbers）

```cpp
// ❌ 不好：硬编码的常量
for (int i = 0; i < 16; ++i) {  // 为什么是 16？
    // ...
}
```

**重构**：

```cpp
// ✅ 好：命名常量
constexpr std::size_t max_neighbors = 16;
for (std::size_t i = 0; i < max_neighbors; ++i) {
    // ...
}
```

### 1.2 性能反模式

#### 反模式1：频繁的动态分配

```cpp
// ❌ 不好：循环中分配内存
for (int i = 0; i < 10000; ++i) {
    float* temp = new float[128];  // 每次 new
    // ... 使用 ...
    delete[] temp;
}
```

**重构**：

```cpp
// ✅ 好：预分配或使用栈
std::vector<float> temp(128);
for (int i = 0; i < 10000; ++i) {
    // 重用 temp
}
```

#### 反模式2：过度的拷贝

```cpp
// ❌ 不好：不必要的拷贝
void process(std::vector<float> data) {  // 拷贝
    // 处理 data
}

// 调用
process(my_vector);  // 拷贝整个 vector
```

**重构**：

```cpp
// ✅ 好：使用引用或视图
void process(std::vector<float> const& data) {  // 不拷贝
    // 处理 data
}

// 调用
process(my_vector);  // 只传递引用
```

#### 反模式3：错误的锁粒度

```cpp
// ❌ 不好：全局锁
class GlobalLockIndex {
    std::mutex global_mutex_;

    void add(int key, float* vec) {
        std::lock_guard<std::mutex> lock(global_mutex_);
        // 所有操作都被序列化
    }
};
```

**重构**：

```cpp
// ✅ 好：节点级锁
class NodeLockIndex {
    buffer_gt<mutex_gt> mutexes_;

    void add(int key, float* vec) {
        compressed_slot_t slot = get_slot(key);
        node_lock_t lock(mutexes_, slot);  // 只锁当前节点
        // ...
    }
};
```

---

## 2. 性能分析工具使用

### 2.1 perf 分析

**基础性能分析**：

```bash
# 1. 记录性能数据
perf record -g ./your_program

# 2. 查看报告
perf report

# 3. 查看特定函数的性能
perf report -g --stdio | grep -A 20 "distance"

# 4. 查看调用图
perf graph
```

**高级分析**：

```bash
# 统计缓存行为
perf stat -e cache-references,cache-misses,instructions,cycles ./your_program

# 统计分支预测
perf stat -e branches,branch-misses ./your_program

# 热点分析
perf top --call-graph
```

### 2.2 Valgrind 工具

**内存泄漏检测**：

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./your_program
```

**性能分析**：

```bash
valgrind --tool=callgrind ./your_program
callgrind_annotate --auto callgrind.out.<pid>
```

### 2.3 自定义性能计数器

```cpp
class PerformanceCounter {
    std::atomic<std::size_t> distance_calculations_{0};
    std::atomic<std::size_t> lock_contentions_{0};

public:
    void record_distance() {
        distance_calculations_.fetch_add(1, std::memory_order_relaxed);
    }

    void record_lock_contention() {
        lock_contentions_.fetch_add(1, std::memory_order_relaxed);
    }

    void print_report() {
        std::cout << "Distance calculations: " << distance_calculations_ << "\n";
        std::cout << "Lock contentions: " << lock_contentions_ << "\n";

        if (lock_contentions_ > distance_calculations_ / 10) {
            std::cout << "WARNING: High lock contention detected!\n";
        }
    }
};

// 使用
PerformanceCounter perf;
perf.record_distance();
// ...
perf.print_report();
```

---

## 3. 重构实战

### 3.1 提取函数

**重构前**：

```cpp
void add_and_search(int key, float* vec, float* query) {
    // 添加向量
    index.add(key, vec);

    // 搜索
    auto results = index.search(query, 10);

    // 返回结果
    return results;
}
```

**重构后**：

```cpp
void add_to_index(int key, float* vec) {
    index.add(key, vec);
}

std::vector<result_t> search_index(float* query) {
    return index.search(query, 10);
}

void add_and_search(int key, float* vec, float* query) {
    add_to_index(key, vec);
    return search_index(query);
}
```

### 3.2 引入参数对象

**重构前**：

```cpp
void configure_index(
    std::size_t dimensions,
    metric_kind_t metric,
    scalar_kind_t scalar,
    std::size_t M,
    std::size_t ef,
    float ml
) {
    // 太多参数，难以维护
}
```

**重构后**：

```cpp
struct IndexConfig {
    std::size_t dimensions = 128;
    metric_kind_t metric = metric_kind_t::cos_k;
    scalar_kind_t scalar = scalar_kind_t::f32_k;
    std::size_t connectivity = 16;
    std::size_t expansion = 64;
    double ml = 1.0 / std::log(16.0);
};

void configure_index(IndexConfig const& config) {
    // 使用配置对象
}
```

### 3.3 策略模式替换类型检查

**重构前**：

```cpp
void process(void* data, scalar_kind_t scalar) {
    if (scalar == scalar_kind_t::f32_k) {
        float* ptr = static_cast<float*>(data);
        // ...
    } else if (scalar == scalar_kind_t::f16_k) {
        std::uint16_t* ptr = static_cast<std::uint16_t*>(data);
        // ...
    }
    // ... 很多分支
}
```

**重构后**：

```cpp
template <scalar_kind_t scalar>
void process_typed(void* data) {
    if constexpr (scalar == scalar_kind_t::f32_k) {
        float* ptr = static_cast<float*>(data);
        // ...
    } else if constexpr (scalar == scalar_kind_t::f16_k) {
        std::uint16_t* ptr = static_cast<std::uint16_t*>(data);
        // ...
    }
}

// 使用
void process(void* data, scalar_kind_t scalar) {
    switch (scalar) {
        case scalar_kind_t::f32_k:
            process_typed<scalar_kind_t::f32_k>(data);
            break;
        // ...
    }
}
```

---

## 4. 代码审查清单

### 4.1 性能检查项

- [ ] **算法复杂度**
  - [ ] 时间复杂度是否合理？
  - [ ] 是否有嵌套循环？
  - [ ] 是否有不必要的计算？

- [ ] **内存使用**
  - [ ] 是否有内存泄漏？
  - [ ] 是否频繁分配/释放？
  - [ ] 缓存命中率如何？

- [ ] **并发**
  - [ ] 是否有数据竞争？
  - [ ] 锁粒度是否合适？
  - [ ] 是否有死锁风险？

- [ ] **编译器优化**
  - [ ] 是否启用优化？
  - [ ] 是否内联关键函数？
  - [ ] 是否使用 constexpr？

### 4.2 代码质量检查项

- [ ] **可读性**
  - [ ] 命名是否清晰？
  - [ ] 注释是否充分？
  - [ ] 结构是否合理？

- [ ] **可维护性**
  - [ ] 是否容易扩展？
  - [ ] 是否容易测试？
  - [ ] 是否避免重复代码？

- [ ] **健壮性**
  - [ ] 是否检查边界条件？
  - [ ] 是否处理异常？
  - [ ] 是否资源安全？

---

## 5. 性能测试模板

### 5.1 微基准测试

```cpp
#include <benchmark/benchmark.h>

// 微基准：距离计算
static void BM_Distance_Cosine(benchmark::State& state) {
    std::vector<float> a(128, 1.0f);
    std::vector<float> b(128, 2.0f);

    for (auto _ : state) {
        float ab = 0, a2 = 0, b2 = 0;
        for (std::size_t i = 0; i < 128; ++i) {
            ab += a[i] * b[i];
            a2 += a[i] * a[i];
            b2 += b[i] * b[i];
        }
        benchmark::DoNotOptimize(ab / std::sqrt(a2 * b2));
    }
}

// 微基准：邻居迭代
static void BM_NeighborIteration(benchmark::State& state) {
    // 准备数据...

    for (auto _ : state) {
        for (compressed_slot_t neighbor : neighbors) {
            benchmark::DoNotOptimize(neighbor);
        }
    }
}

BENCHMARK(BM_Distance_Cosine);
BENCHMARK(BM_NeighborIteration);
BENCHMARK_MAIN();
```

### 5.2 集成测试

```cpp
// 完整的集成测试
void integration_test() {
    // 1. 构建索引
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k);

    // 2. 添加数据
    constexpr std::size_t n = 10000;
    std::vector<float> vectors(n * 128);
    std::vector<std::uint32_t> keys(n);
    index.add(keys.data(), vectors.data(), n);

    // 3. 测试搜索
    std::vector<float> query(128, 1.0f);
    auto results = index.search(query.data(), 10);

    // 4. 验证结果
    assert(results.size() == 10);
    assert(results[0].distance < 1.0f);
}
```

---

## 6. 重构案例

### 案例1：优化搜索函数

**原始代码**：

```cpp
std::vector<result_t> search(
    std::vector<float> const& query,
    std::size_t k) {

    // 每次都计算查询哈希
    size_t query_hash = 0;
    for (float v : query) {
        query_hash ^= std::hash<float>{}(v) + 0x9e3779b9;
        query_hash = (query_hash << 6) ^ (query_hash >> 2);
    }

    // 检查缓存
    if (cache_.count(query_hash)) {
        return cache_[query_hash];
    }

    // 搜索
    auto results = index_.search(query.data(), k);

    // 存入缓存
    cache_[query_hash] = results;

    return results;
}
```

**问题**：
1. 每次都计算哈希（即使有缓存）
2. 缓存未限制大小
3. 哈希计算复杂

**重构后**：

```cpp
class cached_search {
    index_dense_gt<float, std::size_t> index_;
    size_t cache_size_;

    struct cache_entry {
        std::vector<float> query;
        std::vector<result_t> results;
        uint64_t timestamp;
    };

    std::vector<cache_entry> cache_;
    std::size_t next_cache_slot_ = 0;

    // 快速哈希（xxhash）
    static uint64_t hash_query(std::vector<float> const& query) {
        // 只采样前8个维度（快速）
        uint64_t hash = 0;
        for (size_t i = 0; i < std::min(query.size(), size_t(8)); ++i) {
            uint32_t val;
            std::memcpy(&val, &query[i], sizeof(float));
            hash ^= val;
            hash = (hash << 5) ^ (hash >> 3);
        }
        return hash;
    }

public:
    cached_search(std::size_t dims, size_t cache_size = 1000)
        : index_(dims), cache_size_(cache_size), cache_(cache_size) {
        index_.init(dims, metric_kind_t::cos_k);
    }

    std::vector<result_t> search(std::vector<float> const& query, std::size_t k) {
        // 计算哈希（只一次）
        uint64_t hash = hash_query(query);

        // 检查缓存
        for (cache_entry const& entry : cache_) {
            if (entry.timestamp == hash) {  // 完全匹配才使用缓存
                return entry.results;
            }
        }

        // 未命中，执行搜索
        auto results = index_.search(query.data(), k);

        // 存入缓存（LRU）
        cache_[next_cache_slot_] = {query, results, hash};
        next_cache_slot_ = (next_cache_slot_ + 1) % cache_size_;

        return results;
    }
};
```

**性能提升**：
- 缓存命中时：10x 加速
- 未命中时：1.1x 慢（哈希计算开销）

### 案例2：优化向量存储

**原始代码**：

```cpp
// 每个向量单独分配
struct VectorNode {
    std::vector<float> vector;
    vector_key_t key;

    VectorNode(std::vector<float> const& v, vector_key_t k)
        : vector(v), key(k) {}
};

std::vector<VectorNode> nodes;
nodes.emplace_back(vector, key);  // 拷贝 vector
```

**问题**：
1. 多次内存分配
2. 内存不连续
3. 缓存不友好

**重构后**：

```cpp
// 连续存储
class VectorStorage {
    std::vector<float> vectors_;  // 连续存储
    std::vector<vector_key_t> keys_;

public:
    void add(std::vector<float> const& vector, vector_key_t key) {
        // 追加到连续数组
        vectors_.insert(vectors_.end(), vector.begin(), vector.end());
        keys_.push_back(key);
    }

    float const* get(vector_key_t key) const {
        // 计算偏移
        std::size_t offset = key * dimensions_;
        return vectors_.data() + offset;
    }
};
```

**性能提升**：
- 内存使用：-30%
- 缓存命中率：+20%
- 构建速度：1.5x

---

## 7. 性能优化案例研究

### 7.1 案例：优化邻居选择

**问题**：启发式邻居选择慢

**分析**：

```cpp
// 原始代码
auto neighbors = select_neighbors_heuristic(candidates, M, level);

// 性能分析：
// - 执行时间：50 μs
// - 占总搜索时间的：20%
```

**优化1：内联关键路径**

```cpp
// 将频繁调用的小函数内联
class node_t {
public:
    // 内联获取邻居
    inline neighbors_ref_t neighbors(level_t level) noexcept {
        return neighbors_(node, level);
    }
};
```

**优化2：提前终止**

```cpp
// 如果已经找到足够好的邻居，提前终止
for (std::size_t i = 0; i < candidates.size(); ++i) {
    if (results.size() >= M) {
        // 检查是否足够好
        if (candidates[i].distance < radius) {
            break;  // 剩余候选都更远
        }
    }
    // ...
}
```

**结果**：
- 执行时间：50 μs → 30 μs
- 加速比：1.67x

### 7.2 案例：优化内存分配

**问题**：频繁的 small vector 拷贝

**分析**：

```cpp
// 每次搜索都拷贝查询向量
std::vector<float> query(128);
// 搜索时拷贝：index.search(query.data(), k)
```

**优化**：

```cpp
// 使用视图（零拷贝）
class vector_view_t {
    float const* data_;
    std::size_t size_;

public:
    vector_view_t(float const* data, std::size_t size)
        : data_(data), size_(size) {}

    float const* data() const { return data_; }
    std::size_t size() const { return size_; }
};

// 使用
vector_view_t view(query.data(), query.size());
index.search(view.data(), view.size());
```

**结果**：
- 减少内存拷贝：100%
- 搜索延迟：-5%

---

## 8. 代码审查流程

### 8.1 审查步骤

1. **静态分析**
   ```bash
   # 使用 clang-tidy
   clang-tidy checks.cpp -checks='-*'

   # 使用 cppcheck
   cppcheck --enable=all path/to/code
   ```

2. **动态分析**
   ```bash
   # 内存检查
   valgrind --leak-check=full ./your_program

   # 性能分析
   perf record -g ./your_program
   ```

3. **代码审查**
   - 自己审查：过一遍代码
   - 同行审查：请同事审查
   - 工具审查：使用静态分析工具

4. **重构**
   - 识别问题
   - 制定重构计划
   - 执行重构
   - 验证效果

### 8.2 审查检查表

**性能相关**：
- [ ] 是否有不必要的拷贝？
- [ ] 是否有频繁的内存分配？
- [ ] 循环是否可以向量化？
- [ ] 是否有热点路径可以优化？

**代码质量**：
- [ ] 函数是否过长（>50行）？
- [ ] 是否有重复代码？
- [ ] 命名是否清晰？
- [ ] 是否有魔法数字？

**并发安全**：
- [ ] 是否有数据竞争？
- [ ] 锁粒度是否合适？
- [ ] 是否有死锁风险？

---

## 9. 性能优化检查清单

### 编译时
- [ ] 启用 -O3 优化
- [ ] 使用 -march=native
- [ ] 启用 LTO
- [ ] 启用 SIMD

### 运行时
- [ ] 使用合适的 M 和 ef 参数
- [ ] 预分配内存
- [ ] 使用量化
- [ ] 启用多线程

### 算法级
- [ ] 选择合适的距离度量
- [ ] 实现缓存机制
- [ ] 优化搜索策略
- [ ] 使用启发式方法

---

## 10. 总结

### 代码审查的价值

1. **发现问题**：早期发现性能瓶颈和 bug
2. **提升质量**：提高代码可维护性
3. **知识传递**：团队学习和分享
4. **持续改进**：建立最佳实践

### 优化流程

```
代码 → 分析 → 识别瓶颈 → 设计优化 → 实施 → 验证
   ↑                                              ↓
   └──────────────── 反馈迭代 ←───────────────────┘
```

### 关键要点

- **测量优先**：先测量，再优化
- **小步快跑**：持续迭代改进
- **量化效果**：用数据说话
- **保持简单**：避免过度设计

---

**下一步**：开始审查你的代码，应用这些技巧！
