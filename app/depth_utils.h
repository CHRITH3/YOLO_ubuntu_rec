#ifndef DEPTH_UTILS_H_
#define DEPTH_UTILS_H_

#include <cstdint>

#include <opencv2/opencv.hpp>

// Robust depth sampling (median in a local window), ignoring invalid depth
// 深度中值
bool RobustDepthMedianU16(const cv::Mat& depth_mm, int x, int y, int r,
                          uint16_t& out_mm);

#endif  // DEPTH_UTILS_H_
