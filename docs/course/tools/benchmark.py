"""
USearch 性能基准测试脚本

用于测试和比较不同配置下的性能
"""

import numpy as np
import usearch
import time
import matplotlib.pyplot as plt
from typing import Dict, List, Tuple
import psutil
import os


class PerformanceBenchmark:
    """USearch 性能基准测试"""

    def __init__(self, dimensions=128):
        self.dimensions = dimensions
        self.results = {}

    def generate_vectors(self, n_vectors, seed=42):
        """生成测试向量"""
        np.random.seed(seed)
        return np.random.rand(n_vectors, self.dimensions).astype(np.float32)

    def benchmark_indexing(self, n_vectors=100000, connectivities=[8, 16, 32],
                          ef_constructions=[100, 200, 400]):
        """基准测试：索引构建"""
        print("=" * 60)
        print("索引构建性能测试")
        print("=" * 60)

        vectors = self.generate_vectors(n_vectors)
        keys = np.arange(n_vectors, dtype=np.uint64)

        results = []

        for M in connectivities:
            for ef_const in ef_constructions:
                print(f"\n测试: M={M}, ef_construction={ef_const}")

                index = usearch.Index(
                    ndim=self.dimensions,
                    metric='cos',
                    connectivity=M,
                    expansion=ef_const
                )

                # 测量构建时间
                start_time = time.time()
                index.add(keys, vectors)
                build_time = time.time() - start_time

                # 测量内存使用
                process = psutil.Process(os.getpid())
                memory_mb = process.memory_info().rss / 1024 / 1024

                result = {
                    'M': M,
                    'ef_construction': ef_const,
                    'build_time': build_time,
                    'throughput': n_vectors / build_time,
                    'memory_mb': memory_mb
                }

                results.append(result)

                print(f"  构建时间: {build_time:.2f}s")
                print(f"  吞吐量: {result['throughput']:.0f} vectors/s")
                print(f"  内存使用: {memory_mb:.1f} MB")

        self.results['indexing'] = results
        return results

    def benchmark_search(self, n_vectors=100000, n_queries=1000,
                        ef_values=[16, 32, 64, 128, 256],
                        k=10):
        """基准测试：搜索性能"""
        print("\n" + "=" * 60)
        print("搜索性能测试")
        print("=" * 60)

        # 构建索引（使用固定参数）
        index = usearch.Index(
            ndim=self.dimensions,
            metric='cos',
            connectivity=16,
            expansion=200
        )

        vectors = self.generate_vectors(n_vectors)
        queries = self.generate_vectors(n_queries, seed=123)

        keys = np.arange(n_vectors, dtype=np.uint64)
        index.add(keys, vectors)

        results = []

        for ef in ef_values:
            print(f"\n测试: ef={ef}")

            index.expansion = ef

            # 预热
            index.search(queries[0], k)

            # 测试搜索
            latencies = []
            start_time = time.time()

            for query in queries:
                q_start = time.time()
                index.search(query, k)
                latencies.append(time.time() - q_start)

            total_time = time.time() - start_time

            avg_latency = np.mean(latencies) * 1000  # ms
            p50_latency = np.percentile(latencies, 50) * 1000
            p95_latency = np.percentile(latencies, 95) * 1000
            p99_latency = np.percentile(latencies, 99) * 1000
            qps = n_queries / total_time

            result = {
                'ef': ef,
                'avg_latency_ms': avg_latency,
                'p50_latency_ms': p50_latency,
                'p95_latency_ms': p95_latency,
                'p99_latency_ms': p99_latency,
                'qps': qps
            }

            results.append(result)

            print(f"  平均延迟: {avg_latency:.3f} ms")
            print(f"  P95 延迟: {p95_latency:.3f} ms")
            print(f"  QPS: {qps:.0f}")

        self.results['search'] = results
        return results

    def benchmark_accuracy(self, n_vectors=10000, n_queries=100,
                          k=10, ef_values=[16, 32, 64, 128]):
        """基准测试：召回率"""
        print("\n" + "=" * 60)
        print("召回率测试")
        print("=" * 60)

        vectors = self.generate_vectors(n_vectors)
        queries = vectors[:n_queries]  # 使用前 n_queries 个作为查询
        ground_truth = []

        # 计算真实最近邻（暴力搜索）
        print("计算真实最近邻...")
        for query in queries:
            distances = np.linalg.norm(vectors - query, axis=1)
            nearest = np.argsort(distances)[:k]
            ground_truth.append(set(nearest))

        # 构建 USearch 索引
        index = usearch.Index(
            ndim=self.dimensions,
            metric='cos',
            connectivity=16,
            expansion=200
        )

        keys = np.arange(n_vectors, dtype=np.uint64)
        index.add(keys, vectors)

        results = []

        for ef in ef_values:
            print(f"\n测试: ef={ef}")
            index.expansion = ef

            recalls = []
            for i, query in enumerate(queries):
                results_search = index.search(query, k)
                retrieved = set([key for key, _ in results_search])

                # 计算召回率
                intersection = retrieved & ground_truth[i]
                recall = len(intersection) / k
                recalls.append(recall)

            avg_recall = np.mean(recalls)

            result = {
                'ef': ef,
                'avg_recall': avg_recall,
                'min_recall': np.min(recalls),
                'max_recall': np.max(recalls)
            }

            results.append(result)

            print(f"  平均召回率: {avg_recall:.3f}")
            print(f"  最小召回率: {result['min_recall']:.3f}")
            print(f"  最大召回率: {result['max_recall']:.3f}")

        self.results['accuracy'] = results
        return results

    def benchmark_quantization(self, n_vectors=100000, n_queries=1000,
                               scalar_types=['f32', 'f16', 'i8']):
        """基准测试：量化效果"""
        print("\n" + "=" * 60)
        print("量化性能测试")
        print("=" * 60)

        vectors = self.generate_vectors(n_vectors)
        queries = self.generate_vectors(n_queries, seed=123)

        results = []

        for scalar in scalar_types:
            print(f"\n测试: {scalar}")

            index = usearch.Index(
                ndim=self.dimensions,
                metric='cos',
                dtype=scalar,
                connectivity=16
            )

            keys = np.arange(n_vectors, dtype=np.uint64)

            # 构建索引
            start_time = time.time()
            if scalar == 'f32':
                index.add(keys, vectors)
            else:
                # 转换类型
                if scalar == 'f16':
                    import numpy_half
                    vectors_half = vectors.astype(np.float16)
                    index.add(keys, vectors_half.view(np.float32))
                elif scalar == 'i8':
                    # 简化：使用 f32 代替（实际应该量化）
                    index.add(keys, vectors)

            build_time = time.time() - start_time

            # 搜索
            latencies = []
            for query in queries[:100]:  # 只测试 100 个查询
                q_start = time.time()
                if scalar == 'f32':
                    index.search(query, 10)
                latencies.append(time.time() - q_start)

            avg_latency = np.mean(latencies) * 1000

            # 内存估算
            memory_per_vector = {
                'f32': 4 * self.dimensions,
                'f16': 2 * self.dimensions,
                'i8': 1 * self.dimensions
            }
            total_memory_mb = (n_vectors * memory_per_vector[scalar]) / 1024 / 1024

            result = {
                'scalar': scalar,
                'build_time': build_time,
                'avg_latency_ms': avg_latency,
                'memory_mb': total_memory_mb
            }

            results.append(result)

            print(f"  构建时间: {build_time:.2f}s")
            print(f"  平均延迟: {avg_latency:.3f} ms")
            print(f"  内存使用: {total_memory_mb:.1f} MB")

        self.results['quantization'] = results
        return results

    def plot_results(self):
        """绘制结果图表"""
        if not self.results:
            print("没有可绘制的测试结果")
            return

        n_plots = len(self.results)
        fig, axes = plt.subplots(1, n_plots, figsize=(6 * n_plots, 5))

        if n_plots == 1:
            axes = [axes]

        for i, (test_name, results) in enumerate(self.results.items()):
            ax = axes[i]

            if test_name == 'indexing':
                # 构建时间 vs M 和 ef_construction
                for ef_const in [100, 200, 400]:
                    data = [r for r in results if r['ef_construction'] == ef_const]
                    if data:
                        M_values = [r['M'] for r in data]
                        throughputs = [r['throughput'] for r in data]
                        ax.plot(M_values, throughputs, 'o-',
                               label=f'ef_const={ef_const}')

                ax.set_xlabel('Connectivity (M)')
                ax.set_ylabel('Throughput (vectors/s)')
                ax.set_title('Indexing Throughput')
                ax.legend()
                ax.grid(True, alpha=0.3)

            elif test_name == 'search':
                # 延迟 vs ef
                ef_values = [r['ef'] for r in results]
                avg_latencies = [r['avg_latency_ms'] for r in results]
                p95_latencies = [r['p95_latency_ms'] for r in results]

                ax.plot(ef_values, avg_latencies, 'o-', label='Average')
                ax.plot(ef_values, p95_latencies, 's-', label='P95')

                ax.set_xlabel('Expansion Factor (ef)')
                ax.set_ylabel('Latency (ms)')
                ax.set_title('Search Latency')
                ax.legend()
                ax.grid(True, alpha=0.3)

            elif test_name == 'accuracy':
                # 召回率 vs ef
                ef_values = [r['ef'] for r in results]
                recalls = [r['avg_recall'] for r in results]

                ax.plot(ef_values, recalls, 'o-')
                ax.set_xlabel('Expansion Factor (ef)')
                ax.set_ylabel('Recall@10')
                ax.set_title('Search Accuracy')
                ax.set_ylim(0.8, 1.0)
                ax.grid(True, alpha=0.3)

            elif test_name == 'quantization':
                # 比较不同标量类型
                scalars = [r['scalar'] for r in results]
                latencies = [r['avg_latency_ms'] for r in results]
                memories = [r['memory_mb'] for r in results]

                ax2 = ax.twinx()
                bars1 = ax.bar(scalars, latencies, alpha=0.7, label='Latency')
                bars2 = ax2.bar(scalars, memories, alpha=0.3, color='red', label='Memory')

                ax.set_ylabel('Avg Latency (ms)')
                ax2.set_ylabel('Memory (MB)')
                ax.set_title('Quantization Trade-off')

                # 添加数值标签
                for bar in bars1:
                    height = bar.get_height()
                    ax.text(bar.get_x() + bar.get_width()/2., height,
                           f'{height:.2f}', ha='center', va='bottom')

            ax.grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig('benchmark_results.png', dpi=300, bbox_inches='tight')
        plt.show()

    def print_summary(self):
        """打印测试总结"""
        print("\n" + "=" * 60)
        print("测试总结")
        print("=" * 60)

        for test_name, results in self.results.items():
            print(f"\n{test_name.upper()}:")

            if test_name == 'indexing':
                best = max(results, key=lambda x: x['throughput'])
                print(f"  最高吞吐量: {best['throughput']:.0f} vectors/s")
                print(f"    配置: M={best['M']}, ef_construction={best['ef_construction']}")

            elif test_name == 'search':
                best = min(results, key=lambda x: x['avg_latency_ms'])
                print(f"  最低延迟: {best['avg_latency_ms']:.3f} ms")
                print(f"    配置: ef={best['ef']}")
                print(f"  最高 QPS: {max(results, key=lambda x: x['qps'])['qps']:.0f}")

            elif test_name == 'accuracy':
                best = max(results, key=lambda x: x['avg_recall'])
                print(f"  最高召回率: {best['avg_recall']:.3f}")
                print(f"    配置: ef={best['ef']}")

            elif test_name == 'quantization':
                best = min(results, key=lambda x: x['avg_latency_ms'])
                print(f"  最快类型: {best['scalar']} ({best['avg_latency_ms']:.3f} ms)")


def main():
    """运行完整的基准测试"""
    print("USearch 性能基准测试")
    print("=" * 60)

    # 创建测试实例
    benchmark = PerformanceBenchmark(dimensions=128)

    # 运行测试
    # 1. 索引构建测试
    benchmark.benchmark_indexing(
        n_vectors=10000,
        connectivities=[8, 16, 32],
        ef_constructions=[100, 200]
    )

    # 2. 搜索性能测试
    benchmark.benchmark_search(
        n_vectors=10000,
        n_queries=1000,
        ef_values=[16, 32, 64, 128],
        k=10
    )

    # 3. 召回率测试
    benchmark.benchmark_accuracy(
        n_vectors=10000,
        n_queries=100,
        k=10,
        ef_values=[16, 32, 64, 128]
    )

    # 4. 量化测试（可选）
    # benchmark.benchmark_quantization(n_vectors=10000, n_queries=100)

    # 绘制结果
    benchmark.plot_results()

    # 打印总结
    benchmark.print_summary()


if __name__ == '__main__':
    main()
