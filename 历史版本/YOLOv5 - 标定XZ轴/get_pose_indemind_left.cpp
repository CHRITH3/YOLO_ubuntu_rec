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

static cv::Mat cv_in_left, cv_in_left_inv;

// DepthRegion class for mouse interaction to display region depth information
class DepthRegion {
public:
  explicit DepthRegion(std::uint32_t n)
      : n_(std::move(n)), show_(false), selected_(false), point_(0, 0),
        click_count_(0), P1_cam_(0, 0, 0), P2_cam_(0, 0, 0), P3_cam_(0, 0, 0),
        origin_(0, 0, 0), coord_system_ready_(false) {
    rotation_matrix_ = cv::Mat::eye(3, 3, CV_64F);
  }

  ~DepthRegion() = default;

  /**
   * Mouse event handler: Records three clicks to establish coordinate system.
   * Click 1: Origin (P1)
   * Click 2: Defines X-axis direction (P1 -> P2)
   * Click 3: Defines -Z-axis direction (P3 -> P1, so Z is P1 -> P3 reversed)
   * After 3 clicks, coordinate system is locked.
   */
  void OnMouse(const int &event, const int &x, const int &y, const int &flags) {
    (void)flags;
    if (event != cv::EVENT_MOUSEMOVE && event != cv::EVENT_LBUTTONDOWN) {
      return;
    }

    show_ = true;

    if (event == cv::EVENT_MOUSEMOVE) {
      // Update cursor position only if coordinate system not yet established
      if (!coord_system_ready_) {
        point_.x = x;
        point_.y = y;
      }
    } else if (event == cv::EVENT_LBUTTONDOWN) {
      std::cout << "[Click " << (click_count_ + 1) << "] at pixel (" << x << ", " << y << ")" << std::endl;

      // Record click position
      point_.x = x;
      point_.y = y;
      click_count_++;

      // Mark that we need to process this click (calculate 3D coordinate)
      if (click_count_ <= 3) {
        selected_ = true;  // Use selected_ flag to indicate pending 3D calculation
      }
    }
  }

