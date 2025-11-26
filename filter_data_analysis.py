import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# 读取数据
no_filter = pd.read_csv('hip_coords_20251121_140753.csv')
with_filter = pd.read_csv('hip_coords_20251121_140809.csv')

# 计算标准差（波动程度）
print("=== 坐标稳定性分析 ===\n")

print("无滤波标准差:")
print(f"  X: {no_filter['cam_x'].std():.2f} mm")
print(f"  Y: {no_filter['cam_y'].std():.2f} mm")
print(f"  Z: {no_filter['cam_z'].std():.2f} mm")

print("\n有滤波标准差:")
print(f"  X: {with_filter['cam_x'].std():.2f} mm")
print(f"  Y: {with_filter['cam_y'].std():.2f} mm")
print(f"  Z: {with_filter['cam_z'].std():.2f} mm")

# 计算改善百分比
improvement_x = (1 - with_filter['cam_x'].std() / no_filter['cam_x'].std()) * 100
improvement_y = (1 - with_filter['cam_y'].std() / no_filter['cam_y'].std()) * 100
improvement_z = (1 - with_filter['cam_z'].std() / no_filter['cam_z'].std()) * 100

print(f"\n改善效果:")
print(f"  X轴波动减少: {improvement_x:.1f}%")
print(f"  Y轴波动减少: {improvement_y:.1f}%")
print(f"  Z轴波动减少: {improvement_z:.1f}%")

# 绘制对比图
fig, axes = plt.subplots(2, 3, figsize=(15, 8))

# X坐标对比
axes[0, 0].plot(no_filter['cam_x'], 'r-', linewidth=0.5, alpha=0.7)
axes[0, 0].set_title('X Coordinate - No Filter')
axes[0, 0].set_ylabel('X (mm)')
axes[0, 0].grid(True, alpha=0.3)

axes[1, 0].plot(with_filter['cam_x'], 'g-', linewidth=0.5, alpha=0.7)
axes[1, 0].set_title('X Coordinate - With Filter')
axes[1, 0].set_ylabel('X (mm)')
axes[1, 0].set_xlabel('Frame')
axes[1, 0].grid(True, alpha=0.3)

# Y坐标对比
axes[0, 1].plot(no_filter['cam_y'], 'r-', linewidth=0.5, alpha=0.7)
axes[0, 1].set_title('Y Coordinate - No Filter')
axes[0, 1].set_ylabel('Y (mm)')
axes[0, 1].grid(True, alpha=0.3)

axes[1, 1].plot(with_filter['cam_y'], 'g-', linewidth=0.5, alpha=0.7)
axes[1, 1].set_title('Y Coordinate - With Filter')
axes[1, 1].set_ylabel('Y (mm)')
axes[1, 1].set_xlabel('Frame')
axes[1, 1].grid(True, alpha=0.3)

# Z坐标对比
axes[0, 2].plot(no_filter['cam_z'], 'r-', linewidth=0.5, alpha=0.7)
axes[0, 2].set_title('Z Coordinate - No Filter')
axes[0, 2].set_ylabel('Z (mm)')
axes[0, 2].grid(True, alpha=0.3)

axes[1, 2].plot(with_filter['cam_z'], 'g-', linewidth=0.5, alpha=0.7)
axes[1, 2].set_title('Z Coordinate - With Filter')
axes[1, 2].set_ylabel('Z (mm)')
axes[1, 2].set_xlabel('Frame')
axes[1, 2].grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('filter_comparison.png', dpi=300)
print("\n对比图已保存: filter_comparison.png")

# 统计信息
print("\n=== 详细统计 ===\n")
print("无滤波:")
print(f"  帧数: {len(no_filter)}")
print(f"  Z坐标范围: {no_filter['cam_z'].min():.2f} - {no_filter['cam_z'].max():.2f} mm")
print(f"  Z坐标极差: {no_filter['cam_z'].max() - no_filter['cam_z'].min():.2f} mm")

print("\n有滤波:")
print(f"  帧数: {len(with_filter)}")
print(f"  Z坐标范围: {with_filter['cam_z'].min():.2f} - {with_filter['cam_z'].max():.2f} mm")
print(f"  Z坐标极差: {with_filter['cam_z'].max() - with_filter['cam_z'].min():.2f} mm")
