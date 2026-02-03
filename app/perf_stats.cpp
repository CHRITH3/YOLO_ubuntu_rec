#include "perf_stats.h"

#include <iomanip>
#include <iostream>
#include <utility>

PerfStats g_perf_stats;

void PerfStats::AddInference(double ms) {
  inference_ms.push_back(ms);
  if (inference_ms.size() > MAX_SAMPLES) inference_ms.pop_front();
}

void PerfStats::AddDepthMap(double ms) {
  depth_map_ms.push_back(ms);
  if (depth_map_ms.size() > MAX_SAMPLES) depth_map_ms.pop_front();
}

void PerfStats::AddLandingDetect(double ms) {
  landing_detect_ms.push_back(ms);
  if (landing_detect_ms.size() > MAX_SAMPLES) landing_detect_ms.pop_front();
}

void PerfStats::PrintIfNeeded() {
  auto now = std::chrono::steady_clock::now();
  if (!initialized) {
    last_print_time = now;
    initialized = true;
    return;
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_print_time).count();
  if (elapsed >= 2) {
    PrintStats();
    last_print_time = now;
  }
}

void PerfStats::PrintStats() {
  auto calc_stats = [](const std::deque<double>& data) -> std::pair<double, double> {
    if (data.empty()) return {0, 0};
    double sum = 0, max_val = 0;
    for (double v : data) { sum += v; if (v > max_val) max_val = v; }
    return {sum / data.size(), max_val};
  };

  auto [inf_avg, inf_max] = calc_stats(inference_ms);
  auto [dep_avg, dep_max] = calc_stats(depth_map_ms);
  auto [land_avg, land_max] = calc_stats(landing_detect_ms);

  std::cout << "[Perf] Inference: " << std::fixed << std::setprecision(1)
            << inf_avg << "/" << inf_max << "ms"
            << " | Depth: " << dep_avg << "/" << dep_max << "ms"
            << " | Landing: " << land_avg << "/" << land_max << "ms" << std::endl;
}