  template <typename T>
  void ShowElems(const cv::Mat &depth,
                 std::function<std::string(const T &elem)> elem2string,
                 int elem_space = 60,
                 std::function<std::string(
                     const cv::Mat &depth, const cv::Point &point,
                     const std::uint32_t &n, double X, double Y, double Z)>
                     getinfo = nullptr) {
    (void)elem2string;  // Not used anymore (no grid display)
    (void)elem_space;   // Not used anymore
    (void)n_;           // Not used anymore
    (void)getinfo;      // Not used anymore

    if (!show_)
      return;

    // Create window for displaying coordinate information
    cv::Mat im(400, 650, CV_8UC3, cv::Scalar(255, 255, 255));

    int baseline = 0;
    int y_offset = 25;
    int line_height = 28;

    // Calculate (X, Y, Z) in left camera coordinate system for current mouse position
    cv::Mat mouse_left_cor(3, 1, CV_64FC1), mouse_img_cor(3, 1, CV_64FC1);
    mouse_img_cor.at<double>(0, 0) = static_cast<double>(point_.x);
    mouse_img_cor.at<double>(1, 0) = static_cast<double>(point_.y);
    mouse_img_cor.at<double>(2, 0) = 1.0;
    double Z = depth.at<T>(point_.y, point_.x);
    mouse_left_cor = cv_in_left_inv * Z * mouse_img_cor;

    double x = mouse_left_cor.at<double>(0, 0);
    double y = mouse_left_cor.at<double>(1, 0);
    double z = mouse_left_cor.at<double>(2, 0);

    // If we have a pending click (selected_ flag), process it
    if (selected_ && click_count_ >= 1 && click_count_ <= 3) {
      cv::Point3d clicked_point(x, y, z);

      if (click_count_ == 1) {
        P1_cam_ = clicked_point;
        std::cout << "  P1 (Origin) camera coords: [" << std::fixed << std::setprecision(1)
                  << P1_cam_.x << ", " << P1_cam_.y << ", " << P1_cam_.z << "] mm" << std::endl;
      } else if (click_count_ == 2) {
        P2_cam_ = clicked_point;
        std::cout << "  P2 (X-axis) camera coords: [" << std::fixed << std::setprecision(1)
                  << P2_cam_.x << ", " << P2_cam_.y << ", " << P2_cam_.z << "] mm" << std::endl;
      } else if (click_count_ == 3) {
        P3_cam_ = clicked_point;
        std::cout << "  P3 (Z-axis) camera coords: [" << std::fixed << std::setprecision(1)
                  << P3_cam_.x << ", " << P3_cam_.y << ", " << P3_cam_.z << "] mm" << std::endl;
        // Build coordinate system after getting all 3 points
        BuildCoordinateSystem();
      }

      selected_ = false;  // Clear pending flag
    }

    // Display current cursor position
    std::ostringstream depth_pos_str;
    depth_pos_str << "Current depth pos: [" << point_.y << ", " << point_.x << "]";
    cv::putText(im, depth_pos_str.str(), cv::Point(10, y_offset),
                cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(100, 100, 100), 1);
    y_offset += line_height;

    std::ostringstream camera_pos_str;
    camera_pos_str << "Current camera pos: [" << std::fixed << std::setprecision(1)
                   << x << ", " << y << ", " << z << "] mm";
    cv::putText(im, camera_pos_str.str(), cv::Point(10, y_offset),
                cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(100, 100, 100), 1);
    y_offset += line_height + 10;

    // Display recorded points
    if (click_count_ >= 1) {
      cv::putText(im, "--- Recorded Points ---", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(0, 128, 255), 2);
      y_offset += line_height;

      std::ostringstream p1_str;
      p1_str << "P1 (Origin): [" << std::fixed << std::setprecision(1)
             << P1_cam_.x << ", " << P1_cam_.y << ", " << P1_cam_.z << "] mm";
      cv::putText(im, p1_str.str(), cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(0, 0, 255), 1);
      y_offset += line_height;
    }

    if (click_count_ >= 2) {
      std::ostringstream p2_str;
      p2_str << "P2 (X-axis): [" << std::fixed << std::setprecision(1)
             << P2_cam_.x << ", " << P2_cam_.y << ", " << P2_cam_.z << "] mm";
      cv::putText(im, p2_str.str(), cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(0, 128, 0), 1);
      y_offset += line_height;
    }

    if (click_count_ >= 3) {
      std::ostringstream p3_str;
      p3_str << "P3 (Z-axis): [" << std::fixed << std::setprecision(1)
             << P3_cam_.x << ", " << P3_cam_.y << ", " << P3_cam_.z << "] mm";
      cv::putText(im, p3_str.str(), cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(255, 0, 0), 1);
      y_offset += line_height + 10;
    }

    // Display coordinate system status
    if (coord_system_ready_) {
      cv::putText(im, "Coordinate System: READY", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(0, 200, 0), 2);
      y_offset += line_height;
    } else {
      std::ostringstream status_str;
      status_str << "Clicks recorded: " << click_count_ << " / 3";
      cv::putText(im, status_str.str(), cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(200, 0, 0), 1);
      y_offset += line_height;
    }

    cv::imshow("region", im);
  }

