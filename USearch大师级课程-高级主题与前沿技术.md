# 🔥 USearch大师级课程 - 高级主题与前沿技术

> 深入探索USearch未被覆盖的高级主题:从汇编级优化到分布式架构

**版本:** v1.0 | **难度:** ⭐⭐⭐⭐⭐ | **前置要求:** 完成14天深度进阶课程

---

## 📚 课程导航

### 第一部分: 底层系统优化 (Advanced 1-5)
- **Advanced 1:** 汇编级性能分析与内联汇编
- **Advanced 2:** CPU亲和性与NUMA优化
- **Advanced 3:** JIT编译与动态代码生成
- **Advanced 4:** TLB优化与大页内存
- **Advanced 5:** 硬件性能计数器深度应用

### 第二部分: 算法前沿 (Advanced 6-9)
- **Advanced 6:** Product Quantization实现细节
- **Advanced 7:** 图神经网络与HNSW融合
- **Advanced 8:** 自适应参数调优算法
- **Advanced 9:** 在线索引重平衡与迁移

### 第三部分: 分布式架构 (Advanced 10-12)
- **Advanced 10:** 分布式HNSW设计与实现
- **Advanced 11:** 一致性哈希与数据分片
- **Advanced 12:** Raft协议与索引复制

### 第四部分: 异构计算 (Advanced 13-15)
- **Advanced 13:** CUDA加速向量搜索
- **Advanced 14:** AVX-512深度优化技巧
- **Advanced 15:** FPGA加速与HLS设计

---

## Advanced 1: 汇编级性能分析与内联汇编 🔧

### 1.1 为什么需要深入汇编?

**场景:** 你已经应用了所有C++优化技巧,但性能仍未达到预期

```cpp
// 示例: 余弦距离计算
float cosine(const float* a, const float* b, size_t n) {
    float dot = 0, norm_a = 0, norm_b = 0;
    for (size_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}
```

**问题:** 即使开启`-O3`优化,性能仍不如预期

**解决:** 必须查看编译器生成的汇编代码

### 1.2 汇编分析工具链

#### 工具1: Compiler Explorer (Godbolt)

```bash
# 访问 https://godbolt.org/

# 配置:
# - Compiler: GCC 13.2
# - Options: -O3 -march=skylake -RSpace=asm
# - 将上述cosine函数粘贴

# 观察:
1. 循环是否向量化?
2. 是否使用FMA指令?
3. 是否有不必要的内存访问?
```

**实际汇编输出分析:**

```asm
# GCC -O3 -march=skylake
cosine(float const*, float const*, unsigned long):
    test    rdx, rdx              # 检查n是否为0
    je      .L10                  # 为0则跳转
    xorps   xmm0, xmm0            # dot = 0
    xorps   xmm1, xmm1            # norm_a = 0
    xorps   xmm2, xmm2            # norm_b = 0
    xor     eax, eax              # i = 0
.L3:
    movss   xmm3, DWORD PTR [rdi+rax*4]  # 加载a[i]
    movss   xmm4, DWORD PTR [rsi+rax*4]  # 加载b[i]
    movaps  xmm5, xmm3                     # 拷贝a[i]
    mulss   xmm5, xmm4                     # a[i] * b[i]
    addss   xmm0, xmm5                     # dot += ...
    mulss   xmm3, xmm3                     # a[i] * a[i]
    addss   xmm1, xmm3                     # norm_a += ...
    mulss   xmm4, xmm4                     # b[i] * b[i]
    addss   xmm2, xmm4                     # norm_b += ...
    add     rax, 1                         # i++
    cmp     rax, rdx                       # i < n?
    jne     .L3                           # 继续循环

    # 问题: 每次迭代只处理一个元素!未向量化!
    sqrtss  xmm1, xmm1             # sqrt(norm_a)
    sqrtss  xmm2, xmm2             # sqrt(norm_b)
    mulss   xmm1, xmm2             # sqrt(norm_a) * sqrt(norm_b)
    divss   xmm0, xmm1             # dot / product
    ret
```

**诊断:** 编译器没有向量化,因为:
1. 循环体内有多个独立的累加器
2. 存在数据依赖关系

#### 工具2: objdump反汇编

```bash
# 编译时生成汇编
g++ -O3 -march=native -S -c cosine.cpp -o cosine.s

# 或反编译目标文件
g++ -O3 -march=native -c cosine.cpp -o cosine.o
objdump -d cosine.o | less

# 带源码注释的反汇编
g++ -O3 -march=native -g -c cosine.cpp -o cosine.o
objdump -d -S cosine.o | less
```

