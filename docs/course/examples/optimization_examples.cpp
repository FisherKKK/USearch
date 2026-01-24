/**
 * USearch 性能优化示例集
 *
 * 演示各种优化技巧的实际效果
 *
 * 编译：
 * g++ -std=c++17 -O3 -march=native \
 *     -I../../../include \
 *     optimization_examples.cpp -o opt_examples
 *
 * 运行：
 * ./opt_examples
 */

#include <usearch/index.hpp>
#include <usearch/index_dense.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cmath>

using namespace unum::usearch;

// 计时工具
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
// 示例 1：循环优化
//==============================================================================

void example_01_loop_optimization() {
    std::cout << "\n=== 示例 1: 循环优化 ===\n";

    constexpr std::size_t n = 1000000;
    constexpr std::size_t d = 128;

    std::vector<float> a(n * d, 1.0f);
    std::vector<float> b(n * d, 2.0f);
    std::vector<float> result_scalar(n);
    std::vector<float> result_vectorized(n);

    // 优化1：标量版本（基线）
    Timer timer;
    timer.start();
    for (std::size_t i = 0; i < n; ++i) {
        float sum = 0;
        for (std::size_t j = 0; j < d; ++j) {
            sum += a[i * d + j] * b[i * d + j];
        }
        result_scalar[i] = sum;
    }
    double time_scalar = timer.elapsed_ms();
    std::cout << "标量版本: " << time_scalar << " ms\n";

    // 优化2：向量化（OpenMP SIMD）
    timer.start();
    #pragma omp simd
    for (std::size_t i = 0; i < n * d; ++i) {
        result_vectorized[i] = a[i] * b[i];
    }
    double time_vectorized = timer.elapsed_ms();
    std::cout << "向量化版本: " << time_vectorized << " ms\n";

    // 优化3：展开
    timer.start();
    constexpr std::size_t unroll = 4;
    for (std::size_t i = 0; i + unroll <= n; i += unroll) {
        float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        for (std::size_t j = 0; j < d; ++j) {
            s0 += a[i * d + j] * b[i * d + j];
            s1 += a[(i + 1) * d + j] * b[(i + 1) * d + j];
            s2 += a[(i + 2) * d + j] * b[(i + 2) * d + j];
            s3 += a[(i + 3) * d + j] * b[(i + 3) * d + j];
        }
        if (i + 0 < n) result_vectorized[i + 0] = s0;
        if (i + 1 < n) result_vectorized[i + 1] = s1;
        if (i + 2 < n) result_vectorized[i + 2] = s2;
        if (i + 3 < n) result_vectorized[i + 3] = s3;
    }
    double time_unrolled = timer.elapsed_ms();
    std::cout << "循环展开版本: " << time_unrolled << " ms\n";

    std::cout << "加速比:\n";
    std::cout << "  向量化: " << time_scalar / time_vectorized << "x\n";
    std::cout << "  展开: " << time_scalar / time_unrolled << "x\n";
}

//==============================================================================
// 示例 2：分支预测优化
//==============================================================================

void example_02_branch_prediction() {
    std::cout << "\n=== 示例 2: 分支预测优化 ===\n";

    constexpr std::size_t n = 10000000;
    std::vector<int> data(n);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);

    for (auto& val : data) {
        val = dis(gen);
    }

    // 版本1：使用分支
    Timer timer;
    timer.start();
    std::size_t count1 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (data[i] > 50) {  // 分支
            count1++;
        }
    }
    double time_with_branch = timer.elapsed_ms();
    std::cout << "使用分支: " << time_with_branch << " ms\n";

    // 版本2：无分支（位运算）
    timer.start();
    std::size_t count2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        count2 += (data[i] > 50);  // 布尔转整数，无分支
    }
    double time_branchless = timer.elapsed_ms();
    std::cout << "无分支版本: " << time_branchless << " ms\n";

    // 版本3：使用 likely 提示
    timer.start();
    std::size_t count3 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (LIKELY(data[i] > 50)) {  // 提示编译器
            count3++;
        }
    }
    double time_likely = timer.elapsed_ms();
    std::cout << "使用 likely 提示: " << time_likely << " ms\n";

    std::cout << "加速比:\n";
    std::cout << "  无分支: " << time_with_branch / time_branchless << "x\n";
    std::cout << "  likely: " << time_with_branch / time_likely << "x\n";
}

