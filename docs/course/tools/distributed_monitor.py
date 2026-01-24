#!/usr/bin/env python3
"""
分布式向量搜索集群监控工具

功能：
- 实时监控集群状态
- 可视化分片负载分布
- 性能指标收集和展示
- 告警通知

依赖：
- pip install matplotlib psutil flask prometheus-client
"""

import time
import psutil
import threading
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Dict, List, Optional
import json
import requests
import argparse
import sys

# 尝试导入可选依赖
try:
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    from matplotlib.animation import FuncAnimation
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not available. Charts will be disabled.")

try:
    from flask import Flask, render_template, jsonify
    from prometheus_client import Counter, Histogram, Gauge, generate_latest
    FLASK_AVAILABLE = True
except ImportError:
    FLASK_AVAILABLE = False
    print("Warning: Flask not available. Web UI will be disabled.")


@dataclass
class ShardMetrics:
    """分片指标"""
    shard_id: int
    num_vectors: int = 0
    query_count: int = 0
    total_latency_ms: float = 0.0
    error_count: int = 0
    cpu_percent: float = 0.0
    memory_mb: float = 0.0
    last_update: Optional[datetime] = None

    @property
    def avg_latency_ms(self) -> float:
        if self.query_count == 0:
            return 0.0
        return self.total_latency_ms / self.query_count


@dataclass
class ClusterMetrics:
    """集群指标"""
    total_queries: int = 0
    total_errors: int = 0
    qps: float = 0.0
    avg_latency_ms: float = 0.0
    p99_latency_ms: float = 0.0


