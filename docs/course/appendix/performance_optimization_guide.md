# USearch 性能优化实战指南
## 从理论到实践的完整优化路径

---

## 🎯 目标

- 将理论知识转化为实际优化
- 掌握性能分析工具链
- 学习系统化的优化方法
- 获得可量化的性能提升

---

## 1. 性能分析工具链

### 1.1 工具全景图

```
应用性能分析
    ├── CPU 级别
    │   ├── perf (Linux)
    │   ├── VTune (Intel)
    │   └── sample (Apple)
    │
    ├── 内存级別
    │   ├── Valgrind (Memcheck)
    │   ├── Valgrind (Massif)
    │   └── heaptrack
    │
    ├── 缓存级別
    │   ├── perf (cache events)
    │   ├── VTune (Cache Analysis)
    │   └── objdump (反汇编)
    │
    └── 应用级別
        ├── Flame Graph
        ├── Chrome Tracing
        └── Custom profiling
```

### 1.2 快速诊断流程

```
第一步：粗略定位
  $ time ./your_program

  real    0m5.234s
  user    4.890s
  sys     0.344s

  判断：CPU密集型

第二步：详细分析
  $ perf record -g ./your_program
  $ perf report

  查看热点函数

第三步：深入分析
  $ perf stat -e cache-references,cache-misses ./your_program

  分析缓存行为

第四步：优化实施
  根据分析结果实施优化

第五步：验证效果
  对比优化前后的性能指标
```

---

## 2. 热点函数分析

### 2.1 识别性能瓶颈

**场景1：距离计算是瓶颈**

```bash
# 运行性能分析
$ perf record -F 99 -g ./bench_cpp
$ perf report --stdio

# 输出：
34.52%  cosine_f32_scalar
    cosine_f32_scalar()
    ::distances
    search_to_insert_()

22.18%  l2_sq_f32_scalar
    l2_sq_f32_scalar()
    ::distances
    search_to_insert_()

15.37%  add_edge
    add_edge()
    ::insert_node
```

**结论**：距离计算占 56.7% 的时间，必须优化！

### 2.2 优化策略

**策略1：启用 SIMD**

```cpp
// 优化前：标量版本
float cosine_f32_scalar(float const* a, float const* b, std::size_t n) {
    float ab = 0, a2 = 0, b2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        ab += a[i] * b[i];
        a2 += a[i] * a[i];
        b2 += b[i] * b[i];
    }
    return ab / std::sqrt(a2 * b2);
}

// 性能：180 ns per call
```

```cpp
// 优化后：SIMD 版本（使用 SimSIMD）
float cosine_f32_simd(float const* a, float const* b, std::size_t n) {
    simsimd_capability_t caps = simsimd_cap_hardware;
    simsimd_metric_punned_t metric = simsimd_metric_cos_f32;

    // 自动选择最优指令集（AVX2/AVX-512/NEON）
    simsimd_cos_f32(a, b, n, &caps, metric);
}

// 性能：25 ns per call
// 加速比：7.2x
```

**实施步骤**：

1. **检测硬件能力**：
```cpp
simsimd_capability_t caps = simsimd_cap_hardware;
std::cout << "Hardware capabilities: " << caps << "\n";
```

2. **选择最优内核**：
```cpp
simsimd_metric_punned_t metric = NULL;
simsimd_find_kernel_punned(
    simsimd_metric_cos_k,
    simsimd_datatype_f32_k,
    caps,
    allowed,
    &metric,
    &used_kind
);
```

3. **调用优化的距离函数**：
```cpp
float distance = metric(a, b, dimensions);
```

---

## 3. 内存优化实战

### 3.1 问题诊断

**症状**：性能低于预期，CPU 使用率不高

```bash
# 分析缓存行为
$ perf stat -e cache-references,cache-misses,cycles,instructions ./bench_cpp

# 输出：
cache references:      2,345,678
cache misses:           923,456  (39.36% of all cache references)
cycles:                4,567,890
instructions:          1,234,567
IPC:                   0.27  (很低！)
```

**分析**：
- 缓存未命中率 39%（应该 < 10%）
- IPC 只有 0.27（应该 > 1.0）
- 说明内存访问是瓶颈

### 3.2 优化方案

**方案1：改善数据局部性**