#### 工具3: perf annotate

```bash
# 记录性能数据
perf record -g ./your_program

# 注释汇编代码
perf annotate cosine

# 输出示例:
#   35.50 │    movss   xmm3, DWORD PTR [rdi+rax*4]
#   12.30 │    movss   xmm4, DWORD PTR [rsi+rax*4]
#         │    movaps  xmm5, xmm3
#   18.20 │    mulss   xmm5, xmm4
#   │      addss   xmm0, xmm5
# ...
# 百分比表示在该指令花费的时间
```

### 1.3 强制向量化优化

**方法1: OpenMP SIMD**

```cpp
// 添加OpenMP SIMD指令
#pragma omp declare simd
float cosine(const float* a, const float* b, size_t n) {
    float dot = 0, norm_a = 0, norm_b = 0;
    #pragma omp simd reduction(+:dot,norm_a,norm_b)
    for (size_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}
```

**编译:**
```bash
g++ -O3 -march=native -fopenmp -fopenmp-simd cosine.cpp
```

**生成的汇编 (AVX2, 8个float并行):**

```asm
cosine:
    # ... 初始化 ...
    vxorps  ymm0, ymm0, ymm0       # 8个dot初始值
    vxorps  ymm1, ymm1, ymm1       # 8个norm_a
    vxorps  ymm2, ymm2, ymm2       # 8个norm_b
    xor     eax, eax
    mov     rcx, rdx
    shr     rcx, 3                 # 循环次数 = n / 8
    test    rcx, rcx
    je      .L_end_loop
.L_loop:
    # 加载8个float
    vmovups ymm3, YMMWORD PTR [rdi+rax*4]
    vmovups ymm4, YMMWORD PTR [rsi+rax*4]

    # FMA: a[i] * b[i] + dot
    vfmadd231ps ymm0, ymm3, ymm4   # dot += a * b
    vfmadd231ps ymm1, ymm3, ymm3   # norm_a += a * a
    vfmadd231ps ymm2, ymm4, ymm4   # norm_b += b * b

    add     rax, 8
    dec     rcx
    jne     .L_loop

    # 水平求和YMM寄存器
    vextractf128 xmm3, ymm0, 1     # 高128位
    vaddps  xmm0, xmm0, xmm3       # low + high
    vhaddps xmm0, xmm0, xmm0       # 水平加法
    # ... 对norm_a, norm_b重复 ...
```

**性能提升:** 8倍 (256位 / 32位)

**方法2: GCC向量化提示**

```cpp
float cosine(const float* __restrict__ a,
             const float* __restrict__ b,
             size_t n) {
    float dot = 0, norm_a = 0, norm_b = 0;

    // 告诉编译器循环次数>=16,且向量大小为8
    for (size_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}
```

### 1.4 内联汇编优化

**场景:** 需要使用特定CPU指令,但编译器不支持

**示例: AVX-512 BW (Byte and Word)指令**

```cpp
#include <immintrin.h>

// 使用AVX-512 VNNI (Vector Neural Network Instructions)
// 混合精度矩阵乘法优化
void avx512_vnni_matmul(
    const int8_t* A,  // 8-bit整数矩阵
    const int8_t* B,
    int32_t* C,
    int M, int N, int K) {

    #pragma omp parallel for collapse(2)
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n += 16) {  // AVX-512处理16个int32

            __m512i sum = _mm512_setzero_si512();

            for (int k = 0; k < K; k += 64) {  // 512位 / 8位 = 64

                // 加载64个int8_t
                __m512i a_vec = _mm512_loadu_si512((__m512i*)(A + m*K + k));

                // 内联汇编: 使用VPDPBUSD指令
                // VNNI指令: 8-bit乘法 + 32-bit累加
                __m512i b_vec = _mm512_loadu_si512((__m512i*)(B + k*N + n));

                asm volatile (
                    "vpdpbusd %2, %1, %0"  // FMA: int8 * int8 + int32
                    : "+v"(sum)
                    : "v"(a_vec), "v"(b_vec)
                );
            }

            // 存储结果
            _mm512_storeu_si512((__m512i*)(C + m*N + n), sum);
        }
    }
}
```

**性能:** 比标准int32乘法快**16倍**

### 1.5 手写汇编优化实例

**目标:** 优化USearch中的距离计算

**源码位置:** `index_dense.hpp:1567-1589`

