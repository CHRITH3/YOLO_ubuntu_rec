#ifndef DEPTH_REGION_H_
#define DEPTH_REGION_H_

#include "camera_intrinsics.h"
#include "depth_utils.h"
#include "runtime_state.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
    // double Z = depth.at<T>(point_.y, point_.x);
        // Use robust depth for cursor readout to reduce jitter/noise
        uint16_t z_mm = 0;
        if (!RobustDepthMedianU16(depth, point_.x, point_.y, /*r=*/3, z_mm)) {
            // If invalid, skip drawing numeric 3D info for this cursor location
            return;
          }
        double Z = static_cast<double>(z_mm);

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
    int64_t t_ms_since_start; // 精确时间戳（毫秒，从程序启动开始）
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

    // Simple adaptive EMA filter for 3D points
  struct EMA3 {
        bool init = false;
        cv::Point3d v{0, 0, 0};
        double fc = 6.0;  // cutoff frequency (Hz)
        cv::Point3d Step(const cv::Point3d& x, double dt_sec) {
            if (!init) {
                v = x;
                init = true;
                return v;
              }
            if (dt_sec <= 0) dt_sec = 1.0 / 50.0;
            double a = 1.0 - std::exp(-2.0 * M_PI * fc * dt_sec);
            v = v + a * (x - v);
            return v;
          }
        void Reset() { init = false; v = cv::Point3d(0, 0, 0); }
      };

    void ResetLandingState(bool reset_filter = false) {
        was_descending_ = false;
        has_pending_minimum_ = false;
        pending_min_index_ = -1;
        frames_since_minimum_ = 0;
        frame_buffer_.clear();
        last_new_z_ = 0.0;
        if (reset_filter) {
            ema_new_frame_.Reset();
            filter_time_initialized_ = false;
          }
      }

    // Pick a stable target (avoid poses ordering jitter): choose hip closest to last tracked cam pos
    int PickTrackedHipIndex(const std::vector<HipInfo>& hips) {
        if (hips.empty()) return -1;
        int best = 0;
        if (!has_last_track_) {
            return 0;
          }
        double best_d2 = std::numeric_limits<double>::infinity();
        for (int i = 0; i < (int)hips.size(); ++i) {
            const auto& p = hips[i].camera_pos;
            double dx = p.x - last_track_cam_pos_.x;
            double dy = p.y - last_track_cam_pos_.y;
            double dz = p.z - last_track_cam_pos_.z;
            double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = i;
              }
          }
        return best;
      }


  // Update hip data for all detected persons
  void UpdateHipData(const std::vector<HipInfo> &hip_data) {
    hip_data_ = hip_data;

    // Initialize start time on first call
    if (!start_time_initialized_) {
      start_time_ = std::chrono::steady_clock::now();
      start_time_initialized_ = true;
    }

    // // Update history for curve plotting (track first person's Z coordinate)
    // if (!hip_data.empty()) {
    //   z_history_.push_back(hip_data[0].camera_pos.z);
    //   if (z_history_.size() > max_history_size_) {
    //     z_history_.pop_front();
    //   }
    //
    //   // Check for landing point in new coordinate system Z
    //   if (hip_data[0].has_new_frame) {
    //     CheckLandingPoint(hip_data[0]);
    //   }
    // }
      // Missing frames handling: if we lose hip for several frames, reset trend state to avoid false triggers
      //丢帧复位
          if (hip_data.empty()) {
              missing_frames_++;
              if (missing_frames_ >= kMissingResetFrames) {
                  ResetLandingState(/*reset_filter=*/true);
                  missing_frames_ = kMissingResetFrames; // clamp
                  has_last_track_ = false;
                }
              return;
            }
          missing_frames_ = 0;

          // Choose stable target hip (avoid ordering jitter)
          int idx = PickTrackedHipIndex(hip_data);
          if (idx < 0) return;

          HipInfo tracked = hip_data[idx];
          last_track_cam_pos_ = tracked.camera_pos;
          has_last_track_ = true;

          // Filter new-frame 3D to reduce jitter/noise (Z axis is upward-positive)
          if (tracked.has_new_frame) {
              auto now = std::chrono::steady_clock::now();
              double dt = 1.0 / 50.0;
              if (!filter_time_initialized_) {
                  last_filter_time_ = now;
                  filter_time_initialized_ = true;
                } else {
                    dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_filter_time_).count();
                    last_filter_time_ = now;
                  }
              tracked.new_frame_pos = ema_new_frame_.Step(tracked.new_frame_pos, dt);
            }

          // Update history for curve plotting
          // Prefer filtered new-frame Z when available; otherwise use camera Z.
          if (tracked.has_new_frame) {
              z_history_.push_back(tracked.new_frame_pos.z);
            } else {
                z_history_.push_back(tracked.camera_pos.z);
              }
          if (z_history_.size() > max_history_size_) {
              z_history_.pop_front();
            }

          // Check for landing point in new coordinate system Z (filtered)
          if (tracked.has_new_frame) {
              CheckLandingPoint(tracked);
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


          // Enforce minimum interval between landing points (debounce)
          // User requirement: 400ms
          if (last_landing_time_initialized_) {
              auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  min_timestamp - last_landing_time_).count();
              if (delta_ms < kMinLandingIntervalMs) {
                  // Update last_landing_time_ to suppress rapid retriggers
                  last_landing_time_ = min_timestamp;
                  return;
                }
            }
          last_landing_time_ = min_timestamp;
          last_landing_time_initialized_ = true;

    // Create landing point record
    LandingPoint lp;
    lp.time_minutes = minutes;
    lp.time_seconds = seconds;
    lp.t_ms_since_start = elapsed_ms;  // 精确时间戳
    lp.new_frame_x = final_x;
    lp.new_frame_y = final_y;
    lp.new_frame_z = min_z;
    lp.landing_id = 0;  // Will be assigned if recorded

    // Record the landing point (decides whether to store based on record_enabled)
    RecordLandingPoint(lp, window_end - window_start + 1);
  }

  /**
   * Record a landing point - decides whether to store based on record_enabled flag.
   * This function is decoupled from ConfirmLandingPoint() for flexibility.
   */
  void RecordLandingPoint(LandingPoint& lp, int window_size) {
    // Only store landing point if recording is enabled
    if (g_runtime_flags.record_enabled) {
      landing_count_++;
      lp.landing_id = landing_count_;
      landing_points_.push_back(lp);

      // Output to console with detailed info
      std::cout << "\n========================================" << std::endl;
      std::cout << "落点" << lp.landing_id << " (加权平均) [已入库]：" << std::endl;
    } else {
      // Output to console - detected but not stored
      std::cout << "\n========================================" << std::endl;
      std::cout << "检测到落点 (未入库 - REC OFF)：" << std::endl;
    }

    std::cout << "  时间: " << lp.time_minutes << "分" << lp.time_seconds << "秒 (t_ms: " << lp.t_ms_since_start << ")" << std::endl;
    std::cout << "  新坐标系 X: " << std::fixed << std::setprecision(1) << lp.new_frame_x << " mm" << std::endl;
    std::cout << "  新坐标系 Y: " << std::fixed << std::setprecision(1) << lp.new_frame_y << " mm" << std::endl;
    std::cout << "  新坐标系 Z (极低点): " << std::fixed << std::setprecision(1) << lp.new_frame_z << " mm" << std::endl;
    std::cout << "  采样窗口: " << window_size << " 帧" << std::endl;
    std::cout << "  已入库落点数: " << landing_points_.size() << std::endl;
    std::cout << "========================================\n" << std::endl;
  }

  // Get landing points for display
  const std::vector<LandingPoint>& GetLandingPoints() const {
    return landing_points_;
  }

  // Parameter adjustment methods
  void IncreaseNoiseThreshold(double delta = 10.0) {
    noise_threshold_ += delta;
    if (noise_threshold_ > 2000.0) noise_threshold_ = 2000.0;
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

  // Clear all landing points and reset counter
  void ClearLandingPoints() {
    landing_points_.clear();
    landing_count_ = 0;
    ResetLandingState(true);
    std::cout << "[录制] 已清空所有落点数据" << std::endl;
  }

  // Get landing point count
  size_t GetLandingPointCount() const {
    return landing_points_.size();
  }

  // Flush landing points to CSV file
  bool FlushLandingPoints(const std::string& output_dir) {
    if (landing_points_.empty()) {
      std::cout << "[保存] 没有落点数据需要保存" << std::endl;
      return false;
    }

    std::string filepath = output_dir + "/landing_points.csv";
    std::ofstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[保存] 无法打开文件: " << filepath << std::endl;
      return false;
    }

    // Write CSV header
    file << "id,t_ms,new_frame_x,new_frame_y,new_frame_z\n";

    // Write data rows
    for (const auto& lp : landing_points_) {
      file << lp.landing_id << ","
           << lp.t_ms_since_start << ","
           << std::fixed << std::setprecision(1)
           << lp.new_frame_x << ","
           << lp.new_frame_y << ","
           << lp.new_frame_z << "\n";
    }

    file.close();
    std::cout << "[保存] 已保存 " << landing_points_.size()
              << " 个落点到: " << filepath << std::endl;
    return true;
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

    // Debounce / stability helpers
  static constexpr int kMinLandingIntervalMs = 600;     // 落点最小间隔（ms）
    std::chrono::steady_clock::time_point last_landing_time_;
    bool last_landing_time_initialized_ = false;

    // Missing frames reset
    int missing_frames_ = 0;
    static constexpr int kMissingResetFrames = 3;         // 连续丢失N帧后复位趋势状态，可调

    // Target tracking (avoid poses ordering jitter)
    bool has_last_track_ = false;
    cv::Point3d last_track_cam_pos_{0, 0, 0};

    // New-frame EMA filter
    EMA3 ema_new_frame_;
    bool filter_time_initialized_ = false;
    std::chrono::steady_clock::time_point last_filter_time_;

  // Adjustable parameters (can be changed at runtime)
  double noise_threshold_ = 300.0;                // Z坐标变化阈值 (mm)
  int window_half_ = 3;                           // 加权平均窗口半径 (帧数)
};

void OnDepthMouseCallback(int event, int x, int y, int flags, void *userdata);

#endif  // DEPTH_REGION_H_
