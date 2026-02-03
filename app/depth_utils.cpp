#include "depth_utils.h"

#include <algorithm>
#include <vector>

bool RobustDepthMedianU16(const cv::Mat& depth_mm, int x, int y, int r,
                          uint16_t& out_mm) {
  if (depth_mm.empty() || depth_mm.type() != CV_16UC1) return false;
  std::vector<uint16_t> vals;
  vals.reserve((2 * r + 1) * (2 * r + 1));
  for (int dy = -r; dy <= r; ++dy) {
    int yy = y + dy;
    if (yy < 0 || yy >= depth_mm.rows) continue;
    const uint16_t* row = depth_mm.ptr<uint16_t>(yy);
    for (int dx = -r; dx <= r; ++dx) {
      int xx = x + dx;
      if (xx < 0 || xx >= depth_mm.cols) continue;
      uint16_t z = row[xx];
      // filter invalid (0) / out-of-range
      if (z == 0 || z >= 10000) continue;
      vals.push_back(z);
    }
  }
  if (vals.size() < 6) return false;  // too few valid samples
  std::nth_element(vals.begin(), vals.begin() + vals.size() / 2, vals.end());
  out_mm = vals[vals.size() / 2];
  return true;
}
