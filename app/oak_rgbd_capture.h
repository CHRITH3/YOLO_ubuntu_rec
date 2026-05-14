#pragma once

#include <opencv2/opencv.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

struct OakRgbdConfig {
  int rgb_width = 640;
  int rgb_height = 400;
  int mono_width = 640;
  int mono_height = 400;
  float fps = 50.0f;
  double pair_threshold_ms = 5.0;
  int exposure_us = 5000;
  int rgb_iso = 1200;
  int mono_iso = 400;
  int confidence = 180;
  bool subpixel = false;
  bool extended_disparity = false;
  bool enable_post_processing = true;
  bool enable_speckle_filter = true;
  int speckle_range = 48;
  int speckle_diff = 2;
  bool enable_spatial_filter = true;
  float spatial_alpha = 0.50f;
  int spatial_delta = 3;
  int spatial_hole_radius = 2;
  int spatial_iterations = 1;
};

struct TimedRgbdFrame {
  double timestamp_sec = -1.0;
  double pair_dt_ms = 0.0;
  cv::Mat bgr;
  cv::Mat depth_mm;
};

class OakRgbdCapture {
 public:
  explicit OakRgbdCapture(const OakRgbdConfig& cfg = OakRgbdConfig());
  ~OakRgbdCapture();

  OakRgbdCapture(const OakRgbdCapture&) = delete;
  OakRgbdCapture& operator=(const OakRgbdCapture&) = delete;

  bool Start();
  void Stop();
  bool TryGetLatest(TimedRgbdFrame& out);

  cv::Mat GetCameraMatrix() const;
  cv::Mat GetCameraMatrixInv() const;
  std::string GetLastError() const;

  int ImageCount() const { return image_count_.load(); }
  int DepthCount() const { return depth_count_.load(); }
  int PairedCount() const { return paired_count_.load(); }
  int DroppedRgbCount() const { return dropped_rgb_.load(); }
  int DroppedDepthCount() const { return dropped_depth_.load(); }

 private:
  void CaptureLoop();
  void ReportStart(bool ok, const std::string& error_message);
  void PublishFrame(const TimedRgbdFrame& frame);

  OakRgbdConfig cfg_;
  std::atomic<bool> running_{false};
  std::thread worker_;

  mutable std::mutex latest_mutex_;
  TimedRgbdFrame latest_;
  bool has_new_frame_ = false;
  cv::Mat K_;
  cv::Mat K_inv_;

  std::atomic<int> image_count_{0};
  std::atomic<int> depth_count_{0};
  std::atomic<int> paired_count_{0};
  std::atomic<int> dropped_rgb_{0};
  std::atomic<int> dropped_depth_{0};

  mutable std::mutex start_mutex_;
  std::condition_variable start_cv_;
  bool start_reported_ = false;
  bool start_ok_ = false;
  std::string last_error_;
};