//==============================================================================
// 示例 3：内存布局优化
//==============================================================================

struct AoS_Node {
    int id;
    float vector[4];
    int metadata;
};

struct SoA_Nodes {
    std::vector<int> ids;
    std::vector<float> vectors;  // 展平：[n * 4]
    std::vector<int> metadata;

    SoA_Nodes(std::size_t n) : ids(n), vectors(n * 4), metadata(n) {}
};

void example_03_memory_layout() {
    std::cout << "\n=== 示例 3: 内存布局优化 ===\n";

    constexpr std::size_t n = 10000000;

    // AoS 版本
    std::vector<AoS_Node> nodes_aos(n);
    for (std::size_t i = 0; i < n; ++i) {
        nodes_aos[i].id = i;
        nodes_aos[i].metadata = i * 2;
        for (int j = 0; j < 4; ++j) {
            nodes_aos[i].vector[j] = static_cast<float>(j);
        }
    }

    // SoA 版本
    SoA_Nodes nodes_soa(n);
    for (std::size_t i = 0; i < n; ++i) {
        nodes_soa.ids[i] = i;
        nodes_soa.metadata[i] = i * 2;
        for (int j = 0; j < 4; ++j) {
            nodes_soa.vectors[i * 4 + j] = static_cast<float>(j);
        }
    }

    // 测试：遍历所有 ID（缓存不友好，只访问 id）
    Timer timer;
    timer.start();
    volatile int sum1 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum1 += nodes_aos[i].id;
    }
    double time_aos = timer.elapsed_ms();
    std::cout << "AoS (遍历 ID): " << time_aos << " ms\n";

    timer.start();
    volatile int sum2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum2 += nodes_soa.ids[i];
    }
    double time_soa = timer.elapsed_ms();
    std::cout << "SoA (遍历 ID): " << time_soa << " ms\n";
    std::cout << "  加速比: " << time_aos / time_soa << "x\n";

    // 测试：遍历所有向量数据（缓存友好）
    timer.start();
    volatile float sum3 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        for (int j = 0; j < 4; ++j) {
            sum3 += nodes_aos[i].vector[j];
        }
    }
    double time_aos_vector = timer.elapsed_ms();
    std::cout << "AoS (遍历向量): " << time_aos_vector << " ms\n";

    timer.start();
    volatile float sum4 = 0;
    for (std::size_t i = 0; i < n * 4; ++i) {
        sum4 += nodes_soa.vectors[i];
    }
    double time_soa_vector = timer.elapsed_ms();
    std::cout << "SoA (遍历向量): " << time_soa_vector << " ms\n";
    std::cout << "  加速比: " << time_aos_vector / time_soa_vector << "x\n";
}

//==============================================================================
// 示例 4：预取优化
//==============================================================================

template <bool USE_PREFETCH>
void dot_product_with_prefetch(float const* a, float const* b, float* result, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        if constexpr (USE_PREFETCH) {
            // 预取下一个元素
            if (i + 1 < n) {
                __builtin_prefetch(a + i + 1, 0, 3);
                __builtin_prefetch(b + i + 1, 0, 3);
            }
        }

        result[i] = 0;
        for (std::size_t j = 0; j < 128; ++j) {
            result[i] += a[i * 128 + j] * b[i * 128 + j];
        }
    }
}