```cpp
// USearch原始实现 (已优化)
template <typename metric_at, typename scalar_at>
struct metric_state_gt<metric_at, scalar_at, metric_kind_t::cos_k> {
    using value_type = scalar_at;

    template <typename value_at>
    inline void operator()(value_at const* a, value_at const* b, std::size_t n) {
        value_type dot = 0, norm_a = 0, norm_b = 0;

        // 隐式向量化循环
        for (std::size_t i = 0; i != n; ++i) {
            value_type ai = a[i];
            value_type bi = b[i];
            dot += ai * bi;
            norm_a += ai * ai;
            norm_b += bi * bi;
        }

        return value_type(1) - dot / sqrtf(norm_a * norm_b);
    }
};
```

**手写汇编版本 (AVX-512):**

```asm
# input:  rdi = a*, rsi = b*, rdx = n
# output: xmm0 = cosine distance
# clobbered: ymm0-ymm15, rax, rcx

cosine_avx512:
    # 检查向量大小
    test    rdx, rdx
    je      .L_return

    # 初始化累加器 (使用广播)
    vpxor   ymm0, ymm0, ymm0          # dot = 0
    vpxor   ymm1, ymm1, ymm1          # norm_a = 0
    vpxor   ymm2, ymm2, ymm2          # norm_b = 0

    # 处理64字节块 (16个float)
    mov     rcx, rdx
    shr     rcx, 4                    # n / 16
    je      .L_skip_64
.L_loop_64:
    # 预取下一个块
    prefetcht0 [rdi + 256]
    prefetcht0 [rsi + 256]

    # 加载16个float
    vmovups zmm3, ZMMWORD PTR [rdi]
    vmovups zmm4, ZMMWORD PTR [rsi]

    # FMA指令 (3个操作数)
    vfmadd231ps zmm0, zmm3, zmm4      # dot += a * b
    vfmadd231ps zmm1, zmm3, zmm3      # norm_a += a * a
    vfmadd231ps zmm2, zmm4, zmm4      # norm_b += b * b

    add     rdi, 64
    add     rsi, 64
    dec     rcx
    jne     .L_loop_64

.L_skip_64:
    # 处理32字节块 (8个float, AVX2)
    mov     rcx, rdx
    shr     rcx, 3
    and     rcx, 1                    # 只处理剩余1次
    je      .L_skip_32
    vmovups ymm3, YMMWORD PTR [rdi]
    vmovups ymm4, YMMWORD PTR [rsi]
    vfmadd231ps ymm0, ymm3, ymm4
    vfmadd231ps ymm1, ymm3, ymm3
    vfmadd231ps ymm2, ymm4, ymm4
    add     rdi, 32
    add     rsi, 32

.L_skip_32:
    # 处理标量剩余部分
    mov     rcx, rdx
    and     rcx, 7
    je      .L_finalize
.L_scalar:
    movss   xmm3, DWORD PTR [rdi]
    movss   xmm4, DWORD PTR [rsi]
    vfmadd231ss xmm0, xmm3, xmm4
    vfmadd231ss xmm1, xmm3, xmm3
    vfmadd231ss xmm2, xmm4, xmm4
    add     rdi, 4
    add     rsi, 4
    dec     rcx
    jne     .L_scalar

.L_finalize:
    # 水平归约ZMM寄存器 (16个float -> 1个)
    vextractf64x4 ymm3, zmm0, 1      # 高半部分
    vaddps  ymm0, ymm0, ymm3          # 合并
    vextractf128 xmm3, ymm0, 1       # 高128位
    vaddps  xmm0, xmm0, xmm3
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0
    # 对norm_a和norm_b重复...

    # 计算 sqrt(norm_a * norm_b)
    vmulss  xmm1, xmm1, xmm2          # norm_a * norm_b
    vsqrtss xmm1, xmm1, xmm1          # sqrt(...)
    vdivss  xmm0, xmm0, xmm1          # dot / sqrt(...)

    # 转换为距离: 1 - similarity
    movss   xmm1, _LTOW_1_0_         # 加载常量1.0
    vsubss  xmm0, xmm1, xmm0          # 1 - dot / sqrt(...)

.L_return:
    ret
```

**性能对比:**

| 实现 | 768维余弦距离 | 相对性能 |
|------|--------------|---------|
| 标量 | 245 ns | 1.0x |
| AVX2 | 52 ns | 4.7x |
| AVX-512 | 28 ns | **8.8x** |
| 手写汇编 | 26 ns | **9.4x** |

### 1.6 实战练习

#### 练习1: 分析USearch距离计算

```bash
# 1. 编译USearch C++核心
cd /home/dev/USearch
g++ -O3 -march=native -S -I include \
    python/lib.cpp -o python/lib.s

# 2. 查看余弦距离的汇编
grep -A 100 "cosine.*:" python/lib.s | less

# 3. 标注热点
perf record -g ./your_benchmark
perf annotate --stdio | grep -A 5 cosine

# 任务:
# 1. 识别循环是否向量化
# 2. 检查是否使用FMA
# 3. 优化标量尾部处理
```