```cpp
// ❌ 不好：AoS（Array of Structures）
struct Node {
    vector_key_t key;
    level_t level;
    float vector[128];
    uint32_t neighbors[16];
    // 所有数据混在一起
};
std::vector<Node> nodes;

// 访问模式（不连续）：
for (auto& node : nodes) {
    process(node.key);  // 跳过 vector 和 neighbors
}
```

```cpp
// ✅ 好：SoA（Structure of Arrays）
buffer_gt<vector_key_t> keys;     // 连续存储
buffer_gt<level_t> levels;        // 连续存储
buffer_gt<float> vectors;         // 连续存储（分块）
buffer_gt<uint32_t> neighbors;   // 连续存储（分块）

// 访问模式（连续）：
for (std::size_t i = 0; i < count; ++i) {
    process(keys[i]);  // 紧密排列，缓存友好
}
```

**性能提升**：2-3x

**方案2：缓存行对齐**

```cpp
// ❌ 未对齐
struct Node {
    vector_key_t key;      // 8 bytes
    level_t level;         // 2 bytes
    // padding: 6 bytes
    float vector[128];     // 512 bytes
};  // 总计：528 bytes（不是64的倍数）

// ✅ 对齐到 64 字节
struct alignas(64) AlignedNode {
    vector_key_t key;
    level_t level;
    // padding: 54 bytes (显式或隐式)
    float vector[128];
};  // 总计：576 bytes（64的倍数）

// 或者使用编译器属性
struct Node {
    vector_key_t key;
    level_t level;
    float vector[128];
} __attribute__((aligned(64)));
```

**性能提升**：10-20%

**方案3：预取**

```cpp
// 软件预取
for (std::size_t i = 0; i < n; ++i) {
    // 预取下一个元素
    if (i + 1 < n) {
        __builtin_prefetch(&nodes[i + 1], 0, 3);
    }

    // 处理当前元素
    process(nodes[i]);
}
```

**高级预取**：

```cpp
// 自适应预取距离
class AdaptivePrefetcher {
    std::size_t distance_ = 1;

public:
    void update(bool cache_miss) {
        if (cache_miss && distance_ < 16) {
            distance_ *= 2;  // 增大预取距离
        } else if (!cache_miss && distance_ > 1) {
            distance_ /= 2;  // 减小预取距离
        }
    }

    std::size_t get_distance() const { return distance_; }
};
```

---

## 4. 并发优化实战

### 4.1 问题：多线程扩展性差

**症状**：
```
1 线程: 10000 QPS
2 线程: 15000 QPS (期望 20000)
4 线程: 18000 QPS (期望 40000)
8 线程: 19000 QPS (期望 80000)
```

**诊断**：

```cpp
// 可能的原因：
// 1. 全局锁竞争
// 2. false sharing
// 3. 内存带宽饱和
```

### 4.2 优化方案

**方案1：减少锁粒度**

```cpp
// ❌ 全局锁
class GlobalLockIndex {
    std::mutex mutex_;

    void add(int key, float* vector) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 所有操作都锁住
    }
};

// 扩展性：1.5x (2线程)
```

```cpp
// ✅ 节点级锁
class NodeLockIndex {
    buffer_gt<mutex_gt> mutexes_;

    void add(int key, float* vector) {
        compressed_slot_t slot = get_slot(key);
        node_lock_t lock(mutexes_, slot);  // 只锁当前节点

        // 其他线程可以操作不同节点
    }
};

// 扩展性：6x (8线程)
```

**方案2：无锁数据结构**

```cpp
// 原子计数器
class LockFreeIndex {
    std::atomic<std::size_t> size_{0};

    void add(int key, float* vector) {
        // 原子操作（CAS）
        std::size_t slot = size_.fetch_add(1, std::memory_order_relaxed);

        // 无锁写入
        nodes_[slot] = create_node(key, vector);
    }
};

// 扩展性：7x (8线程)
```

**方案3：批量处理**

```cpp
// 批量添加（减少锁操作）
void add_many(std::vector<int> keys, std::vector<float*> vectors) {
    const std::size_t batch_size = 1000;

    for (std::size_t i = 0; i < keys.size(); i += batch_size) {
        std::size_t end = std::min(i + batch_size, keys.size());

        // 每个线程处理一个 batch
        #pragma omp parallel for
        for (std::size_t j = i; j < end; ++j) {
            add(keys[j], vectors[j]);
        }
    }
}

// 扩展性：接近线性
```