void example_04_prefetch() {
    std::cout << "\n=== 示例 4: 预取优化 ===\n";

    constexpr std::size_t n = 10000;
    constexpr std::size_t d = 128;

    std::vector<float> a(n * d, 1.0f);
    std::vector<float> b(n * d, 2.0f);
    std::vector<float> result1(n);
    std::vector<float> result2(n);

    // 无预取
    Timer timer;
    timer.start();
    dot_product_with_prefetch<false>(a.data(), b.data(), result1.data(), n);
    double time_no_prefetch = timer.elapsed_ms();
    std::cout << "无预取: " << time_no_prefetch << " ms\n";

    // 有预取
    timer.start();
    dot_product_with_prefetch<true>(a.data(), b.data(), result2.data(), n);
    double time_with_prefetch = timer.elapsed_ms();
    std::cout << "有预取: " << time_with_prefetch << " ms\n";

    std::cout << "加速比: " << time_no_prefetch / time_with_prefetch << "x\n";
}

//==============================================================================
// 示例 5：对齐优化
//==============================================================================

struct alignas(64) AlignedStruct {
    int data[16];
};

struct DefaultStruct {
    int data[16];
};

void example_05_alignment() {
    std::cout << "\n=== 示例 5: 对齐优化 ===\n";

    constexpr std::size_t n = 10000000;

    // 未对齐
    std::vector<DefaultStruct> default_vec(n);
    Timer timer;
    timer.start();
    volatile int sum1 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        for (int j = 0; j < 16; ++j) {
            sum1 += default_vec[i].data[j];
        }
    }
    double time_default = timer.elapsed_ms();
    std::cout << "默认对齐: " << time_default << " ms\n";

    // 缓存行对齐
    std::vector<AlignedStruct> aligned_vec(n);
    timer.start();
    volatile int sum2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        for (int j = 0; j < 16; ++j) {
            sum2 += aligned_vec[i].data[j];
        }
    }
    double time_aligned = timer.elapsed_ms();
    std::cout << "缓存行对齐 (64字节): " << time_aligned << " ms\n";

    std::cout << "加速比: " << time_default / time_aligned << "x\n";
}

//==============================================================================
// 示例 6：查找表优化
//==============================================================================

float sqrt_fast(float x) {
    // 查表法：快速平方根近似
    static constexpr std::size_t table_size = 256;
    static constexpr float table[table_size] = {
        0.0f, 0.0625f, 0.0884f, 0.1083f, 0.1250f, /* ... */
    };

    // 简化版本：使用查表
    std::size_t idx = static_cast<std::size_t>(x * 16);
    idx = std::min(idx, table_size - 1);
    return table[idx];
}

void example_06_lookup_table() {
    std::cout << "\n=== 示例 6: 查找表优化 ===\n";

    constexpr std::size_t n = 10000000;
    std::vector<float> data(n);
    for (auto& val : data) {
        val = static_cast<float>(rand()) / RAND_MAX;
    }

    // 标准库 sqrt
    Timer timer;
    timer.start();
    volatile float sum1 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum1 += std::sqrt(data[i]);
    }
    double time_std = timer.elapsed_ms();
    std::cout << "标准 sqrt: " << time_std << " ms\n";

    // 快速近似
    timer.start();
    volatile float sum2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum2 += sqrt_fast(data[i]);
    }
    double time_fast = timer.elapsed_ms();
    std::cout << "快速近似: " << time_fast << " ms\n";

    std::cout << "加速比: " << time_std / time_fast << "x\n";
    std::cout << "精度损失: （近似值，可能不准确）\n";
}

//==============================================================================
// 示例 7：批处理优化
//==============================================================================

