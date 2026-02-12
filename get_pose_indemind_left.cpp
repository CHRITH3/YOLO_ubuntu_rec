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

#include "app/camera_intrinsics.h"
#include "app/depth_region.h"
#include "app/depth_utils.h"
#include "app/perf_stats.h"
#include "app/runtime_state.h"
#include "app/queue_utils.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <queue>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <deque>
#include <limits>

using namespace indem;

#define FONT_FACE cv::FONT_HERSHEY_PLAIN
#define FONT_SCALE 1
#define FONT_COLOR cv::Scalar(255, 255, 255)
#define THICKNESS 1

// Performance optimization: limit queue size to prevent backlog
#define MAX_QUEUE_SIZE 2

struct Pose3DInfo {
  std::vector<cv::Point3d> kp_cam;
  std::vector<cv::Point3d> kp_bed;
  std::vector<bool> kp_valid;
  bool pelvis_valid = false;
  cv::Point3d pelvis_cam{0, 0, 0};
  cv::Point3d pelvis_bed{0, 0, 0};
};

struct BodyFrame {
  bool valid = false;
  cv::Point3d origin_bed{0, 0, 0};
  cv::Mat R_body_bed = cv::Mat::eye(3, 3, CV_64F);
  cv::Mat R_body_cam = cv::Mat::eye(3, 3, CV_64F);
  cv::Mat R_rel = cv::Mat::eye(3, 3, CV_64F);
  cv::Vec4d quat{1.0, 0.0, 0.0, 0.0}; // w, x, y, z
  cv::Vec3d euler_rad{0.0, 0.0, 0.0}; // roll(x), pitch(y), yaw(z)
};

struct RotationTracker {
  bool initialized = false;
  cv::Vec3d last_angles{0.0, 0.0, 0.0};
  cv::Vec3d cumulative{0.0, 0.0, 0.0};

  void Reset() {
    initialized = false;
    last_angles = cv::Vec3d(0.0, 0.0, 0.0);
    cumulative = cv::Vec3d(0.0, 0.0, 0.0);
  }

  void Update(const cv::Vec3d &angles_rad) {
    if (!initialized) {
      last_angles = angles_rad;
      initialized = true;
      return;
    }
    for (int i = 0; i < 3; ++i) {
      double delta = angles_rad[i] - last_angles[i];
      while (delta > M_PI) delta -= 2.0 * M_PI;
      while (delta < -M_PI) delta += 2.0 * M_PI;
      cumulative[i] += delta;
      last_angles[i] = angles_rad[i];
    }
  }
};

static bool NormalizeVec(const cv::Vec3d &v, cv::Vec3d &out) {
  double n = std::sqrt(v.dot(v));
  if (n < 1e-6) {
    return false;
  }
  out = v / n;
  return true;
}

static cv::Vec4d RotationMatrixToQuaternion(const cv::Mat &R) {
  cv::Vec4d q(1.0, 0.0, 0.0, 0.0);
  if (R.empty()) {
    return q;
  }
  double trace = R.at<double>(0, 0) + R.at<double>(1, 1) + R.at<double>(2, 2);
  if (trace > 0.0) {
    double s = std::sqrt(trace + 1.0) * 2.0;
    q[0] = 0.25 * s;
    q[1] = (R.at<double>(2, 1) - R.at<double>(1, 2)) / s;
    q[2] = (R.at<double>(0, 2) - R.at<double>(2, 0)) / s;
    q[3] = (R.at<double>(1, 0) - R.at<double>(0, 1)) / s;
  } else {
    if (R.at<double>(0, 0) > R.at<double>(1, 1) &&
        R.at<double>(0, 0) > R.at<double>(2, 2)) {
      double s = std::sqrt(1.0 + R.at<double>(0, 0) - R.at<double>(1, 1) - R.at<double>(2, 2)) * 2.0;
      q[0] = (R.at<double>(2, 1) - R.at<double>(1, 2)) / s;
      q[1] = 0.25 * s;
      q[2] = (R.at<double>(0, 1) + R.at<double>(1, 0)) / s;
      q[3] = (R.at<double>(0, 2) + R.at<double>(2, 0)) / s;
    } else if (R.at<double>(1, 1) > R.at<double>(2, 2)) {
      double s = std::sqrt(1.0 + R.at<double>(1, 1) - R.at<double>(0, 0) - R.at<double>(2, 2)) * 2.0;
      q[0] = (R.at<double>(0, 2) - R.at<double>(2, 0)) / s;
      q[1] = (R.at<double>(0, 1) + R.at<double>(1, 0)) / s;
      q[2] = 0.25 * s;
      q[3] = (R.at<double>(1, 2) + R.at<double>(2, 1)) / s;
    } else {
      double s = std::sqrt(1.0 + R.at<double>(2, 2) - R.at<double>(0, 0) - R.at<double>(1, 1)) * 2.0;
      q[0] = (R.at<double>(1, 0) - R.at<double>(0, 1)) / s;
      q[1] = (R.at<double>(0, 2) + R.at<double>(2, 0)) / s;
      q[2] = (R.at<double>(1, 2) + R.at<double>(2, 1)) / s;
      q[3] = 0.25 * s;
    }
  }
  return q;
}

static cv::Vec3d RotationMatrixToEulerXYZ(const cv::Mat &R) {
  if (R.empty()) {
    return cv::Vec3d(0.0, 0.0, 0.0);
  }
  double r00 = R.at<double>(0, 0);
  double r10 = R.at<double>(1, 0);
  double r20 = R.at<double>(2, 0);
  double r21 = R.at<double>(2, 1);
  double r22 = R.at<double>(2, 2);

  double pitch = std::asin(std::max(-1.0, std::min(1.0, -r20)));
  double roll = std::atan2(r21, r22);
  double yaw = std::atan2(r10, r00);
  return cv::Vec3d(roll, pitch, yaw);
}