---

## 5. 编译优化

### 5.1 编译器标志

```bash
# 基础优化（O2）
-O2 -DNDEBUG

# 最高级别优化（O3）
-O3 -DNDEBUG

# 目标架构优化
-march=native          # 当前 CPU
-march=haswell         # Intel Haswell
-march=znver3          # AMD Zen3

# 链接时优化（LTO）
-flto=auto

# 配置文件导向优化（PGO）
-fprofile-generate     # 第一步：生成 profile
-fprofile-use          # 第二步：使用 profile
```

### 5.2 实际案例

**测试代码**：

```cpp
#include <usearch/index.hpp>
#include <benchmark/benchmark.h>

static void BM_DistanceCalculation(benchmark::State& state) {
    std::vector<float> a(128, 1.0f);
    std::vector<float> b(128, 2.0f);

    for (auto _ : state) {
        float sum = 0;
        for (std::size_t i = 0; i < 128; ++i) {
            sum += a[i] * b[i];
        }
        benchmark::DoNotOptimize(sum);
    }
}

BENCHMARK(BM_DistanceCalculation);
BENCHMARK_MAIN();
```

**编译测试**：

```bash
# 无优化
g++ -O0 test.cpp -lbenchmark -o test_O0
./test_O0
# 结果: ~500 ns per iteration

# O2 优化
g++ -O2 test.cpp -lbenchmark -o test_O2
./test_O2
# 结果: ~50 ns per iteration (10x 加速)

# O3 + march=native
g++ -O3 -march=native test.cpp -lbenchmark -o test_O3
./test_O3
# 结果: ~30 ns per iteration (16x 加速)

# O3 + march=native + LTO
g++ -O3 -march=native -flto test.cpp -lbenchmark -o test_LTO
./test_LTO
# 结果: ~25 ns per iteration (20x 加速)
```

---

## 6. 量化实战

### 6.1 精度-性能权衡

**实验：SIFT-1M 数据集（100万 128维向量）**

| 数据类型 | 内存 | Recall@10 | 搜索延迟 |
|---------|------|-----------|----------|
| f64 | 800 MB | 0.96 | 0.10 ms |
| f32 | 400 MB | 0.96 | 0.10 ms |
| f16 | 200 MB | 0.95 | 0.08 ms |
| bf16 | 200 MB | 0.95 | 0.08 ms |
| i8 | 100 MB | 0.91 | 0.07 ms |
| b1x8 | 12.5 MB | 0.75 | 0.05 ms |

**优化建议**：

1. **内存受限**：使用 f16 或 i8
   - 内存节省：50-75%
   - 精度损失：< 5%
   - 速度提升：10-30%

2. **延迟敏感**：使用 f16 + SIMD
   - 内存节省：50%
   - 精度损失：< 1%
   - 速度提升：15-25%

3. **极度受限**：使用 i8 或 PQ
   - 内存节省：75-95%
   - 精度损失：5-25%
   - 需要重排序

### 6.2 量化实施

**步骤1：评估影响**

```python
import numpy as np
import usearch

# 创建 f32 索引
index_f32 = usearch.Index(ndim=128, metric='cos', dtype='f32')
# ... 添加数据 ...

# 测试 f32 性能
latency_f32 = measure_latency(index_f32)
recall_f32 = measure_recall(index_f32)

print(f"f32: {latency_f32:.3f} ms, Recall: {recall_f32:.3f}")
```

**步骤2：尝试 f16**

```python
# 创建 f16 索引
index_f16 = usearch.Index(ndim=128, metric='cos', dtype='f16')

# 添加相同的数据（自动量化）
index_f16.add(keys, vectors_f32)  # 自动转换为 f16

# 测试 f16 性能
latency_f16 = measure_latency(index_f16)
recall_f16 = measure_recall(index_f16)

memory_f32 = get_memory_usage(index_f32)
memory_f16 = get_memory_usage(index_f16)

print(f"f16: {latency_f16:.3f} ms, Recall: {recall_f16:.3f}")
print(f"内存节省: {(1 - memory_f16/memory_f32)*100:.1f}%")
```

**步骤3：决策**

```python
if recall_f16 >= 0.95:
    print("✓ 使用 f16")
else:
    print("✗ 保持 f32（精度要求高）")
```

---

## 7. 端到端优化案例

### 7.1 案例：优化图像搜索系统

