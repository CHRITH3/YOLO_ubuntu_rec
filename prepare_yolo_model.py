#!/usr/bin/env python3
"""
YOLO Pose Model Preparation Script
This script downloads YOLOv8-pose model and exports it to ONNX format
"""

import os
import sys

def check_dependencies():
    """Check if required packages are installed"""
    try:
        import torch
        print(f"✓ PyTorch {torch.__version__} installed")
    except ImportError:
        print("✗ PyTorch not installed")
        print("  Install: pip install torch torchvision")
        return False

    try:
        from ultralytics import YOLO
        print("✓ Ultralytics installed")
    except ImportError:
        print("✗ Ultralytics not installed")
        print("  Install: pip install ultralytics")
        return False

    return True

def export_model(model_size='n', imgsz=640):
    """
    Download and export YOLOv8-pose model to ONNX format

    Args:
        model_size: Model size ('n', 's', 'm', 'l', 'x')
                   n = nano (fastest, least accurate)
                   s = small
                   m = medium
                   l = large
                   x = extra large (slowest, most accurate)
        imgsz: Input image size (default 640)
    """
    from ultralytics import YOLO

    model_name = f'yolov8{model_size}-pose.pt'
    output_name = f'yolov8{model_size}-pose.onnx'

    print(f"\n{'='*60}")
    print(f"Preparing YOLOv8-{model_size}-pose model")
    print(f"{'='*60}")

    # Create models directory if not exists
    models_dir = 'models'
    if not os.path.exists(models_dir):
        os.makedirs(models_dir)
        print(f"✓ Created directory: {models_dir}/")

    # Load model (will download if not exists)
    print(f"\n[1/3] Loading {model_name}...")
    model = YOLO(model_name)
    print(f"✓ Model loaded successfully")

    # Export to ONNX
    print(f"\n[2/3] Exporting to ONNX format...")
    print(f"  - Input size: {imgsz}x{imgsz}")
    print(f"  - Format: ONNX")
    print(f"  - Simplify: True")

    model.export(
        format='onnx',
        imgsz=imgsz,
        simplify=True,
        opset=12
    )

    # Move to models directory
    if os.path.exists(output_name):
        target_path = os.path.join(models_dir, output_name)
        if os.path.exists(target_path):
            os.remove(target_path)
        os.rename(output_name, target_path)
        print(f"✓ Exported: {target_path}")
    else:
        print(f"✗ Export failed: {output_name} not found")
        return False

    # Verify output
    print(f"\n[3/3] Verifying model...")
    file_size = os.path.getsize(target_path) / (1024 * 1024)  # MB
    print(f"✓ Model size: {file_size:.2f} MB")

    print(f"\n{'='*60}")
    print("Model preparation complete!")
    print(f"{'='*60}")
    print(f"\nModel location: {os.path.abspath(target_path)}")
    print(f"\nModel specifications:")
    print(f"  - Input shape: [1, 3, {imgsz}, {imgsz}] (NCHW)")
    print(f"  - Output shape: [1, 56, 8400]")
    print(f"  - Output format: [4bbox, 1obj_conf, 1cls_conf, 17x3keypoints]")
    print(f"\nCOCO 17 Keypoints:")
    keypoints = [
        "0-Nose", "1-Left Eye", "2-Right Eye", "3-Left Ear", "4-Right Ear",
        "5-Left Shoulder", "6-Right Shoulder", "7-Left Elbow", "8-Right Elbow",
        "9-Left Wrist", "10-Right Wrist", "11-Left Hip", "12-Right Hip",
        "13-Left Knee", "14-Right Knee", "15-Left Ankle", "16-Right Ankle"
    ]
    for i, kp in enumerate(keypoints):
        if i % 3 == 0:
            print()
        print(f"  {kp:20s}", end="")
    print("\n")

    return True

def main():
    print("="*60)
    print("YOLOv8-Pose Model Preparation for IMSEE-SDK")
    print("="*60)

    # Check dependencies
    print("\nChecking dependencies...")
    if not check_dependencies():
        print("\n✗ Please install missing dependencies first")
        print("\nQuick install:")
        print("  pip install torch torchvision ultralytics")
        return 1

    # Model size selection
    print("\nAvailable model sizes:")
    print("  n - Nano   (fastest,  ~6MB,  best for real-time)")
    print("  s - Small  (fast,     ~12MB)")
    print("  m - Medium (balanced, ~26MB)")
    print("  l - Large  (accurate, ~52MB)")
    print("  x - XLarge (most accurate, ~98MB)")

    model_size = input("\nSelect model size [n/s/m/l/x] (default: n): ").strip().lower()
    if not model_size:
        model_size = 'n'

    if model_size not in ['n', 's', 'm', 'l', 'x']:
        print(f"✗ Invalid model size: {model_size}")
        return 1

    # Image size selection
    imgsz_input = input("Input image size (default: 640): ").strip()
    imgsz = int(imgsz_input) if imgsz_input else 640

    # Export model
    if export_model(model_size, imgsz):
        print("✓ Success! You can now compile and run the C++ program")
        return 0
    else:
        print("✗ Model preparation failed")
        return 1

if __name__ == '__main__':
    sys.exit(main())
