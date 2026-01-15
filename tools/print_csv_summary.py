#!/usr/bin/env python3
"""
print_csv_summary.py - 落点数据CSV统计工具

用法:
    python3 tools/print_csv_summary.py [csv_file_or_session_dir]

示例:
    python3 tools/print_csv_summary.py runs/20250115_143052/landing_points.csv
    python3 tools/print_csv_summary.py runs/20250115_143052/
    python3 tools/print_csv_summary.py  # 自动查找最新会话
"""

import csv
import sys
import os
from pathlib import Path
from datetime import timedelta


def find_latest_session():
    """查找最新的会话目录"""
    runs_dir = Path("runs")
    if not runs_dir.exists():
        return None

    sessions = sorted(runs_dir.iterdir(), reverse=True)
    for session in sessions:
        csv_file = session / "landing_points.csv"
        if csv_file.exists():
            return csv_file
    return None


def read_csv(filepath):
    """读取CSV文件并返回数据列表"""
    data = []
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                'landing_id': int(row['landing_id']),
                'time_min': int(row['time_min']),
                'time_sec': int(row['time_sec']),
                't_ms': int(row['t_ms']),
                'new_x': float(row['new_x']),
                'new_y': float(row['new_y']),
                'new_z': float(row['new_z']),
            })
    return data


def print_summary(data, filepath):
    """打印数据统计摘要"""
    print("=" * 60)
    print(f"落点数据统计报告")
    print("=" * 60)
    print(f"文件: {filepath}")
    print(f"落点总数: {len(data)}")
    print()

    if not data:
        print("(无数据)")
        return

    # 时间范围
    t_min = min(d['t_ms'] for d in data)
    t_max = max(d['t_ms'] for d in data)
    duration = timedelta(milliseconds=t_max - t_min)
    print(f"时间范围: {t_min}ms - {t_max}ms (持续 {duration})")
    print()

    # 坐标统计
    x_vals = [d['new_x'] for d in data]
    y_vals = [d['new_y'] for d in data]
    z_vals = [d['new_z'] for d in data]

    print("坐标统计 (mm):")
    print(f"  X: min={min(x_vals):.1f}, max={max(x_vals):.1f}, "
          f"avg={sum(x_vals)/len(x_vals):.1f}")
    print(f"  Y: min={min(y_vals):.1f}, max={max(y_vals):.1f}, "
          f"avg={sum(y_vals)/len(y_vals):.1f}")
    print(f"  Z: min={min(z_vals):.1f}, max={max(z_vals):.1f}, "
          f"avg={sum(z_vals)/len(z_vals):.1f}")
    print()

    # 落点间隔
    if len(data) > 1:
        intervals = []
        for i in range(1, len(data)):
            intervals.append(data[i]['t_ms'] - data[i-1]['t_ms'])
        avg_interval = sum(intervals) / len(intervals)
        print(f"落点间隔: avg={avg_interval:.0f}ms, "
              f"min={min(intervals)}ms, max={max(intervals)}ms")
        print()

    # 详细列表
    print("落点详情:")
    print("-" * 60)
    print(f"{'ID':>4} {'时间':>10} {'X':>10} {'Y':>10} {'Z':>10}")
    print("-" * 60)
    for d in data:
        time_str = f"{d['time_min']}:{d['time_sec']:02d}"
        print(f"{d['landing_id']:>4} {time_str:>10} "
              f"{d['new_x']:>10.1f} {d['new_y']:>10.1f} {d['new_z']:>10.1f}")
    print("=" * 60)


def main():
    # 确定CSV文件路径
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
        if path.is_dir():
            csv_file = path / "landing_points.csv"
        else:
            csv_file = path
    else:
        csv_file = find_latest_session()
        if csv_file is None:
            print("错误: 未找到任何会话数据")
            print("用法: python3 tools/print_csv_summary.py [csv_file]")
            sys.exit(1)

    if not csv_file.exists():
        print(f"错误: 文件不存在: {csv_file}")
        sys.exit(1)

    # 读取并打印统计
    data = read_csv(csv_file)
    print_summary(data, csv_file)


if __name__ == "__main__":
    main()
