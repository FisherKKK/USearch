# USearch 快速参考卡

## 核心概念速查

### 数据类型

```cpp
// 向量键类型
using vector_key_t = std::uint64_t;

// 图层级类型
using level_t = std::int16_t;

// 压缩槽位类型
using compressed_slot_t = std::uint32_t;

// 距离类型
using distance_t = float;
```

### 标量类型大小

| 类型 | 大小 | 说明 |
|------|------|------|
| f64 | 8 bytes | 双精度浮点 |
| f32 | 4 bytes | 单精度浮点 |
| f16 | 2 bytes | IEEE 半精度 |
| bf16 | 2 bytes | 脑浮点 |
| i8 | 1 byte | 8位有符号整数 |
| b1x8 | 1/8 byte | 1-bit x8 压缩 |

### 距离度量

```cpp
// 内积
metric_kind_t::ip_k

// 余弦相似度
metric_kind_t::cos_k

// L2 平方距离
metric_kind_t::l2sq_k

// 汉明距离
metric_kind_t::hamming_k

// Haversine 距离
metric_kind_t::haversine_k
```

### 关键配置参数

```cpp
// 连接数
std::size_t connectivity = 16;  // 每层最大邻居数

// 扩展因子
std::size_t expansion = 64;     // 搜索时的候选集大小

// 构建扩展因子
std::size_t ef_construction = 200;

// 层级乘数
double ml = 1.0 / std::log(connectivity);
```

## 常用代码模式

### 创建索引

```cpp
// C++
#include <usearch/index_dense.hpp>

using namespace unum::usearch;

index_dense_gt<float, std::uint32_t> index;
index.init(
    128,                        // 维度
    metric_kind_t::cos_k,       // 度量
    scalar_kind_t::f32_k        // 标量类型
);
```

```python
# Python
import usearch

index = usearch.Index(
    ndim=128,
    metric='cos',
    dtype='f32',
    connectivity=16,
    expansion=64
)
```

### 添加向量

```cpp
// 单个添加
std::vector<float> vector(128);
index.add(key, vector.data());

// 批量添加
std::vector<std::uint32_t> keys(n);
std::vector<float> vectors(n * 128);
index.add(keys.data(), vectors.data(), n);
```

### 搜索

```cpp
// 搜索
std::vector<float> query(128);
auto results = index.search(query.data(), k=10);

// 访问结果
for (auto& [key, distance] : results) {
    std::cout << key << ": " << distance << "\n";
}
```

### 保存和加载

```cpp
// 保存
index.save("index.usearch");

// 加载
index.load("index.usearch");

// 内存映射
index.restore("index.usearch", view=true);
```

## 性能优化清单

### 编译选项

```bash
# Release 构建
cmake -D CMAKE_BUILD_TYPE=Release \
      -D USEARCH_USE_OPENMP=1 \
      -D USEARCH_USE_SIMSIMD=1 \
      -D USEARCH_USE_JEMALLOC=1 \
      ..
```

### Python 优化

```python
# 启用 SIMD
index = usearch.Index(
    ndim=128,
    metric='cos',
    dtype='f32'
)
print(index.hardware_acceleration)  # 检查是否启用
```

### 参数调优

| 场景 | M | ef | ef_construction |
|------|---|----|----|
| 高精度 | 32-64 | 128-256 | 400 |
| 平衡 | 16 | 64 | 200 |
| 低延迟 | 8 | 32 | 100 |
| 内存受限 | 8-16 | 32-64 | 100-200 |

## 内存估算

### 公式

```
总内存 = 向量内存 + 图内存 + 元数据

向量内存 = N × D × sizeof(scalar)
图内存 ≈ N × M × log(N) × sizeof(pointer)
元数据 ≈ N × (key_size + level_size)
```

### 快速估算

| N | D (f32) | 总内存 (M=16) |
|---|---------|--------------|
| 1K | 128 | ~1 MB |
| 100K | 128 | ~80 MB |
| 1M | 128 | ~800 MB |
| 10M | 128 | ~8 GB |

## 复杂度速查

| 操作 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| 搜索 | O(log N × M) | O(1) |
| 插入 | O(log N × M) | O(1) |
| 构建 | O(N × M × log N) | O(N × M × log N) |
| 保存 | O(N × M × log N) | - |
| 加载 | O(1)* | O(N × M × log N) |

*内存映射模式下

## 调试技巧

### 启用详细输出

```cpp
// 设置日志级别
index.verbose(true);
```

### 性能分析

```bash
# 使用 perf
perf record -g ./your_program
perf report

# 检查缓存命中率
perf stat -e cache-references,cache-misses ./your_program
```

### 内存分析

```bash
# Valgrind
valgrind --leak-check=full ./your_program

# 追踪内存分配
valgrind --tool=massif ./your_program
```

## 常见问题速查

### Q: 搜索结果不一致？

**A**: 检查 ef 参数，或使用固定的随机种子。

### Q: 内存占用过高？

**A**: 尝试减小 M，或使用量化（f16/i8）。

### Q: 构建速度慢？

**A**: 减小 ef_construction，或使用批量添加。

### Q: 搜索速度慢？

**A**: 减小 ef，或启用 SIMD 加速。

### Q: 召回率低？

**A**: 增大 ef，或增大 M 和 ef_construction。

## 性能基准

### 硬件：Intel i7-12700K

| 数据集 | N | D | Recall@10 | 延迟 | QPS |
|--------|---|---|-----------|------|-----|
| SIFT-1M | 1M | 128 | 0.96 | 0.1ms | 10K |
| GloVe | 2M | 25 | 0.94 | 0.05ms | 20K |
| BERT | 1M | 768 | 0.95 | 0.2ms | 5K |

## 最佳实践

### ✅ DO

- 批量添加向量
- 预分配空间
- 使用适当的量化
- 监控性能指标
- 定期保存索引

### ❌ DON'T

- 循环中单条添加
- 盲目增大参数
- 忽略数据归一化
- 在热路径上锁
- 忘记序列化

## 资源链接

- **GitHub**: https://github.com/unum-cloud/usearch
- **文档**: https://unum-cloud.github.io/usearch/
- **论文**: HNSW 原论文 arXiv:1603.09320
- **Discussions**: GitHub Discussions

## 版本兼容性

| USearch | Python | C++ | Notes |
|---------|--------|-----|-------|
| 2.23.x | 3.8-3.12 | C++11 | 当前稳定版 |
| 2.22.x | 3.8-3.11 | C++11 | 上一版本 |

## 快速命令

```bash
# 编译
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 测试
./test_cpp

# 基准测试
./bench_cpp

# Python 安装
pip install usearch
```

---

**提示**: 将此参考卡打印出来，放在桌面上随时查阅！
