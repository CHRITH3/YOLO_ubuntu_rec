// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// Human Pose Detection using RGB Image Only
// No depth camera required - works with webcam, video file, or image
//

#include "yolo_pose_detector.h"
#include "pose_utils.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <iostream>

#define FONT_FACE cv::FONT_HERSHEY_PLAIN
#define FONT_SCALE 1
#define FONT_COLOR cv::Scalar(255, 255, 255)
#define THICKNESS 1

void print_usage(const char* program_name) {
    std::cout << "\n=== YOLO Pose Detection (RGB Only) ===\n" << std::endl;
    std::cout << "Usage:\n" << std::endl;
    std::cout << "  1. Webcam mode (default):" << std::endl;
    std::cout << "     " << program_name << "\n" << std::endl;
    std::cout << "  2. Video file mode:" << std::endl;
    std::cout << "     " << program_name << " /path/to/video.mp4\n" << std::endl;
    std::cout << "  3. Image file mode:" << std::endl;
    std::cout << "     " << program_name << " /path/to/image.jpg\n" << std::endl;
    std::cout << "  4. Specify model:" << std::endl;
    std::cout << "     " << program_name << " <source> <model_path>\n" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  q / ESC : Quit" << std::endl;
    std::cout << "  b       : Toggle bounding box" << std::endl;
    std::cout << "  k       : Toggle keypoints" << std::endl;
    std::cout << "  s       : Toggle skeleton" << std::endl;
    std::cout << "  i       : Toggle info overlay" << std::endl;
    std::cout << "  SPACE   : Save current frame / Next image\n" << std::endl;
}