**初始性能**：
```
索引构建：10万张图片，耗时 5 分钟
搜索延迟：平均 200ms
内存使用：2 GB
```

**优化过程**：

**优化1：启用 SIMD**
```python
# 检查硬件加速
print(index.hardware_acceleration)

# 结果：从 none → avx2
# 搜索延迟：200ms → 150ms (1.33x)
```

**优化2：调整参数**
```python
# 降低 M（连接数）
index = usearch.Index(connectivity=8)  # 从 16 降到 8

# 搜索延迟：150ms → 100ms (1.5x)
# 内存使用：2GB → 1.2GB
```

**优化3：使用 f16 量化**
```python
index = usearch.Index(dtype='f16')

# 搜索延迟：100ms → 80ms
# 内存使用：1.2GB → 600MB
```

**优化4：批量搜索**
```python
from concurrent.futures import ThreadPoolExecutor

def batch_search(index, queries, k=10, n_workers=4):
    with ThreadPoolExecutor(max_workers=n_workers) as executor:
        results = executor.map(lambda q: index.search(q, k), queries)
    return list(results)

# QPS：10 → 35 (3.5x)
```

**最终性能**：
```
索引构建：5 分钟 → 2 分钟
搜索延迟：200ms → 80ms (单线程)
QPS：10 → 35 (多线程)
内存使用：2GB → 600MB

总体加速：2.5x (单线程), 8.75x (并发)
```

### 7.2 案例：优化推荐系统

**场景**：100万用户，10万物品

**初始性能**：
```
索引大小：8 GB
更新延迟：100ms
搜索延迟：50ms
```

**优化策略**：

**优化1：使用 i8 量化**
```python
index = usearch.Index(ndim=64, metric='ip', dtype='i8')

# 索引大小：8GB → 2GB (4x 压缩)
# 搜索精度：Recall 0.94 → 0.91
```

**优化2：分片索引**
```python
# 按类别分片
indexes = {
    'electronics': usearch.Index(...),
    'clothing': usearch.Index(...),
    'books': usearch.Index(...)
}

# 只搜索相关类别
def search(category, user_vector, k=10):
    return indexes[category].search(user_vector, k)

# 搜索延迟：50ms → 20ms (只搜索一个分片)
```

**优化3：缓存热门查询**
```python
from functools import lru_cache

@lru_cache(maxsize=1000)
def search_cached(user_id, k=10):
    return search(user_id, k)

# 缓存命中率：40%
# 平均延迟：20ms → 12ms
```

**最终性能**：
```
索引大小：8GB → 2GB (4x)
搜索延迟：50ms → 12ms (4.17x)
更新延迟：100ms → 50ms
```

---

## 8. 性能测试框架

### 8.1 基准测试模板

```cpp
#include <usearch/index.hpp>
#include <benchmark/benchmark.h>
#include <vector>

// 基准测试：搜索性能
static void BM_Search(benchmark::State& state) {
    using namespace unum::usearch;

    // 准备数据
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    const std::size_t n_vectors = 100000;
    std::vector<float> vectors(n_vectors * 128);
    std::vector<std::uint32_t> keys(n_vectors);
    index.add(keys.data(), vectors.data(), n_vectors);

    // 查询向量
    std::vector<float> query(128, 1.0f);

    // 预热
    for (int i = 0; i < 100; ++i) {
        index.search(query.data(), 10);
    }

    // 基准测试
    for (auto _ : state) {
        auto results = index.search(query.data(), 10);
        benchmark::DoNotOptimize(results);
    }
}

// 基准测试：批量插入
static void BM_BatchInsert(benchmark::State& state) {
    using namespace unum::usearch;

    const std::size_t batch_size = 1000;
    std::vector<float> vectors(batch_size * 128);
    std::vector<std::uint32_t> keys(batch_size);

    for (auto _ : state) {
        state.PauseTiming();
        // 只测试插入，不测试索引构建
        state.ResumeTiming();

        index_dense_gt<float, std::uint32_t> index;
        index.init(128, metric_kind_t::cos_k);
        index.add(keys.data(), vectors.data(), batch_size);
    }
}

BENCHMARK(BM_Search);
BENCHMARK(BM_BatchInsert);
BENCHMARK_MAIN();
```

**编译和运行**：