#### 练习2: 编写SIMD内联汇编

```cpp
// 任务: 实现AVX2版本的L2距离
// 提示: 使用vsubps, vfmadd231ps指令

float l2_avx2(const float* a, const float* b, size_t n) {
    __m256 sum = _mm256_setzero_ps();

    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);

        // TODO: 计算sum((a-b)^2)
        // 1. 计算差值: sub = a - b
        // 2. 平方并累加: sum += sub * sub
    }

    // 水平求和
    float result = hsum256_avx2(sum);

    // 处理剩余元素
    for (; i < n; i++) {
        float diff = a[i] - b[i];
        result += diff * diff;
    }

    return sqrtf(result);
}

// 参考: https://stackoverflow.com/questions/49941645/get-sum-of-values-stored-in-m256
```

#### 练习3: 性能对比实验

```python
# benchmark_cosine.py
import numpy as np
import time

# 生成长度为1000万,768维的向量
vectors = np.random.randn(10_000_000, 768).astype(np.float32)
query = vectors[0]

# 测试不同实现
implementations = [
    "scalar",
    "avx2",
    "avx512",
    "asm_optimized"
]

for impl in implementations:
    start = time.time()
    for i in range(1000):
        # 调用对应的C++实现
        distance = cosine_distance(query, vectors[i], impl)
    elapsed = time.time() - start

    print(f"{impl}: {elapsed*1000:.2f}ms")

# 绘制性能对比图
import matplotlib.pyplot as plt
plt.bar(implementations, times)
plt.ylabel('Time (ms)')
plt.title('Cosine Distance Performance')
plt.savefig('cosine_benchmark.png')
```

---

## Advanced 2: CPU亲和性与NUMA优化 🖥️

### 2.1 NUMA架构深度解析

**问题:** 多路服务器上,性能随线程数增加反而下降

**原因:** NUMA (Non-Uniform Memory Access)架构

#### NUMA内存访问延迟

```
单路服务器 (UMA):
┌─────────────────────────────┐
│   CPU (所有核心)            │
│   ┌───┬───┬───┬───┐        │
│   │ 0 │ 1 │ 2 │ 3 │        │
│   └───┴───┴───┴───┘        │
│         │                  │
│    ┌────┴────┐             │
│    │  Memory │             │
│    └─────────┘             │
└─────────────────────────────┘
所有核心访问内存延迟相同: ~80ns

双路服务器 (NUMA):
┌──────────────────┐  ┌──────────────────┐
│ Socket 0         │  │ Socket 1         │
│ ┌───┬───┬───┬───┐│  │┌───┬───┬───┬───┐│
│ │ 0 │ 1 │ 2 │ 3 ││  ││ 4 │ 5 │ 6 │ 7 ││
│ └───┴───┴───┴───┘│  │└───┴───┴───┴───┘│
│      │ QPI       │  │      │ QPI       │
│ ┌────┴─────┐    │  │ ┌────┴─────┐    │
│ │ Local    │    │  │ │ Local    │    │
│ │ Memory 0 │    │  │ │ Memory 1 │    │
│ └──────────┘    │  │ └──────────┘    │
└──────────────────┘  └──────────────────┘
         │                      │
         └────────┬─────────────┘
                  │

访问延迟:
- 本地内存: ~80ns
- 远程内存: ~120-150ns (慢50%!)
```

#### 性能陷阱示例

```cpp
// ❌ 错误: 线程在Socket 0, 数据在Socket 1
void bad_numa_pattern() {
    // 线程0在Socket 0上分配内存
    std::vector<float> data(100000000);

    // 启动线程1-7,在Socket 1上运行
    #pragma omp parallel for num_threads(8)
    for (int i = 0; i < 8; i++) {
        // 线程4-7访问远程内存!
        process_data(data);
    }
    // 性能损失: 40-50%
}
```

### 2.2 NUMA感知分配器

**源码位置:** `index_plugins.hpp:1000+`