int main(int argc, char **argv) {
    // Parse arguments
    std::string source = "0";  // Default to webcam
    std::string model_path = "models/yolov8n-pose.onnx";

    if (argc > 1) {
        source = argv[1];
        if (source == "-h" || source == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    if (argc > 2) {
        model_path = argv[2];
    }

    print_usage(argv[0]);

    // Initialize YOLO Pose Detector
    std::cout << "Model: " << model_path << std::endl;
    YOLOPoseDetector pose_detector(model_path, 640, 0.5f, 0.45f);
    if (!pose_detector.Init()) {
        std::cerr << "Failed to initialize YOLO Pose Detector!" << std::endl;
        std::cerr << "Make sure you have run: python3 prepare_yolo_model.py" << std::endl;
        return -1;
    }

    // Determine input mode
    bool is_image_mode = false;
    bool is_video_mode = false;
    bool is_webcam_mode = false;

    cv::Mat static_image;
    cv::VideoCapture cap;

    // Try to open as webcam (0, 1, 2, etc.)
    if (source.length() == 1 && isdigit(source[0])) {
        int camera_id = std::stoi(source);
        cap.open(camera_id);
        if (cap.isOpened()) {
            is_webcam_mode = true;
            std::cout << "\nMode: Webcam (ID: " << camera_id << ")" << std::endl;
        }
    }

    // Try to open as image file
    if (!is_webcam_mode) {
        static_image = cv::imread(source);
        if (!static_image.empty()) {
            is_image_mode = true;
            std::cout << "\nMode: Image file" << std::endl;
            std::cout << "Image: " << source << std::endl;
            std::cout << "Size: " << static_image.cols << "x" << static_image.rows << std::endl;
        }
    }

    // Try to open as video file
    if (!is_webcam_mode && !is_image_mode) {
        cap.open(source);
        if (cap.isOpened()) {
            is_video_mode = true;
            std::cout << "\nMode: Video file" << std::endl;
            std::cout << "Video: " << source << std::endl;
            int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
            int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
            std::cout << "FPS: " << fps << ", Frames: " << total_frames << std::endl;
        }
    }

    // Check if any input is available
    if (!is_image_mode && !is_video_mode && !is_webcam_mode) {
        std::cerr << "\nError: Cannot open input source: " << source << std::endl;
        std::cerr << "Please check:\n";
        std::cerr << "  - Webcam is connected (try '0', '1', etc.)\n";
        std::cerr << "  - Image/video file exists\n";
        std::cerr << "  - File path is correct\n" << std::endl;
        return -1;
    }

    // Display options
    bool show_bbox = true;
    bool show_keypoints = true;
    bool show_skeleton = true;
    bool show_info = true;

    int frame_count = 0;
    int pose_count = 0;
    int frame_save_count = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto last_fps_time = start_time;
    int fps_frame_count = 0;
    double current_fps = 0;

    std::cout << "\n=== Starting Detection ===\n" << std::endl;

    // Main loop
    while (true) {
        cv::Mat frame;

        // Get frame based on mode
        if (is_image_mode) {
            frame = static_image.clone();
        } else {
            cap >> frame;
            if (frame.empty()) {
                if (is_video_mode) {
                    std::cout << "\nVideo finished." << std::endl;
                    break;
                } else {
                    std::cerr << "\nWarning: Empty frame from webcam" << std::endl;
                    continue;
                }
            }
        }

        ++frame_count;

        // Detect poses
        auto pose_start = std::chrono::steady_clock::now();
        std::vector<PoseResult> poses = pose_detector.Detect(frame);
        auto pose_end = std::chrono::steady_clock::now();

        double inference_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            pose_end - pose_start).count();

        if (!poses.empty()) {
            ++pose_count;
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

        // Visualize
        cv::Mat display = frame.clone();
        DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.5f);

        // Draw info overlay (2D only, no depth)
        if (show_info) {
            DrawPoseInfo(display, poses, false);  // false = no 3D info
        }

        // Draw performance stats
        std::ostringstream ss;

        if (!is_image_mode) {
            ss << "FPS: " << std::fixed << std::setprecision(1) << current_fps;
            cv::putText(display, ss.str(), cv::Point(10, display.rows - 60),
                        FONT_FACE, 1.5, cv::Scalar(0, 255, 0), 2);
            ss.str("");
        }

        ss << "Inference: " << std::fixed << std::setprecision(0)
           << inference_time << " ms";
        cv::putText(display, ss.str(), cv::Point(10, display.rows - 35),
                    FONT_FACE, 1.2, cv::Scalar(0, 255, 255), 2);

        ss.str("");
        ss << "Detected: " << poses.size() << " person(s)";
        cv::putText(display, ss.str(), cv::Point(10, display.rows - 10),
                    FONT_FACE, 1.2, cv::Scalar(255, 255, 255), 2);

        // Add mode indicator
        std::string mode_str;
        if (is_image_mode) mode_str = "IMAGE";
        else if (is_video_mode) mode_str = "VIDEO";
        else mode_str = "WEBCAM";

        cv::putText(display, mode_str, cv::Point(10, 25),
                    FONT_FACE, 1.5, cv::Scalar(255, 0, 255), 2);

        cv::imshow("YOLO Pose Detection (RGB Only)", display);

        // Handle keyboard input
        int wait_time = is_image_mode ? 0 : 1;
        char key = static_cast<char>(cv::waitKey(wait_time));

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
            if (is_image_mode) {
                // For image mode, space exits
                std::cout << "Exiting image mode." << std::endl;
                break;
            } else {
                // For video/webcam, save frame
                std::ostringstream filename;
                filename << "pose_frame_" << std::setfill('0') << std::setw(4)
                         << frame_save_count++ << ".jpg";
                cv::imwrite(filename.str(), display);
                std::cout << "Saved: " << filename.str() << std::endl;
            }
        }

        // In image mode, loop until user presses key
        if (is_image_mode && key == -1) {
            continue;
        }
    }

    // Cleanup
    if (cap.isOpened()) {
        cap.release();
    }
    cv::destroyAllWindows();

    // Statistics
    auto end_time = std::chrono::steady_clock::now();
    double total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count() / 1000.0;

    std::cout << "\n=== Performance Statistics ===" << std::endl;
    std::cout << "Total runtime: " << std::fixed << std::setprecision(2)
              << total_time << " seconds" << std::endl;
    std::cout << "Total frames processed: " << frame_count << std::endl;
    std::cout << "Frames with pose detected: " << pose_count << std::endl;

    if (total_time > 0 && !is_image_mode) {
        std::cout << "Average FPS: " << std::fixed << std::setprecision(1)
                  << (frame_count / total_time) << std::endl;
    }

    std::cout << "\n==============================\n" << std::endl;

    return 0;
}

