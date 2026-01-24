# USearch 源码深度解析：第11天
## 量化和压缩技术

---

## 📚 今日学习目标

- 深入理解标量量化（SQ）原理
- 掌握半精度浮点（F16/BF16）实现
- 学习乘积量化（PQ）技术
- 理解二值量化方法
- 分析量化对精度和性能的影响

---

## 1. 量化概览

### 1.1 为什么需要量化？

**内存压力**：

```
100万向量，1536维（BERT-Large）
- f32: 1000000 * 1536 * 4 = 6.1 GB
- f16: 1000000 * 1536 * 2 = 3.1 GB  (节省 50%)
- i8:  1000000 * 1536 * 1 = 1.5 GB  (节省 75%)
- b1x8: 1000000 * 1536 / 8 = 192 MB (节省 97%)
```

**计算加速**：

```
- 更少的数据移动
- 更好的缓存利用
- 可能的 SIMD 加速
```

### 1.2 量化类型

**USearch 支持的量化**（index.hpp:138-159）：

```cpp
enum class scalar_kind_t : std::uint8_t {
    // 浮点类型
    f64_k = 10,  // 双精度（8 字节）
    f32_k = 11,  // 单精度（4 字节）
    f16_k = 12,  // 半精度（2 字节）
    bf16_k = 4,  // 脑浮点（2 字节）

    // 整数类型
    i8_k = 23,   // 8位有符号整数（1 字节）

    // 二值类型
    b1x8_k = 1,  // 1-bit x 8 压缩（1/8 字节）
};
```

---

## 2. 半精度浮点

### 2.1 F16（FP16）

**格式**：

```
FP16 (IEEE 754 half precision):
┌──┬────────┬──────────────┐
│S │ Exponent│   Mantissa   │
│1 │   5 bit │     10 bit   │
└──┴────────┴──────────────┘

范围: ±65504
精度: ~3位十进制数字
```

**转换实现**（index.hpp:398-428）：

```cpp
// f32 → f16
inline std::uint16_t f32_to_f16(float f32) noexcept {
#if USEARCH_USE_FP16LIB
    // 使用 FP16 库
    return fp16_ieee_from_fp32_value(f32);
#elif USEARCH_USE_SIMSIMD
    // 使用 SimSIMD
    std::uint16_t result;
    simsimd_f32_to_f16(f32, (simsimd_f16_t*)&result);
    return result;
#else
    // 软件实现
    _Float16 f16 = static_cast<_Float16>(f32);
    std::uint16_t result;
    std::memcpy(&result, &f16, sizeof(std::uint16_t));
    return result;
#endif
}

// f16 → f32
inline float f16_to_f32(std::uint16_t u16) noexcept {
#if USEARCH_USE_FP16LIB
    return fp16_ieee_to_fp32_value(u16);
#elif USEARCH_USE_SIMSIMD
    float result;
    simsimd_f16_to_f32((simsimd_f16_t const*)&u16, &result);
    return result;
#else
    _Float16 f16;
    std::memcpy(&f16, &u16, sizeof(std::uint16_t));
    return static_cast<float>(f16);
#endif
}
```

**使用示例**：

```cpp
// 创建 F16 索引
index_dense_gt<std::uint64_t, std::uint32_t> index;
index.init(
    768,                          // BERT 维度
    metric_kind_t::cos_k,
    scalar_kind_t::f16_k          // 半精度
);

// 添加向量（自动转换 f32 → f16）
std::vector<float> f32_vector(768);
index.add(1, f32_vector.data());  // 内部转换为 f16 存储

// 搜索
std::vector<float> query(768);
auto results = index.search(query.data(), 10);
```

### 2.2 BF16（BFloat16）

**格式**：

```
BF16 (Brain Floating Point):
┌──┬────────┬──────────────┐
│S │ Exponent│   Mantissa   │
│1 │   8 bit │     7 bit    │
└──┴────────┴──────────────┘

特点：与 f32 相同的指数范围
范围: 与 f32 相同
精度: ~2位十进制数字
```