  /**
   * Build coordinate system from three points.
   * X-axis: P1 -> P2
   * Z-axis: P3 -> P1 (reversed, so it's actually P1 -> P3 with negation)
   * Y-axis: Z x X (right-hand rule)
   */
  void BuildCoordinateSystem() {
    // Calculate X-axis direction: P2 - P1
    cv::Point3d X_vec(P2_cam_.x - P1_cam_.x,
                      P2_cam_.y - P1_cam_.y,
                      P2_cam_.z - P1_cam_.z);
    double X_norm = std::sqrt(X_vec.x * X_vec.x + X_vec.y * X_vec.y + X_vec.z * X_vec.z);

    if (X_norm < 1e-6) {
      std::cout << "[ERROR] P1 and P2 are too close! Cannot establish X-axis." << std::endl;
      return;
    }

    // Normalize X-axis
    X_vec.x /= X_norm;
    X_vec.y /= X_norm;
    X_vec.z /= X_norm;

    // Calculate Z-axis direction: P1 - P3 (so -Z points from P1 to P3)
    cv::Point3d Z_vec(P1_cam_.x - P3_cam_.x,
                      P1_cam_.y - P3_cam_.y,
                      P1_cam_.z - P3_cam_.z);
    double Z_norm = std::sqrt(Z_vec.x * Z_vec.x + Z_vec.y * Z_vec.y + Z_vec.z * Z_vec.z);

    if (Z_norm < 1e-6) {
      std::cout << "[ERROR] P1 and P3 are too close! Cannot establish Z-axis." << std::endl;
      return;
    }

    // Normalize Z-axis
    Z_vec.x /= Z_norm;
    Z_vec.y /= Z_norm;
    Z_vec.z /= Z_norm;

    // Calculate Y-axis: Y = Z x X (right-hand rule)
    cv::Point3d Y_vec(Z_vec.y * X_vec.z - Z_vec.z * X_vec.y,
                      Z_vec.z * X_vec.x - Z_vec.x * X_vec.z,
                      Z_vec.x * X_vec.y - Z_vec.y * X_vec.x);
    double Y_norm = std::sqrt(Y_vec.x * Y_vec.x + Y_vec.y * Y_vec.y + Y_vec.z * Y_vec.z);

    if (Y_norm < 1e-6) {
      std::cout << "[ERROR] X and Z axes are parallel! Cannot establish Y-axis." << std::endl;
      return;
    }

    // Normalize Y-axis
    Y_vec.x /= Y_norm;
    Y_vec.y /= Y_norm;
    Y_vec.z /= Y_norm;

    // Build rotation matrix: R = [X_vec | Y_vec | Z_vec]
    // Each column is an axis vector
    rotation_matrix_.at<double>(0, 0) = X_vec.x;
    rotation_matrix_.at<double>(1, 0) = X_vec.y;
    rotation_matrix_.at<double>(2, 0) = X_vec.z;

    rotation_matrix_.at<double>(0, 1) = Y_vec.x;
    rotation_matrix_.at<double>(1, 1) = Y_vec.y;
    rotation_matrix_.at<double>(2, 1) = Y_vec.z;

    rotation_matrix_.at<double>(0, 2) = Z_vec.x;
    rotation_matrix_.at<double>(1, 2) = Z_vec.y;
    rotation_matrix_.at<double>(2, 2) = Z_vec.z;

    // Set origin
    origin_ = P1_cam_;

    coord_system_ready_ = true;

    std::cout << "\n[Coordinate System Established]" << std::endl;
    std::cout << "Origin (P1): [" << std::fixed << std::setprecision(1)
              << origin_.x << ", " << origin_.y << ", " << origin_.z << "] mm" << std::endl;
    std::cout << "X-axis (P1->P2): [" << X_vec.x << ", " << X_vec.y << ", " << X_vec.z << "]" << std::endl;
    std::cout << "Y-axis (ZxX): [" << Y_vec.x << ", " << Y_vec.y << ", " << Y_vec.z << "]" << std::endl;
    std::cout << "Z-axis (P3->P1): [" << Z_vec.x << ", " << Z_vec.y << ", " << Z_vec.z << "]" << std::endl;
  }

