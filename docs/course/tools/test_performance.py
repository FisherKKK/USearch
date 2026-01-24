#!/usr/bin/env python3
"""
USearch 性能测试套件
Comprehensive performance testing framework

运行：
    python test_performance.py
"""

import time
import numpy as np
import psutil
import matplotlib.pyplot as plt
from dataclasses import dataclass
from typing import List, Dict, Callable, Optional
import json
from pathlib import Path

# 尝试导入 usearch
try:
    from usearch.index import Index
    USEARCH_AVAILABLE = True
except ImportError:
    USEARCH_AVAILABLE = False
    print("Warning: usearch not available. Install with: pip install usearch")


@dataclass
class BenchmarkResult:
    """基准测试结果"""
    name: str
    n_vectors: int
    dimensions: int
    build_time: float
    search_latency: float
    qps: float
    memory_mb: float
    recall: float = 0.0

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'n_vectors': self.n_vectors,
            'dimensions': self.dimensions,
            'build_time': self.build_time,
            'search_latency': self.search_latency,
            'qps': self.qps,
            'memory_mb': self.memory_mb,
            'recall': self.recall
        }


class PerformanceTester:
    """性能测试器"""

    def __init__(self):
        self.results: List[BenchmarkResult] = []

    def generate_vectors(self, n: int, dimensions: int, dtype=np.float32) -> np.ndarray:
        """生成随机向量"""
        return np.random.rand(n, dimensions).astype(dtype)

    def normalize_vectors(self, vectors: np.ndarray) -> np.ndarray:
        """L2 归一化"""
        norms = np.linalg.norm(vectors, axis=1, keepdims=True)
        return vectors / norms

    def test_build_performance(
        self,
        n_vectors: int,
        dimensions: int,
        metric: str = 'cos',
        dtype: str = 'f32',
        connectivity: int = 16,
        expansion: int = 64
    ) -> BenchmarkResult:
        """测试构建性能"""
        if not USEARCH_AVAILABLE:
            raise RuntimeError("usearch not available")

        print(f"\n{'='*60}")
        print(f"测试: 构建性能 ({n_vectors:,} 向量, {dimensions} 维)")
        print(f"配置: metric={metric}, dtype={dtype}, M={connectivity}, ef={expansion}")

        # 生成数据
        print("生成数据...")
        vectors = self.generate_vectors(n_vectors, dimensions)
        ids = np.arange(n_vectors, dtype=np.uint32)

        if metric == 'cos':
            vectors = self.normalize_vectors(vectors)

        # 记录初始内存
        process = psutil.Process()
        initial_memory = process.memory_info().rss / 1024 / 1024  # MB

        # 创建索引
        print("创建索引...")
        index = Index(
            ndim=dimensions,
            metric=metric,
            dtype=dtype,
            connectivity=connectivity,
            expansion=expansion
        )

        # 构建索引
        print("添加向量...")
        start = time.time()
        index.add(ids, vectors)
        build_time = time.time() - start

        print(f"✓ 构建完成: {build_time:.2f} 秒")
        print(f"  速度: {n_vectors / build_time:,.0f} 向量/秒")

        # 测试搜索性能
        print("\n测试搜索性能...")
        n_queries = 1000
        queries = self.generate_vectors(n_queries, dimensions)

        if metric == 'cos':
            queries = self.normalize_vectors(queries)

        start = time.time()
        for i in range(n_queries):
            index.search(queries[i], k=10)
        total_time = time.time() - start

        search_latency = (total_time / n_queries) * 1000  # ms
        qps = n_queries / total_time

        print(f"✓ 搜索性能:")
        print(f"  平均延迟: {search_latency:.2f} ms")
        print(f"  QPS: {qps:,.0f}")

        # 内存使用
        final_memory = process.memory_info().rss / 1024 / 1024  # MB
        memory_mb = final_memory - initial_memory

        print(f"  内存使用: {memory_mb:.1f} MB")
        print(f"  每向量: {memory_mb * 1024 / n_vectors:.2f} KB")

        return BenchmarkResult(
            name=f"build_{n_vectors}_{dimensions}",
            n_vectors=n_vectors,
            dimensions=dimensions,
            build_time=build_time,
            search_latency=search_latency,
            qps=qps,
            memory_mb=memory_mb
        )

    def test_recall(
        self,
        n_vectors: int,
        dimensions: int,
        metric: str = 'cos',
        k: int = 10
    ) -> float:
        """测试召回率"""
        if not USEARCH_AVAILABLE:
            raise RuntimeError("usearch not available")

        print(f"\n{'='*60}")
        print(f"测试: 召回率 ({n_vectors:,} 向量, {dimensions} 维)")

        # 生成数据
        vectors = self.generate_vectors(n_vectors, dimensions)
        ids = np.arange(n_vectors, dtype=np.uint32)

        if metric == 'cos':
            vectors = self.normalize_vectors(vectors)

        # 创建索引
        index = Index(ndim=dimensions, metric=metric)
        index.add(ids, vectors)

        # 测试召回率
        n_queries = 100
        queries = vectors[:n_queries]  # 使用前100个作为查询

        correct = 0
        for i in range(n_queries):
            results = index.search(queries[i], k=k)
            # 第一个结果应该是自己（距离≈0）
            if len(results) > 0 and results[0].key == ids[i]:
                correct += 1

        recall = correct / n_queries
        print(f"✓ 召回率: {recall:.2%}")

        return recall

    def test_scaling(self):
        """测试扩展性"""
        print(f"\n{'='*60}")
        print("测试: 扩展性")

        dimensions = 128
        sizes = [1000, 10000, 100000, 1000000]

        for size in sizes:
            try:
                result = self.test_build_performance(
                    n_vectors=size,
                    dimensions=dimensions,
                    metric='cos',
                    connectivity=16,
                    expansion=64
                )
                self.results.append(result)
            except Exception as e:
                print(f"✗ 失败: {e}")

    def test_configurations(self):
        """测试不同配置"""
        print(f"\n{'='*60}")
        print("测试: 不同配置")

        n_vectors = 100000
        dimensions = 768

        configs = [
            {'connectivity': 8, 'expansion': 32, 'name': 'fast'},
            {'connectivity': 16, 'expansion': 64, 'name': 'balanced'},
            {'connectivity': 32, 'expansion': 128, 'name': 'accurate'},
        ]

        for config in configs:
            print(f"\n配置: {config['name']}")
            try:
                result = self.test_build_performance(
                    n_vectors=n_vectors,
                    dimensions=dimensions,
                    metric='cos',
                    connectivity=config['connectivity'],
                    expansion=config['expansion']
                )
                result.name = f"config_{config['name']}"

                # 测试召回率
                result.recall = self.test_recall(n_vectors, dimensions)

                self.results.append(result)
            except Exception as e:
                print(f"✗ 失败: {e}")

    def test_quantization(self):
        """测试量化效果"""
        print(f"\n{'='*60}")
        print("测试: 量化效果")

        n_vectors = 100000
        dimensions = 768

        dtypes = ['f32', 'f16']

        for dtype in dtypes:
            print(f"\n量化: {dtype}")
            try:
                result = self.test_build_performance(
                    n_vectors=n_vectors,
                    dimensions=dimensions,
                    metric='cos',
                    dtype=dtype
                )
                result.name = f"quant_{dtype}"
                self.results.append(result)
            except Exception as e:
                print(f"✗ 失败: {e}")

    def test_metrics(self):
        """测试不同距离度量"""
        print(f"\n{'='*60}")
        print("测试: 距离度量")

        n_vectors = 100000
        dimensions = 128

        metrics = ['cos', 'l2sq', 'ip']

        for metric in metrics:
            print(f"\n度量: {metric}")
            try:
                result = self.test_build_performance(
                    n_vectors=n_vectors,
                    dimensions=dimensions,
                    metric=metric
                )
                result.name = f"metric_{metric}"
                self.results.append(result)
            except Exception as e:
                print(f"✗ 失败: {e}")

    def plot_results(self, save_path: str = "performance_plots.png"):
        """绘制性能图表"""
        if not self.results:
            print("没有结果可绘制")
            return

        fig, axes = plt.subplots(2, 2, figsize=(12, 10))

        # 1. 构建时间 vs 向量数量
        if len(self.results) > 0:
            scaling_results = [r for r in self.results if 'build_' in r.name]
            if scaling_results:
                axes[0, 0].plot(
                    [r.n_vectors for r in scaling_results],
                    [r.build_time for r in scaling_results],
                    marker='o'
                )
                axes[0, 0].set_xscale('log')
                axes[0, 0].set_xlabel('向量数量')
                axes[0, 0].set_ylabel('构建时间 (秒)')
                axes[0, 0].set_title('构建时间 vs 向量数量')
                axes[0, 0].grid(True)

        # 2. QPS vs 配置
        config_results = [r for r in self.results if 'config_' in r.name]
        if config_results:
            configs = [r.name.replace('config_', '') for r in config_results]
            qps_values = [r.qps for r in config_results]

            axes[0, 1].bar(configs, qps_values)
            axes[0, 1].set_ylabel('QPS')
            axes[0, 1].set_title('QPS vs 配置')
            axes[0, 1].grid(True, axis='y')

        # 3. 内存使用
        if len(self.results) > 0:
            quant_results = [r for r in self.results if 'quant_' in r.name]
            if quant_results:
                dtypes = [r.name.replace('quant_', '') for r in quant_results]
                memory = [r.memory_mb for r in quant_results]

                axes[1, 0].bar(dtypes, memory)
                axes[1, 0].set_ylabel('内存 (MB)')
                axes[1, 0].set_title('内存使用 vs 量化类型')
                axes[1, 0].grid(True, axis='y')

        # 4. 延迟 vs 召回率
        config_results = [r for r in self.results if 'config_' in r.name and r.recall > 0]
        if config_results:
            recalls = [r.recall for r in config_results]
            latencies = [r.search_latency for r in config_results]

            axes[1, 1].scatter(recalls, latencies, s=100)
            for i, r in enumerate(config_results):
                axes[1, 1].annotate(
                    r.name.replace('config_', ''),
                    (recalls[i], latencies[i])
                )
            axes[1, 1].set_xlabel('召回率')
            axes[1, 1].set_ylabel('搜索延迟 (ms)')
            axes[1, 1].set_title('延迟 vs 召回率')
            axes[1, 1].grid(True)

        plt.tight_layout()
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"\n✓ 图表已保存: {save_path}")

    def save_results(self, path: str = "performance_results.json"):
        """保存结果到 JSON"""
        data = {
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S'),
            'results': [r.to_dict() for r in self.results]
        }

        with open(path, 'w') as f:
            json.dump(data, f, indent=2)

        print(f"✓ 结果已保存: {path}")

    def print_summary(self):
        """打印测试总结"""
        if not self.results:
            print("\n没有测试结果")
            return

        print(f"\n{'='*60}")
        print("测试总结")
        print(f"{'='*60}\n")

        print(f"{'测试':<20} {'向量数':<12} {'构建(s)':<10} {'延迟(ms)':<10} {'QPS':<10} {'内存(MB)'}")
        print("-" * 80)

        for r in self.results:
            print(f"{r.name:<20} {r.n_vectors:<12,} "
                  f"{r.build_time:<10.2f} {r.search_latency:<10.2f} "
                  f"{r.qps:<10,.0f} {r.memory_mb:<10.1f}")

        # 统计
        print(f"\n总测试数: {len(self.results)}")
        print(f"总向量数: {sum(r.n_vectors for r in self.results):,}")
        print(f"总测试时间: {sum(r.build_time for r in self.results):.1f} 秒")


def main():
    """主测试函数"""
    tester = PerformanceTester()

    # 1. 扩展性测试
    print("\n" + "="*60)
    print("扩展性测试")
    print("="*60)
    tester.test_scaling()

    # 2. 配置测试
    print("\n" + "="*60)
    print("配置测试")
    print("="*60)
    tester.test_configurations()

    # 3. 量化测试
    print("\n" + "="*60)
    print("量化测试")
    print("="*60)
    tester.test_quantization()

    # 4. 距离度量测试
    print("\n" + "="*60)
    print("距离度量测试")
    print("="*60)
    tester.test_metrics()

    # 打印总结
    tester.print_summary()

    # 保存结果
    tester.save_results()

    # 绘制图表
    if USEARCH_AVAILABLE:
        try:
            tester.plot_results()
        except Exception as e:
            print(f"绘制图表失败: {e}")


if __name__ == '__main__':
    main()
