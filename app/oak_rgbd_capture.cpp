#include "app/oak_rgbd_capture.h"

#include <depthai/depthai.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace {

constexpr std::size_t kPairBufferLimit = 90;
constexpr unsigned int kOutputQueueSize = 120;
constexpr bool kOutputQueueBlocking = true;
constexpr int kRgbSensorWidth = 1920;
constexpr int kRgbSensorHeight = 1200;

template <typename Clock, typename Duration>
double TimePointMs(const std::chrono::time_point<Clock, Duration>& tp) {
  return std::chrono::duration<double, std::milli>(tp.time_since_epoch()).count();
}

template <typename Clock, typename Duration>
double TimePointSec(const std::chrono::time_point<Clock, Duration>& tp) {
  return std::chrono::duration<double>(tp.time_since_epoch()).count();
}

double TimestampMs(const std::shared_ptr<dai::ImgFrame>& msg) {
  return TimePointMs(msg->getTimestamp());
}

double TimestampSec(const std::shared_ptr<dai::ImgFrame>& msg) {
  return TimePointSec(msg->getTimestamp());
}

void AppendAndTrim(std::deque<std::shared_ptr<dai::ImgFrame>>& buf,
                   const std::shared_ptr<dai::ImgFrame>& msg,
                   std::size_t limit,
                   std::atomic<int>& dropped_counter) {
  buf.push_back(msg);
  while (buf.size() > limit) {
    buf.pop_front();
    ++dropped_counter;
  }
}

bool PopClosestPair(std::deque<std::shared_ptr<dai::ImgFrame>>& primary_buf,
                    std::deque<std::shared_ptr<dai::ImgFrame>>& secondary_buf,
                    double threshold_ms,
                    std::shared_ptr<dai::ImgFrame>& primary_out,
                    std::shared_ptr<dai::ImgFrame>& secondary_out,
                    double& pair_dt_ms,
                    std::atomic<int>& dropped_primary,
                    std::atomic<int>& dropped_secondary) {
  if (primary_buf.empty() || secondary_buf.empty()) {
    return false;
  }

  const double primary_ts = TimestampMs(primary_buf.front());
  std::size_t best_index = 0;
  double best_dt = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < secondary_buf.size(); ++i) {
    const double dt = std::abs(TimestampMs(secondary_buf[i]) - primary_ts);
    if (dt < best_dt) {
      best_dt = dt;
      best_index = i;
    }
  }

  const double secondary_ts = TimestampMs(secondary_buf[best_index]);
  if (best_dt > threshold_ms) {
    if (primary_ts < secondary_ts) {
      primary_buf.pop_front();
      ++dropped_primary;
    } else {
      secondary_buf.pop_front();
      ++dropped_secondary;
    }
    return false;
  }

  primary_out = primary_buf.front();
  primary_buf.pop_front();
  secondary_out = secondary_buf[best_index];
  for (std::size_t i = 0; i <= best_index; ++i) {
    secondary_buf.pop_front();
  }
  pair_dt_ms = best_dt;
  return true;
}

cv::Mat Nv12ToBgr(const std::shared_ptr<dai::ImgFrame>& msg, int width, int height) {
  cv::Mat frame;
  try {
    frame = msg->getCvFrame();
  } catch (const std::exception&) {
    frame.release();
  }

  if (!frame.empty()) {
    if (frame.type() == CV_8UC3) {
      return frame.clone();
    }
    if (frame.type() == CV_8UC1) {
      cv::Mat bgr;
      if (frame.rows == height * 3 / 2 && frame.cols == width) {
        cv::cvtColor(frame, bgr, cv::COLOR_YUV2BGR_NV12);
      } else {
        cv::cvtColor(frame, bgr, cv::COLOR_GRAY2BGR);
      }
      return bgr;
    }
  }

  cv::Mat raw = msg->getFrame();
  if (raw.empty()) {
    return cv::Mat();
  }
  if (raw.type() != CV_8UC1) {
    raw.convertTo(raw, CV_8U);
  }
  if (raw.rows == 1 || raw.cols == 1) {
    raw = raw.reshape(1, height * 3 / 2);
  }

  cv::Mat bgr;
  if (raw.rows == height * 3 / 2 && raw.cols == width) {
    cv::cvtColor(raw, bgr, cv::COLOR_YUV2BGR_NV12);
  } else if (raw.rows == height && raw.cols == width) {
    cv::cvtColor(raw, bgr, cv::COLOR_GRAY2BGR);
  }
  return bgr;
}

