# USearch API 完整参考手册
## Complete API Reference Guide

---

## 📚 目录

1. [核心类参考](#核心类参考)
2. [配置参数详解](#配置参数详解)
3. [距离度量](#距离度量)
4. [量化类型](#量化类型)
5. [常用模式](#常用模式)
6. [性能优化API](#性能优化api)
7. [多语言绑定](#多语言绑定)

---

## 1. 核心类参考

### 1.1 index_dense_gt

稠密向量索引的主类，用于固定维度的向量搜索。

#### 模板参数

```cpp
template <
    typename key_t = std::uint32_t,      // 键类型
    typename compressed_t = float,       // 压缩向量类型
    std::size_t alignment = 8ul          // 内存对齐
>
class index_dense_gt
```

**参数说明**：
- `key_t`: 向量的键类型，通常是 `std::uint32_t` 或 `std::uint64_t`
- `compressed_t`: 压缩存储类型，影响内存使用和精度
- `alignment`: 内存对齐，影响 SIMD 性能

#### 构造函数

```cpp
// 默认构造
index_dense_gt() = default;

// 带配置构造构
explicit index_dense_gt(index_dense_config_t config);

// 拷贝和移动
index_dense_gt(index_dense_gt const&) = delete;
index_dense_gt(index_dense_gt&&) = default;
index_dense_gt& operator=(index_dense_gt&&) = default;
```

#### 核心方法

##### init() - 初始化索引

```cpp
/**
 * 初始化索引
 *
 * @param dimensions    向量维度
 * @param metric        距离度量类型
 * @param scalar        标量类型（f32, f16, i8等）
 * @param config        额外配置
 * @return              成功返回 true
 */
bool init(std::size_t dimensions,
         metric_kind_t metric = metric_kind_t::cos_k,
         scalar_kind_t scalar = scalar_kind_t::f32_k,
         index_dense_config_t config = {});
```

**示例**：
```cpp
index_dense_gt<float, std::uint32_t> index;

// 基础初始化
index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

// 带配置初始化
index_dense_config_t config;
config.connectivity = 16;      // M 参数
config.expansion = 64;         // ef_construction 参数
index.init(768, metric_kind_t::ip_k, scalar_kind_t::f16_k, config);
```

##### add() - 添加向量

```cpp
/**
 * 添加单个向量
 *
 * @param key       向量唯一标识
 * @param vector    向量数据指针
 * @return          成功返回 true
 */
bool add(key_t key, float const* vector);

/**
 * 批量添加向量（推荐）
 *
 * @param keys      键数组
 * @param vectors   向量数组（连续存储）
 * @param count     向量数量
 * @return          成功返回 true
 */
bool add(key_t const* keys, float const* vectors, std::size_t count);
```

**示例**：
```cpp
// 单个添加
index.add(1, my_vector);

// 批量添加（高效）
std::vector<std::uint32_t> keys = {1, 2, 3, 4, 5};
std::vector<float> vectors = { /* 5 * 128 维的数据 */ };
index.add(keys.data(), vectors.data(), 5);
```

##### search() - 搜索最近邻

```cpp
/**
 * 搜索最近邻
 *
 * @param query    查询向量
 * @param k        返回结果数量
 * @return         结果向量（按距离排序）
 */
std::vector<result_t> search(float const* query, std::size_t k) const;

/**
 * 带过滤的搜索
 *
 * @param query       查询向量
 * @param k           返回结果数量
 * @param filter      过滤条件函数
 * @return            结果向量
 */
std::vector<result_t> search(
    float const* query,
    std::size_t k,
    std::function<bool(key_t)> filter
) const;
```

**result_t 结构**：
```cpp
struct result_t {
    key_t key;           // 向量键
    float distance;      // 距离值
};
```

**示例**：
```cpp
// 基础搜索
auto results = index.search(query_vector, 10);

// 带过滤的搜索（只搜索特定范围的键）
auto filtered_results = index.search(query, 10, [](key_t key) {
    return key >= 1000 && key < 2000;
});

// 处理结果
for (auto const& result : results) {
    std::cout << "Key: " << result.key
              << ", Distance: " << result.distance << "\n";
}
```

##### remove() - 删除向量

```cpp
/**
 * 删除向量（标记为删除）
 *
 * @param key    要删除的键
 * @return       成功返回 true
 */
bool remove(key_t key);

/**
 * 批量删除
 *
 * @param keys   键数组
 * @param count  数量
 * @return       成功删除的数量
 */
std::size_t remove(key_t const* keys, std::size_t count);
```

**示例**：
```cpp
// 单个删除
index.remove(123);

// 批量删除
std::vector<key_t> keys_to_remove = {100, 101, 102};
std::size_t removed = index.remove(keys_to_remove.data(), keys_to_remove.size());
```

##### 容量查询

```cpp
/**
 * @return 当前索引中的向量数量
 */
std::size_t size() const;

/**
 * @return 已分配的容量
 */
std::size_t capacity() const;

/**
 * @brief 预留容量
 */
void reserve(std::size_t capacity);

/**
 * @brief 检查索引是否为空
 */
bool empty() const;
```

---

### 1.2 index_config_t

索引配置参数集合。

```cpp
struct index_config_t {
    // 连接性参数（HNSW 的 M）
    std::size_t connectivity = 16;

    // 扩展因子（ef_construction / ef_search）
    std::size_t expansion = 64;

    // 最大层级
    std::size_t max_elements_additive_ratio = 0.05;  // 5% of base layer

    // 内存对齐
    std::size_t alignment = 64;  // 缓存行对齐
};
```

**参数调优指南**：

| 参数 | 范围 | 效果 | 推荐值 |
|------|------|------|--------|
| connectivity | 8-32 | ↑精度，↓速度，↑内存 | 16（通用）<br>32（高精度）<br>8（节省内存） |
| expansion | 32-256 | ↑精度，↓速度 | 64（通用）<br>128（高精度）<br>32（快速） |

---

## 2. 配置参数详解

### 2.1 metric_kind_t - 距离度量

```cpp
enum class metric_kind_t {
    unknown_k = 0,

    // 余弦距离（推荐）
    cos_k,

    // 内积（点积）
    ip_k,

    // 欧氏距离（L2）
    l2sq_k,

    // 其他
    haversine_k,        // 球面距离
    divergence_k,       // KL 散度
    pearson_k,          // 皮尔逊相关
    jaccard_k,          // Jaccard 相似度
    hamming_k,          // 汉明距离
    tanimoto_k,         // Tanimoto 系数
    sorensen_k,         // Sørensen–Dice 系数
};
```

**选择指南**：

| 度量 | 适用场景 | 向量要求 |
|------|----------|----------|
| cos_k | 文本、图像嵌入 | 归一化向量 |
| ip_k | 推荐系统 | 不需要归一化 |
| l2sq_k | 计算机视觉 | 通用 |

**示例**：
```cpp
// 余弦距离（最常用）
index.init(128, metric_kind_t::cos_k);

// 内积（推荐系统）
index.init(64, metric_kind_t::ip_k);

// L2 距离
index.init(256, metric_kind_t::l2sq_k);
```

### 2.2 scalar_kind_t - 标量类型

```cpp
enum class scalar_kind_t {
    unknown_k = 0,

    // 浮点类型
    f32_k,    // 32位浮点（标准）
    f64_k,    // 64位浮点
    f16_k,    // 16位浮点（半精度）
    bf16_k,   // BFloat16

    // 整数类型
    i8_k,     // 8位整数（量化）
    i8_k,     // 1位（二进制）
};
```

**内存和精度对比**：

| 类型 | 每维度字节数 | 精度 | 内存节省 | 推荐场景 |
|------|-------------|------|----------|----------|
| f32_k | 4 | 高 | 1x | 通用 |
| f16_k | 2 | 中 | 2x | 深度学习 |
| bf16_k | 2 | 中高 | 2x | 训练模型 |
| i8_k | 1 | 低 | 4x | 大规模部署 |

**示例**：
```cpp
// 标准浮点（最精确）
index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

// 半精度（节省内存）
index.init(128, metric_kind_t::cos_k, scalar_kind_t::f16_k);

// 8位量化（大规模）
index.init(128, metric_kind_t::cos_k, scalar_kind_t::i8_k);
```

---

## 3. 序列化 API

### 3.1 save() - 保存索引

```cpp
/**
 * 保存索引到文件
 *
 * @param path    文件路径
 * @return        成功返回 true
 */
bool save(char const* path) const;

/**
 * 保存到内存缓冲区
 *
 * @param buffer  输出缓冲区
 * @return        成功返回 true
 */
bool save(std::vector<char>& buffer) const;
```

**示例**：
```cpp
// 保存到文件
index.save("my_index.usearch");

// 保存到内存
std::vector<char> buffer;
index.save(buffer);
```

### 3.2 load() - 加载索引

```cpp
/**
 * 从文件加载索引
 *
 * @param path    文件路径
 * @return        成功返回 true
 */
bool load(char const* path);

/**
 * 从内存加载
 *
 * @param buffer  数据缓冲区
 * @return        成功返回 true
 */
bool load(char const* buffer, std::size_t length);
```

**示例**：
```cpp
// 从文件加载
index.load("my_index.usearch");

// 从内存加载
std::vector<char> buffer = load_file("index.usearch");
index.load(buffer.data(), buffer.size());
```

### 3.3 view() - 内存映射

```cpp
/**
 * 内存映射文件（零拷贝加载）
 *
 * @param path    文件路径
 * @return        成功返回 true
 */
bool view(char const* path);
```

**示例**：
```cpp
// 大文件使用内存映射（不占用RAM）
index.view("huge_index.usearch");
```

---

## 4. 并发控制

### 4.1 线程安全

USearch 提供两种并发模式：

```cpp
// 配置并发
struct index_config_t {
    // 启用多线程
    bool multi = false;

    // 锁粒度
    enum class lock_level_t {
        none,       // 无锁（单线程）
        node,       // 节点级锁（默认）
        shard,      // 分片级锁
    } lock_level = lock_level_t::node;
};
```

**示例**：
```cpp
// 多线程配置
index_dense_config_t config;
config.multi = true;
config.lock_level = index_dense_config_t::lock_level_t::node;

index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k, config);

// 并发添加
#pragma omp parallel for
for (int i = 0; i < n; ++i) {
    index.add(keys[i], vectors + i * 128);
}
```

---

## 5. 高级 API

### 5.1 过滤搜索

```cpp
// 布尔过滤
auto results = index.search(query, 10, [](key_t key) {
    // 只返回特定类别的向量
    return get_category(key) == "documents";
});

// 范围过滤
auto results = index.search(query, 10, [](key_t key) {
    return key >= min_id && key <= max_id;
});
```

### 5.2 批量搜索

```cpp
/**
 * 批量搜索（优化版本）
 *
 * @param queries    查询向量数组
 * @param k          每个查询的结果数
 * @return           二维结果数组
 */
std::vector<std::vector<result_t>> search_batch(
    float const* queries,
    std::size_t n_queries,
    std::size_t k
) const;
```

**示例**：
```cpp
// 批量查询 1000 个向量
std::vector<float> queries(1000 * 128);
// ... 填充查询 ...

auto batch_results = index.search_batch(queries.data(), 1000, 10);

// 处理结果
for (std::size_t i = 0; i < batch_results.size(); ++i) {
    std::cout << "Query " << i << " found "
              << batch_results[i].size() << " results\n";
}
```

### 5.3 索引融合

```cpp
/**
 * 融合另一个索引
 *
 * @param other    另一个索引
 * @return         成功返回 true
 */
bool merge(index_dense_gt const& other);
```

**示例**：
```cpp
// 合并两个索引
index_dense_gt<float, std::uint32_t> index1, index2;

// 各自构建
index1.init(128);
index1.add(keys1.data(), vectors1.data(), n1);

index2.init(128);
index2.add(keys2.data(), vectors2.data(), n2);

// 融合
index1.merge(index2);
```

---

## 6. 性能优化 API

### 6.1 内存预分配

```cpp
// 预分配避免频繁重分配
index.reserve(1000000);  // 预留100万向量的空间
```

### 6.2 批量操作优化

```cpp
// 批量添加比单个添加快 10-100 倍
// ❌ 慢
for (std::size_t i = 0; i < n; ++i) {
    index.add(keys[i], vectors + i * d);
}

// ✅ 快
index.add(keys, vectors, n);
```

### 6.3 配置优化

```cpp
index_dense_config_t config;

// 高精度配置
config.connectivity = 32;      // 更高连通性
config.expansion = 128;        // 更大搜索范围
config.multi = true;           // 启用多线程

index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k, config);
```

---

## 7. 常用模式

### 7.1 基础使用模式

```cpp
#include <usearch/index_dense.hpp>

using namespace unum::usearch;

// 1. 创建索引
index_dense_gt<float, std::uint32_t> index;
index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

// 2. 添加向量
std::vector<std::uint32_t> keys(1000);
std::vector<float> vectors(1000 * 128);
// ... 填充数据 ...
index.add(keys.data(), vectors.data(), 1000);

// 3. 搜索
float query[128];
// ... 填充查询 ...
auto results = index.search(query, 10);

// 4. 使用结果
for (auto const& result : results) {
    std::cout << result.key << ": " << result.distance << "\n";
}
```

### 7.2 增量更新模式

```cpp
// 持续添加新向量
while (has_new_data()) {
    std::vector<key_t> new_keys;
    std::vector<float> new_vectors;

    // 收集新数据
    collect_new_data(new_keys, new_vectors);

    // 批量添加
    if (!new_keys.empty()) {
        index.add(new_keys.data(), new_vectors.data(), new_keys.size());
    }

    // 定期保存检查点
    if (need_checkpoint()) {
        index.save("checkpoint.usearch");
    }
}
```

### 7.3 多线程搜索模式

```cpp
#include <omp.h>

// 并行搜索多个查询
std::vector<std::vector<result_t>> parallel_search(
    index_dense_gt<float, std::uint32_t>& index,
    float const* queries,
    std::size_t n_queries,
    std::size_t k
) {
    std::vector<std::vector<result_t>> all_results(n_queries);

    #pragma omp parallel for
    for (std::size_t i = 0; i < n_queries; ++i) {
        all_results[i] = index.search(queries + i * 128, k);
    }

    return all_results;
}
```

---

## 8. 错误处理

### 8.1 错误类型

```cpp
enum class error_t {
    success_k = 0,
    error_opening_file_k,
    error_reading_file_k,
    error_writing_file_k,
    error_memory_allocation_k,
    error_invalid_metric_k,
    error_invalid_dimension_k,
    error_invalid_argument_k,
};
```

### 8.2 错误处理示例

```cpp
// 带错误检查的加载
if (!index.load("index.usearch")) {
    std::cerr << "Failed to load index\n";
    // 处理错误
}

// 带异常的包装
template <typename Func>
auto try_or_throw(Func&& func, const char* msg) {
    auto result = func();
    if (!result) {
        throw std::runtime_error(msg);
    }
    return result;
}

// 使用
try {
    try_or_throw([&] { return index.load("index.usearch"); },
                 "Cannot load index");
} catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

---

## 9. Python API 快速参考

### 9.1 基础使用

```python
from usearch.index import Index

# 创建索引
index = Index(
    ndim=128,              # 维度
    metric='cos',          # 距离度量
    dtype='f32',           # 数据类型
    connectivity=16,       # M 参数
    expansion=64           # ef 参数
)

# 添加向量
keys = [1, 2, 3, 4, 5]
vectors = [...]  # 形状 (5, 128)
index.add(keys, vectors)

# 搜索
results = index.search(query, k=10)

# 保存和加载
index.save("index.usearch")
index.load("index.usearch")
```

### 9.2 NumPy 集成

```python
import numpy as np
from usearch.index import Index

# 从 NumPy 数组
index = Index(ndim=128)
vectors = np.random.rand(1000, 128).astype(np.float32)
keys = np.arange(1000)

index.add(keys, vectors)

# 搜索（返回 NumPy 数组）
query = np.random.rand(128).astype(np.float32)
matches, distances = index.search(query, k=10)

# 批量搜索
queries = np.random.rand(100, 128).astype(np.float32)
batch_results = index.search_batch(queries, k=10)
```

---

## 10. 性能基准

### 10.1 不同配置的性能

| 配置 | 构建时间 | 搜索延迟 | 召回率 | 内存 |
|------|---------|---------|--------|------|
| M=8, ef=32 | 100 ms | 0.5 ms | 85% | 1x |
| M=16, ef=64 | 200 ms | 1 ms | 95% | 1.5x |
| M=32, ef=128 | 400 ms | 2 ms | 98% | 2x |

### 10.2 优化建议

**对于延迟敏感**：
- 使用较小的 M (8-12)
- 使用 f16 量化
- 启用 SIMD

**对于精度敏感**：
- 使用较大的 M (24-32)
- 使用较大的 ef (128+)
- 保持 f32 精度

**对于大规模部署**：
- 批量操作
- 分布式分片
- 使用量化

---

## 附录

### A. 完整示例

```cpp
#include <usearch/index_dense.hpp>
#include <iostream>
#include <vector>

using namespace unum::usearch;

int main() {
    // 1. 创建索引
    index_dense_gt<float, std::uint32_t> index;
    index_dense_config_t config;
    config.connectivity = 16;
    config.expansion = 64;
    index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k, config);

    // 2. 生成数据
    constexpr std::size_t n = 10000;
    std::vector<std::uint32_t> keys(n);
    std::vector<float> vectors(n * 128);

    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i;
        for (std::size_t j = 0; j < 128; ++j) {
            vectors[i * 128 + j] = static_cast<float>(rand()) / RAND_MAX;
        }
    }

    // 3. 添加向量
    auto start = std::chrono::high_resolution_clock::now();
    index.add(keys.data(), vectors.data(), n);
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Added " << n << " vectors in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms\n";

    // 4. 搜索
    float query[128];
    for (auto& v : query) {
        v = static_cast<float>(rand()) / RAND_MAX;
    }

    start = std::chrono::high_resolution_clock::now();
    auto results = index.search(query, 10);
    end = std::chrono::high_resolution_clock::now();

    std::cout << "Search completed in "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
              << " us\n";

    // 5. 输出结果
    for (std::size_t i = 0; i < results.size(); ++i) {
        std::cout << (i + 1) << ". Key: " << results[i].key
                  << ", Distance: " << results[i].distance << "\n";
    }

    // 6. 保存索引
    index.save("example_index.usearch");
    std::cout << "Index saved\n";

    return 0;
}
```

### B. 编译命令

```bash
# 基础编译
g++ -std=c++17 -O3 -I/path/to/usearch/include \
    example.cpp -o example

# 启用 OpenMP
g++ -std=c++17 -O3 -fopenmp -I/path/to/usearch/include \
    example.cpp -o example

# 启用所有优化
g++ -std=c++17 -O3 -march=native -fopenmp \
    -I/path/to/usearch/include \
    example.cpp -o example
```

---

**版本**: v1.0
**最后更新**: 2025-01-24
