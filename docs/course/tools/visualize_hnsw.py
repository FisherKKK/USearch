"""
USearch HNSW 可视化工具

用于可视化 HNSW 图的结构、层级分布和搜索过程
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyArrowPatch
import networkx as nx
from typing import List, Tuple, Dict
import random


class HNSWVisualizer:
    """HNSW 图可视化工具"""

    def __init__(self, dimensions=2):
        self.dimensions = dimensions
        self.nodes = {}  # {node_id: {level: position}}
        self.edges = {}  # {level: [(src, dst), ...]}

    def generate_random_graph(self, n_nodes=50, n_levels=3, avg_connections=4):
        """生成随机 HNSW 图用于演示"""
        # 生成节点位置（2D）
        for node_id in range(n_nodes):
            self.nodes[node_id] = {
                'position': np.random.rand(2) * 10,
                'level': random.randint(0, n_levels)
            }

        # 为每一层生成边
        for level in range(n_levels):
            self.edges[level] = []

            # 该层包含的节点
            nodes_in_level = [nid for nid in range(n_nodes)
                             if self.nodes[nid]['level'] >= level]

            # 随机连接
            for nid in nodes_in_level:
                n_connections = random.randint(1, avg_connections)
                candidates = [n for n in nodes_in_level if n != nid]
                if candidates:
                    neighbors = random.sample(candidates,
                                            min(n_connections, len(candidates)))
                    for neighbor in neighbors:
                        self.edges[level].append((nid, neighbor))

    def visualize_level(self, level=0, save_path=None):
        """可视化特定层"""
        fig, ax = plt.subplots(figsize=(12, 10))

        # 收集该层的节点和边
        nodes_in_level = [nid for nid in self.nodes
                         if self.nodes[nid]['level'] >= level]
        edges_in_level = self.edges.get(level, [])

        # 绘制边
        for src, dst in edges_in_level:
            if src in nodes_in_level and dst in nodes_in_level:
                pos1 = self.nodes[src]['position']
                pos2 = self.nodes[dst]['position']
                ax.plot([pos1[0], pos2[0]], [pos1[1], pos2[1]],
                       'b-', alpha=0.3, linewidth=1)

        # 绘制节点
        for nid in nodes_in_level:
            pos = self.nodes[nid]['position']
            node_level = self.nodes[nid]['level']

            # 不同层级使用不同颜色和大小
            colors = ['lightgreen', 'yellow', 'orange', 'red']
            color = colors[min(node_level, len(colors)-1)]
            size = 100 + node_level * 50

            ax.scatter(pos[0], pos[1], s=size, c=color,
                      edgecolors='black', linewidths=1, zorder=10)
            ax.text(pos[0], pos[1], str(nid),
                   ha='center', va='center', fontsize=8,
                   fontweight='bold', zorder=11)

        ax.set_title(f'HNSW Layer {level} Visualization', fontsize=16)
        ax.set_xlabel('X', fontsize=12)
        ax.set_ylabel('Y', fontsize=12)
        ax.grid(True, alpha=0.3)

        # 图例
        legend_elements = [
            mpatches.Patch(color='lightgreen', label='Level 0'),
            mpatches.Patch(color='yellow', label='Level 1'),
            mpatches.Patch(color='orange', label='Level 2'),
            mpatches.Patch(color='red', label='Level 3+')
        ]
        ax.legend(handles=legend_elements, loc='upper right')

        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"图表已保存到: {save_path}")

        plt.show()

    def visualize_search_process(self, start_node=0, target_node=25,
                               level=0, save_path=None):
        """可视化搜索过程"""
        fig, ax = plt.subplots(figsize=(14, 10))

        # 收集该层的节点和边
        nodes_in_level = [nid for nid in self.nodes
                         if self.nodes[nid]['level'] >= level]
        edges_in_level = self.edges.get(level, [])

        # 绘制所有边（灰色）
        for src, dst in edges_in_level:
            if src in nodes_in_level and dst in nodes_in_level:
                pos1 = self.nodes[src]['position']
                pos2 = self.nodes[dst]['position']
                ax.plot([pos1[0], pos2[0]], [pos1[1], pos2[1]],
                       'gray', alpha=0.2, linewidth=1)

        # 模拟搜索路径（简化版：BFS）
        visited = {start_node}
        queue = [start_node]
        search_path = [start_node]
        found = False

        while queue and not found:
            current = queue.pop(0)
            search_path.append(current)

            if current == target_node:
                found = True
                break

            # 找邻居
            for src, dst in edges_in_level:
                if src == current and dst in nodes_in_level and dst not in visited:
                    visited.add(dst)
                    queue.append(dst)

        # 绘制搜索路径
        for i in range(len(search_path) - 1):
            src = search_path[i]
            dst = search_path[i + 1]
            pos1 = self.nodes[src]['position']
            pos2 = self.nodes[dst]['position']

            # 根据在路径中的位置设置颜色
            progress = i / len(search_path)
            color = plt.cm.RdYlGn(progress)
            ax.plot([pos1[0], pos2[0]], [pos1[1], pos2[1]],
                   color=color, linewidth=3, alpha=0.8)

        # 绘制节点
        for nid in nodes_in_level:
            pos = self.nodes[nid]['position']

            if nid == start_node:
                color = 'green'
                size = 300
                label = 'Start'
            elif nid == target_node:
                color = 'red'
                size = 300
                label = 'Target'
            elif nid in visited:
                color = 'lightblue'
                size = 150
                label = str(nid)
            else:
                color = 'lightgray'
                size = 100
                label = ''

            ax.scatter(pos[0], pos[1], s=size, c=color,
                      edgecolors='black', linewidths=2, zorder=10)
            if label:
                ax.text(pos[0], pos[1], label,
                       ha='center', va='center', fontsize=10,
                       fontweight='bold', zorder=11)

        ax.set_title(f'HNSW Search Process (Level {level})', fontsize=16)
        ax.set_xlabel('X', fontsize=12)
        ax.set_ylabel('Y', fontsize=12)
        ax.grid(True, alpha=0.3)

        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"图表已保存到: {save_path}")

        plt.show()


def visualize_level_distribution(max_level=10, ml=0.5, n_nodes=10000):
    """可视化层级分布"""
    # 生成层级
    levels = []
    for _ in range(n_nodes):
        u = random.random()
        level = int(-np.log(u) / np.log(1/ml))
        levels.append(level)

    # 统计
    level_counts = {}
    for level in levels:
        level_counts[level] = level_counts.get(level, 0) + 1

    # 理论分布
    theoretical_levels = range(max_level + 1)
    theoretical_probs = [(ml ** level) * (1 - ml) if level < max_level else ml ** level
                         for level in theoretical_levels]
    theoretical_counts = [p * n_nodes for p in theoretical_probs]

    # 绘图
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # 柱状图
    ax1.bar(level_counts.keys(), level_counts.values(),
           alpha=0.7, label='实际分布')
    ax1.plot(theoretical_levels, theoretical_counts,
            'r-', linewidth=2, label='理论分布')
    ax1.set_xlabel('Level', fontsize=12)
    ax1.set_ylabel('Count', fontsize=12)
    ax1.set_title('Node Level Distribution', fontsize=14)
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # 对数坐标
    ax2.bar(level_counts.keys(), level_counts.values(),
           alpha=0.7, label='实际分布')
    ax2.plot(theoretical_levels, theoretical_counts,
            'r-', linewidth=2, label='理论分布')
    ax2.set_xlabel('Level', fontsize=12)
    ax2.set_ylabel('Count (log scale)', fontsize=12)
    ax2.set_title('Node Level Distribution (Log Scale)', fontsize=14)
    ax2.set_yscale('log')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig('level_distribution.png', dpi=300, bbox_inches='tight')
    plt.show()


def visualize_performance_comparison():
    """可视化性能对比"""
    # 数据（来自课程）
    algorithms = ['Brute Force', 'KD-Tree', 'NSW', 'HNSW']

    build_time = [0, 50, 120, 120]  # 秒
    search_latency = [1000, 10, 5, 0.1]  # 毫秒
    memory_usage = [512, 550, 700, 714]  # MB

    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 5))

    # 构建时间
    bars1 = ax1.bar(algorithms, build_time, color='steelblue', alpha=0.7)
    ax1.set_ylabel('Time (s)', fontsize=12)
    ax1.set_title('Build Time', fontsize=14)
    ax1.bar_label(bars1, padding=3)
    ax1.grid(True, alpha=0.3, axis='y')

    # 搜索延迟（对数刻度）
    bars2 = ax2.bar(algorithms, search_latency, color='coral', alpha=0.7)
    ax2.set_ylabel('Latency (ms)', fontsize=12)
    ax2.set_title('Search Latency (log scale)', fontsize=14)
    ax2.set_yscale('log')
    ax2.bar_label(bars2, padding=3, fmt='%.1f')
    ax2.grid(True, alpha=0.3, axis='y')

    # 内存使用
    bars3 = ax3.bar(algorithms, memory_usage, color='seagreen', alpha=0.7)
    ax3.set_ylabel('Memory (MB)', fontsize=12)
    ax3.set_title('Memory Usage', fontsize=14)
    ax3.bar_label(bars3, padding=3)
    ax3.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()
    plt.savefig('performance_comparison.png', dpi=300, bbox_inches='tight')
    plt.show()


def visualize_recall_vs_latency():
    """可视化召回率 vs 延迟权衡"""
    ef_values = [16, 32, 64, 128, 256]
    recalls = [0.85, 0.92, 0.96, 0.98, 0.99]
    latencies = [0.05, 0.08, 0.15, 0.30, 0.60]  # ms

    fig, ax = plt.subplots(figsize=(10, 8))

    # 绘制曲线
    ax.plot(latencies, recalls, 'o-', linewidth=3, markersize=10,
           color='steelblue', label='HNSW')

    # 标注 ef 值
    for ef, r, l in zip(ef_values, recalls, latencies):
        ax.annotate(f'ef={ef}', (l, r),
                   textcoords="offset points", xytext=(0,10), ha='center',
                   fontsize=10, fontweight='bold')

    ax.set_xlabel('Search Latency (ms)', fontsize=14)
    ax.set_ylabel('Recall@10', fontsize=14)
    ax.set_title('Recall vs Latency Trade-off', fontsize=16)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=12)

    # 添加理想区域标注
    ax.fill_between([0, 0.1], 0.95, 1.0, alpha=0.2, color='green',
                   label='Ideal Region')

    plt.tight_layout()
    plt.savefig('recall_vs_latency.png', dpi=300, bbox_inches='tight')
    plt.show()


def visualize_quantization_impact():
    """可视化的影响"""
    scalar_types = ['f64', 'f32', 'f16', 'bf16', 'i8', 'b1x8']

    memory_per_vector = [8, 4, 2, 2, 1, 0.125]  # bytes for 128d
    recall = [0.96, 0.96, 0.95, 0.95, 0.91, 0.75]  # approximate

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # 内存使用
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b']
    bars1 = ax1.bar(scalar_types, memory_per_vector, color=colors, alpha=0.7)
    ax1.set_ylabel('Memory per Vector (bytes)', fontsize=12)
    ax1.set_title('Memory Usage by Scalar Type', fontsize=14)
    ax1.bar_label(bars1, padding=3)
    ax1.grid(True, alpha=0.3, axis='y')

    # 召回率
    bars2 = ax2.bar(scalar_types, recall, color=colors, alpha=0.7)
    ax2.set_ylabel('Recall@10', fontsize=12)
    ax2.set_title('Recall by Scalar Type', fontsize=14)
    ax2.set_ylim(0.7, 1.0)
    ax2.bar_label(bars2, padding=3, fmt='%.2f')
    ax2.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()
    plt.savefig('quantization_impact.png', dpi=300, bbox_inches='tight')
    plt.show()


if __name__ == '__main__':
    print("HNSW 可视化工具")
    print("=" * 50)

    # 1. 生成并可视化图结构
    print("\n1. 生成随机 HNSW 图...")
    viz = HNSWVisualizer()
    viz.generate_random_graph(n_nodes=30, n_levels=3, avg_connections=3)

    print("2. 可视化各层...")
    for level in range(3):
        viz.visualize_level(level=level, save_path=f'hnsw_level_{level}.png')

    print("3. 可视化搜索过程...")
    viz.visualize_search_process(start_node=0, target_node=15,
                               level=0, save_path='hnsw_search.png')

    # 4. 层级分布
    print("4. 可视化层级分布...")
    visualize_level_distribution()

    # 5. 性能对比
    print("5. 可视化性能对比...")
    visualize_performance_comparison()

    # 6. 召回率 vs 延迟
    print("6. 可视化召回率 vs 延迟...")
    visualize_recall_vs_latency()

    # 7. 量化影响
    print("7. 可视化量化影响...")
    visualize_quantization_impact()

    print("\n所有图表生成完成！")