class ClusterMonitor:
    """集群监控器"""

    def __init__(self, shard_addresses: List[str]):
        self.shard_addresses = shard_addresses
        self.shards: Dict[int, ShardMetrics] = {}
        self.history: Dict[str, List[tuple]] = defaultdict(list)  # 时间序列数据

        # 初始化分片指标
        for i, addr in enumerate(shard_addresses):
            self.shards[i] = ShardMetrics(shard_id=i)

        self._start_time = datetime.now()
        self._running = False

    def start(self, interval_seconds: int = 5):
        """启动监控"""
        self._running = True
        self._interval = interval_seconds

        # 启动收集线程
        self._thread = threading.Thread(target=self._collect_loop, daemon=True)
        self._thread.start()

        print(f"Monitor started, collecting metrics every {interval_seconds}s")

    def stop(self):
        """停止监控"""
        self._running = False
        if hasattr(self, '_thread'):
            self._thread.join(timeout=5)

    def _collect_loop(self):
        """指标收集循环"""
        while self._running:
            self.collect_metrics()
            time.sleep(self._interval)

    def collect_metrics(self):
        """收集所有分片的指标"""
        for shard_id, addr in enumerate(self.shard_addresses):
            try:
                # 尝试从分片获取指标
                metrics = self._fetch_shard_metrics(addr)

                # 更新本地指标
                self.shards[shard_id].num_vectors = metrics.get('num_vectors', 0)
                self.shards[shard_id].query_count = metrics.get('query_count', 0)
                self.shards[shard_id].total_latency_ms = metrics.get('total_latency_ms', 0.0)
                self.shards[shard_id].error_count = metrics.get('error_count', 0)
                self.shards[shard_id].last_update = datetime.now()

                # 系统指标
                self.shards[shard_id].cpu_percent = metrics.get('cpu_percent', 0.0)
                self.shards[shard_id].memory_mb = metrics.get('memory_mb', 0.0)

            except Exception as e:
                print(f"Error collecting metrics from shard {shard_id}: {e}")

        # 记录历史数据
        now = datetime.now()
        for shard_id, shard in self.shards.items():
            self.history[f'shard_{shard_id}_queries'].append((now, shard.query_count))
            self.history[f'shard_{shard_id}_latency'].append((now, shard.avg_latency_ms))
            self.history[f'shard_{shard_id}_errors'].append((now, shard.error_count))

        # 清理旧数据（保留最近1小时）
        cutoff = datetime.now() - timedelta(hours=1)
        for key in self.history:
            self.history[key] = [(t, v) for t, v in self.history[key] if t > cutoff]

    def _fetch_shard_metrics(self, address: str) -> dict:
        """从分片获取指标（模拟）"""
        # 实际实现中，这里应该发送 HTTP 请求到分片的 metrics 端点
        # 例如：response = requests.get(f"http://{address}/metrics")

        # 模拟返回数据
        return {
            'num_vectors': 100000 + shard_id * 1000,
            'query_count': self.shards.get(shard_id, ShardMetrics(shard_id=0)).query_count + 10,
            'total_latency_ms': 500.0,
            'error_count': 0,
            'cpu_percent': psutil.cpu_percent(interval=0.1),
            'memory_mb': psutil.virtual_memory().used / (1024 * 1024)
        }

    def get_cluster_metrics(self) -> ClusterMetrics:
        """计算集群级指标"""
        total_queries = sum(s.query_count for s in self.shards.values())
        total_errors = sum(s.error_count for s in self.shards.values())

        # 计算 QPS（基于时间窗口）
        elapsed = (datetime.now() - self._start_time).total_seconds()
        qps = total_queries / elapsed if elapsed > 0 else 0

        # 计算平均延迟
        avg_latencies = [s.avg_latency_ms for s in self.shards.values() if s.query_count > 0]
        avg_latency = sum(avg_latencies) / len(avg_latencies) if avg_latencies else 0

        # 计算 P99 延迟（简化）
        p99_latency = avg_latency * 1.5  # 简化计算

        return ClusterMetrics(
            total_queries=total_queries,
            total_errors=total_errors,
            qps=qps,
            avg_latency_ms=avg_latency,
            p99_latency_ms=p99_latency
        )

    def print_status(self):
        """打印集群状态"""
        cluster_metrics = self.get_cluster_metrics()

        print("\n" + "=" * 80)
        print(f"Cluster Status - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 80)

        print(f"\nCluster Level:")
        print(f"  Total Queries: {cluster_metrics.total_queries}")
        print(f"  Total Errors: {cluster_metrics.total_errors}")
        print(f"  QPS: {cluster_metrics.qps:.2f}")
        print(f"  Avg Latency: {cluster_metrics.avg_latency_ms:.2f} ms")
        print(f"  P99 Latency: {cluster_metrics.p99_latency_ms:.2f} ms")

        print(f"\nShard Level:")
        print(f"{'Shard':<10} {'Vectors':<15} {'Queries':<15} {'Avg Latency':<15} {'Errors':<10} {'CPU':<10} {'Memory':<10}")
        print("-" * 90)

        for shard_id, shard in self.shards.items():
            print(f"{shard_id:<10} {shard.num_vectors:<15} {shard.query_count:<15} "
                  f"{shard.avg_latency_ms:<15.2f} {shard.error_count:<10} "
                  f"{shard.cpu_percent:<10.1f} {shard.memory_mb:<10.1f}")

    def check_alerts(self, thresholds: dict) -> List[str]:
        """检查告警条件"""
        alerts = []
        cluster_metrics = self.get_cluster_metrics()

        # QPS 过低
        if cluster_metrics.qps < thresholds.get('min_qps', 0):
            alerts.append(f"WARNING: QPS ({cluster_metrics.qps:.2f}) below threshold")

        # 平均延迟过高
        if cluster_metrics.avg_latency_ms > thresholds.get('max_latency', float('inf')):
            alerts.append(f"WARNING: Avg latency ({cluster_metrics.avg_latency_ms:.2f} ms) exceeds threshold")

        # 错误率过高
        error_rate = cluster_metrics.total_errors / max(cluster_metrics.total_queries, 1)
        if error_rate > thresholds.get('max_error_rate', 0.0):
            alerts.append(f"WARNING: Error rate ({error_rate:.2%}) exceeds threshold")

        # 检查每个分片
        for shard_id, shard in self.shards.items():
            # CPU 使用率过高
            if shard.cpu_percent > thresholds.get('max_cpu', 100):
                alerts.append(f"WARNING: Shard {shard_id} CPU ({shard.cpu_percent:.1f}%) exceeds threshold")

            # 内存使用过高
            if shard.memory_mb > thresholds.get('max_memory_mb', float('inf')):
                alerts.append(f"WARNING: Shard {shard_id} memory ({shard.memory_mb:.1f} MB) exceeds threshold")

        return alerts


class Visualizer:
    """可视化工具"""

    def __init__(self, monitor: ClusterMonitor):
        self.monitor = monitor

    def plot_query_distribution(self, save_path: Optional[str] = None):
        """绘制查询分布图"""
        if not MATPLOTLIB_AVAILABLE:
            print("matplotlib not available, skipping plot")
            return

        fig, axes = plt.subplots(2, 2, figsize=(12, 8))

        # 1. 各分片查询量
        shard_ids = list(self.monitor.shards.keys())
        query_counts = [s.query_count for s in self.monitor.shards.values()]

        axes[0, 0].bar(shard_ids, query_counts)
        axes[0, 0].set_xlabel('Shard ID')
        axes[0, 0].set_ylabel('Query Count')
        axes[0, 0].set_title('Query Distribution')

        # 2. 各分片延迟
        avg_latencies = [s.avg_latency_ms for s in self.monitor.shards.values()]

        axes[0, 1].bar(shard_ids, avg_latencies)
        axes[0, 1].set_xlabel('Shard ID')
        axes[0, 1].set_ylabel('Avg Latency (ms)')
        axes[0, 1].set_title('Latency by Shard')

        # 3. CPU 使用率
        cpu_percents = [s.cpu_percent for s in self.monitor.shards.values()]

        axes[1, 0].bar(shard_ids, cpu_percents)
        axes[1, 0].set_xlabel('Shard ID')
        axes[1, 0].set_ylabel('CPU %')
        axes[1, 0].set_title('CPU Usage')

        # 4. 内存使用
        memory_mb = [s.memory_mb for s in self.monitor.shards.values()]

        axes[1, 1].bar(shard_ids, memory_mb)
        axes[1, 1].set_xlabel('Shard ID')
        axes[1, 1].set_ylabel('Memory (MB)')
        axes[1, 1].set_title('Memory Usage')

        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=150, bbox_inches='tight')
            print(f"Plot saved to {save_path}")
        else:
            plt.show()

    def plot_time_series(self, save_path: Optional[str] = None):
        """绘制时间序列图"""
        if not MATPLOTLIB_AVAILABLE:
            print("matplotlib not available, skipping plot")
            return

        fig, axes = plt.subplots(2, 1, figsize=(12, 8))

        # 1. QPS 时间序列
        for shard_id in self.monitor.shards.keys():
            key = f'shard_{shard_id}_queries'
            if key in self.monitor.history and self.monitor.history[key]:
                times, values = zip(*self.monitor.history[key])
                # 计算增量
                increments = [values[i] - (values[i-1] if i > 0 else 0)
                            for i in range(len(values))]
                axes[0].plot(times, increments, label=f'Shard {shard_id}')

        axes[0].set_xlabel('Time')
        axes[0].set_ylabel('Queries')
        axes[0].set_title('Query Rate Over Time')
        axes[0].legend()
        axes[0].xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
        plt.setp(axes[0].xaxis.get_majorticklabels(), rotation=45)

        # 2. 延迟时间序列
        for shard_id in self.monitor.shards.keys():
            key = f'shard_{shard_id}_latency'
            if key in self.monitor.history and self.monitor.history[key]:
                times, values = zip(*self.monitor.history[key])
                axes[1].plot(times, values, label=f'Shard {shard_id}')

        axes[1].set_xlabel('Time')
        axes[1].set_ylabel('Latency (ms)')
        axes[1].set_title('Latency Over Time')
        axes[1].legend()
        axes[1].xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
        plt.setp(axes[1].xaxis.get_majorticklabels(), rotation=45)

        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=150, bbox_inches='tight')
            print(f"Plot saved to {save_path}")
        else:
            plt.show()