void example_07_batching() {
    std::cout << "\n=== 示例 7: 批处理优化 ===\n";

    using namespace unum::usearch;

    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;
    constexpr std::size_t n_queries = 1000;

    index_dense_gt<float, std::uint32_t> index;
    index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    // 生成数据
    std::vector<float> vectors(n_vectors * dimensions);
    std::vector<std::uint32_t> keys(n_vectors);
    std::vector<float> queries(n_queries * dimensions);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (auto& val : vectors) {
        val = dist(rng);
    }
    for (auto& val : queries) {
        val = dist(rng);
    }

    // 构建索引
    index.add(keys.data(), vectors.data(), n_vectors);

    // 单个搜索
    Timer timer;
    timer.start();
    for (std::size_t i = 0; i < n_queries; ++i) {
        index.search(queries.data() + i * dimensions, 10);
    }
    double time_single = timer.elapsed_ms();
    std::cout << "单个搜索: " << time_single << " ms\n";
    std::cout << "  QPS: " << (n_queries * 1000 / time_single) << "\n";

    // 批量搜索（使用 OpenMP）
    #if defined(_OPENMP)
    timer.start();
    #pragma omp parallel for
    for (std::size_t i = 0; i < n_queries; ++i) {
        index.search(queries.data() + i * dimensions, 10);
    }
    double time_batch = timer.elapsed_ms();
    std::cout << "批量搜索 (OpenMP): " << time_batch << " ms\n";
    std::cout << "  QPS: " << (n_queries * 1000 / time_batch) << "\n";
    std::cout << "  加速比: " << time_single / time_batch << "x\n";
    #else
    std::cout << "OpenMP 未启用，跳过批量搜索测试\n";
    #endif
}

//==============================================================================
// 示例 8：USearch 性能对比
//==============================================================================

void example_08_usearch_comparison() {
    std::cout << "\n=== 示例 8: USearch 配置对比 ===\n";

    using namespace unum::usearch;

    constexpr std::size_t dimensions = 128;
    constexpr std::size_t n_vectors = 10000;
    constexpr std::size_t n_queries = 100;

    struct Config {
        int M;
        int ef;
        const char* name;
    };

    std::vector<Config> configs = {
        {8, 32, "M=8, ef=32 (低质量)"},
        {16, 64, "M=16, ef=64 (平衡)"},
        {32, 128, "M=32, ef=128 (高质量)"}
    };

    std::vector<float> vectors(n_vectors * dimensions);
    std::vector<float> queries(n_queries * dimensions);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (auto& val : vectors) {
        val = dist(rng);
    }
    for (auto& val : queries) {
        val = dist(rng);
    }

    std::cout << std::left << std::setw(20) << "配置"
              << std::setw(12) << "构建时间"
              << std::setw(12) << "搜索延迟"
              << std::setw(10) << "QPS"
              << "\n";
    std::cout << std::string(54, '-') << "\n";

    for (auto& config : configs) {
        index_dense_gt<float, std::uint32_t> index;
        index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k,
                 index_config_t{}.connectivity_base = config.M,
                 index_config_t{}.connectivity_layer = config.M,
                 index_config_t{}.expansion = config.ef);

        // 构建
        Timer timer;
        timer.start();
        std::vector<std::uint32_t> keys(n_vectors);
        index.add(keys.data(), vectors.data(), n_vectors);
        double build_time = timer.elapsed_ms();

        // 搜索
        timer.start();
        for (std::size_t i = 0; i < n_queries; ++i) {
            index.search(queries.data() + i * dimensions, 10);
        }
        double search_latency = timer.elapsed_ms() / n_queries;  // 平均延迟

        std::cout << std::left << std::setw(20) << config.name
                  << std::setw(12) << build_time << " ms"
                  << std::setw(12) << search_latency << " ms"
                  << std::setw(10) << static_cast<int>(n_queries * 1000 / search_latency)
                  << "\n";
    }
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "==========================================\n";
    std::cout << "  USearch 性能优化示例\n";
    std::cout << "==========================================\n";
    std::cout << "\n编译: g++ -std=c++17 -O3 -march=native optimization_examples.cpp -o opt_examples\n";

    example_01_loop_optimization();
    example_02_branch_prediction();
    example_03_memory_layout();
    example_04_prefetch();
    example_05_alignment();
    example_06_lookup_table();
    example_07_batching();
    example_08_usearch_comparison();

    std::cout << "\n==========================================\n";
    std::cout << "  所有示例运行完成！\n";
    std::cout << "==========================================\n";

    return 0;
}
