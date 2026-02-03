#ifndef PERF_STATS_H_
#define PERF_STATS_H_

#include <chrono>
#include <deque>

// Performance timing statistics
struct PerfStats {
  std::deque<double> inference_ms;
  std::deque<double> depth_map_ms;
  std::deque<double> landing_detect_ms;
  static constexpr size_t MAX_SAMPLES = 100;
  std::chrono::steady_clock::time_point last_print_time;
  bool initialized = false;

  void AddInference(double ms);
  void AddDepthMap(double ms);
  void AddLandingDetect(double ms);
  void PrintIfNeeded();
  void PrintStats();
};

extern PerfStats g_perf_stats;

#endif  // PERF_STATS_H_