  /**
   * Transform a point from camera coordinate system to new coordinate system.
   * Returns cv::Point3d with transformed coordinates.
   */
  cv::Point3d TransformToNewFrame(const cv::Point3d &point_cam) const {
    if (!coord_system_ready_) {
      return cv::Point3d(0, 0, 0);
    }

    // Translate: relative to origin
    cv::Point3d relative(point_cam.x - origin_.x,
                        point_cam.y - origin_.y,
                        point_cam.z - origin_.z);

    // Rotate: new_coords = R^T * relative
    cv::Point3d new_coords;
    new_coords.x = rotation_matrix_.at<double>(0, 0) * relative.x +
                   rotation_matrix_.at<double>(1, 0) * relative.y +
                   rotation_matrix_.at<double>(2, 0) * relative.z;

    new_coords.y = rotation_matrix_.at<double>(0, 1) * relative.x +
                   rotation_matrix_.at<double>(1, 1) * relative.y +
                   rotation_matrix_.at<double>(2, 1) * relative.z;

    new_coords.z = rotation_matrix_.at<double>(0, 2) * relative.x +
                   rotation_matrix_.at<double>(1, 2) * relative.y +
                   rotation_matrix_.at<double>(2, 2) * relative.z;

    return new_coords;
  }

  void DrawRect(cv::Mat &image) { // NOLINT
    if (!show_)
      return;
    std::uint32_t n = (n_ > 1) ? n_ : 1;
    n += 1; // outside the region
    cv::rectangle(image, cv::Point(point_.x - n, point_.y - n),
                  cv::Point(point_.x + n, point_.y + n),
                  selected_ ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 1);
  }

  // Public getter for coordinate system status
  bool IsCoordSystemReady() const {
    return coord_system_ready_;
  }

private:
  std::uint32_t n_;
  bool show_;
  bool selected_;
  cv::Point point_;

  // Coordinate system construction variables
  int click_count_;                   // Number of clicks recorded (0-3)
  cv::Point3d P1_cam_, P2_cam_, P3_cam_;  // Three points in camera coordinate system
  cv::Mat rotation_matrix_;            // 3x3 rotation matrix from camera to new frame
  cv::Point3d origin_;                 // Origin of new coordinate system (P1)
  bool coord_system_ready_;            // Whether coordinate system is established
};