static bool ProjectPoint(const cv::Point3d &p_cam, const cv::Mat &K, cv::Point &out) {
  if (p_cam.z <= 0.0 || K.empty()) {
    return false;
  }
  cv::Mat pt_cam = (cv::Mat_<double>(3, 1) << p_cam.x, p_cam.y, p_cam.z);
  cv::Mat pt_img = K * pt_cam / p_cam.z;
  out = cv::Point(static_cast<int>(pt_img.at<double>(0, 0)),
                  static_cast<int>(pt_img.at<double>(1, 0)));
  return true;
}

struct PostureMetrics {
  bool left_valid = false;
  bool right_valid = false;
  bool avg_valid = false;
  double left_tt = 0.0;
  double left_ts = 0.0;
  double right_tt = 0.0;
  double right_ts = 0.0;
  double avg_tt = 0.0;
  double avg_ts = 0.0;
  std::string label = "Unknown";
};

static bool GetKpCam(const PoseResult &pose,
                     const Pose3DInfo &info,
                     int idx,
                     float min_conf,
                     cv::Point3d &out) {
  if (idx < 0 || idx >= static_cast<int>(info.kp_cam.size())) {
    return false;
  }
  if (!info.kp_valid[idx]) {
    return false;
  }
  if (pose.keypoints[idx].confidence < min_conf) {
    return false;
  }
  out = info.kp_cam[idx];
  return true;
}

static bool AngleDeg(const cv::Vec3d &a, const cv::Vec3d &b, double &out_deg) {
  double na = std::sqrt(a.dot(a));
  double nb = std::sqrt(b.dot(b));
  if (na < 1e-6 || nb < 1e-6) {
    return false;
  }
  double cosv = a.dot(b) / (na * nb);
  cosv = std::max(-1.0, std::min(1.0, cosv));
  out_deg = std::acos(cosv) * (180.0 / M_PI);
  return true;
}

static std::string ClassifyPosture(double tt_deg, double ts_deg) {
  if (tt_deg <= 135.0) {
    if (ts_deg <= 135.0) {
      return "Tuck";
    }
    return "Pike";
  }
  return "Straight";
}

static PostureMetrics ComputePostureMetrics(const PoseResult &pose,
                                            const Pose3DInfo &info) {
  PostureMetrics metrics;

  const float min_conf = 0.3f;
  cv::Point3d lh, rh, ls, rs;
  bool lh_ok = GetKpCam(pose, info, LEFT_HIP, min_conf, lh);
  bool rh_ok = GetKpCam(pose, info, RIGHT_HIP, min_conf, rh);
  bool ls_ok = GetKpCam(pose, info, LEFT_SHOULDER, min_conf, ls);
  bool rs_ok = GetKpCam(pose, info, RIGHT_SHOULDER, min_conf, rs);

  if (!(lh_ok && rh_ok && ls_ok && rs_ok)) {
    return metrics;
  }

  cv::Point3d hip_mid(0.5 * (lh.x + rh.x),
                      0.5 * (lh.y + rh.y),
                      0.5 * (lh.z + rh.z));
  cv::Point3d sh_mid(0.5 * (ls.x + rs.x),
                     0.5 * (ls.y + rs.y),
                     0.5 * (ls.z + rs.z));
  cv::Vec3d trunk(sh_mid.x - hip_mid.x, sh_mid.y - hip_mid.y, sh_mid.z - hip_mid.z);

  cv::Point3d lk, la, rk, ra;
  bool lk_ok = GetKpCam(pose, info, LEFT_KNEE, min_conf, lk);
  bool la_ok = GetKpCam(pose, info, LEFT_ANKLE, min_conf, la);
  bool rk_ok = GetKpCam(pose, info, RIGHT_KNEE, min_conf, rk);
  bool ra_ok = GetKpCam(pose, info, RIGHT_ANKLE, min_conf, ra);

  if (lh_ok && lk_ok && la_ok) {
    cv::Vec3d thigh(lk.x - lh.x, lk.y - lh.y, lk.z - lh.z);
    cv::Vec3d shank(la.x - lk.x, la.y - lk.y, la.z - lk.z);
    cv::Vec3d rev_thigh(-thigh[0], -thigh[1], -thigh[2]);
    metrics.left_valid =
        AngleDeg(trunk, thigh, metrics.left_tt) &&
        AngleDeg(rev_thigh, shank, metrics.left_ts);
  }

  if (rh_ok && rk_ok && ra_ok) {
    cv::Vec3d thigh(rk.x - rh.x, rk.y - rh.y, rk.z - rh.z);
    cv::Vec3d shank(ra.x - rk.x, ra.y - rk.y, ra.z - rk.z);
    cv::Vec3d rev_thigh(-thigh[0], -thigh[1], -thigh[2]);
    metrics.right_valid =
        AngleDeg(trunk, thigh, metrics.right_tt) &&
        AngleDeg(rev_thigh, shank, metrics.right_ts);
  }

  if (metrics.left_valid && metrics.right_valid) {
    metrics.avg_tt = 0.5 * (metrics.left_tt + metrics.right_tt);
    metrics.avg_ts = 0.5 * (metrics.left_ts + metrics.right_ts);
    metrics.avg_valid = true;
  } else if (metrics.left_valid) {
    metrics.avg_tt = metrics.left_tt;
    metrics.avg_ts = metrics.left_ts;
    metrics.avg_valid = true;
  } else if (metrics.right_valid) {
    metrics.avg_tt = metrics.right_tt;
    metrics.avg_ts = metrics.right_ts;
    metrics.avg_valid = true;
  }

  if (metrics.left_valid && metrics.right_valid) {
    const std::string left_label = ClassifyPosture(metrics.left_tt, metrics.left_ts);
    const std::string right_label = ClassifyPosture(metrics.right_tt, metrics.right_ts);
    if (left_label == right_label) {
      metrics.label = left_label;
    }
  } else if (metrics.left_valid) {
    metrics.label = ClassifyPosture(metrics.left_tt, metrics.left_ts);
  } else if (metrics.right_valid) {
    metrics.label = ClassifyPosture(metrics.right_tt, metrics.right_ts);
  }

  return metrics;
}

