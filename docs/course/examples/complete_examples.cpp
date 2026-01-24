/**
 * USearch 课程配套示例代码
 *
 * 本文件包含课程中提到的所有关键概念的完整实现示例
 * 编译：g++ -std=c++17 -O3 -march=native -I ../include complete_examples.cpp -o complete_examples
 */

#include <usearch/index.hpp>
#include <usearch/index_dense.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

using namespace unum::usearch;

//==============================================================================
// 示例 1: 基础使用
//==============================================================================

void example_01_basic() {
    std::cout << "\n=== 示例 1: 基础使用 ===\n";

    // 1. 创建索引
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    std::cout << "✓ 索引创建成功\n";

    // 2. 添加向量
    std::vector<std::vector<float>> vectors;
    for (int i = 0; i < 100; ++i) {
        std::vector<float> vec(128);
        for (auto& val : vec) {
            val = static_cast<float>(rand()) / RAND_MAX;
        }
        vectors.push_back(vec);
        index.add(i, vec.data());
    }

    std::cout << "✓ 添加了 100 个向量\n";

    // 3. 搜索
    std::vector<float> query(128);
    for (auto& val : query) {
        val = static_cast<float>(rand()) / RAND_MAX;
    }

    auto results = index.search(query.data(), 5);
    std::cout << "✓ 找到 " << results.size() << " 个最近邻\n";

    // 4. 打印结果
    for (std::size_t i = 0; i < results.size(); ++i) {
        std::cout << "  [" << i << "] key=" << results[i].key
                  << ", distance=" << results[i].distance << "\n";
    }
}

//==============================================================================
// 示例 2: 不同距离度量
//==============================================================================

void example_02_metrics() {
    std::cout << "\n=== 示例 2: 不同距离度量 ===\n";

    struct MetricInfo {
        metric_kind_t kind;
        const char* name;
    };

    std::vector<MetricInfo> metrics = {
        {metric_kind_t::cos_k, "Cosine"},
        {metric_kind_t::l2sq_k, "L2 Squared"},
        {metric_kind_t::ip_k, "Inner Product"}
    };

    std::vector<float> vec1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> vec2 = {2.0f, 3.0f, 4.0f};

    for (auto& metric_info : metrics) {
        index_dense_gt<float, std::uint32_t> index;
        index.init(3, metric_info.kind, scalar_kind_t::f32_k);

        index.add(0, vec1.data());
        index.add(1, vec2.data());

        auto results = index.search(vec1.data(), 2);

        std::cout << metric_info.name << " 距离: " << results[1].distance << "\n";
    }
}

//==============================================================================
// 示例 3: 批量操作
//==============================================================================

void example_03_batch() {
    std::cout << "\n=== 示例 3: 批量操作 ===\n";

    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    // 1. 批量添加
    constexpr std::size_t n_vectors = 1000;
    std::vector<float> vectors(n_vectors * 128);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (auto& val : vectors) {
        val = dist(rng);
    }

    std::vector<std::uint32_t> keys(n_vectors);
    for (std::size_t i = 0; i < n_vectors; ++i) {
        keys[i] = static_cast<std::uint32_t>(i);
    }

    auto start = std::chrono::high_resolution_clock::now();
    index.add(keys.data(), vectors.data(), n_vectors);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "批量添加 " << n_vectors << " 个向量: " << duration.count() << " ms\n";
    std::cout << "吞吐量: " << (n_vectors * 1000 / duration.count()) << " vectors/s\n";

    // 2. 批量搜索
    constexpr std::size_t n_queries = 100;
    std::vector<float> queries(n_queries * 128);
    for (auto& val : queries) {
        val = dist(rng);
    }

    start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < n_queries; ++i) {
        index.search(queries.data() + i * 128, 10);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "批量搜索 " << n_queries << " 个查询: " << duration.count() << " ms\n";
    std::cout << "吞吐量: " << (n_queries * 1000 / duration.count()) << " QPS\n";
}

//==============================================================================
// 示例 4: 序列化和加载
//==============================================================================

void example_04_serialization() {
    std::cout << "\n=== 示例 4: 序列化和加载 ===\n";

    const char* index_path = "/tmp/test_index.usearch";

    // 1. 创建并保存索引
    {
        index_dense_gt<float, std::uint32_t> index;
        index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

        std::vector<float> vec(128);
        for (auto& val : vec) {
            val = static_cast<float>(rand()) / RAND_MAX;
        }

        for (int i = 0; i < 100; ++i) {
            index.add(i, vec.data());
        }

        auto result = index.save(index_path);
        if (result)
            std::cout << "✗ 保存失败: " << result.what() << "\n";
        else
            std::cout << "✓ 索引保存成功\n";
    }

    // 2. 加载索引
    {
        index_dense_gt<float, std::uint32_t> index;
        index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

        auto result = index.load(index_path);
        if (result)
            std::cout << "✗ 加载失败: " << result.what() << "\n";
        else
            std::cout << "✓ 索引加载成功, 大小: " << index.size() << "\n";
    }

    std::remove(index_path);
}

//==============================================================================
// 示例 5: 精度召回率测试
//==============================================================================