cv::Mat DepthToU16(const std::shared_ptr<dai::ImgFrame>& msg) {
  cv::Mat frame = msg->getFrame();
  if (frame.empty()) {
    return cv::Mat();
  }
  if (frame.type() == CV_16UC1) {
    return frame.clone();
  }
  if (frame.type() == CV_16SC1) {
    cv::Mat out;
    frame.convertTo(out, CV_16U);
    return out;
  }
  if (frame.depth() == CV_16U && frame.channels() == 1) {
    return frame.clone();
  }
  return cv::Mat();
}

dai::ImageFiltersConfig BuildHostFilterConfig(const OakRgbdConfig& cfg) {
  dai::ImageFiltersConfig config;

  if (!cfg.enable_post_processing) {
    return config;
  }

  if (cfg.enable_speckle_filter) {
    dai::SpeckleFilterParams speckle;
    speckle.enable = true;
    speckle.speckleRange = cfg.speckle_range;
    speckle.differenceThreshold = cfg.speckle_diff;
    config.insertFilter(speckle);
  }

  if (cfg.enable_spatial_filter) {
    dai::SpatialFilterParams spatial;
    spatial.enable = true;
    spatial.alpha = cfg.spatial_alpha;
    spatial.delta = cfg.spatial_delta;
    spatial.holeFillingRadius = cfg.spatial_hole_radius;
    spatial.numIterations = cfg.spatial_iterations;
    config.insertFilter(spatial);
  }

  return config;
}

void PrintConnectedCameras(dai::Device& device) {
  std::cout << "Connected cameras:" << std::endl;
  for (const auto& cam : device.getConnectedCameraFeatures()) {
    std::cout << "  socket=" << static_cast<int>(cam.socket)
              << ", sensor=" << cam.sensorName
              << ", " << cam.width << "x" << cam.height << std::endl;
  }
}

}  // namespace

OakRgbdCapture::OakRgbdCapture(const OakRgbdConfig& cfg) : cfg_(cfg) {}

OakRgbdCapture::~OakRgbdCapture() {
  Stop();
}

bool OakRgbdCapture::Start() {
  if (running_.load()) {
    return true;
  }

  {
    std::lock_guard<std::mutex> lock(start_mutex_);
    start_reported_ = false;
    start_ok_ = false;
    last_error_.clear();
  }

  running_.store(true);
  worker_ = std::thread(&OakRgbdCapture::CaptureLoop, this);

  std::unique_lock<std::mutex> lock(start_mutex_);
  start_cv_.wait(lock, [this] { return start_reported_; });
  return start_ok_;
}

void OakRgbdCapture::Stop() {
  running_.store(false);
  if (worker_.joinable()) {
    worker_.join();
  }
}

bool OakRgbdCapture::TryGetLatest(TimedRgbdFrame& out) {
  std::lock_guard<std::mutex> lock(latest_mutex_);
  if (!has_new_frame_) {
    return false;
  }
  out.timestamp_sec = latest_.timestamp_sec;
  out.pair_dt_ms = latest_.pair_dt_ms;
  out.bgr = latest_.bgr.clone();
  out.depth_mm = latest_.depth_mm.clone();
  has_new_frame_ = false;
  return true;
}

cv::Mat OakRgbdCapture::GetCameraMatrix() const {
  std::lock_guard<std::mutex> lock(latest_mutex_);
  return K_.clone();
}

cv::Mat OakRgbdCapture::GetCameraMatrixInv() const {
  std::lock_guard<std::mutex> lock(latest_mutex_);
  return K_inv_.clone();
}

std::string OakRgbdCapture::GetLastError() const {
  std::lock_guard<std::mutex> lock(start_mutex_);
  return last_error_;
}

void OakRgbdCapture::ReportStart(bool ok, const std::string& error_message) {
  {
    std::lock_guard<std::mutex> lock(start_mutex_);
    start_ok_ = ok;
    start_reported_ = true;
    last_error_ = error_message;
  }
  start_cv_.notify_all();
}

void OakRgbdCapture::PublishFrame(const TimedRgbdFrame& frame) {
  std::lock_guard<std::mutex> lock(latest_mutex_);
  latest_.timestamp_sec = frame.timestamp_sec;
  latest_.pair_dt_ms = frame.pair_dt_ms;
  latest_.bgr = frame.bgr.clone();
  latest_.depth_mm = frame.depth_mm.clone();
  has_new_frame_ = true;
}