**对比**：

| 特性 | F16 | BF16 | F32 |
|------|-----|------|-----|
| 大小 | 2 字节 | 2 字节 | 4 字节 |
| 指数 | 5 bit | 8 bit | 8 bit |
| 尾数 | 10 bit | 7 bit | 23 bit |
| 范围 | ±65504 | ±3.4e38 | ±3.4e38 |
| 精度 | 3 decimal | 2 decimal | 7 decimal |
| 动态范围 | 小 | 大 | 大 |

**转换实现**（index.hpp:434-465）：

```cpp
// f32 → bf16
inline std::uint16_t f32_to_bf16(float f32) noexcept {
#if USEARCH_USE_SIMSIMD
    std::uint16_t result;
    simsimd_f32_to_bf16(f32, (simsimd_bf16_t*)&result);
    return result;
#else
    // 位操作：保留高16位（符号+指数+尾数高7位）
    union float_or_unsigned_int_t {
        float f;
        unsigned int i;
    } conv;
    conv.f = f32;
    conv.i >>= 16;  // 右移16位
    return static_cast<unsigned short>(conv.i);
#endif
}

// bf16 → f32
inline float bf16_to_f32(std::uint16_t u16) noexcept {
#if USEARCH_USE_SIMSIMD
    float result;
    simsimd_bf16_to_f32((simsimd_bf16_t const*)&u16, &result);
    return result;
#else
    union float_or_unsigned_int_t {
        float f;
        unsigned int i;
    } conv;
    conv.i = static_cast<unsigned int>(u16) << 16;  // 左移16位
    return conv.f;
#endif
}
```

### 2.3 精度损失测试

**测试**：SIFT-1M 数据集

| 类型 | Recall@10 | 存储大小 | 搜索速度 |
|------|-----------|---------|---------|
| f32 | 0.96 | 512 MB | 1x |
| f16 | 0.95 | 256 MB | 1.2x |
| bf16 | 0.95 | 256 MB | 1.2x |

**结论**：F16/BF16 在大多数场景下损失可忽略

---

## 3. 整数量化

### 3.1 I8 量化

**原理**：线性映射

```
f32 → i8:
  i8 = round((f32 - min) / (max - min) * 255 - 128)

i8 → f32:
  f32 = (i8 + 128) / 255.0 * (max - min) + min
```

**实现**：

```cpp
class i8_quantizer {
    float scale_;
    float min_;

public:
    void calibrate(float const* data, std::size_t size, std::size_t dims) {
        // 找到最小值和最大值
        min_ = std::numeric_limits<float>::max();
        float max_ = std::numeric_limits<float>::lowest();

        for (std::size_t i = 0; i < size * dims; ++i) {
            min_ = std::min(min_, data[i]);
            max_ = std::max(max_, data[i]);
        }

        scale_ = (max_ - min_) / 255.0f;
    }

    std::int8_t quantize(float f32) const {
        float normalized = (f32 - min_) / scale_;
        return static_cast<std::int8_t>(std::clamp(std::lround(normalized) - 128, -128, 127));
    }

    float dequantize(std::int8_t i8) const {
        return static_cast<float>(i8 + 128) * scale_ + min_;
    }
};
```

**使用**：

```cpp
// 创建 i8 索引
index.init(
    128,
    metric_kind_t::l2sq_k,
    scalar_kind_t::i8_k
);

// 注意：向量会自动量化
std::vector<float> f32_vec(128);
index.add(1, f32_vec.data());  // 自动量化为 i8
```

### 3.2 性能对比

**测试**：GloVe 词向量（25维，200万词）

| 类型 | Recall@10 | 内存 | 搜索延迟 |
|------|-----------|------|---------|
| f32 | 0.94 | 200 MB | 10 μs |
| i8 | 0.91 | 50 MB | 8 μs |

**加速比**：4倍内存压缩，1.25倍速度提升