void example_05_accuracy() {
    std::cout << "\n=== 示例 5: 精度召回率测试 ===\n";

    constexpr std::size_t n_vectors = 1000;
    constexpr std::size_t n_queries = 100;
    constexpr std::size_t dimensions = 128;

    // 1. 创建数据
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> vectors(n_vectors * dimensions);
    for (auto& val : vectors) {
        val = dist(rng);
    }

    // 2. 创建索引
    index_dense_gt<float, std::uint32_t> index;
    index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    std::vector<std::uint32_t> keys(n_vectors);
    for (std::size_t i = 0; i < n_vectors; ++i) {
        keys[i] = static_cast<std::uint32_t>(i);
    }

    index.add(keys.data(), vectors.data(), n_vectors);

    // 3. 测试不同的 ef 参数
    std::vector<std::size_t> ef_values = {16, 32, 64, 128};

    std::cout << "ef\tRecall@10\tLatency (us)\n";
    std::cout << "----------------------------------------\n";

    for (std::size_t ef : ef_values) {
        index.expansion(ef);

        float total_recall = 0.0f;
        float total_latency = 0.0f;

        for (std::size_t q = 0; q < n_queries; ++q) {
            const float* query = vectors.data() + q * dimensions;

            auto start = std::chrono::high_resolution_clock::now();
            auto results = index.search(query, 10);
            auto end = std::chrono::high_resolution_clock::now();

            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            total_latency += latency.count();

            // 计算召回率（假设精确结果是最近的10个）
            // 这里简化处理：检查是否包含最近的键
            std::size_t found = 0;
            for (auto& result : results) {
                if (result.key >= q && result.key < q + 10) {
                    found++;
                }
            }
            total_recall += static_cast<float>(found) / 10.0f;
        }

        float avg_recall = total_recall / n_queries;
        float avg_latency = total_latency / n_queries;

        std::cout << ef << "\t" << avg_recall << "\t\t" << avg_latency << "\n";
    }
}

//==============================================================================
// 示例 6: 内存使用分析
//==============================================================================

void example_06_memory() {
    std::cout << "\n=== 示例 6: 内存使用分析 ===\n";

    struct MemoryStats {
        std::size_t vectors_memory;
        std::size_t graph_memory;
        std::size_t total_memory;
    };

    auto calculate_memory = [](std::size_t n_vectors, std::size_t dimensions,
                               std::size_t connectivity = 16) -> MemoryStats {
        // 向量内存
        std::size_t vectors_memory = n_vectors * dimensions * sizeof(float);

        // 图内存（简化计算）
        float avg_levels = std::log2(n_vectors) * 0.25f;
        std::size_t edges_per_node = static_cast<std::size_t>(connectivity * avg_levels);
        std::size_t graph_memory = n_vectors * edges_per_node * sizeof(std::uint32_t);

        return {
            vectors_memory,
            graph_memory,
            vectors_memory + graph_memory
        };
    };

    std::vector<std::size_t> sizes = {1000, 10000, 100000, 1000000};
    constexpr std::size_t dimensions = 768;

    std::cout << "向量数\t\t向量\t图\t总计\n";
    std::cout << "----------------------------------------------\n";

    for (std::size_t n : sizes) {
        auto stats = calculate_memory(n, dimensions);

        std::cout << n << "\t\t"
                  << (stats.vectors_memory / 1024 / 1024) << " MB\t"
                  << (stats.graph_memory / 1024 / 1024) << " MB\t"
                  << (stats.total_memory / 1024 / 1024) << " MB\n";
    }
}

//==============================================================================
// 示例 7: 并发搜索
//==============================================================================

#include <thread>
#include <mutex>

void example_07_concurrent() {
    std::cout << "\n=== 示例 7: 并发搜索 ===\n";

    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    // 添加数据
    constexpr std::size_t n_vectors = 10000;
    std::vector<float> vectors(n_vectors * 128);
    std::vector<std::uint32_t> keys(n_vectors);

    for (std::size_t i = 0; i < n_vectors; ++i) {
        keys[i] = static_cast<std::uint32_t>(i);
        for (std::size_t j = 0; j < 128; ++j) {
            vectors[i * 128 + j] = static_cast<float>(rand()) / RAND_MAX;
        }
    }

    index.add(keys.data(), vectors.data(), n_vectors);

    // 并发搜索
    constexpr std::size_t n_threads = 4;
    constexpr std::size_t n_queries_per_thread = 100;

    auto worker = [&](std::size_t thread_id) {
        std::vector<float> query(128);
        for (std::size_t i = 0; i < 128; ++i) {
            query[i] = static_cast<float>(rand()) / RAND_MAX;
        }

        for (std::size_t q = 0; q < n_queries_per_thread; ++q) {
            index.search(query.data(), 10);
        }

        return n_queries_per_thread;
    };

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < n_threads; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::size_t total_queries = n_threads * n_queries_per_thread;
    std::cout << "并发搜索 (" << n_threads << " 线程):\n";
    std::cout << "  总查询: " << total_queries << "\n";
    std::cout << "  总时间: " << duration.count() << " ms\n";
    std::cout << "  吞吐量: " << (total_queries * 1000 / duration.count()) << " QPS\n";
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "  USearch 课程示例代码\n";
    std::cout << "==============================================\n";

    example_01_basic();
    example_02_metrics();
    example_03_batch();
    example_04_serialization();
    example_05_accuracy();
    example_06_memory();
    example_07_concurrent();

    std::cout << "\n==============================================\n";
    std::cout << "  所有示例运行完成!\n";
    std::cout << "==============================================\n";

    return 0;
}