void OakRgbdCapture::CaptureLoop() {
  try {
    setenv("DEPTHAI_AUTOCALIBRATION", "OFF", 1);

    dai::Device::Config device_config;
    device_config.board.gpio[6] = dai::BoardConfig::GPIO(
        dai::BoardConfig::GPIO::Direction::OUTPUT,
        dai::BoardConfig::GPIO::Level::HIGH);

    auto device = std::make_shared<dai::Device>(device_config);
    std::cout << "DepthAI USB speed enum: " << static_cast<int>(device->getUsbSpeed()) << std::endl;
    PrintConnectedCameras(*device);

    auto calibration = device->readCalibration();
    auto intrinsics = calibration.getCameraIntrinsics(
        dai::CameraBoardSocket::CAM_A, cfg_.rgb_width, cfg_.rgb_height);

    cv::Mat K = cv::Mat::eye(3, 3, CV_64F);
    K.at<double>(0, 0) = intrinsics[0][0];
    K.at<double>(1, 1) = intrinsics[1][1];
    K.at<double>(0, 2) = intrinsics[0][2];
    K.at<double>(1, 2) = intrinsics[1][2];
    {
      std::lock_guard<std::mutex> lock(latest_mutex_);
      K_ = K.clone();
      K_inv_ = K.inv();
    }

    dai::Pipeline pipeline(device);

    auto cam_rgb = pipeline.create<dai::node::Camera>()->build(
        dai::CameraBoardSocket::CAM_A,
        std::make_pair(kRgbSensorWidth, kRgbSensorHeight),
        cfg_.fps);
    auto cam_left = pipeline.create<dai::node::Camera>()->build(
        dai::CameraBoardSocket::CAM_B,
        std::make_pair(cfg_.mono_width, cfg_.mono_height),
        cfg_.fps);
    auto cam_right = pipeline.create<dai::node::Camera>()->build(
        dai::CameraBoardSocket::CAM_C,
        std::make_pair(cfg_.mono_width, cfg_.mono_height),
        cfg_.fps);

    cam_rgb->initialControl.setFrameSyncMode(dai::CameraControl::FrameSyncMode::INPUT);
    cam_left->initialControl.setFrameSyncMode(dai::CameraControl::FrameSyncMode::OUTPUT);
    cam_right->initialControl.setFrameSyncMode(dai::CameraControl::FrameSyncMode::INPUT);

    cam_rgb->initialControl.setManualExposure(cfg_.exposure_us, cfg_.rgb_iso);
    cam_left->initialControl.setManualExposure(cfg_.exposure_us, cfg_.mono_iso);
    cam_right->initialControl.setManualExposure(cfg_.exposure_us, cfg_.mono_iso);
    cam_rgb->initialControl.setAntiBandingMode(dai::CameraControl::AntiBandingMode::OFF);
    cam_left->initialControl.setAntiBandingMode(dai::CameraControl::AntiBandingMode::OFF);
    cam_right->initialControl.setAntiBandingMode(dai::CameraControl::AntiBandingMode::OFF);

    auto rgb_out = cam_rgb->requestOutput(
        std::make_pair(cfg_.rgb_width, cfg_.rgb_height),
        dai::ImgFrame::Type::NV12,
        dai::ImgResizeMode::STRETCH,
        cfg_.fps,
        true);
    auto left_out = cam_left->requestOutput(
        std::make_pair(cfg_.mono_width, cfg_.mono_height),
        std::nullopt,
        dai::ImgResizeMode::STRETCH,
        cfg_.fps,
        false);
    auto right_out = cam_right->requestOutput(
        std::make_pair(cfg_.mono_width, cfg_.mono_height),
        std::nullopt,
        dai::ImgResizeMode::STRETCH,
        cfg_.fps,
        false);

    auto stereo = pipeline.create<dai::node::StereoDepth>();
    stereo->setInputResolution(cfg_.mono_width, cfg_.mono_height);
    stereo->setOutputSize(cfg_.rgb_width, cfg_.rgb_height);
    stereo->setOutputKeepAspectRatio(false);
    stereo->setDefaultProfilePreset(dai::node::StereoDepth::PresetMode::FAST_ACCURACY);
    stereo->setLeftRightCheck(true);
    stereo->setSubpixel(cfg_.subpixel);
    stereo->setExtendedDisparity(cfg_.extended_disparity);
    stereo->setFrameSync(true);
    stereo->initialConfig->setConfidenceThreshold(cfg_.confidence);
    stereo->inputConfig.setBlocking(false);

    left_out->link(stereo->left);
    right_out->link(stereo->right);

    // RVC2 path: align stereo depth to CAM_A RGB output through inputAlignTo.
    rgb_out->link(stereo->inputAlignTo);

    auto filters = pipeline.create<dai::node::ImageFilters>();
    filters->setRunOnHost(true);
    *filters->initialConfig = BuildHostFilterConfig(cfg_);
    stereo->depth.link(filters->input);

    auto rgb_queue = rgb_out->createOutputQueue(kOutputQueueSize, kOutputQueueBlocking);
    auto depth_queue = filters->output.createOutputQueue(kOutputQueueSize, kOutputQueueBlocking);

    std::cout << "\nOAK RGBD capture started" << std::endl;
    std::cout << "  FSYNC: CAM_B OUTPUT master, CAM_A/C INPUT" << std::endl;
    std::cout << "  RGB:   NV12 " << cfg_.rgb_width << "x" << cfg_.rgb_height
              << " @ " << cfg_.fps << " FPS" << std::endl;
    std::cout << "  Mono:  " << cfg_.mono_width << "x" << cfg_.mono_height
              << " @ " << cfg_.fps << " FPS" << std::endl;
    std::cout << "  Pair threshold: " << cfg_.pair_threshold_ms << " ms" << std::endl;
    std::cout << "  LR-check: enabled, depth aligned to CAM_A" << std::endl;
    std::cout << "  Depth source: StereoDepth raw aligned -> host ImageFilters" << std::endl;
    std::cout << "  Depth filters: host speckle -> spatial, median off, temporal off" << std::endl;

    ReportStart(true, "");

    std::deque<std::shared_ptr<dai::ImgFrame>> rgb_buf;
    std::deque<std::shared_ptr<dai::ImgFrame>> depth_buf;

    pipeline.start();
    while (running_.load() && pipeline.isRunning()) {
      bool got_any = false;

      while (true) {
        auto msg = rgb_queue->tryGet<dai::ImgFrame>();
        if (!msg) {
          break;
        }
        AppendAndTrim(rgb_buf, msg, kPairBufferLimit, dropped_rgb_);
        ++image_count_;
        got_any = true;
      }

      while (true) {
        auto msg = depth_queue->tryGet<dai::ImgFrame>();
        if (!msg) {
          break;
        }
        AppendAndTrim(depth_buf, msg, kPairBufferLimit, dropped_depth_);
        ++depth_count_;
        got_any = true;
      }

      if (!got_any) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
      }

      while (running_.load()) {
        std::shared_ptr<dai::ImgFrame> rgb_msg;
        std::shared_ptr<dai::ImgFrame> depth_msg;
        double pair_dt = 0.0;
        if (!PopClosestPair(rgb_buf, depth_buf, cfg_.pair_threshold_ms,
                            rgb_msg, depth_msg, pair_dt,
                            dropped_rgb_, dropped_depth_)) {
          break;
        }

        cv::Mat bgr = Nv12ToBgr(rgb_msg, cfg_.rgb_width, cfg_.rgb_height);
        cv::Mat depth = DepthToU16(depth_msg);

        if (bgr.empty() || depth.empty()) {
          continue;
        }
        if (bgr.cols != cfg_.rgb_width || bgr.rows != cfg_.rgb_height || bgr.type() != CV_8UC3) {
          std::cerr << "OAK RGB frame has unexpected format: "
                    << bgr.cols << "x" << bgr.rows << " type=" << bgr.type() << std::endl;
          continue;
        }
        if (depth.cols != cfg_.rgb_width || depth.rows != cfg_.rgb_height || depth.type() != CV_16UC1) {
          std::cerr << "OAK depth frame has unexpected format: "
                    << depth.cols << "x" << depth.rows << " type=" << depth.type() << std::endl;
          continue;
        }

        TimedRgbdFrame frame;
        frame.timestamp_sec = TimestampSec(rgb_msg);
        frame.pair_dt_ms = pair_dt;
        frame.bgr = std::move(bgr);
        frame.depth_mm = std::move(depth);
        PublishFrame(frame);
        ++paired_count_;
      }
    }

    pipeline.stop();
    pipeline.wait();
  } catch (const std::exception& e) {
    running_.store(false);
    bool need_report = false;
    {
      std::lock_guard<std::mutex> lock(start_mutex_);
      need_report = !start_reported_;
    }
    if (need_report) {
      ReportStart(false, e.what());
    } else {
      std::cerr << "OAK RGBD capture error: " << e.what() << std::endl;
    }
  }
}