/*
================================================================================
                    USAGE INSTRUCTIONS - RGB POSE DETECTION
================================================================================

PROGRAM NAME: get_pose_rgb_only
VERSION: 1.0 (YOLO Pose - RGB Only)

DESCRIPTION:
  Real-time human pose detection using YOLOv8-pose model with RGB images only.
  No depth camera required - works with:
    - Webcam (built-in or USB)
    - Video files (mp4, avi, etc.)
    - Static images (jpg, png, etc.)

PREREQUISITES:
  1. ONNX Runtime installed:
     ./install_onnxruntime.sh

  2. YOLO model prepared:
     python3 prepare_yolo_model.py

  3. Compiled:
     cmake .
     make get_pose_rgb_only

COMPILATION:
  make get_pose_rgb_only

EXECUTION:

  # Webcam (default, camera ID 0)
  ./build/yolo_pose_rgb_only

  # Specific webcam
  ./build/yolo_pose_rgb_only 1

  # Video file
  ./build/yolo_pose_rgb_only /path/to/video.mp4

  # Image file
  ./build/yolo_pose_rgb_only /path/to/image.jpg

  # With custom model
  ./build/yolo_pose_rgb_only 0 models/yolov8m-pose.onnx

FEATURES:
  ✓ Real-time human pose detection (17 COCO keypoints)
  ✓ Multi-person detection
  ✓ Colored skeleton visualization
  ✓ Bounding box with confidence score
  ✓ Performance monitoring (FPS, inference time)
  ✓ Frame capture capability
  ✓ Works with webcam, video, or images
  ✗ No depth information (2D only)

KEYBOARD CONTROLS:
  - 'q' or ESC : Quit the application
  - 'b' or 'B' : Toggle bounding box on/off
  - 'k' or 'K' : Toggle keypoints display on/off
  - 's' or 'S' : Toggle skeleton lines on/off
  - 'i' or 'I' : Toggle info overlay on/off
  - SPACE      : Save current frame (video/webcam) or exit (image mode)

OUTPUT INFORMATION:
  On screen:
    - Person detection count
    - FPS and inference time (video/webcam mode)
    - Per-person confidence
    - Input mode (IMAGE/VIDEO/WEBCAM)

  On exit:
    - Total runtime
    - Total frames processed
    - Average FPS

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
  - Detection FPS: 15-25 (CPU), 50+ (GPU with CUDA)
  - Inference time: 40-70ms (CPU), <20ms (GPU)
  - Multi-person: Up to 10+ persons simultaneously

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
    * Check ONNX Runtime installation: ldd build/yolo_pose_rgb_only
    * Verify model file exists: ls -lh models/yolov8n-pose.onnx
    * Run preparation script: python3 prepare_yolo_model.py

  - "Cannot open input source":
    * Webcam: Try different camera IDs (0, 1, 2)
    * Video: Check file exists and is readable
    * Image: Verify file format (jpg, png, bmp supported)

  - Low FPS:
    * Use smaller model (yolov8n-pose)
    * Reduce video resolution
    * Close other applications
    * Consider GPU acceleration

  - Poor detection:
    * Ensure good lighting
    * Person should be fully visible
    * Avoid extreme poses or occlusion
    * Try different confidence thresholds

NOTES:
  - No sudo required (unlike camera version)
  - RGB only - no depth information
  - Keypoints are 2D image coordinates
  - Best results with full-body visibility
  - Partial occlusion supported with reduced accuracy

COMPARISON WITH DEPTH VERSION:
  RGB Only (this program):
    ✓ No special hardware needed
    ✓ Works with any camera/video/image
    ✓ Faster (no depth processing)
    ✗ 2D only (no distance/height measurements)

  With Depth (get_pose_with_depth):
    ✓ 3D coordinates available
    ✓ Distance and height measurements
    ✗ Requires IMSEE dual-camera
    ✗ Requires sudo for camera access

================================================================================
*/