def main():
    parser = argparse.ArgumentParser(description='Distributed Vector Search Cluster Monitor')
    parser.add_argument('--shards', nargs='+', default=['localhost:5000', 'localhost:5001'],
                       help='Shard addresses (default: localhost:5000 localhost:5001)')
    parser.add_argument('--interval', type=int, default=5,
                       help='Metrics collection interval in seconds (default: 5)')
    parser.add_argument('--duration', type=int, default=60,
                       help='Monitoring duration in seconds (default: 60)')
    parser.add_argument('--plot', action='store_true',
                       help='Generate plots after monitoring')
    parser.add_argument('--plot-dir', default='./plots',
                       help='Directory to save plots (default: ./plots)')
    parser.add_argument('--alert-thresholds', type=str,
                       help='JSON file with alert thresholds')

    args = parser.parse_args()

    # 加载告警阈值
    thresholds = {
        'min_qps': 0,
        'max_latency': 1000,
        'max_error_rate': 0.05,
        'max_cpu': 80,
        'max_memory_mb': 8192
    }

    if args.alert_thresholds:
        with open(args.alert_thresholds, 'r') as f:
            thresholds.update(json.load(f))

    # 创建监控器
    monitor = ClusterMonitor(args.shards)
    monitor.start(interval_seconds=args.interval)

    # 创建可视化器
    visualizer = Visualizer(monitor)

    try:
        print(f"\nMonitoring cluster for {args.duration} seconds...")
        print("Press Ctrl+C to stop early\n")

        start_time = time.time()
        while time.time() - start_time < args.duration:
            monitor.print_status()

            # 检查告警
            alerts = monitor.check_alerts(thresholds)
            if alerts:
                print("\n⚠️  ALERTS:")
                for alert in alerts:
                    print(f"  {alert}")
                print()

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")

    finally:
        monitor.stop()

        # 生成图表
        if args.plot:
            import os
            os.makedirs(args.plot_dir, exist_ok=True)

            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            visualizer.plot_query_distribution(
                save_path=f"{args.plot_dir}/query_distribution_{timestamp}.png"
            )
            visualizer.plot_time_series(
                save_path=f"{args.plot_dir}/time_series_{timestamp}.png"
            )

        print("\nFinal Cluster Status:")
        monitor.print_status()


if __name__ == '__main__':
    main()