static void DrawBodyFrameBox(cv::Mat &image,
                             const cv::Mat &R_body_cam,
                             const cv::Point3d &center_cam,
                             double half_x,
                             double half_y,
                             double half_z,
                             const cv::Mat &K) {
  if (R_body_cam.empty() || K.empty()) {
    return;
  }

  std::array<cv::Point3d, 8> corners_body = {
      cv::Point3d(-half_x, -half_y, -half_z),
      cv::Point3d(half_x, -half_y, -half_z),
      cv::Point3d(half_x, half_y, -half_z),
      cv::Point3d(-half_x, half_y, -half_z),
      cv::Point3d(-half_x, -half_y, half_z),
      cv::Point3d(half_x, -half_y, half_z),
      cv::Point3d(half_x, half_y, half_z),
      cv::Point3d(-half_x, half_y, half_z)
  };

  std::array<cv::Point, 8> corners_2d;
  std::array<bool, 8> visible{};

  for (size_t i = 0; i < corners_body.size(); ++i) {
    const cv::Point3d &p_body = corners_body[i];
    cv::Point3d p_cam(
        center_cam.x + R_body_cam.at<double>(0, 0) * p_body.x +
            R_body_cam.at<double>(0, 1) * p_body.y + R_body_cam.at<double>(0, 2) * p_body.z,
        center_cam.y + R_body_cam.at<double>(1, 0) * p_body.x +
            R_body_cam.at<double>(1, 1) * p_body.y + R_body_cam.at<double>(1, 2) * p_body.z,
        center_cam.z + R_body_cam.at<double>(2, 0) * p_body.x +
            R_body_cam.at<double>(2, 1) * p_body.y + R_body_cam.at<double>(2, 2) * p_body.z);
    visible[i] = ProjectPoint(p_cam, K, corners_2d[i]);
  }

  auto draw_edge = [&](int a, int b) {
    if (visible[a] && visible[b]) {
      cv::line(image, corners_2d[a], corners_2d[b], cv::Scalar(0, 200, 255), 2, cv::LINE_AA);
    }
  };

  draw_edge(0, 1);
  draw_edge(1, 2);
  draw_edge(2, 3);
  draw_edge(3, 0);
  draw_edge(4, 5);
  draw_edge(5, 6);
  draw_edge(6, 7);
  draw_edge(7, 4);
  draw_edge(0, 4);
  draw_edge(1, 5);
  draw_edge(2, 6);
  draw_edge(3, 7);
}

static bool BuildBodyFrameFromPose(const PoseResult &pose,
                                   const Pose3DInfo &info,
                                   const cv::Mat &R_bed_cam,
                                   BodyFrame &out) {
  if (R_bed_cam.empty()) {
    return false;
  }

  auto kp_ok = [&](int idx, cv::Point3d &pt) -> bool {
    if (idx < 0 || idx >= static_cast<int>(info.kp_bed.size())) {
      return false;
    }
    if (!info.kp_valid[idx]) {
      return false;
    }
    if (pose.keypoints[idx].confidence < 0.3f) {
      return false;
    }
    pt = info.kp_bed[idx];
    return true;
  };

  cv::Point3d lh, rh, ls, rs;
  bool lh_ok = kp_ok(LEFT_HIP, lh);
  bool rh_ok = kp_ok(RIGHT_HIP, rh);
  bool ls_ok = kp_ok(LEFT_SHOULDER, ls);
  bool rs_ok = kp_ok(RIGHT_SHOULDER, rs);

  if ((!lh_ok && !rh_ok) || (!ls_ok && !rs_ok)) {
    return false;
  }

  cv::Point3d pelvis = lh_ok && rh_ok
                           ? cv::Point3d(0.5 * (lh.x + rh.x), 0.5 * (lh.y + rh.y), 0.5 * (lh.z + rh.z))
                           : (lh_ok ? lh : rh);

  cv::Point3d shoulders = ls_ok && rs_ok
                              ? cv::Point3d(0.5 * (ls.x + rs.x), 0.5 * (ls.y + rs.y), 0.5 * (ls.z + rs.z))
                              : (ls_ok ? ls : rs);

  // Define body axes in bed coordinates:
  // y_body: hip_mid -> shoulder_mid
  // x_body: right_hip -> left_hip (left minus right)
  // z_body: x_body × y_body
  cv::Vec3d y_raw(shoulders.x - pelvis.x, shoulders.y - pelvis.y, shoulders.z - pelvis.z);
  cv::Vec3d x_raw(0.0, 0.0, 0.0);
  if (lh_ok && rh_ok) {
    x_raw = cv::Vec3d(lh.x - rh.x, lh.y - rh.y, lh.z - rh.z);
  } else if (ls_ok && rs_ok) {
    x_raw = cv::Vec3d(ls.x - rs.x, ls.y - rs.y, ls.z - rs.z);
  } else {
    return false;
  }

  cv::Vec3d x_axis;
  if (!NormalizeVec(x_raw, x_axis)) {
    return false;
  }

  // Gram-Schmidt: make y orthogonal to x, then normalize.
  cv::Vec3d y_orth = y_raw - x_axis * (y_raw.dot(x_axis));
  cv::Vec3d y_axis;
  if (!NormalizeVec(y_orth, y_axis)) {
    return false;
  }

  cv::Vec3d z_axis = x_axis.cross(y_axis);
  if (!NormalizeVec(z_axis, z_axis)) {
    return false;
  }

  out.origin_bed = pelvis;
  out.R_body_bed = cv::Mat::eye(3, 3, CV_64F);
  out.R_body_bed.at<double>(0, 0) = x_axis[0];
  out.R_body_bed.at<double>(1, 0) = x_axis[1];
  out.R_body_bed.at<double>(2, 0) = x_axis[2];

  out.R_body_bed.at<double>(0, 1) = y_axis[0];
  out.R_body_bed.at<double>(1, 1) = y_axis[1];
  out.R_body_bed.at<double>(2, 1) = y_axis[2];

  out.R_body_bed.at<double>(0, 2) = z_axis[0];
  out.R_body_bed.at<double>(1, 2) = z_axis[1];
  out.R_body_bed.at<double>(2, 2) = z_axis[2];

  out.R_body_cam = R_bed_cam * out.R_body_bed;
  out.R_rel = R_bed_cam.t() * out.R_body_cam;
  out.quat = RotationMatrixToQuaternion(out.R_rel);
  out.euler_rad = RotationMatrixToEulerXYZ(out.R_rel);
  out.valid = true;
  return true;
}