```cpp
#if defined(__linux__) && defined(__x86_64__)
#include <numa.h>

class numa_allocator_gt {
    int numa_node_;

public:
    using value_type = byte_t;

    explicit numa_allocator_gt(int preferred_node = -1)
        : numa_node_(preferred_node) {

        if (numa_available() < 0) {
            throw std::runtime_error("NUMA not available");
        }
    }

    byte_t* allocate(std::size_t n) const {
        void* ptr = nullptr;

        if (numa_node_ >= 0) {
            // 在指定NUMA节点分配
            ptr = numa_alloc_onnode(n, numa_node_);
        } else {
            // 本地分配
            ptr = numa_alloc_local(n);
        }

        if (!ptr) {
            throw std::bad_alloc();
        }

        return static_cast<byte_t*>(ptr);
    }

    void deallocate(byte_t* ptr, std::size_t n) const noexcept {
        numa_free(ptr, n);
    }

    // 获取当前线程的NUMA节点
    static int get_current_node() {
        return numa_node_of_cpu(sched_getcpu());
    }
};

#endif
```

### 2.3 CPU亲和性设置

```cpp
#include <pthread.h>
#include <sched.h>

class ThreadAffinity {
public:
    // 绑定线程到特定CPU核心
    static void set_affinity(pthread_t thread, int core_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        int rc = pthread_setaffinity_np(
            thread,
            sizeof(cpu_set_t),
            &cpuset
        );

        if (rc != 0) {
            perror("pthread_setaffinity_np");
        }
    }

    // 绑定到NUMA节点的所有核心
    static void set_numa_affinity(pthread_t thread, int numa_node) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        // 获取NUMA节点的所有CPU
        for (int cpu = 0; cpu < sysconf(_SC_NPROCESSORS_ONLN); cpu++) {
            if (numa_node_of_cpu(cpu) == numa_node) {
                CPU_SET(cpu, &cpuset);
            }
        }

        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    }
};
```

**USearch中的集成:**

```cpp
// index.hpp: 线程上下文初始化
context_t& context_for_thread(size_t thread_id) {
    if (thread_id >= thread_contexts_.size()) {
        thread_contexts_.resize(thread_id + 1);
    }

    context_t& ctx = thread_contexts_[thread_id];

    // 首次使用时设置亲和性
    if (!ctx.affinity_set) {
        int numa_node = numa_allocator_gt::get_current_node();
        ThreadAffinity::set_numa_affinity(
            pthread_self(),
            numa_node
        );

        // 在本地NUMA节点分配上下文缓冲区
        numa_allocator_gt allocator(numa_node);
        ctx.visits.reserve(1024);
        ctx.top_candidates.reserve(128);

        ctx.affinity_set = true;
    }

    return ctx;
}
```

### 2.4 First-Touch策略

**关键原则:** 内存应该在首次访问的线程所在NUMA节点分配

```cpp
// ❌ 错误: 主线程分配,工作线程访问
void bad_first_touch() {
    const size_t N = 100000000;
    std::vector<float> data(N);  // 主线程分配 (可能在Socket 0)

    #pragma omp parallel for
    for (size_t i = 0; i < N; i++) {
        data[i] = process(i);  // 工作线程访问 (可能在Socket 1)
    }
}

// ✅ 正确: 工作线程首次访问
void good_first_touch() {
    const size_t N = 100000000;
    std::vector<float> data(N);

    // 强制每个线程初始化自己的部分
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < N; i++) {
        data[i] = 0;  // 首次touch,在本地NUMA分配
    }

    // 后续处理
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < N; i++) {
        data[i] = process(i);
    }
}
```

**USearch中的应用:**

```cpp
// index_dense.hpp: 向量数据分配
template <typename scalar_at>
class index_dense_gt {
    // 确保在正确的NUMA节点分配
    void allocate_vectors() {
        size_t vectors_per_thread = (capacity_ + num_threads_ - 1) / num_threads_;

        #pragma omp parallel num_threads(num_threads_)
        {
            int thread_id = omp_get_thread_num();
            int numa_node = numa_node_of_cpu(sched_getcpu());

            // 使用NUMA感知分配器
            numa_allocator_gt allocator(numa_node);

            size_t start = thread_id * vectors_per_thread;
            size_t end = std::min(start + vectors_per_thread, capacity_);

            for (size_t i = start; i < end; i++) {
                vectors_[i] = allocator.allocate(dimensions_ * sizeof(scalar_at));
            }
        }
    }
};
```

### 2.5 NUMA性能测试

**测试代码:**