```bash
# 安装 Google Benchmark
git clone https://github.com/google/benchmark.git
cd benchmark && mkdir build && cd build
cmake .. && make && make install

# 编译基准测试
g++ -std=c++17 -O3 -march=native \
    -I../../../include \
    benchmark_search.cpp \
    -lbenchmark \
    -lpthread \
    -o benchmark_search

# 运行
./benchmark_search --benchmark_repetitions=10
```

### 8.2 性能对比工具

```python
import usearch
import numpy as np
import time
import matplotlib.pyplot as plt

def compare_configurations():
    """对比不同配置的性能"""

    dimensions = 128
    n_vectors = 10000
    n_queries = 1000

    vectors = np.random.rand(n_vectors, dimensions).astype(np.float32)
    queries = np.random.rand(n_queries, dimensions).astype(np.float32)

    configs = [
        {'M': 8, 'ef': 16, 'dtype': 'f32'},
        {'M': 16, 'ef': 32, 'dtype': 'f32'},
        {'M': 16, 'ef': 64, 'dtype': 'f32'},
        {'M': 16, 'ef': 64, 'dtype': 'f16'},
    ]

    results = []

    for config in configs:
        # 创建索引
        index = usearch.Index(
            ndim=dimensions,
            metric='cos',
            connectivity=config['M'],
            expansion=config['ef'],
            dtype=config['dtype']
        )

        # 添加向量
        keys = np.arange(n_vectors, dtype=np.uint64)
        start = time.time()
        index.add(keys, vectors)
        build_time = time.time() - start

        # 测试搜索
        latencies = []
        for query in queries:
            start = time.perf_counter()
            index.search(query, 10)
            latencies.append(time.perf_counter() - start)

        avg_latency = np.mean(latencies) * 1000  # ms

        results.append({
            'config': config,
            'build_time': build_time,
            'avg_latency_ms': avg_latency,
            'qps': n_queries / sum(latencies)
        })

    # 可视化
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    configs_str = [str(r['config']) for r in results]
    build_times = [r['build_time'] for r in results]
    latencies = [r['avg_latency_ms'] for r in results]

    ax1.bar(configs_str, build_times)
    ax1.set_ylabel('Build Time (s)')
    ax1.set_title('Index Build Time')
    ax1.tick_params(axis='x', rotation=45)

    ax2.bar(configs_str, latencies)
    ax2.set_ylabel('Avg Latency (ms)')
    ax2.set_title('Search Latency')
    ax2.tick_params(axis='x', rotation=45)

    plt.tight_layout()
    plt.savefig('config_comparison.png', dpi=300)
    plt.show()

    return results

if __name__ == '__main__':
    results = compare_configurations()
    for r in results:
        print(f"{r['config']}: Build={r['build_time']:.2f}s, "
              f"Latency={r['avg_latency_ms']:.3f}ms, QPS={r['qps']:.0f}")
```

---

## 9. 性能优化检查清单

### 编译时优化
- [ ] 使用 Release 模式 (-O3)
- [ ] 指定目标架构 (-march=native)
- [ ] 启用 LTO (-flto=auto)
- [ ] 启用 PGO (fprofile-generate/use)
- [ ] 启用 SIMD (-DUSEARCH_USE_SIMSIMD=1)

### 算法优化
- [ ] 选择合适的距离度量
- [ ] 调整 M (connectivity) 参数
- [ ] 调整 ef (expansion) 参数
- [ ] 使用量化（f16/i8/PQ）
- [ ] 启用预取

### 数据结构优化
- [ ] 使用 SoA 布局
- [ ] 缓存行对齐（64字节）
- [ ] 内存池
- [ ] 压缩存储

### 并发优化
- [ ] 批量操作
- [ ] 细粒度锁
- [ ] 无锁数据结构
- [ ] OpenMP 并行化

### 监控和分析
- [ ] 使用 perf 分析热点
- [ ] 使用 Valgrind 检测内存泄漏
- [ ] 使用 Flame Graph 可视化
- [ ] 定期运行基准测试

---

## 10. 总结

性能优化是一个**系统化的工程**：

1. **测量**：使用工具找出瓶颈
2. **分析**：理解根本原因
3. **优化**：针对性实施优化
4. **验证**：量化优化效果
5. **迭代**：持续改进

记住：**过早优化是万恶之源**，但在了解瓶颈后，优化是必需的！

---

**下一步**：选择一个实际项目，应用这些优化技巧！