int main(int argc, char **argv) {
  (void)argc;

  // Check model file
  std::string model_path = "models/yolov8m-pose-1280.onnx";
  if (argc > 1) {
    model_path = argv[1];
  }

  std::cout << "\n=== YOLO Pose Detection with INDEMIND Left Camera ===\n" << std::endl;
  std::cout << "Using: Left camera RGB image only (no depth processing)" << std::endl;

  // Initialize IMSEE SDK
  auto m_pSDK = new CIMRSDK();
  MRCONFIG config = {0};
  config.bSlam = false;
  config.imgResolution = IMG_1280;
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
  auto param = module_params._left_camera[RESOLUTION::RES_1280X800];

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
  YOLOPoseDetector pose_detector(model_path, 1280, 0.5f, 0.45f, true);  // true = 启用 CUDA
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
  std::cout << "  k       : Toggle keypoints" << std::endl;
  std::cout << "  t       : Toggle skeleton" << std::endl;
  std::cout << "  i       : Toggle info overlay" << std::endl;
  std::cout << "  l       : Toggle hip coords recording (Start/Stop)" << std::endl;
  std::cout << "  SPACE   : Save current frame" << std::endl;
  std::cout << "  + / -   : Increase/Decrease Z threshold" << std::endl;
  std::cout << "  [ / ]   : Decrease/Increase window radius" << std::endl;
  std::cout << "  p       : Print current parameters" << std::endl;
  std::cout << "  --- Trampoline ROI ---" << std::endl;
  std::cout << "  Mouse   : Click 4 corners in YOLO window to fit bed plane" << std::endl;
  std::cout << "  --- Landing Points Recording ---" << std::endl;
  std::cout << "  r       : Toggle REC (ON: record landing points)" << std::endl;
  std::cout << "  c       : Clear landing points cache" << std::endl;
  std::cout << "  s       : Save landing points to CSV\n" << std::endl;

  // Display options
  bool show_bbox = false;
  bool show_keypoints = true;
  bool show_skeleton = true;
  bool show_info = true;

  // Body frame tracking state
  RotationTracker rotation_tracker;
  cv::Point3d last_tracked_pelvis_cam(0, 0, 0);
  bool has_tracked_person = false;
  int missing_body_frames = 0;

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
        ClearQueue(image_queue);  // Clear queue to always process latest frame
      }
    }

    // Get depth from queue
    {
      std::unique_lock<std::mutex> lock(mutex_depth);
      if (!depth_queue.empty()) {
        depth_data = depth_queue.front();
        ClearQueue(depth_queue);
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
      g_perf_stats.AddInference(inference_time);

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

      // Calculate 3D keypoints (camera + trampoline coordinates) and hip info
      std::vector<DepthRegion::HipInfo> hip_data_list;
      std::vector<Pose3DInfo> pose_3d_infos(poses.size());
      BodyFrame tracked_body_frame;
      int tracked_pose_index = -1;

      bool bed_ready = depth_region.IsCoordSystemReady();
      cv::Mat R_bed_cam;
      cv::Point3d bed_origin;
      if (bed_ready) {
        depth_region.GetCoordinateSystem(R_bed_cam, bed_origin);
        (void)bed_origin;
      }

      auto depth_start = std::chrono::steady_clock::now();
      if (!depth_data.empty() && !poses.empty()) {
        for (size_t p = 0; p < poses.size(); ++p) {
          const auto &pose = poses[p];
          Pose3DInfo info;
          info.kp_cam.assign(pose.keypoints.size(), cv::Point3d(0, 0, 0));
          info.kp_bed.assign(pose.keypoints.size(), cv::Point3d(0, 0, 0));
          info.kp_valid.assign(pose.keypoints.size(), false);

          for (size_t k = 0; k < pose.keypoints.size(); ++k) {
            const auto &kp = pose.keypoints[k];
            if (kp.confidence < 0.3f) {
              continue;
            }

            int px = static_cast<int>(kp.x);
            int py = static_cast<int>(kp.y);
            if (px < 0 || px >= depth_data.cols || py < 0 || py >= depth_data.rows) {
              continue;
            }

            uint16_t z_mm = 0;
            if (!RobustDepthMedianU16(depth_data, px, py, /*r=*/3, z_mm)) {
              continue;
            }
            double Z = static_cast<double>(z_mm);

            cv::Mat kp_img_cor(3, 1, CV_64FC1);
            kp_img_cor.at<double>(0, 0) = static_cast<double>(px);
            kp_img_cor.at<double>(1, 0) = static_cast<double>(py);
            kp_img_cor.at<double>(2, 0) = 1.0;

            cv::Mat kp_camera_cor = cv_in_left_inv * Z * kp_img_cor;
            cv::Point3d cam_pt(kp_camera_cor.at<double>(0, 0),
                               kp_camera_cor.at<double>(1, 0),
                               kp_camera_cor.at<double>(2, 0));

            info.kp_cam[k] = cam_pt;
            info.kp_valid[k] = true;

            if (bed_ready) {
              info.kp_bed[k] = depth_region.TransformToNewFrame(cam_pt);
            }
          }

          bool lh_ok = pose.keypoints.size() > LEFT_HIP &&
                       pose.keypoints[LEFT_HIP].confidence > 0.5f &&
                       info.kp_valid[LEFT_HIP];
          bool rh_ok = pose.keypoints.size() > RIGHT_HIP &&
                       pose.keypoints[RIGHT_HIP].confidence > 0.5f &&
                       info.kp_valid[RIGHT_HIP];

          if (lh_ok || rh_ok) {
            cv::Point3d pelvis_cam = lh_ok && rh_ok
                                         ? cv::Point3d(0.5 * (info.kp_cam[LEFT_HIP].x + info.kp_cam[RIGHT_HIP].x),
                                                      0.5 * (info.kp_cam[LEFT_HIP].y + info.kp_cam[RIGHT_HIP].y),
                                                      0.5 * (info.kp_cam[LEFT_HIP].z + info.kp_cam[RIGHT_HIP].z))
                                         : (lh_ok ? info.kp_cam[LEFT_HIP] : info.kp_cam[RIGHT_HIP]);

            info.pelvis_cam = pelvis_cam;
            info.pelvis_valid = true;
            if (bed_ready) {
              info.pelvis_bed = depth_region.TransformToNewFrame(pelvis_cam);
            }

            DepthRegion::HipInfo hip_info;
            hip_info.person_id = static_cast<int>(p) + 1;
            hip_info.camera_pos = pelvis_cam;
            hip_info.has_new_frame = bed_ready;
            if (hip_info.has_new_frame) {
              hip_info.new_frame_pos = info.pelvis_bed;
            }
            hip_data_list.push_back(hip_info);
          }

          pose_3d_infos[p] = info;
          if (bed_ready) {
            for (size_t k = 0; k < pose.keypoints.size(); ++k) {
              if (info.kp_valid[k]) {
                poses[p].keypoints[k].pos3d = cv::Point3f(
                    static_cast<float>(info.kp_bed[k].x),
                    static_cast<float>(info.kp_bed[k].y),
                    static_cast<float>(info.kp_bed[k].z));
              }
            }
          }
        }
      }

      if (bed_ready && !pose_3d_infos.empty()) {
        double best_d2 = std::numeric_limits<double>::infinity();
        for (size_t p = 0; p < pose_3d_infos.size(); ++p) {
          if (!pose_3d_infos[p].pelvis_valid) {
            continue;
          }
          if (!has_tracked_person) {
            tracked_pose_index = static_cast<int>(p);
            break;
          }
          double dx = pose_3d_infos[p].pelvis_cam.x - last_tracked_pelvis_cam.x;
          double dy = pose_3d_infos[p].pelvis_cam.y - last_tracked_pelvis_cam.y;
          double dz = pose_3d_infos[p].pelvis_cam.z - last_tracked_pelvis_cam.z;
          double d2 = dx * dx + dy * dy + dz * dz;
          if (d2 < best_d2) {
            best_d2 = d2;
            tracked_pose_index = static_cast<int>(p);
          }
        }

        if (tracked_pose_index >= 0) {
          has_tracked_person = true;
          last_tracked_pelvis_cam = pose_3d_infos[tracked_pose_index].pelvis_cam;
          BuildBodyFrameFromPose(poses[tracked_pose_index], pose_3d_infos[tracked_pose_index],
                                 R_bed_cam, tracked_body_frame);
        } else {
          has_tracked_person = false;
        }
      }

      if (tracked_body_frame.valid) {
        missing_body_frames = 0;
        rotation_tracker.Update(tracked_body_frame.euler_rad);
      } else {
        missing_body_frames++;
        if (missing_body_frames >= 3) {
          rotation_tracker.Reset();
        }
      }

      PostureMetrics posture_metrics;
      if (tracked_pose_index >= 0 &&
          tracked_pose_index < static_cast<int>(pose_3d_infos.size())) {
        posture_metrics = ComputePostureMetrics(
            poses[tracked_pose_index], pose_3d_infos[tracked_pose_index]);
      }
      auto depth_end = std::chrono::steady_clock::now();
      double depth_time = std::chrono::duration_cast<std::chrono::microseconds>(
          depth_end - depth_start).count() / 1000.0;
      g_perf_stats.AddDepthMap(depth_time);

      // Update hip data in region window
      auto landing_start = std::chrono::steady_clock::now();
      depth_region.UpdateHipData(hip_data_list);
      auto landing_end = std::chrono::steady_clock::now();
      double landing_time = std::chrono::duration_cast<std::chrono::microseconds>(
          landing_end - landing_start).count() / 1000.0;
      g_perf_stats.AddLandingDetect(landing_time);

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

      // Draw body axes for tracked person
      if (tracked_body_frame.valid && tracked_pose_index >= 0 &&
          tracked_pose_index < static_cast<int>(pose_3d_infos.size()) &&
          pose_3d_infos[tracked_pose_index].pelvis_valid) {
        const double axis_length = 260.0;
        const int axis_thickness = 1;
        const cv::Point3d &pelvis_cam = pose_3d_infos[tracked_pose_index].pelvis_cam;

        cv::Point3d x_end(pelvis_cam.x + tracked_body_frame.R_body_cam.at<double>(0, 0) * axis_length,
                          pelvis_cam.y + tracked_body_frame.R_body_cam.at<double>(1, 0) * axis_length,
                          pelvis_cam.z + tracked_body_frame.R_body_cam.at<double>(2, 0) * axis_length);
        cv::Point3d y_end(pelvis_cam.x + tracked_body_frame.R_body_cam.at<double>(0, 1) * axis_length,
                          pelvis_cam.y + tracked_body_frame.R_body_cam.at<double>(1, 1) * axis_length,
                          pelvis_cam.z + tracked_body_frame.R_body_cam.at<double>(2, 1) * axis_length);
        cv::Point3d z_end(pelvis_cam.x + tracked_body_frame.R_body_cam.at<double>(0, 2) * axis_length,
                          pelvis_cam.y + tracked_body_frame.R_body_cam.at<double>(1, 2) * axis_length,
                          pelvis_cam.z + tracked_body_frame.R_body_cam.at<double>(2, 2) * axis_length);

        cv::Point origin_2d, x_2d, y_2d, z_2d;
        if (ProjectPoint(pelvis_cam, cv_in_left, origin_2d)) {
          if (ProjectPoint(x_end, cv_in_left, x_2d)) {
            cv::arrowedLine(display, origin_2d, x_2d, cv::Scalar(0, 0, 255), axis_thickness, cv::LINE_AA, 0, 0.2);
            cv::putText(display, "Xb", x_2d + cv::Point(6, 6),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
          }
          if (ProjectPoint(y_end, cv_in_left, y_2d)) {
            cv::arrowedLine(display, origin_2d, y_2d, cv::Scalar(0, 255, 0), axis_thickness, cv::LINE_AA, 0, 0.2);
            cv::putText(display, "Yb", y_2d + cv::Point(6, 6),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
          }
          if (ProjectPoint(z_end, cv_in_left, z_2d)) {
            cv::arrowedLine(display, origin_2d, z_2d, cv::Scalar(255, 0, 0), axis_thickness, cv::LINE_AA, 0, 0.2);
            cv::putText(display, "Zb", z_2d + cv::Point(6, 6),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
          }
        }

        auto get_kp_cam = [&](int idx, cv::Point3d &pt) -> bool {
          if (idx < 0 || idx >= static_cast<int>(pose_3d_infos[tracked_pose_index].kp_cam.size())) {
            return false;
          }
          if (!pose_3d_infos[tracked_pose_index].kp_valid[idx]) {
            return false;
          }
          pt = pose_3d_infos[tracked_pose_index].kp_cam[idx];
          return true;
        };

        cv::Point3d lh, rh, ls, rs;
        bool lh_ok = get_kp_cam(LEFT_HIP, lh);
        bool rh_ok = get_kp_cam(RIGHT_HIP, rh);
        bool ls_ok = get_kp_cam(LEFT_SHOULDER, ls);
        bool rs_ok = get_kp_cam(RIGHT_SHOULDER, rs);

        cv::Point3d hip_mid = lh_ok && rh_ok ? cv::Point3d(0.5 * (lh.x + rh.x),
                                                           0.5 * (lh.y + rh.y),
                                                           0.5 * (lh.z + rh.z))
                                             : (lh_ok ? lh : rh);
        cv::Point3d shoulder_mid = ls_ok && rs_ok ? cv::Point3d(0.5 * (ls.x + rs.x),
                                                                0.5 * (ls.y + rs.y),
                                                                0.5 * (ls.z + rs.z))
                                                  : (ls_ok ? ls : rs);

        double torso_height = 0.0;
        if ((ls_ok || rs_ok) && (lh_ok || rh_ok)) {
          cv::Point3d delta = shoulder_mid - hip_mid;
          torso_height = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        }

        double torso_width = 0.0;
        if (ls_ok && rs_ok) {
          cv::Point3d delta = ls - rs;
          torso_width = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        }
        if (lh_ok && rh_ok) {
          cv::Point3d delta = lh - rh;
          double hip_width = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
          torso_width = torso_width > 1.0 ? 0.5 * (torso_width + hip_width) : hip_width;
        }

        if (torso_height < 1.0) {
          torso_height = 400.0;
        }
        if (torso_width < 1.0) {
          torso_width = 300.0;
        }
        double torso_depth = std::max(120.0, torso_width * 0.4);

        cv::Point3d y_dir(tracked_body_frame.R_body_cam.at<double>(0, 1),
                          tracked_body_frame.R_body_cam.at<double>(1, 1),
                          tracked_body_frame.R_body_cam.at<double>(2, 1));
        cv::Point3d center_cam = pelvis_cam + y_dir * (torso_height * 0.5);

        DrawBodyFrameBox(display,
                         tracked_body_frame.R_body_cam,
                         center_cam,
                         torso_width * 0.5,
                         torso_height * 0.5,
                         torso_depth * 0.5,
                         cv_in_left);
      }

      // Set mouse callback on YOLO Pose window for depth interaction
      cv::setMouseCallback("YOLO Pose - INDEMIND Left Camera", OnDepthMouseCallback, &depth_region);

      // Display status panel (top-right corner)
      int panel_x = display.cols - 180;
      int panel_y = 20;
      int line_height = 22;

      // REC status
      std::string rec_status = g_runtime_flags.record_enabled ? "REC: ON" : "REC: OFF";
      cv::Scalar rec_color = g_runtime_flags.record_enabled ? cv::Scalar(0, 0, 255) : cv::Scalar(128, 128, 128);
      cv::putText(display, rec_status, cv::Point(panel_x, panel_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, rec_color, 2);

      // Landing point count
      panel_y += line_height;
      std::ostringstream lp_count_str;
      lp_count_str << "LP: " << depth_region.GetLandingPointCount();
      cv::putText(display, lp_count_str.str(), cv::Point(panel_x, panel_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

      // Last landing point info
      const auto& lps = depth_region.GetLandingPoints();
      if (!lps.empty()) {
        const auto& last_lp = lps.back();
        panel_y += line_height;
        std::ostringstream last_str;
        last_str << "Last: (" << std::fixed << std::setprecision(0)
                 << last_lp.new_frame_x << "," << last_lp.new_frame_y << "," << last_lp.new_frame_z << ")";
        cv::putText(display, last_str.str(), cv::Point(panel_x, panel_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(200, 200, 200), 1);
      }

      panel_y += line_height;
      std::ostringstream posture_ss;
      posture_ss << "Posture: " << posture_metrics.label;
      cv::putText(display, posture_ss.str(), cv::Point(panel_x, panel_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 255), 1);

      // Body frame metrics window
      cv::Mat metrics_panel(950, 650, CV_8UC3, cv::Scalar(245, 245, 245));
      int metrics_y = 30;
      const int metrics_line = 22;
      cv::putText(metrics_panel, "Body Frame Metrics",
                  cv::Point(10, metrics_y), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                  cv::Scalar(20, 20, 20), 2);
      metrics_y += metrics_line + 5;

      std::ostringstream status_ss;
      status_ss << "Trampoline frame: " << (bed_ready ? "READY" : "NOT READY");
      cv::putText(metrics_panel, status_ss.str(), cv::Point(10, metrics_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(60, 60, 60), 1);
      metrics_y += metrics_line;

      std::ostringstream track_ss;
      if (tracked_pose_index >= 0) {
        track_ss << "Tracked person: " << (tracked_pose_index + 1);
      } else {
        track_ss << "Tracked person: NONE";
      }
      cv::putText(metrics_panel, track_ss.str(), cv::Point(10, metrics_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(60, 60, 60), 1);
      metrics_y += metrics_line;

      if (tracked_body_frame.valid) {
        cv::Vec3d angles_deg = tracked_body_frame.euler_rad * (180.0 / M_PI);
        cv::Vec3d cumulative_deg = rotation_tracker.cumulative * (180.0 / M_PI);
        cv::Vec3d counts = rotation_tracker.cumulative * (1.0 / (2.0 * M_PI));

        std::ostringstream quat_ss;
        quat_ss << "Quat (w,x,y,z): [" << std::fixed << std::setprecision(3)
                << tracked_body_frame.quat[0] << ", "
                << tracked_body_frame.quat[1] << ", "
                << tracked_body_frame.quat[2] << ", "
                << tracked_body_frame.quat[3] << "]";
        cv::putText(metrics_panel, quat_ss.str(), cv::Point(10, metrics_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
        metrics_y += metrics_line;

        std::ostringstream angle_ss;
        angle_ss << "Angles deg (x,y,z): [" << std::fixed << std::setprecision(1)
                 << angles_deg[0] << ", " << angles_deg[1] << ", " << angles_deg[2] << "]";
        cv::putText(metrics_panel, angle_ss.str(), cv::Point(10, metrics_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
        metrics_y += metrics_line;

        std::ostringstream cum_ss;
        cum_ss << "Cumulative deg (x,y,z): [" << std::fixed << std::setprecision(1)
               << cumulative_deg[0] << ", " << cumulative_deg[1] << ", " << cumulative_deg[2] << "]";
        cv::putText(metrics_panel, cum_ss.str(), cv::Point(10, metrics_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
        metrics_y += metrics_line;

        std::ostringstream count_ss;
        count_ss << "Counts (flip/twist/side): ["
                 << std::fixed << std::setprecision(2)
                 << counts[0] << ", " << counts[1] << ", " << counts[2] << "]";
        cv::putText(metrics_panel, count_ss.str(), cv::Point(10, metrics_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
        metrics_y += metrics_line;
      } else {
        cv::putText(metrics_panel, "Body frame: INVALID",
                    cv::Point(10, metrics_y), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(80, 0, 0), 2);
        metrics_y += metrics_line;
      }

      std::ostringstream posture_line;
      posture_line << "Posture: " << posture_metrics.label;
      cv::putText(metrics_panel, posture_line.str(), cv::Point(10, metrics_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(20, 20, 20), 2);
      metrics_y += metrics_line;

      auto format_angle = [](bool valid, double value) {
        std::ostringstream os;
        if (!valid) {
          os << "N/A";
        } else {
          os << std::fixed << std::setprecision(1) << value;
        }
        return os.str();
      };

      std::ostringstream left_ss;
      left_ss << "Left TT/TS (deg): "
              << format_angle(posture_metrics.left_valid, posture_metrics.left_tt)
              << " / "
              << format_angle(posture_metrics.left_valid, posture_metrics.left_ts);
      cv::putText(metrics_panel, left_ss.str(), cv::Point(10, metrics_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
      metrics_y += metrics_line;

      std::ostringstream right_ss;
      right_ss << "Right TT/TS (deg): "
               << format_angle(posture_metrics.right_valid, posture_metrics.right_tt)
               << " / "
               << format_angle(posture_metrics.right_valid, posture_metrics.right_ts);
      cv::putText(metrics_panel, right_ss.str(), cv::Point(10, metrics_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
      metrics_y += metrics_line;

      std::ostringstream avg_ss;
      avg_ss << "Avg TT/TS (deg): "
             << format_angle(posture_metrics.avg_valid, posture_metrics.avg_tt)
             << " / "
             << format_angle(posture_metrics.avg_valid, posture_metrics.avg_ts);
      cv::putText(metrics_panel, avg_ss.str(), cv::Point(10, metrics_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1);
      metrics_y += metrics_line;

      metrics_y += 5;
      cv::putText(metrics_panel, "3D Skeleton (trampoline coords, mm):",
                  cv::Point(10, metrics_y), cv::FONT_HERSHEY_SIMPLEX, 0.55,
                  cv::Scalar(30, 30, 30), 1);
      metrics_y += metrics_line;

      if (bed_ready && tracked_pose_index >= 0 &&
          tracked_pose_index < static_cast<int>(pose_3d_infos.size())) {
        const auto &info = pose_3d_infos[tracked_pose_index];
        for (size_t k = 0; k < poses[tracked_pose_index].keypoints.size(); ++k) {
          if (metrics_y > metrics_panel.rows - 20) {
            break;
          }
          std::ostringstream kp_ss;
          kp_ss << GetKeypointName(static_cast<int>(k)) << ": ";
          if (k < info.kp_valid.size() && info.kp_valid[k]) {
            const auto &pt = info.kp_bed[k];
            kp_ss << std::fixed << std::setprecision(1)
                  << pt.x << ", " << pt.y << ", " << pt.z;
          } else {
            kp_ss << "N/A";
          }
          cv::putText(metrics_panel, kp_ss.str(), cv::Point(10, metrics_y),
                      cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(50, 50, 50), 1);
          metrics_y += metrics_line;
        }
      } else {
        cv::putText(metrics_panel, "No valid 3D skeleton available.",
                    cv::Point(10, metrics_y), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(50, 50, 50), 1);
        metrics_y += metrics_line;
      }

      cv::imshow("Body Frame Metrics", metrics_panel);

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

    // Print performance stats periodically
    g_perf_stats.PrintIfNeeded();

    // Handle keyboard input
    char key = static_cast<char>(cv::waitKey(1));
    if (key == 27 || key == 'q' || key == 'Q') {
      break;
    } else if (key == 'k' || key == 'K') {
      show_keypoints = !show_keypoints;
      std::cout << "Keypoints: " << (show_keypoints ? "ON" : "OFF") << std::endl;
    } else if (key == 't' || key == 'T') {
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
    } else if (key == 'r' || key == 'R') {
      // Toggle recording
      bool was_off = !g_runtime_flags.record_enabled;
      g_runtime_flags.record_enabled = !g_runtime_flags.record_enabled;

      if (g_runtime_flags.record_enabled && was_off) {
        // OFF -> ON: Create new session
        CreateNewSession();
        depth_region.ClearLandingPoints();  // Clear previous data
      }

      std::cout << "[录制] REC: " << (g_runtime_flags.record_enabled ? "ON" : "OFF") << std::endl;
    } else if (key == 'c' || key == 'C') {
      // Clear landing points cache
      depth_region.ClearLandingPoints();
    } else if (key == 's' || key == 'S') {
      // Save landing points to CSV
      if (g_current_session.active) {
        depth_region.FlushLandingPoints(g_current_session.output_dir);
      } else {
        std::cout << "[保存] 请先按 R 开始录制会话" << std::endl;
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
  left camera RGB images. Click 4 corners on the image to define a
  trampoline bed ROI and fit a plane using depth data.

  Key features:
    ✓ Uses left camera image for pose detection (RGB)
    ✓ 4-click ROI to fit trampoline bed plane (RANSAC)
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

  Optional: Specify model path (default: models/yolov8n-pose-640.onnx)
  sudo ./build/yolo_pose_indemind_left models/yolov8s-pose.onnx

FEATURES:
  ✓ Real-time human pose detection (17 COCO keypoints)
  ✓ Multi-person detection
  ✓ Colored skeleton visualization
  ✓ Torso-aligned body frame box (3D)
  ✓ Performance monitoring (FPS, inference time)
  ✓ Frame capture capability
  ✓ Uses INDEMIND left camera
  ✓ Mouse ROI selection for trampoline bed plane
  ✓ Region window shows plane-fit status

KEYBOARD CONTROLS:
  - 'q' or ESC : Quit the application
  - 'k' or 'K' : Toggle keypoints display on/off
  - 's' or 'S' : Toggle skeleton lines on/off
  - 'i' or 'I' : Toggle info overlay on/off
  - SPACE      : Save current frame to disk

MOUSE INTERACTION:
  On "YOLO Pose - INDEMIND Left Camera" window:
  - Move mouse: Preview depth values at cursor location
  - Left click (4 corners): Define trampoline bed ROI and fit plane (RANSAC)
  - Additional click: Reset ROI and start a new selection

  "region" window shows (appears on click):
  - Image coordinates [row, col]
  - 3D camera coordinates [X, Y, Z] in mm
  - ROI point list and plane-fit status

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
  - Torso Box: 3D body-frame-aligned rectangular box
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
    * Run: ls -lh models/yolov8n-pose-640.onnx

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