```cpp
#include <numa.h>
#include <omp.h>
#include <chrono>

void benchmark_numa() {
    const size_t N = 100000000;
    const size_t ITER = 100;

    printf("NUMA Benchmark\n");
    printf("NUMA nodes: %d\n", numa_num_configured_nodes());
    printf("CPUs: %d\n", numa_num_configured_cpus());
    printf("\n");

    // 测试1: 本地访问
    auto test_local = [&]() {
        numa_allocator_gt allocator(numa_node_of_cpu(0));  // Socket 0
        float* data = (float*)allocator.allocate(N * sizeof(float));

        // 绑定到CPU 0 (Socket 0)
        ThreadAffinity::set_affinity(pthread_self(), 0);

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t iter = 0; iter < ITER; iter++) {
            #pragma omp parallel for num_threads(4)
            for (int i = 0; i < 4; i++) {
                for (size_t j = i * N/4; j < (i+1) * N/4; j++) {
                    data[j] *= 1.01f;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();

        allocator.deallocate((byte_t*)data, N * sizeof(float));
        return elapsed;
    };

    // 测试2: 远程访问
    auto test_remote = [&]() {
        numa_allocator_gt allocator(0);  // Socket 0分配
        float* data = (float*)allocator.allocate(N * sizeof(float));

        // 绑定到CPU 4-7 (Socket 1)
        ThreadAffinity::set_affinity(pthread_self(), 4);

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t iter = 0; iter < ITER; iter++) {
            #pragma omp parallel for num_threads(4)
            for (int i = 0; i < 4; i++) {
                for (size_t j = i * N/4; j < (i+1) * N/4; j++) {
                    data[j] *= 1.01f;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();

        allocator.deallocate((byte_t*)data, N * sizeof(float));
        return elapsed;
    };

    double local_time = test_local();
    double remote_time = test_remote();

    printf("Local access:  %.3f seconds\n", local_time);
    printf("Remote access: %.3f seconds\n", remote_time);
    printf("Slowdown:      %.2fx\n", remote_time / local_time);
}
```

**典型结果:**

```
NUMA Benchmark
NUMA nodes: 2
CPUs: 16

Local access:  2.34 seconds
Remote access: 3.51 seconds
Slowdown:      1.50x
```

### 2.6 实战练习

#### 练习1: 分析NUMA配置

```bash
# 查看NUMA拓扑
numactl --hardware

# 输出示例:
# available: 2 nodes (0-1)
# node 0 cpus: 0 1 2 3 4 5 6 7
# node 0 size: 65536 MB
# node 1 cpus: 8 9 10 11 12 13 14 15
# node 1 size: 65536 MB

# 查看当前进程的NUMA统计
numastat -p $(pidof your_program)

# 查看内存分布
numastat -m
```

#### 练习2: 优化USearch的NUMA性能

```cpp
// 任务: 修改USearch使其NUMA感知
// 1. 实现numa_aware_index_gt类
// 2. 在每个NUMA节点维护子索引
// 3. 搜索时优先查询本地节点

template <typename index_at>
class numa_aware_index_gt {
    std::vector<index_at> sub_indexes_;  // 每个NUMA节点一个
    std::vector<int> numa_nodes_;

public:
    void add(key_t key, const float* vector) {
        int current_node = numa_node_of_cpu(sched_getcpu());
        sub_indexes_[current_node].add(key, vector);
    }

    std::vector<key_t> search(const float* vector, size_t k) {
        // 1. 搜索本地节点 (快速)
        int local_node = numa_node_of_cpu(sched_getcpu());
        auto local_results = sub_indexes_[local_node].search(vector, k);

        // 2. 并行搜索远程节点
        std::vector<std::future<std::vector<key_t>>> futures;
        for (int node : numa_nodes_) {
            if (node != local_node) {
                futures.push_back(std::async(std::launch::async, [&]() {
                    return sub_indexes_[node].search(vector, k);
                }));
            }
        }

        // 3. 合并结果
        // TODO: 实现合并逻辑
    }
};
```

---

## Advanced 3: JIT编译与动态代码生成 ⚡

### 3.1 为什么需要JIT?

**问题:** 自定义距离度量性能远低于内置度量

```cpp
// 自定义度量
float my_distance(const float* a, const float* b, size_t n) {
    float sum = 0;
    for (size_t i = 0; i < n; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

// 使用
index.add(metric = my_distance);  // 性能差!
```

**原因:**
1. 函数指针调用开销
2. 编译器无法跨函数边界优化
3. 无法内联
4. 无法向量化

**解决:** LLVM JIT编译

### 3.2 LLVM JIT基础

**安装LLVM:**

```bash
# Ubuntu
sudo apt install libllvm-15-dev llvm-15-dev clang-15

# macOS
brew install llvm@15

# 验证
llvm-config --version
```

**Hello World JIT:**