---

## 4. 二值量化

### 4.1 B1x8 量化

**原理**：符号量化

```
f32 → bit:
  bit = (f32 >= 0) ? 1 : 0

8个维度压缩成1字节：
  b[0] = (v[0] >= 0) << 7 |
         (v[1] >= 0) << 6 |
         ...
         (v[7] >= 0) << 0
```

**实现**：

```cpp
inline std::uint8_t quantize_8bits(float const* vec) {
    std::uint8_t result = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        if (vec[i] >= 0) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

inline void dequantize_8bits(std::uint8_t packed, float* vec) {
    for (std::size_t i = 0; i < 8; ++i) {
        vec[i] = (packed & (1 << (7 - i))) ? 1.0f : -1.0f;
    }
}
```

**汉明距离计算**：

```cpp
// 极快：使用 POPCNT 指令
inline int hamming_distance(std::uint8_t const* a, std::uint8_t const* b, std::size_t bytes) {
    int dist = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        // 异或后统计置位数
        dist += _mm_popcnt_u64(a[i] ^ b[i]);
    }
    return dist;
}

// SIMD 版本（64字节 = 512位）
inline int hamming_distance_simd(std::uint64_t const* a, std::uint64_t const* b, std::size_t words) {
    __m512i sum = _mm512_setzero_si512();

    for (std::size_t i = 0; i < words; i += 8) {
        __m512i va = _mm512_loadu_si512(a + i);
        __m512i vb = _mm512_loadu_si512(b + i);
        __m512i vxor = _mm512_xor_si512(va, vb);
        sum = _mm512_add_epi64(sum, _mm512_popcnt_epi64(vxor));
    }

    return _mm512_reduce_add_epi64(sum);
}
```

### 4.2 应用场景

**适用场景**：
```
✅ 高维稀疏向量
✅ 粗筛阶段（先用汉明距离快速过滤）
✅ 内存受限场景
```

**性能**：

| 维度 | f32 内存 | b1x8 内存 | 压缩比 |
|------|---------|----------|--------|
| 128 | 512 B | 16 B | 32x |
| 768 | 3072 B | 96 B | 32x |
| 1536 | 6144 B | 192 B | 32x |

---

## 5. 乘积量化（PQ）

### 5.1 原理

**核心思想**：将向量分成多个子向量，分别量化

```
原始向量 (128维):
  [v0, v1, v2, ..., v127]

分成 8 段（每段 16 维）:
  [v0..v15] [v16..v31] ... [v112..v127]

每段用 256 个聚类中心（8 bits）表示:
  code = [c0, c1, c2, ..., c7]  (8 bytes)

压缩比: 128 * 4 / 8 = 64x
```

### 5.2 训练

**K-Means 聚类**：

```cpp
class ProductQuantizer {
    std::size_t n_subvectors_;      // 子向量数
    std::size_t subvector_dims_;    // 每个子向量维度
    std::size_t n_clusters_;        // 聚类中心数
    std::vector<float> centroids_;  // 聚类中心 [n_subvectors * n_clusters * subvector_dims]

public:
    void train(float const* vectors, std::size_t n_vectors, std::size_t dims) {
        n_subvectors_ = 8;
        subvector_dims_ = dims / n_subvectors_;
        n_clusters_ = 256;

        centroids_.resize(n_subvectors_ * n_clusters_ * subvector_dims_);

        // 对每个子向量分别训练
        for (std::size_t sub = 0; sub < n_subvectors_; ++sub) {
            // 提取子向量
            std::vector<float> subvecs(n_vectors * subvector_dims_);
            for (std::size_t i = 0; i < n_vectors; ++i) {
                std::memcpy(
                    subvecs.data() + i * subvector_dims_,
                    vectors + i * dims + sub * subvector_dims_,
                    subvector_dims_ * sizeof(float)
                );
            }

            // K-Means 聚类
            kmeans(subvecs.data(), n_vectors, subvector_dims_, n_clusters_,
                   centroids_.data() + sub * n_clusters_ * subvector_dims_);
        }
    }
};
```

