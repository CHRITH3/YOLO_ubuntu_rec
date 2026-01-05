// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// Human Pose Detection using INDEMIND Left Camera (RGB Only)
// Uses left camera image from INDEMIND dual-camera system
// No depth processing - pure RGB pose detection
//

#include "imrdata.h"
#include "imrsdk.h"
#include "logging.h"
#include "types.h"
#include "yolo_pose_detector.h"
#include "pose_utils.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <queue>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace indem;

#define FONT_FACE cv::FONT_HERSHEY_PLAIN
#define FONT_SCALE 1
#define FONT_COLOR cv::Scalar(255, 255, 255)
#define THICKNESS 1

// Performance optimization: limit queue size to prevent backlog
#define MAX_QUEUE_SIZE 2

template <typename T> void clear(std::queue<T> &q) {
  std::queue<T> empty;
  swap(empty, q);
}

int main(int argc, char **argv) {
  (void)argc;

  // Check model file
  std::string model_path = "models/yolov8n-pose.onnx";
  if (argc > 1) {
    model_path = argv[1];
  }

  std::cout << "\n=== YOLO Pose Detection with INDEMIND Left Camera ===\n" << std::endl;
  std::cout << "Using: Left camera RGB image only (no depth processing)" << std::endl;

  // Initialize IMSEE SDK
  auto m_pSDK = new CIMRSDK();
  MRCONFIG config = {0};
  config.bSlam = false;
  config.imgResolution = IMG_640;
  config.imgFrequency = 50;
  config.imuFrequency = 0;  // Disabled for performance

  std::cout << "\nInitializing INDEMIND camera..." << std::endl;
  m_pSDK->Init(config);

  // Get camera intrinsics (for reference, not used in 2D pose detection)
  auto param = m_pSDK->GetModuleParams()._left_camera[RESOLUTION::RES_640X400];
  std::cout << "\nLeft Camera Intrinsics:" << std::endl;
  std::cout << "  fx: " << param._K[0] << ", fy: " << param._K[4] << std::endl;
  std::cout << "  cx: " << param._K[2] << ", cy: " << param._K[5] << std::endl;
  std::cout << "  Resolution: 640x400" << std::endl;

  // Initialize YOLO Pose Detector
  std::cout << "\nModel: " << model_path << std::endl;
  YOLOPoseDetector pose_detector(model_path, 640, 0.5f, 0.45f);
  if (!pose_detector.Init()) {
    std::cerr << "Failed to initialize YOLO Pose Detector!" << std::endl;
    delete m_pSDK;
    return -1;
  }

  // Queue for image stream
  std::queue<cv::Mat> image_queue;
  std::mutex mutex_image;

  int img_count = 0;
  int pose_count = 0;
  int dropped_images = 0;
  double last_img_time = -1.0;

  // Register image callback - only use left camera
  m_pSDK->RegistImgCallback([&](double time, cv::Mat left, cv::Mat right) {
    (void)right;  // Ignore right camera
    if (!left.empty()) {
      if (last_img_time >= 0) {
        // Calculate camera FPS
        double fps = 1.0 / (time - last_img_time);
        (void)fps;
      }
      last_img_time = time;

      {
        std::unique_lock<std::mutex> lock(mutex_image);
        if (image_queue.size() < MAX_QUEUE_SIZE) {
          // Convert grayscale to BGR for YOLO (which expects color images)
          cv::Mat color_image;
          if (left.channels() == 1) {
            cv::cvtColor(left, color_image, cv::COLOR_GRAY2BGR);
          } else {
            color_image = left.clone();
          }
          image_queue.push(color_image);
        } else {
          ++dropped_images;
        }
      }
      ++img_count;
    }
  });

  std::cout << "\n=== Controls ===\n" << std::endl;
  std::cout << "  q / ESC : Quit" << std::endl;
  std::cout << "  b       : Toggle bounding box" << std::endl;
  std::cout << "  k       : Toggle keypoints" << std::endl;
  std::cout << "  s       : Toggle skeleton" << std::endl;
  std::cout << "  i       : Toggle info overlay" << std::endl;
  std::cout << "  SPACE   : Save current frame\n" << std::endl;

  // Display options
  bool show_bbox = true;
  bool show_keypoints = true;
  bool show_skeleton = true;
  bool show_info = true;

  int frame_save_count = 0;

  // FPS calculation
  auto loop_start = std::chrono::steady_clock::now();
  auto last_fps_time = loop_start;
  int fps_frame_count = 0;
  double current_fps = 0;

  std::cout << "=== Starting Detection ===\n" << std::endl;
  std::cout << "Waiting for camera images..." << std::endl;

  // Main loop
  while (true) {
    cv::Mat left_image;

    // Get image from queue
    {
      std::unique_lock<std::mutex> lock(mutex_image);
      if (!image_queue.empty()) {
        left_image = image_queue.front();
        clear(image_queue);  // Clear queue to always process latest frame
      }
    }

    // Process if we have an image
    if (!left_image.empty()) {
      auto pose_start = std::chrono::steady_clock::now();

      // Detect poses
      std::vector<PoseResult> poses = pose_detector.Detect(left_image);

      auto pose_end = std::chrono::steady_clock::now();
      double inference_time = std::chrono::duration_cast<std::chrono::milliseconds>(
          pose_end - pose_start).count();

      if (!poses.empty()) {
        ++pose_count;

        // Debug: Print first detection keypoint info (only once every 30 frames)
        static int debug_counter = 0;
        if (debug_counter++ % 30 == 0 && poses.size() > 0) {
          std::cout << "\n[DEBUG] First person keypoint confidences:" << std::endl;
          for (size_t i = 0; i < poses[0].keypoints.size(); i++) {
            const auto& kp = poses[0].keypoints[i];
            std::cout << "  KP" << i << ": conf=" << std::fixed << std::setprecision(3)
                     << kp.confidence << " pos=(" << (int)kp.x << "," << (int)kp.y << ")" << std::endl;
          }
        }
      }

      // Calculate FPS
      ++fps_frame_count;
      auto current_time = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          current_time - last_fps_time).count() / 1000.0;

      if (elapsed >= 1.0) {
        current_fps = fps_frame_count / elapsed;
        fps_frame_count = 0;
        last_fps_time = current_time;
      }

      // Visualize (use lower threshold for keypoints: 0.3 instead of 0.5)
      cv::Mat display = left_image.clone();
      DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.3f);

      // Draw info overlay (2D only, no depth)
      if (show_info) {
        DrawPoseInfo(display, poses, false);  // false = no 3D info
      }

      // Draw performance stats
      std::ostringstream ss;

      ss << "FPS: " << std::fixed << std::setprecision(1) << current_fps;
      cv::putText(display, ss.str(), cv::Point(10, display.rows - 60),
                  FONT_FACE, 1.5, cv::Scalar(0, 255, 0), 2);

      ss.str("");
      ss << "Inference: " << std::fixed << std::setprecision(0)
         << inference_time << " ms";
      cv::putText(display, ss.str(), cv::Point(10, display.rows - 35),
                  FONT_FACE, 1.2, cv::Scalar(0, 255, 255), 2);

      ss.str("");
      ss << "Detected: " << poses.size() << " person(s)";
      cv::putText(display, ss.str(), cv::Point(10, display.rows - 10),
                  FONT_FACE, 1.2, cv::Scalar(255, 255, 255), 2);

      // Add camera indicator and display settings
      cv::putText(display, "INDEMIND LEFT", cv::Point(10, 25),
                  FONT_FACE, 1.5, cv::Scalar(255, 0, 255), 2);

      // Display current settings
      std::string settings = "";
      if (show_bbox) settings += "B";
      if (show_keypoints) settings += "K";
      if (show_skeleton) settings += "S";
      if (show_info) settings += "I";
      if (!settings.empty()) {
        cv::putText(display, "[" + settings + "]", cv::Point(display.cols - 80, 25),
                    FONT_FACE, 1.5, cv::Scalar(0, 255, 255), 2);
      }

      cv::imshow("YOLO Pose - INDEMIND Left Camera", display);
    }

    // Handle keyboard input
    char key = static_cast<char>(cv::waitKey(1));
    if (key == 27 || key == 'q' || key == 'Q') {
      break;
    } else if (key == 'b' || key == 'B') {
      show_bbox = !show_bbox;
      std::cout << "Bounding box: " << (show_bbox ? "ON" : "OFF") << std::endl;
    } else if (key == 'k' || key == 'K') {
      show_keypoints = !show_keypoints;
      std::cout << "Keypoints: " << (show_keypoints ? "ON" : "OFF") << std::endl;
    } else if (key == 's' || key == 'S') {
      show_skeleton = !show_skeleton;
      std::cout << "Skeleton: " << (show_skeleton ? "ON" : "OFF") << std::endl;
    } else if (key == 'i' || key == 'I') {
      show_info = !show_info;
      std::cout << "Info overlay: " << (show_info ? "ON" : "OFF") << std::endl;
    } else if (key == ' ') {
      // Save frame
      if (!left_image.empty()) {
        std::ostringstream filename;
        filename << "pose_frame_" << std::setfill('0') << std::setw(4)
                 << frame_save_count++ << ".jpg";
        cv::imwrite(filename.str(), left_image);
        std::cout << "Saved: " << filename.str() << std::endl;
      }
    }
  }

  auto loop_end = std::chrono::steady_clock::now();
  double total_time = std::chrono::duration_cast<std::chrono::seconds>(
      loop_end - loop_start).count();

  delete m_pSDK;
  cv::destroyAllWindows();

  // Statistics
  std::cout << "\n=== Performance Statistics ===\n" << std::endl;
  std::cout << "Total runtime: " << total_time << " seconds" << std::endl;
  std::cout << "Total images captured: " << img_count << std::endl;
  std::cout << "Total pose detections: " << pose_count << std::endl;
  std::cout << "Dropped image frames: " << dropped_images << std::endl;

  if (total_time > 0) {
    std::cout << "\nAverage rates:" << std::endl;
    std::cout << "  Image capture: " << (img_count / total_time) << " FPS" << std::endl;
    std::cout << "  Pose detection: " << (pose_count / total_time) << " FPS" << std::endl;
  }

  std::cout << "\n==============================\n" << std::endl;

  return 0;
}