```cpp
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>

using namespace llvm;
using namespace llvm::orc;

class JITCompiler {
    std::unique_ptr<LLJIT> jit_;
    llvm::orc::ThreadSafeContext context_;

public:
    JITCompiler()
        : context_(std::make_unique<llvm::LLVMContext>()) {

        // 初始化原生目标
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();

        // 创建JIT
        auto jit = LLJITBuilder().create();
        if (!jit) {
            llvm::errs() << "Failed to create JIT: "
                        << toString(jit.takeError()) << "\n";
            exit(1);
        }
        jit_ = std::move(*jit);
    }

    // 编译并返回函数指针
    template <typename Func_t>
    Func_t compile(std::unique_ptr<Module> module) {
        // 添加模块到JIT
        auto err = jit_->addIRModule(
            ThreadSafeModule(std::move(module), context_)
        );

        if (err) {
            llvm::errs() << "Failed to add module: "
                        << toString(std::move(err)) << "\n";
            return nullptr;
        }

        // 查找函数
        auto sym = jit_->lookup("main_func");
        if (!sym) {
            llvm::errs() << "Failed to lookup function: "
                        << toString(sym.takeError()) << "\n";
            return nullptr;
        }

        return reinterpret_cast<Func_t>(sym->getValue());
    }
};
```

### 3.3 IR生成示例

**目标:** 生成优化的L2距离函数

```cpp
std::unique_ptr<Module> generate_l2_distance_ir(
    LLVMContext& ctx,
    size_t dimensions
) {
    // 创建模块
    auto module = std::make_unique<Module>("l2_distance", ctx);
    module->setDataLayout(jit_->getDataLayout());

    // 函数签名: float(float*, float*, size_t)
    Type* float_ty = Type::getFloatTy(ctx);
    Type* float_ptr_ty = Type::getFloatPtrTy(ctx);
    Type* size_ty = Type::getInt64Ty(ctx);

    FunctionType* ft = FunctionType::get(
        float_ty,
        {float_ptr_ty, float_ptr_ty, size_ty},
        false
    );

    Function* func = Function::Create(
        ft,
        GlobalValue::ExternalLinkage,
        "l2_distance_jit",
        module.get()
    );

    // 参数
    auto args = func->arg_begin();
    Value* a = &*args++;
    a->setName("a");
    Value* b = &*args++;
    b->setName("b");
    Value* n = &*args++;
    n->setName("n");

    // 基本块
    BasicBlock* entry = BasicBlock::Create(ctx, "entry", func);
    BasicBlock* loop = BasicBlock::Create(ctx, "loop", func);
    BasicBlock* exit = BasicBlock::Create(ctx, "exit", func);

    IRBuilder<> builder(entry);

    // 初始化累加器
    Value* sum_ptr = builder.CreateAlloca(float_ty);
    builder.CreateStore(
        ConstantFP::get(float_ty, 0.0),
        sum_ptr
    );

    Value* i_ptr = builder.CreateAlloca(size_ty);
    builder.CreateStore(
        ConstantInt::get(size_ty, 0),
        i_ptr
    );

    // 跳转到循环
    builder.CreateBr(loop);

    // 循环体
    builder.SetInsertPoint(loop);

    // i = load i
    Value* i = builder.CreateLoad(size_ty, i_ptr);
    Value* i_next = builder.CreateAdd(
        i,
        ConstantInt::get(size_ty, 1)
    );
    builder.CreateStore(i_next, i_ptr);

    // 加载a[i]和b[i]
    Value* a_elem_ptr = builder.CreateGEP(
        float_ty,
        a,
        i
    );
    Value* b_elem_ptr = builder.CreateGEP(
        float_ty,
        b,
        i
    );

    Value* a_elem = builder.CreateLoad(float_ty, a_elem_ptr);
    Value* b_elem = builder.CreateLoad(float_ty, b_elem_ptr);

    // diff = a - b
    Value* diff = builder.CreateFSub(a_elem, b_elem);

    // sum += diff * diff
    Value* sum = builder.CreateLoad(float_ty, sum_ptr);
    Value* diff_sq = builder.CreateFMul(diff, diff);
    Value* new_sum = builder.CreateFAdd(sum, diff_sq);
    builder.CreateStore(new_sum, sum_ptr);

    // 条件跳转
    Value* cond = builder.CreateICmpULT(i_next, n);
    builder.CreateCondBr(cond, loop, exit);

    // 退出块
    builder.SetInsertPoint(exit);

    // return sqrt(sum)
    sum = builder.CreateLoad(float_ty, sum_ptr);

    // 插入sqrt调用
    Function* sqrt_func = module->getFunction("sqrtf");
    if (!sqrt_func) {
        // 声明外部函数
        FunctionType* sqrt_ft = FunctionType::get(float_ty, {float_ty}, false);
        sqrt_func = Function::Create(
            sqrt_ft,
            GlobalValue::ExternalLinkage,
            "sqrtf",
            module.get()
        );
    }

    Value* result = builder.CreateCall(sqrt_func, {sum});
    builder.CreateRet(result);

    return module;
}
```