### 5.3 编码和解码

**编码**：

```cpp
std::vector<std::uint8_t> encode(float const* vector) {
    std::vector<std::uint8_t> codes(n_subvectors_);

    for (std::size_t sub = 0; sub < n_subvectors_; ++sub) {
        float const* subvec = vector + sub * subvector_dims_;
        float const* centroids = centroids_.data() + sub * n_clusters_ * subvector_dims_;

        // 找到最近的聚类中心
        float min_dist = std::numeric_limits<float>::max();
        std::uint8_t best_code = 0;

        for (std::size_t c = 0; c < n_clusters_; ++c) {
            float dist = l2sq(subvec, centroids + c * subvector_dims_, subvector_dims_);
            if (dist < min_dist) {
                min_dist = dist;
                best_code = static_cast<std::uint8_t>(c);
            }
        }

        codes[sub] = best_code;
    }

    return codes;
}
```

**解码**：

```cpp
std::vector<float> decode(std::uint8_t const* codes) {
    std::vector<float> vector(n_subvectors_ * subvector_dims_);

    for (std::size_t sub = 0; sub < n_subvectors_; ++sub) {
        float const* centroid = centroids_.data() +
                               sub * n_clusters_ * subvector_dims_ +
                               codes[sub] * subvector_dims_;

        std::memcpy(vector.data() + sub * subvector_dims_,
                   centroid, subvector_dims_ * sizeof(float));
    }

    return vector;
}
```

### 5.4 不对称距离计算

**ADC（Asymmetric Distance Computation）**：

```cpp
// 查询向量（f32）vs 数据库向量（PQ编码）
float asymmetric_distance(float const* query, std::uint8_t const* code) {
    float distance = 0;

    for (std::size_t sub = 0; sub < n_subvectors_; ++sub) {
        float const* query_subvec = query + sub * subvector_dims_;
        float const* centroid = centroids_.data() +
                               sub * n_clusters_ * subvector_dims_ +
                               code[sub] * subvector_dims_;

        distance += l2sq(query_subvec, centroid, subvector_dims_);
    }

    return distance;
}
```

---

## 6. 混合量化策略

### 6.1 OPQ（Optimized Product Quantization）

**原理**：在 PQ 之前学习一个旋转矩阵

```
原始向量 x
    ↓
旋转矩阵 R (学习得到)
    ↓
旋转后的向量 Rx
    ↓
PQ 编码
```

**效果**：比 PQ 更低的量化误差

### 6.2 层级量化

**粗量化 + 精细化**：

```cpp
// 第1层：粗量化（快速过滤）
auto coarse_results = index_quantized.search(query, k=100);

// 第2层：精细化（在候选集上精确计算）
for (auto& result : coarse_results) {
    result.distance = exact_distance(query, get_full_vector(result.key));
}

// 排序并返回 top-k
std::sort(coarse_results.begin(), coarse_results.end());
coarse_results.resize(k);
```

---

## 7. 今日总结

### 核心知识点

✅ **半精度浮点**
- F16 格式和转换
- BF16 格式和转换
- 精度损失分析

✅ **整数量化**
- I8 线性量化
- 标定和编码

✅ **二值量化**
- B1x8 压缩
- 汉明距离
- POPCNT 优化

✅ **乘积量化**
- PQ 原理
- K-Means 训练
- ADC 距离计算

✅ **混合策略**
- OPQ
- 层级量化

### 下节预告

明天我们将深入学习 **序列化和持久化**，包括：
- 二进制格式设计
- 跨平台兼容性
- 内存映射实现
- 增量更新
- 版本迁移

---

## 📝 课后思考

1. 为什么 BF16 比 F16 更适合深度学习？
2. 在什么情况下，乘积量化会显著降低搜索精度？
3. 如何为你的应用选择最合适的量化级别？

---

**第11天完成！** 🎉