/*
================================================================================
                USAGE INSTRUCTIONS - INDEMIND LEFT CAMERA POSE
================================================================================

PROGRAM NAME: get_pose_indemind_left
VERSION: 1.0 (YOLO Pose - INDEMIND Left Camera RGB Only)

DESCRIPTION:
  Real-time human pose detection using YOLOv8-pose model with INDEMIND
  left camera RGB images. No depth processing - pure 2D pose detection.

  Key differences from get_pose_with_depth:
    ✓ Uses only left camera image (RGB)
    ✗ No depth processing
    ✗ No 3D coordinates
    ✗ No distance/height measurements
    ✓ Faster inference (no depth overhead)
    ✓ Lower CPU usage

PREREQUISITES:
  1. ONNX Runtime installed:
     ./install_onnxruntime.sh

  2. YOLO model prepared:
     python3 prepare_yolo_model.py

  3. INDEMIND camera connected

  4. Compiled:
     cmake .
     make get_pose_indemind_left

COMPILATION:
  make get_pose_indemind_left

EXECUTION:
  sudo ./build/yolo_pose_indemind_left [model_path]

  Optional: Specify model path (default: models/yolov8n-pose.onnx)
  sudo ./build/yolo_pose_indemind_left models/yolov8s-pose.onnx

FEATURES:
  ✓ Real-time human pose detection (17 COCO keypoints)
  ✓ Multi-person detection
  ✓ Colored skeleton visualization
  ✓ Bounding box with confidence score
  ✓ Performance monitoring (FPS, inference time)
  ✓ Frame capture capability
  ✓ Uses INDEMIND left camera
  ✗ No depth information (2D only)

KEYBOARD CONTROLS:
  - 'q' or ESC : Quit the application
  - 'b' or 'B' : Toggle bounding box on/off
  - 'k' or 'K' : Toggle keypoints display on/off
  - 's' or 'S' : Toggle skeleton lines on/off
  - 'i' or 'I' : Toggle info overlay on/off
  - SPACE      : Save current frame to disk

OUTPUT INFORMATION:
  On screen:
    - Person detection count
    - FPS and inference time
    - Per-person confidence (2D only)
    - Camera indicator (INDEMIND LEFT)

  On exit:
    - Total runtime
    - Total frames captured
    - Total pose detections
    - Average frame rates
    - Dropped frame statistics

VISUALIZATION:
  - Bounding Box: Green rectangle around detected person
  - Keypoints: Colored circles (color indicates confidence)
    * Red: High confidence (>0.8)
    * Orange: Medium confidence (0.6-0.8)
    * Yellow: Low confidence (0.5-0.6)
  - Skeleton: Colored lines connecting keypoints
    * Yellow: Head
    * Cyan: Torso
    * Green: Left arm
    * Blue: Right arm
    * Magenta: Left leg
    * Orange: Right leg

COCO 17 KEYPOINTS:
  0-Nose, 1-Left Eye, 2-Right Eye, 3-Left Ear, 4-Right Ear,
  5-Left Shoulder, 6-Right Shoulder, 7-Left Elbow, 8-Right Elbow,
  9-Left Wrist, 10-Right Wrist, 11-Left Hip, 12-Right Hip,
  13-Left Knee, 14-Right Knee, 15-Left Ankle, 16-Right Ankle

PERFORMANCE:
  Expected performance (YOLOv8n-pose):
  - Detection FPS: 20-30 (CPU), 50+ (GPU with CUDA)
  - Inference time: 30-50ms (CPU), <20ms (GPU)
  - Multi-person: Up to 10+ persons simultaneously

  Performance comparison:
    get_pose_indemind_left (this program):
      - Faster: No depth processing overhead
      - Lower CPU: Single stream processing
      - Higher FPS: Lighter computation

    get_pose_with_depth:
      - Slower: Depth processing + 3D mapping
      - Higher CPU: Dual stream + depth computation
      - Lower FPS: More computation required

  For better performance:
  - Use YOLOv8n-pose (fastest)
  - Close other applications
  - Enable GPU acceleration (CUDA-enabled ONNX Runtime)

  For better accuracy:
  - Use YOLOv8m-pose or YOLOv8l-pose
  - Ensure good lighting conditions
  - Position person fully visible in frame

TROUBLESHOOTING:
  - "Failed to initialize detector":
    * Check ONNX Runtime installation
    * Verify model file exists
    * Run: ls -lh models/yolov8n-pose.onnx

  - "Cannot initialize camera":
    * Check INDEMIND camera is connected
    * Run: lsusb | grep INDEMIND
    * Check USB permissions
    * Try: sudo ./build/yolo_pose_indemind_left

  - Low FPS:
    * Use smaller model (yolov8n-pose)
    * Close other applications
    * Consider GPU acceleration

  - Poor detection:
    * Ensure good lighting
    * Person should be fully visible
    * Avoid extreme poses or occlusion
    * Try different confidence thresholds

NOTES:
  - Requires sudo privileges for camera access
  - Uses only left camera (right camera ignored)
  - RGB only - no depth information
  - Keypoints are 2D image coordinates
  - Best results with full-body visibility
  - Partial occlusion supported with reduced accuracy
  - Camera resolution: 640x400 (automatically converted to RGB)

COMPARISON WITH OTHER PROGRAMS:

  get_pose_indemind_left (this program):
    ✓ INDEMIND left camera
    ✓ RGB only, no depth
    ✓ Faster than depth version
    ✗ 2D only (no 3D coordinates)
    ✓ Requires sudo

  get_pose_with_depth:
    ✓ INDEMIND dual camera
    ✓ RGB + Depth fusion
    ✓ 3D coordinates, distance, height
    ✗ Slower (depth processing)
    ✓ Requires sudo

  get_pose_rgb_only:
    ✓ Any webcam/video/image
    ✓ RGB only, no depth
    ✓ Most flexible input
    ✗ 2D only (no 3D coordinates)
    ✗ No sudo required

USE CASES:

  Use get_pose_indemind_left when:
    - You have INDEMIND camera
    - You only need 2D pose detection
    - You want maximum performance
    - Depth information is not required

  Use get_pose_with_depth when:
    - You need 3D coordinates
    - You need distance/height measurements
    - You want depth-aware pose estimation

  Use get_pose_rgb_only when:
    - You don't have INDEMIND camera
    - You want to process videos/images
    - You need flexibility in input sources

================================================================================
*/
