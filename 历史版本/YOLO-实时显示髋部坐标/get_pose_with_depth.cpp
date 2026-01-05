// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// Human Pose Detection with Depth Fusion
// Based on get_disparity_with_image_V2_optimized.cpp with YOLO Pose

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
  (void)argc; (void)argv;

  // Check model file
  std::string model_path = "models/yolov8n-pose.onnx";
  if (argc > 1) {
    model_path = argv[1];
  }

  std::cout << "\n=== YOLO Pose Detection with IMSEE Depth ===\n" << std::endl;

  // Initialize IMSEE SDK first
  auto m_pSDK = new CIMRSDK();
  MRCONFIG config = {0};
  config.bSlam = false;
  config.imgResolution = IMG_640;
  config.imgFrequency = 50;
  config.imuFrequency = 0;  // Disabled for performance

  m_pSDK->Init(config);

  // Get camera intrinsics
  auto param = m_pSDK->GetModuleParams()._left_camera[RESOLUTION::RES_640X400];
  cv::Mat cv_in_left = cv::Mat::eye(3, 3, CV_64F);
  cv_in_left.at<double>(0, 0) = param._K[0];  // fx
  cv_in_left.at<double>(1, 1) = param._K[4];  // fy
  cv_in_left.at<double>(0, 2) = param._K[2];  // cx
  cv_in_left.at<double>(1, 2) = param._K[5];  // cy

  std::cout << "Camera Intrinsics:" << std::endl;
  std::cout << "  fx: " << param._K[0] << ", fy: " << param._K[4] << std::endl;
  std::cout << "  cx: " << param._K[2] << ", cy: " << param._K[5] << std::endl;

  // Initialize YOLO Pose Detector
  std::cout << "\nModel: " << model_path << std::endl;
  YOLOPoseDetector pose_detector(model_path, 640, 0.5f, 0.45f);
  if (!pose_detector.Init()) {
    std::cerr << "Failed to initialize YOLO Pose Detector!" << std::endl;
    delete m_pSDK;
    return -1;
  }

  // Queues for data streams
  std::queue<cv::Mat> image_queue;
  std::queue<cv::Mat> depth_queue;
  std::mutex mutex_image;
  std::mutex mutex_depth;

  int img_count = 0;
  int depth_count = 0;
  int pose_count = 0;
  int dropped_images = 0;
  int dropped_depth = 0;
  double last_img_time = -1.0;
  double last_pose_time = -1.0;

  // Register image callback
  m_pSDK->RegistImgCallback([&](double time, cv::Mat left, cv::Mat right) {
    (void)right;  // Only use left image for pose detection
    if (!left.empty()) {
      if (last_img_time >= 0) {
        // Calculate FPS
        double fps = 1.0 / (time - last_img_time);
        (void)fps;  // Used below
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

  // Register depth callback
  if (m_pSDK->EnableDepthProcessor()) {
    m_pSDK->RegistDepthCallback([&](double time, cv::Mat depth) {
      (void)time;
      if (!depth.empty()) {
        // Convert depth from meters to millimeters
        cv::Mat depth_mm;
        depth.convertTo(depth_mm, CV_16U, 1000.0);

        {
          std::unique_lock<std::mutex> lock(mutex_depth);
          if (depth_queue.size() < MAX_QUEUE_SIZE) {
            depth_queue.push(depth_mm);
          } else {
            ++dropped_depth;
          }
        }
        ++depth_count;
      }
    });
  } else {
    std::cerr << "Failed to enable depth processor!" << std::endl;
    delete m_pSDK;
    return -1;
  }

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

  // Main loop
  auto loop_start = std::chrono::steady_clock::now();

  while (true) {
    cv::Mat left_image, depth_data;

    // Get image and depth
    {
      std::unique_lock<std::mutex> lock(mutex_image);
      if (!image_queue.empty()) {
        left_image = image_queue.front();
        clear(image_queue);
      }
    }

    {
      std::unique_lock<std::mutex> lock(mutex_depth);
      if (!depth_queue.empty()) {
        depth_data = depth_queue.front();
        clear(depth_queue);
      }
    }

    // Process if we have both image and depth
    if (!left_image.empty() && !depth_data.empty()) {
      auto pose_start = std::chrono::steady_clock::now();

      // Detect poses
      std::vector<PoseResult> poses = pose_detector.Detect(left_image);

      auto pose_end = std::chrono::steady_clock::now();
      double pose_time = std::chrono::duration_cast<std::chrono::milliseconds>(
          pose_end - pose_start).count() / 1000.0;

      // Map to 3D using depth
      if (!poses.empty()) {
        MapPoseTo3D(poses, depth_data, cv_in_left);
        ++pose_count;
      }

      // Calculate FPS
      double fps = 0;
      if (last_pose_time > 0) {
        fps = 1.0 / pose_time;
      }
      last_pose_time = pose_time;

      // Visualize
      cv::Mat display = left_image.clone();
      DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.5f);

      // Draw info overlay
      if (show_info) {
        DrawPoseInfo(display, poses, true);
      }

      // Draw performance stats
      std::ostringstream ss;
      ss << "FPS: " << std::fixed << std::setprecision(1) << fps;
      cv::putText(display, ss.str(), cv::Point(10, display.rows - 60),
                  FONT_FACE, 1.5, cv::Scalar(0, 255, 0), 2);

      ss.str("");
      ss << "Inference: " << std::fixed << std::setprecision(0)
         << (pose_time * 1000) << " ms";
      cv::putText(display, ss.str(), cv::Point(10, display.rows - 35),
                  FONT_FACE, 1.2, cv::Scalar(0, 255, 255), 2);

      ss.str("");
      ss << "Detected: " << poses.size() << " person(s)";
      cv::putText(display, ss.str(), cv::Point(10, display.rows - 10),
                  FONT_FACE, 1.2, cv::Scalar(255, 255, 255), 2);

      cv::imshow("Pose Detection + Depth", display);
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

  // Statistics
  std::cout << "\n=== Performance Statistics ===\n" << std::endl;
  std::cout << "Total runtime: " << total_time << " seconds" << std::endl;
  std::cout << "Total images captured: " << img_count << std::endl;
  std::cout << "Total depth maps: " << depth_count << std::endl;
  std::cout << "Total pose detections: " << pose_count << std::endl;
  std::cout << "Dropped image frames: " << dropped_images << std::endl;
  std::cout << "Dropped depth frames: " << dropped_depth << std::endl;

  if (total_time > 0) {
    std::cout << "\nAverage rates:" << std::endl;
    std::cout << "  Image: " << (img_count / total_time) << " FPS" << std::endl;
    std::cout << "  Depth: " << (depth_count / total_time) << " FPS" << std::endl;
    std::cout << "  Pose: " << (pose_count / total_time) << " FPS" << std::endl;
  }

  std::cout << "\n==============================\n" << std::endl;

  return 0;
}

/*
================================================================================
                    USAGE INSTRUCTIONS - POSE WITH DEPTH
================================================================================

PROGRAM NAME: get_pose_with_depth
VERSION: 1.0 (YOLO Pose + IMSEE Depth Fusion)

DESCRIPTION:
  Real-time human pose detection and estimation using YOLOv8-pose model
  with 3D depth information from IMSEE dual-camera system.

PREREQUISITES:
  1. ONNX Runtime installed:
     ./install_onnxruntime.sh

  2. YOLO model prepared:
     python3 prepare_yolo_model.py

  3. Compiled:
     cmake .
     make get_pose_with_depth

COMPILATION:
  make get_pose_with_depth

EXECUTION:
  sudo ./output/bin/get_pose_with_depth [model_path]

  Optional: Specify model path (default: models/yolov8n-pose.onnx)
  sudo ./output/bin/get_pose_with_depth models/yolov8s-pose.onnx

FEATURES:
  ✓ Real-time human pose detection (17 COCO keypoints)
  ✓ 3D coordinate mapping using depth data
  ✓ Multi-person detection
  ✓ Colored skeleton visualization
  ✓ Bounding box with confidence score
  ✓ Body height estimation
  ✓ Average depth display
  ✓ Performance monitoring (FPS, inference time)
  ✓ Frame capture capability

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
    - Per-person confidence
    - Average depth from camera
    - Estimated body height

  On exit:
    - Total runtime
    - Total frames processed
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
  - Detection FPS: 15-20 (CPU), 50+ (GPU with CUDA)
  - End-to-end latency: <100ms
  - Multi-person: Up to 10+ persons simultaneously

  For better performance:
  - Use YOLOv8n-pose (fastest)
  - Reduce input size (480 instead of 640)
  - Enable GPU acceleration (CUDA-enabled ONNX Runtime)

  For better accuracy:
  - Use YOLOv8m-pose or YOLOv8l-pose
  - Increase input size
  - Ensure good lighting conditions

TROUBLESHOOTING:
  - "Failed to initialize detector":
    * Check ONNX Runtime installation
    * Verify model file exists
    * Run: ls -lh models/yolov8n-pose.onnx

  - Low FPS:
    * Use smaller model (yolov8n-pose)
    * Close other applications
    * Consider GPU acceleration

  - Poor detection:
    * Ensure good lighting
    * Person should be fully visible
    * Try different confidence thresholds

  - Depth values invalid:
    * Ensure textured background
    * Check camera exposure settings
    * Verify depth processor is running

NOTES:
  - Requires sudo privileges for camera access
  - Best results with full-body visibility
  - Partial occlusion supported with reduced accuracy
  - 3D coordinates in left camera frame
  - Depth in millimeters, distances in mm

================================================================================
*/
