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
#include <fstream>
#include <deque>

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
   * Click 3: Defines Y-axis direction (P1 -> P3)
   * Z-axis is auto-generated using right-hand rule (Z = X x Y)
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

    // Create window for displaying coordinate information (larger size for curve)
    cv::Mat im(1000, 650, CV_8UC3, cv::Scalar(255, 255, 255));

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
        std::cout << "  P3 (Y-axis) camera coords: [" << std::fixed << std::setprecision(1)
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
      cv::putText(im, "=== Recorded Points ===", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(0, 128, 255), 2);
      y_offset += line_height;

      // Add instruction text
      cv::putText(im, "(Click 3 times: P1=Origin, P2=X-axis, P3=Y-axis)", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(100, 100, 100), 1);
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
      p3_str << "P3 (Y-axis): [" << std::fixed << std::setprecision(1)
             << P3_cam_.x << ", " << P3_cam_.y << ", " << P3_cam_.z << "] mm";
      cv::putText(im, p3_str.str(), cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(255, 0, 0), 1);
      y_offset += line_height + 10;
    }

    // Display coordinate system status
    if (coord_system_ready_) {
      cv::putText(im, "Coordinate System: READY", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(0, 200, 0), 2);
      y_offset += line_height;
    } else {
      std::ostringstream status_str;
      status_str << "Clicks: " << click_count_ << " / 3";
      cv::putText(im, status_str.str(), cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(200, 0, 0), 1);
      y_offset += line_height;
    }

    // Display current detection parameters
    y_offset += 10;
    cv::putText(im, "=== Detection Parameters ===", cv::Point(10, y_offset),
                cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(128, 0, 128), 2);
    y_offset += line_height;

    std::ostringstream param_str1;
    param_str1 << "Z Threshold: " << std::fixed << std::setprecision(0) << noise_threshold_ << " mm  (+/-)";
    cv::putText(im, param_str1.str(), cv::Point(10, y_offset),
                cv::FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(80, 80, 80), 1);
    y_offset += line_height;

    std::ostringstream param_str2;
    param_str2 << "Window: " << window_half_ << " frames (" << (2 * window_half_ + 1) << " total)  ([/])";
    cv::putText(im, param_str2.str(), cv::Point(10, y_offset),
                cv::FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(80, 80, 80), 1);
    y_offset += line_height;

    // Display hip coordinates for all detected persons
    if (!hip_data_.empty()) {
      y_offset += 10;
      cv::putText(im, "=== Hip Coordinates ===", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(0, 128, 255), 2);
      y_offset += line_height;

      for (const auto& hip : hip_data_) {
        // Display person ID and camera coordinates
        std::ostringstream hip_cam_str;
        hip_cam_str << "Person " << hip.person_id << " (Camera): ["
                    << std::fixed << std::setprecision(1)
                    << hip.camera_pos.x << ", " << hip.camera_pos.y << ", "
                    << hip.camera_pos.z << "] mm";
        cv::putText(im, hip_cam_str.str(), cv::Point(10, y_offset),
                    cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(0, 0, 0), 2);
        y_offset += line_height + 5;

        // Display new frame coordinates if available
        if (hip.has_new_frame) {
          std::ostringstream hip_new_str;
          hip_new_str << "Person " << hip.person_id << " (New Frame): ["
                      << std::fixed << std::setprecision(1)
                      << hip.new_frame_pos.x << ", " << hip.new_frame_pos.y << ", "
                      << hip.new_frame_pos.z << "] mm";
          cv::putText(im, hip_new_str.str(), cv::Point(10, y_offset),
                      cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(0, 0, 0), 2);
          y_offset += line_height + 5;
        }
      }
    }

    // Display landing points (极低点落点信息)
    if (!landing_points_.empty()) {
      y_offset += 15;
      cv::putText(im, "=== Landing Points ===", cv::Point(10, y_offset),
                  cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(255, 0, 128), 2);
      y_offset += line_height;

      // Show only the last 5 landing points to avoid overcrowding
      size_t start_idx = (landing_points_.size() > 5) ? landing_points_.size() - 5 : 0;
      for (size_t i = start_idx; i < landing_points_.size(); i++) {
        const auto& lp = landing_points_[i];

        std::ostringstream lp_str;
        lp_str << "landing point" << lp.landing_id << ": "
               << lp.time_minutes << "min " << lp.time_seconds << "s "
               << "X=" << std::fixed << std::setprecision(1) << lp.new_frame_x
               << " Y=" << lp.new_frame_y << " mm";
        cv::putText(im, lp_str.str(), cv::Point(10, y_offset),
                    cv::FONT_HERSHEY_PLAIN, 1.3, cv::Scalar(200, 0, 100), 2);
        y_offset += line_height;
      }

      // Show total count if more than displayed
      if (landing_points_.size() > 5) {
        std::ostringstream total_str;
        total_str << "(Total landing points: " << landing_points_.size() << ")";
        cv::putText(im, total_str.str(), cv::Point(10, y_offset),
                    cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(100, 100, 100), 1);
        y_offset += line_height;
      }
    }

    // Draw fluctuation curve at the bottom
    if (!hip_data_.empty()) {
      y_offset += 20;
      DrawFluctuationCurve(im, y_offset);
    }

    cv::imshow("region", im);
  }

  /**
   * Build coordinate system from three points.
   * X-axis: P1 -> P2
   * Y-axis: P1 -> P3
   * Z-axis: X x Y (right-hand rule)
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

    // Calculate Y-axis direction: P3 - P1
    cv::Point3d Y_vec(P3_cam_.x - P1_cam_.x,
                      P3_cam_.y - P1_cam_.y,
                      P3_cam_.z - P1_cam_.z);
    double Y_norm = std::sqrt(Y_vec.x * Y_vec.x + Y_vec.y * Y_vec.y + Y_vec.z * Y_vec.z);

    if (Y_norm < 1e-6) {
      std::cout << "[ERROR] P1 and P3 are too close! Cannot establish Y-axis." << std::endl;
      return;
    }

    // Normalize Y-axis
    Y_vec.x /= Y_norm;
    Y_vec.y /= Y_norm;
    Y_vec.z /= Y_norm;

    // Calculate Z-axis: Z = X x Y (right-hand rule)
    cv::Point3d Z_vec(X_vec.y * Y_vec.z - X_vec.z * Y_vec.y,
                      X_vec.z * Y_vec.x - X_vec.x * Y_vec.z,
                      X_vec.x * Y_vec.y - X_vec.y * Y_vec.x);
    double Z_norm = std::sqrt(Z_vec.x * Z_vec.x + Z_vec.y * Z_vec.y + Z_vec.z * Z_vec.z);

    if (Z_norm < 1e-6) {
      std::cout << "[ERROR] X and Y axes are parallel! Cannot establish Z-axis." << std::endl;
      return;
    }

    // Normalize Z-axis
    Z_vec.x /= Z_norm;
    Z_vec.y /= Z_norm;
    Z_vec.z /= Z_norm;

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
    std::cout << "Y-axis (P1->P3): [" << Y_vec.x << ", " << Y_vec.y << ", " << Y_vec.z << "]" << std::endl;
    std::cout << "Z-axis (XxY): [" << Z_vec.x << ", " << Z_vec.y << ", " << Z_vec.z << "]" << std::endl;
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

  // Draw coordinate system axes on RGB image
  void DrawCoordinateSystem(cv::Mat &image, const cv::Mat &camera_matrix_inv) {
    if (!coord_system_ready_) {
      return;
    }

    // Define axis length in mm (e.g., 200mm = 20cm)
    const double axis_length = 200.0;

    // Calculate 3D positions of axis endpoints
    cv::Point3d X_end(origin_.x + rotation_matrix_.at<double>(0, 0) * axis_length,
                      origin_.y + rotation_matrix_.at<double>(1, 0) * axis_length,
                      origin_.z + rotation_matrix_.at<double>(2, 0) * axis_length);

    cv::Point3d Y_end(origin_.x + rotation_matrix_.at<double>(0, 1) * axis_length,
                      origin_.y + rotation_matrix_.at<double>(1, 1) * axis_length,
                      origin_.z + rotation_matrix_.at<double>(2, 1) * axis_length);

    cv::Point3d Z_end(origin_.x + rotation_matrix_.at<double>(0, 2) * axis_length,
                      origin_.y + rotation_matrix_.at<double>(1, 2) * axis_length,
                      origin_.z + rotation_matrix_.at<double>(2, 2) * axis_length);

    // Project 3D points to 2D image coordinates
    // Formula: [u, v, 1]^T = (1/Z) * K * [X, Y, Z]^T
    auto project3Dto2D = [&camera_matrix_inv](const cv::Point3d &point_3d) -> cv::Point {
      if (point_3d.z <= 0) {
        return cv::Point(-1, -1);  // Invalid projection
      }

      // Get camera matrix (inverse of camera_matrix_inv)
      cv::Mat K = camera_matrix_inv.inv();

      // Create 3D point vector
      cv::Mat pt_3d = (cv::Mat_<double>(3, 1) << point_3d.x, point_3d.y, point_3d.z);

      // Project: [u, v, 1]^T = (1/Z) * K * [X, Y, Z]^T
      cv::Mat pt_2d = K * pt_3d / point_3d.z;

      return cv::Point(static_cast<int>(pt_2d.at<double>(0, 0)),
                      static_cast<int>(pt_2d.at<double>(1, 0)));
    };

    cv::Point origin_2d = project3Dto2D(origin_);
    cv::Point X_end_2d = project3Dto2D(X_end);
    cv::Point Y_end_2d = project3Dto2D(Y_end);
    cv::Point Z_end_2d = project3Dto2D(Z_end);

    // Check if projections are valid and within image bounds
    if (origin_2d.x < 0 || origin_2d.y < 0 || origin_2d.x >= image.cols || origin_2d.y >= image.rows) {
      return;
    }

    // Draw origin as a filled circle
    cv::circle(image, origin_2d, 8, cv::Scalar(255, 255, 255), -1);  // White center
    cv::circle(image, origin_2d, 10, cv::Scalar(0, 0, 0), 2);         // Black outline

    // Draw X-axis (Red)
    if (X_end_2d.x >= 0 && X_end_2d.y >= 0 && X_end_2d.x < image.cols && X_end_2d.y < image.rows) {
      cv::arrowedLine(image, origin_2d, X_end_2d, cv::Scalar(0, 0, 255), 3, cv::LINE_AA, 0, 0.2);
      cv::putText(image, "X", X_end_2d + cv::Point(10, 10),
                  cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    }

    // Draw Y-axis (Green)
    if (Y_end_2d.x >= 0 && Y_end_2d.y >= 0 && Y_end_2d.x < image.cols && Y_end_2d.y < image.rows) {
      cv::arrowedLine(image, origin_2d, Y_end_2d, cv::Scalar(0, 255, 0), 3, cv::LINE_AA, 0, 0.2);
      cv::putText(image, "Y", Y_end_2d + cv::Point(10, 10),
                  cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    }

    // Draw Z-axis (Blue)
    if (Z_end_2d.x >= 0 && Z_end_2d.y >= 0 && Z_end_2d.x < image.cols && Z_end_2d.y < image.rows) {
      cv::arrowedLine(image, origin_2d, Z_end_2d, cv::Scalar(255, 0, 0), 3, cv::LINE_AA, 0, 0.2);
      cv::putText(image, "Z", Z_end_2d + cv::Point(10, 10),
                  cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
    }
  }

  // Public getter for coordinate system status
  bool IsCoordSystemReady() const {
    return coord_system_ready_;
  }

  // Structure to store hip information for one person
  struct HipInfo {
    int person_id;
    cv::Point3d camera_pos;
    cv::Point3d new_frame_pos;
    bool has_new_frame;
  };

  // Structure to store landing point information
  struct LandingPoint {
    int landing_id;           // 落点编号 (从1开始)
    int time_minutes;         // 时间-分钟部分
    int time_seconds;         // 时间-秒部分
    double new_frame_x;       // 新坐标系X坐标
    double new_frame_y;       // 新坐标系Y坐标
    double new_frame_z;       // 新坐标系Z坐标（极低点值）
  };

  // Structure to store frame data for weighted averaging
  struct FrameData {
    double x;                 // 新坐标系X坐标
    double y;                 // 新坐标系Y坐标
    double z;                 // 新坐标系Z坐标
    std::chrono::steady_clock::time_point timestamp;  // 帧时间戳
  };

  // Update hip data for all detected persons
  void UpdateHipData(const std::vector<HipInfo> &hip_data) {
    hip_data_ = hip_data;

    // Initialize start time on first call
    if (!start_time_initialized_) {
      start_time_ = std::chrono::steady_clock::now();
      start_time_initialized_ = true;
    }

    // Update history for curve plotting (track first person's Z coordinate)
    if (!hip_data.empty()) {
      z_history_.push_back(hip_data[0].camera_pos.z);
      if (z_history_.size() > max_history_size_) {
        z_history_.pop_front();
      }

      // Check for landing point in new coordinate system Z
      if (hip_data[0].has_new_frame) {
        CheckLandingPoint(hip_data[0]);
      }
    }
  }

  /**
   * Check if the current frame represents a landing point (local minimum of new frame Z).
   * Uses delayed confirmation and Z-weighted averaging for improved accuracy.
   *
   * Algorithm:
   * 1. Store frame data in a circular buffer
   * 2. Detect potential minimum when Z transitions from descending to ascending
   * 3. Wait for CONFIRM_FRAMES to confirm the minimum
   * 4. Calculate weighted average of X, Y coordinates (weight based on Z proximity to minimum)
   */
  void CheckLandingPoint(const HipInfo &hip) {
    double current_z = hip.new_frame_pos.z;
    auto now = std::chrono::steady_clock::now();

    // Add current frame to buffer
    FrameData fd;
    fd.x = hip.new_frame_pos.x;
    fd.y = hip.new_frame_pos.y;
    fd.z = hip.new_frame_pos.z;
    fd.timestamp = now;
    frame_buffer_.push_back(fd);

    // Keep buffer size limited
    while (frame_buffer_.size() > BUFFER_SIZE) {
      frame_buffer_.pop_front();
      // Adjust pending index if we removed elements
      if (has_pending_minimum_ && pending_min_index_ > 0) {
        pending_min_index_--;
      } else if (has_pending_minimum_ && pending_min_index_ <= 0) {
        // Lost the pending minimum due to buffer overflow
        has_pending_minimum_ = false;
        pending_min_index_ = -1;
        frames_since_minimum_ = 0;
      }
    }

    // Need at least 2 frames to detect trend
    if (frame_buffer_.size() < 2) {
      last_new_z_ = current_z;
      return;
    }

    // Use member variable for noise threshold (adjustable at runtime)
    double noise_threshold = noise_threshold_;

    // Check if we have a pending minimum to confirm
    if (has_pending_minimum_) {
      frames_since_minimum_++;

      // Check if current Z is still higher than the pending minimum (confirming upward trend)
      double min_z = frame_buffer_[pending_min_index_].z;
      bool still_ascending = (current_z > min_z + noise_threshold * 0.5);

      if (still_ascending && frames_since_minimum_ >= static_cast<int>(CONFIRM_FRAMES)) {
        // Confirmed! Calculate weighted average and record landing point
        ConfirmLandingPoint();
        has_pending_minimum_ = false;
        pending_min_index_ = -1;
        frames_since_minimum_ = 0;
        was_descending_ = false;
      } else if (!still_ascending && current_z < min_z) {
        // Found a new lower point, update pending minimum
        pending_min_index_ = static_cast<int>(frame_buffer_.size()) - 1;
        frames_since_minimum_ = 0;
      } else if (frames_since_minimum_ > static_cast<int>(BUFFER_SIZE)) {
        // Timeout, cancel pending minimum
        has_pending_minimum_ = false;
        pending_min_index_ = -1;
        frames_since_minimum_ = 0;
      }
    } else {
      // Look for a new potential minimum
      bool significant_descent = (last_new_z_ - current_z) > noise_threshold;
      bool is_ascending = (current_z > last_new_z_);

      if (was_descending_ && is_ascending && (current_z - last_new_z_) > noise_threshold * 0.5) {
        // Potential minimum detected at previous frame
        has_pending_minimum_ = true;
        pending_min_index_ = static_cast<int>(frame_buffer_.size()) - 2;  // Previous frame
        if (pending_min_index_ < 0) pending_min_index_ = 0;
        frames_since_minimum_ = 1;
      }

      // Update descending flag
      if (significant_descent) {
        was_descending_ = true;
      }
    }

    last_new_z_ = current_z;
  }

  /**
   * Confirm the landing point and calculate weighted average coordinates.
   * Weight formula: w_i = 1 / (|Z_i - Z_min| + epsilon)
   * This gives higher weight to frames with Z closer to the minimum.
   */
  void ConfirmLandingPoint() {
    if (pending_min_index_ < 0 || pending_min_index_ >= static_cast<int>(frame_buffer_.size())) {
      return;
    }

    // Find the actual minimum Z in the buffer (in case of noise)
    double min_z = frame_buffer_[pending_min_index_].z;
    int actual_min_idx = pending_min_index_;

    // Search nearby frames for the actual minimum
    int search_start = std::max(0, pending_min_index_ - 3);
    int search_end = std::min(static_cast<int>(frame_buffer_.size()) - 1, pending_min_index_ + 3);

    for (int i = search_start; i <= search_end; i++) {
      if (frame_buffer_[i].z < min_z) {
        min_z = frame_buffer_[i].z;
        actual_min_idx = i;
      }
    }

    // Define the window for weighted averaging (use member variable)
    int window_start = std::max(0, actual_min_idx - window_half_);
    int window_end = std::min(static_cast<int>(frame_buffer_.size()) - 1, actual_min_idx + window_half_);

    // Calculate weighted average
    constexpr double epsilon = 1.0;  // Prevent division by zero
    double weight_sum = 0.0;
    double weighted_x = 0.0;
    double weighted_y = 0.0;

    for (int i = window_start; i <= window_end; i++) {
      double z_diff = std::abs(frame_buffer_[i].z - min_z);
      double weight = 1.0 / (z_diff + epsilon);

      weighted_x += weight * frame_buffer_[i].x;
      weighted_y += weight * frame_buffer_[i].y;
      weight_sum += weight;
    }

    // Calculate final coordinates
    double final_x = weighted_x / weight_sum;
    double final_y = weighted_y / weight_sum;

    // Calculate time since start
    auto min_timestamp = frame_buffer_[actual_min_idx].timestamp;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        min_timestamp - start_time_).count();
    int total_seconds = static_cast<int>(elapsed_ms / 1000);
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;

    // Create landing point record
    landing_count_++;
    LandingPoint lp;
    lp.landing_id = landing_count_;
    lp.time_minutes = minutes;
    lp.time_seconds = seconds;
    lp.new_frame_x = final_x;
    lp.new_frame_y = final_y;
    lp.new_frame_z = min_z;

    landing_points_.push_back(lp);

    // Output to console with detailed info
    std::cout << "\n========================================" << std::endl;
    std::cout << "落点" << lp.landing_id << " (加权平均)：" << std::endl;
    std::cout << "  时间: " << lp.time_minutes << "分" << lp.time_seconds << "秒" << std::endl;
    std::cout << "  新坐标系 X: " << std::fixed << std::setprecision(1) << lp.new_frame_x << " mm" << std::endl;
    std::cout << "  新坐标系 Y: " << std::fixed << std::setprecision(1) << lp.new_frame_y << " mm" << std::endl;
    std::cout << "  新坐标系 Z (极低点): " << std::fixed << std::setprecision(1) << lp.new_frame_z << " mm" << std::endl;
    std::cout << "  采样窗口: " << (window_end - window_start + 1) << " 帧" << std::endl;
    std::cout << "========================================\n" << std::endl;
  }

  // Get landing points for display
  const std::vector<LandingPoint>& GetLandingPoints() const {
    return landing_points_;
  }

  // Parameter adjustment methods
  void IncreaseNoiseThreshold(double delta = 10.0) {
    noise_threshold_ += delta;
    if (noise_threshold_ > 500.0) noise_threshold_ = 500.0;
    std::cout << "[参数调整] Z阈值: " << noise_threshold_ << " mm" << std::endl;
  }

  void DecreaseNoiseThreshold(double delta = 10.0) {
    noise_threshold_ -= delta;
    if (noise_threshold_ < 10.0) noise_threshold_ = 10.0;
    std::cout << "[参数调整] Z阈值: " << noise_threshold_ << " mm" << std::endl;
  }

  void IncreaseWindowHalf(int delta = 1) {
    window_half_ += delta;
    if (window_half_ > 7) window_half_ = 7;
    std::cout << "[参数调整] 窗口半径: " << window_half_ << " 帧 (共" << (2 * window_half_ + 1) << "帧)" << std::endl;
  }

  void DecreaseWindowHalf(int delta = 1) {
    window_half_ -= delta;
    if (window_half_ < 1) window_half_ = 1;
    std::cout << "[参数调整] 窗口半径: " << window_half_ << " 帧 (共" << (2 * window_half_ + 1) << "帧)" << std::endl;
  }

  void PrintParameters() const {
    std::cout << "\n=== 当前落点检测参数 ===" << std::endl;
    std::cout << "  Z阈值: " << noise_threshold_ << " mm" << std::endl;
    std::cout << "  窗口半径: " << window_half_ << " 帧 (共" << (2 * window_half_ + 1) << "帧参与加权平均)" << std::endl;
    std::cout << "========================\n" << std::endl;
  }

  double GetNoiseThreshold() const { return noise_threshold_; }
  int GetWindowHalf() const { return window_half_; }

  // Draw Z-coordinate fluctuation curve
  void DrawFluctuationCurve(cv::Mat &im, int start_y) {
    if (z_history_.size() < 2) {
      return;
    }

    const int graph_height = 150;
    const int graph_width = 600;
    const int margin_left = 10;
    const int margin_bottom = 20;

    // Draw background
    cv::rectangle(im,
                  cv::Point(margin_left, start_y),
                  cv::Point(margin_left + graph_width, start_y + graph_height),
                  cv::Scalar(240, 240, 240), -1);

    // Find min and max for scaling
    double min_z = *std::min_element(z_history_.begin(), z_history_.end());
    double max_z = *std::max_element(z_history_.begin(), z_history_.end());
    double range = max_z - min_z;
    if (range < 10) range = 10;  // Minimum range for visibility

    // Draw curve
    for (size_t i = 1; i < z_history_.size(); i++) {
      double z1 = z_history_[i - 1];
      double z2 = z_history_[i];

      int x1 = margin_left + (i - 1) * graph_width / max_history_size_;
      int x2 = margin_left + i * graph_width / max_history_size_;

      int y1 = start_y + graph_height - margin_bottom -
               ((z1 - min_z) / range) * (graph_height - margin_bottom - 10);
      int y2 = start_y + graph_height - margin_bottom -
               ((z2 - min_z) / range) * (graph_height - margin_bottom - 10);

      cv::line(im, cv::Point(x1, y1), cv::Point(x2, y2),
               cv::Scalar(0, 0, 255), 2);  // Red curve
    }

    // Draw axes
    cv::line(im,
             cv::Point(margin_left, start_y + graph_height - margin_bottom),
             cv::Point(margin_left + graph_width, start_y + graph_height - margin_bottom),
             cv::Scalar(0, 0, 0), 2);  // X axis

    // Draw labels
    std::ostringstream title_str;
    title_str << "Z-coordinate Fluctuation (Last " << max_history_size_ << " frames)";
    cv::putText(im, title_str.str(),
                cv::Point(margin_left, start_y - 5),
                cv::FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(0, 0, 0), 1);

    std::ostringstream range_str;
    range_str << "Range: " << std::fixed << std::setprecision(1)
              << min_z << " - " << max_z << " mm (Variation: "
              << (max_z - min_z) << " mm)";
    cv::putText(im, range_str.str(),
                cv::Point(margin_left, start_y + graph_height + 15),
                cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(100, 100, 100), 1);
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

  // Hip data for all detected persons
  std::vector<HipInfo> hip_data_;

  // History data for fluctuation curve
  std::deque<double> z_history_;
  const size_t max_history_size_ = 100;  // Store last 100 frames

  // Landing point detection variables (for new frame Z coordinate)
  std::deque<double> new_z_history_;              // 新坐标系Z轴历史数据（用于极低点检测）
  std::vector<LandingPoint> landing_points_;      // 已检测到的落点列表
  int landing_count_ = 0;                         // 落点计数器
  bool was_descending_ = false;                   // 上一帧是否处于下降趋势
  double last_new_z_ = 0.0;                       // 上一帧的新坐标系Z值
  std::chrono::steady_clock::time_point start_time_;  // 程序启动时间
  bool start_time_initialized_ = false;           // 是否已初始化启动时间

  // Delayed confirmation and weighted averaging variables
  std::deque<FrameData> frame_buffer_;            // 帧数据缓冲区（存储最近N帧）
  static constexpr size_t BUFFER_SIZE = 15;       // 缓冲区大小（前后各7帧 + 1个极低点）
  static constexpr size_t CONFIRM_FRAMES = 5;     // 确认所需的上升帧数
  int pending_min_index_ = -1;                    // 待确认的极低点在缓冲区中的索引
  bool has_pending_minimum_ = false;              // 是否有待确认的极低点
  int frames_since_minimum_ = 0;                  // 自极低点以来的帧数

  // Adjustable parameters (can be changed at runtime)
  double noise_threshold_ = 150.0;                // Z坐标变化阈值 (mm)
  int window_half_ = 3;                           // 加权平均窗口半径 (帧数)
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
  bool init_result = m_pSDK->Init(config);
  if (!init_result) {
    std::cerr << "ERROR: Failed to initialize INDEMIND SDK!" << std::endl;
    delete m_pSDK;
    return -1;
  }

  // Get camera intrinsics and build intrinsic matrix for 3D calculation
  auto module_params = m_pSDK->GetModuleParams();
  auto param = module_params._left_camera[RESOLUTION::RES_640X400];

  // Debug: print raw parameter values
  std::cout << "\nDEBUG: Raw camera parameters:" << std::endl;
  std::cout << "  K[0]=" << param._K[0] << ", K[4]=" << param._K[4] << std::endl;
  std::cout << "  K[2]=" << param._K[2] << ", K[5]=" << param._K[5] << std::endl;
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

  // Initialize YOLO Pose Detector with CUDA
  std::cout << "\nModel: " << model_path << std::endl;
  YOLOPoseDetector pose_detector(model_path, 640, 0.5f, 0.45f, true);  // true = 启用 CUDA
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

  // Data recording control
  bool recording_data = false;
  std::ofstream csv_file;
  int frame_count = 0;

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
  std::cout << "  l       : Toggle data recording (Start/Stop)" << std::endl;
  std::cout << "  SPACE   : Save current frame" << std::endl;
  std::cout << "  + / -   : Increase/Decrease Z threshold" << std::endl;
  std::cout << "  [ / ]   : Decrease/Increase window radius" << std::endl;
  std::cout << "  p       : Print current parameters\n" << std::endl;

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

      // Display recording status
      if (recording_data) {
        cv::putText(display, "[REC]", cv::Point(display.cols - 80, 50),
                    FONT_FACE, 1.5, cv::Scalar(0, 0, 255), 2);
      }

      // Calculate hip coordinates for all detected persons and pass to region window
      std::vector<DepthRegion::HipInfo> hip_data_list;
      if (!depth_data.empty() && !poses.empty()) {
        int person_id = 1;
        for (const auto& pose : poses) {
          if (pose.keypoints.size() > 11) {
            const auto& left_hip = pose.keypoints[11];

            if (left_hip.confidence > 0.6f) {
              int px = static_cast<int>(left_hip.x);
              int py = static_cast<int>(left_hip.y);

              if (px >= 0 && px < depth_data.cols && py >= 0 && py < depth_data.rows) {
                double Z = depth_data.at<ushort>(py, px);

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

                  // Create hip info
                  DepthRegion::HipInfo hip_info;
                  hip_info.person_id = person_id;
                  hip_info.camera_pos = cv::Point3d(X, Y, Z_val);
                  hip_info.has_new_frame = depth_region.IsCoordSystemReady();

                  if (hip_info.has_new_frame) {
                    hip_info.new_frame_pos = depth_region.TransformToNewFrame(hip_info.camera_pos);
                  }

                  hip_data_list.push_back(hip_info);
                }
              }
            }
          }
          person_id++;
        }
      }

      // Update hip data in region window
      depth_region.UpdateHipData(hip_data_list);

      // Record data to CSV if recording is enabled
      if (recording_data && !hip_data_list.empty()) {
        auto current_time = std::chrono::steady_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - loop_start).count();

        for (const auto& hip : hip_data_list) {
          csv_file << frame_count << ","
                   << timestamp << ","
                   << hip.person_id << ","
                   << std::fixed << std::setprecision(2)
                   << hip.camera_pos.x << ","
                   << hip.camera_pos.y << ","
                   << hip.camera_pos.z;

          if (hip.has_new_frame) {
            csv_file << ","
                     << hip.new_frame_pos.x << ","
                     << hip.new_frame_pos.y << ","
                     << hip.new_frame_pos.z;
          } else {
            csv_file << ",,,";
          }

          csv_file << "\n";
        }
        frame_count++;
      }

      // Draw coordinate system axes on RGB image
      depth_region.DrawCoordinateSystem(display, cv_in_left_inv);

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
    } else if (key == 'l' || key == 'L') {
      // Toggle data recording
      if (!recording_data) {
        // Start recording
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream filename;
        filename << "hip_coords_" << std::put_time(std::localtime(&now_time), "%Y%m%d_%H%M%S") << ".csv";

        csv_file.open(filename.str());
        if (csv_file.is_open()) {
          // Write CSV header
          csv_file << "frame,timestamp_ms,person_id,"
                   << "cam_x,cam_y,cam_z,"
                   << "new_x,new_y,new_z\n";
          recording_data = true;
          frame_count = 0;
          std::cout << "Recording started: " << filename.str() << std::endl;
        } else {
          std::cerr << "Failed to open file: " << filename.str() << std::endl;
        }
      } else {
        // Stop recording
        csv_file.close();
        recording_data = false;
        std::cout << "Recording stopped. Total frames: " << frame_count << std::endl;
      }
    } else if (key == ' ') {
      // Save frame
      if (!left_image.empty()) {
        std::ostringstream filename;
        filename << "pose_frame_" << std::setfill('0') << std::setw(4)
                 << frame_save_count++ << ".jpg";
        cv::imwrite(filename.str(), left_image);
        std::cout << "Saved: " << filename.str() << std::endl;
      }
    } else if (key == '+' || key == '=') {
      // Increase Z threshold
      depth_region.IncreaseNoiseThreshold();
    } else if (key == '-' || key == '_') {
      // Decrease Z threshold
      depth_region.DecreaseNoiseThreshold();
    } else if (key == ']' || key == '}') {
      // Increase window half
      depth_region.IncreaseWindowHalf();
    } else if (key == '[' || key == '{') {
      // Decrease window half
      depth_region.DecreaseWindowHalf();
    } else if (key == 'p' || key == 'P') {
      // Print current parameters
      depth_region.PrintParameters();
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