void OnDepthMouseCallback(int event, int x, int y, int flags, void *userdata) {
  DepthRegion *region = reinterpret_cast<DepthRegion *>(userdata);
  region->OnMouse(event, x, y, flags);
}

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

  // Get camera intrinsics and build intrinsic matrix for 3D calculation
  auto param = m_pSDK->GetModuleParams()._left_camera[RESOLUTION::RES_640X400];
  cv_in_left = cv::Mat::eye(3, 3, CV_64F);
  cv_in_left.at<double>(0, 0) = param._K[0];  // fx
  cv_in_left.at<double>(1, 1) = param._K[4];  // fy
  cv_in_left.at<double>(0, 2) = param._K[2];  // cx
  cv_in_left.at<double>(1, 2) = param._K[5];  // cy
  cv_in_left_inv = cv_in_left.inv();

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
  std::queue<cv::Mat> depth_queue;
  std::mutex mutex_image;
  std::mutex mutex_depth;

  int img_count = 0;
  int pose_count = 0;
  int depth_count = 0;
  int dropped_images = 0;
  int dropped_depth = 0;
  double last_img_time = -1.0;

  // Initialize DepthRegion for mouse interaction
  DepthRegion depth_region(3);
  auto depth_info = [](const cv::Mat &depth, const cv::Point &point,
                       const std::uint32_t &n, double X, double Y, double Z) {
    std::ostringstream os;
    os << "depth pos(" << n << "): [" << point.y << ", " << point.x << "]"
       << " camera pos: [" << X << ", " << Y << ", " << Z << "]"
       << ", unit: mm";
    return os.str();
  };

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

  // Register depth callback
  if (m_pSDK->EnableDepthProcessor()) {
    std::cout << "Depth processor enabled for mouse interaction." << std::endl;
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
    std::cout << "Warning: Failed to enable depth processor. Mouse depth interaction disabled." << std::endl;
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

  // FPS calculation
  auto loop_start = std::chrono::steady_clock::now();
  auto last_fps_time = loop_start;
  int fps_frame_count = 0;
  double current_fps = 0;

  std::cout << "=== Starting Detection ===\n" << std::endl;
  std::cout << "Waiting for camera images..." << std::endl;

  // Depth data persists across frames (updated when new data available)
  cv::Mat depth_data;

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

    // Get depth from queue
    {
      std::unique_lock<std::mutex> lock(mutex_depth);
      if (!depth_queue.empty()) {
        depth_data = depth_queue.front();
        clear(depth_queue);
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

      // Display 3D coordinates for keypoint 11 (left hip) if detected
      if (!depth_data.empty() && !poses.empty()) {
        // Check all detected persons for valid keypoint 11
        for (const auto& pose : poses) {
          if (pose.keypoints.size() > 11) {
            const auto& left_hip = pose.keypoints[11];

            // Only display if confidence > 0.6
            if (left_hip.confidence > 0.6f) {
              int px = static_cast<int>(left_hip.x);
              int py = static_cast<int>(left_hip.y);

              // Check if coordinates are within depth map bounds
              if (px >= 0 && px < depth_data.cols && py >= 0 && py < depth_data.rows) {
                // Get depth value (in mm)
                double Z = depth_data.at<ushort>(py, px);

                // Only calculate if depth is valid (< 10000 mm)
                if (Z > 0 && Z < 10000) {
                  // Calculate 3D coordinates in camera coordinate system
                  cv::Mat kp_img_cor(3, 1, CV_64FC1);
                  kp_img_cor.at<double>(0, 0) = static_cast<double>(px);
                  kp_img_cor.at<double>(1, 0) = static_cast<double>(py);
                  kp_img_cor.at<double>(2, 0) = 1.0;

                  cv::Mat kp_camera_cor = cv_in_left_inv * Z * kp_img_cor;

                  double X = kp_camera_cor.at<double>(0, 0);
                  double Y = kp_camera_cor.at<double>(1, 0);
                  double Z_val = kp_camera_cor.at<double>(2, 0);

                  // Display camera coordinate system coordinates
                  std::ostringstream coord_text;
                  coord_text << "Left Hip (KP11) Camera: [" << std::fixed << std::setprecision(1)
                            << X << ", " << Y << ", " << Z_val << "] mm";

                  // Position text below the keypoint
                  int text_x = px - 120;
                  int text_y = py + 30;

                  // Keep text within image bounds
                  if (text_x < 0) text_x = 5;
                  if (text_y > display.rows - 40) text_y = py - 40;

                  // Draw semi-transparent background for camera coords text
                  int baseline = 0;
                  cv::Size text_size = cv::getTextSize(coord_text.str(),
                                                       cv::FONT_HERSHEY_SIMPLEX,
                                                       0.5, 1, &baseline);

                  cv::rectangle(display,
                              cv::Point(text_x - 2, text_y - text_size.height - 2),
                              cv::Point(text_x + text_size.width + 2, text_y + baseline + 2),
                              cv::Scalar(0, 0, 0), cv::FILLED);

                  // Draw camera coords text in cyan color
                  cv::putText(display, coord_text.str(),
                            cv::Point(text_x, text_y),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(255, 255, 0), 1, cv::LINE_AA);

                  // If coordinate system is established, show transformed coordinates
                  if (depth_region.IsCoordSystemReady()) {
                    cv::Point3d hip_cam(X, Y, Z_val);
                    cv::Point3d hip_new = depth_region.TransformToNewFrame(hip_cam);

                    std::ostringstream new_coord_text;
                    new_coord_text << "Left Hip (KP11) New Frame: [" << std::fixed << std::setprecision(1)
                                  << hip_new.x << ", " << hip_new.y << ", " << hip_new.z << "] mm";

                    // Position second line of text below first line
                    int text_y2 = text_y + 22;
                    if (text_y2 > display.rows - 5) text_y2 = py - 10;

                    // Draw background for new coords text
                    cv::Size text_size2 = cv::getTextSize(new_coord_text.str(),
                                                         cv::FONT_HERSHEY_SIMPLEX,
                                                         0.5, 1, &baseline);

                    cv::rectangle(display,
                                cv::Point(text_x - 2, text_y2 - text_size2.height - 2),
                                cv::Point(text_x + text_size2.width + 2, text_y2 + baseline + 2),
                                cv::Scalar(0, 0, 0), cv::FILLED);

                    // Draw new coords text in green color
                    cv::putText(display, new_coord_text.str(),
                              cv::Point(text_x, text_y2),
                              cv::FONT_HERSHEY_SIMPLEX, 0.5,
                              cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
                  }
                }
              }
            }
          }
        }
      }

      // Draw depth region rectangle on YOLO Pose window
      if (!depth_data.empty()) {
        depth_region.DrawRect(display);
      }

      // Set mouse callback on YOLO Pose window for depth interaction
      cv::setMouseCallback("YOLO Pose - INDEMIND Left Camera", OnDepthMouseCallback, &depth_region);

      cv::imshow("YOLO Pose - INDEMIND Left Camera", display);
    }

    // Show depth region details when depth data is available
    if (!depth_data.empty()) {
      depth_region.ShowElems<ushort>(
          depth_data,
          [](const ushort &elem) {
            if (elem >= 10000) {
              return std::string("invalid");
            }
            return std::to_string(elem);
          },
          90, depth_info);
    } else {
      // Debug: print once per second if depth data is empty
      static auto last_debug = std::chrono::steady_clock::now();
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - last_debug).count() >= 2) {
        std::cout << "[DEBUG] No depth data available for region display. Depth count: "
                  << depth_count << std::endl;
        last_debug = now;
      }
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
  std::cout << "Total depth maps: " << depth_count << std::endl;
  std::cout << "Total pose detections: " << pose_count << std::endl;
  std::cout << "Dropped image frames: " << dropped_images << std::endl;
  std::cout << "Dropped depth frames: " << dropped_depth << std::endl;

  if (total_time > 0) {
    std::cout << "\nAverage rates:" << std::endl;
    std::cout << "  Image capture: " << (img_count / total_time) << " FPS" << std::endl;
    std::cout << "  Depth: " << (depth_count / total_time) << " FPS" << std::endl;
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
VERSION: 2.0 (YOLO Pose - INDEMIND Left Camera with 3D Coordinate Inspection)

DESCRIPTION:
  Real-time human pose detection using YOLOv8-pose model with INDEMIND
  left camera RGB images. Click on the image to inspect 3D coordinates
  of any region using depth data.

  Key features:
    ✓ Uses left camera image for pose detection (RGB)
    ✓ Click to inspect 3D coordinates of any region
    ✓ Real-time depth value display
    ✓ Camera coordinate calculation (X, Y, Z)

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
  ✓ Mouse click to inspect 3D coordinates
  ✓ Region depth value grid display

KEYBOARD CONTROLS:
  - 'q' or ESC : Quit the application
  - 'b' or 'B' : Toggle bounding box on/off
  - 'k' or 'K' : Toggle keypoints display on/off
  - 's' or 'S' : Toggle skeleton lines on/off
  - 'i' or 'I' : Toggle info overlay on/off
  - SPACE      : Save current frame to disk

MOUSE INTERACTION:
  On "YOLO Pose - INDEMIND Left Camera" window:
  - Move mouse: Preview depth values at cursor location
  - Left click: Lock the region for 3D coordinate inspection
  - Click again on selected region: Unlock/deselect

  "region" window shows (appears on click):
  - 7x7 grid of depth values (in mm) around selected point
  - Center point highlighted in red
  - Image coordinates [row, col]
  - 3D camera coordinates [X, Y, Z] in mm

WINDOWS DISPLAYED:
  1. "YOLO Pose - INDEMIND Left Camera": Main pose detection display
  2. "region": 3D coordinate details (appears when you click on the image)

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