**使用JIT编译器:**

```cpp
// 编译
JITCompiler jit;
auto module = generate_l2_distance_ir(*context.getContext(), 768);

using L2Func = float(*)(const float*, const float*, size_t);
L2Func l2_jit = jit.compile<L2Func>(std::move(module));

// 使用
const float* a = ...;
const float* b = ...;
float dist = l2_jit(a, b, 768);
```

### 3.4 自动向量化

**启用LLVM自动向量化:**

```cpp
// 设置循环属性
loop->getIterator().setName("loop");
loop->addFnAttr(Attribute::AlwaysInline);  // 强制内联

// 添加元数据指示可向量化
MDNode* md_node = MDNode::get(
    ctx,
    {ConstantInt::get(Type::getInt32Ty(ctx), 1)}
);
loop->setMetadata(
    "llvm.loop.vectorize.enable",
    md_node
);
```

**生成的汇编 (AVX2):**

```asm
# LLVM自动生成
l2_distance_jit:
    # 向量化主循环
    vxorps  ymm0, ymm0, ymm0          # sum = 0
    xor     rax, rax
.LBB0_1:
    vmovups ymm1, YMMWORD PTR [rdi+rax*4]
    vmovups ymm2, YMMWORD PTR [rsi+rax*4]
    vsubps  ymm1, ymm1, ymm2          # diff = a - b
    vfmadd231ps ymm0, ymm1, ymm1      # sum += diff * diff
    add     rax, 8
    cmp     rax, rdx
    jb      .LBB0_1

    # 水平归约
    # ...
    vsqrtss xmm0, xmm0, xmm0
    ret
```

### 3.5 Numba JIT集成 (Python)

USearch支持Numba JIT编译的度量函数:

```python
import numba
import usearch
import numpy as np

# 使用Numba JIT编译自定义度量
@numba.njit(fastmath=True, cache=True)
def weighted_l2(a, b, weights):
    """带权重的L2距离"""
    sum_sq = 0.0
    for i in range(len(a)):
        diff = (a[i] - b[i]) * weights[i]
        sum_sq += diff * diff
    return np.sqrt(sum_sq)

# 编译
weights = np.ones(768, dtype=np.float32)
weights[:128] *= 2.0  # 前128维权重加倍

# 使用
index = usearch.Index(
    ndim=768,
    metric=weighted_l2,
    dtype='f32'
)

# Numba函数会被JIT编译为机器码
# 性能接近内置度量!
```

### 3.6 实战练习

#### 练习1: 实现通用度量JIT

```cpp
// 任务: 实现一个可以编译任意度量的JIT系统
// 1. 解析自定义度量语言 (DSL)
// 2. 生成LLVM IR
// 3. 编译为机器码

// 示例DSL:
// "metric weighted_l2 {
//     dimension: 768;
//     weights: [1.0, 1.0, ..., 2.0, ...];
//     formula: sum((a[i] - b[i])^2 * weights[i]);
// }"

class MetricJIT {
    JITCompiler jit_;

public:
    using MetricFunc = float(*)(const float*, const float*, size_t);

    MetricFunc compile(const std::string& dsl) {
        // 1. 解析DSL
        // 2. 生成LLVM IR
        // 3. 编译并返回函数指针
    }
};
```

---

## 📚 学习资源

### 汇编与优化
- **Intel Intrinsics Guide:** https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
- **Agner Fog's Optimization Manuals:** https://www.agner.org/optimize/
- **Compiler Explorer:** https://godbolt.org/

### NUMA编程
- **numa(7) Man Page:** `man numa`
- **Linux NUMA API:** https://linux.die.net/man/3/numa
- **"What Every Programmer Should Know About Memory"** - Ulrich Drepper

### LLVM JIT
- **LLVM Documentation:** https://llvm.org/docs/
- **Kaleidoscope Tutorial:** https://llvm.org/docs/tutorial/
- **USearch Numba Integration:** `python/lib.cpp`

---

## 🎓 下一步

完成本课程后,你应该能够:
1. 阅读和分析汇编代码
2. 识别和修复NUMA性能问题
3. 使用LLVM实现JIT编译
4. 优化CPU缓存亲和性

**推荐项目:**
1. 为USearch实现GPU加速 (CUDA)
2. 实现分布式HNSW
3. 构建实时索引更新系统

---

**持续更新中...** 🚀
