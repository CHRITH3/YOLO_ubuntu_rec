// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// Pose Utilities Header
// Functions for 3D mapping and visualization

#ifndef POSE_UTILS_H_
#define POSE_UTILS_H_

#include "yolo_pose_detector.h"
#include <opencv2/opencv.hpp>
#include <vector>

// Skeleton connections for COCO 17 keypoints
struct SkeletonConnection {
    int start_idx;
    int end_idx;
    cv::Scalar color;
};

/**
 * Get COCO skeleton connections
 * @return Vector of skeleton connections with colors
 */
std::vector<SkeletonConnection> GetCocoSkeleton();

/**
 * Map 2D keypoints to 3D space using depth map
 * @param poses Detected poses (will be modified with 3D coordinates)
 * @param depth Depth map in millimeters (CV_16U)
 * @param camera_matrix Camera intrinsic matrix K
 */
void MapPoseTo3D(
    std::vector<PoseResult>& poses,
    const cv::Mat& depth,
    const cv::Mat& camera_matrix);

/**
 * Draw poses on image with skeleton
 * @param image Image to draw on (will be modified)
 * @param poses Detected poses
 * @param show_bbox Whether to show bounding box
 * @param show_keypoints Whether to show keypoint circles
 * @param show_skeleton Whether to show skeleton lines
 * @param keypoint_conf_threshold Minimum confidence to show keypoint (default 0.5)
 */
void DrawPoses(
    cv::Mat& image,
    const std::vector<PoseResult>& poses,
    bool show_bbox = true,
    bool show_keypoints = true,
    bool show_skeleton = true,
    float keypoint_conf_threshold = 0.5f);

/**
 * Draw detailed pose information on image
 * @param image Image to draw on
 * @param poses Detected poses
 * @param show_depth Whether to show depth information
 */
void DrawPoseInfo(
    cv::Mat& image,
    const std::vector<PoseResult>& poses,
    bool show_depth = true);

/**
 * Get keypoint name from index
 * @param idx Keypoint index (0-16)
 * @return Keypoint name string
 */
std::string GetKeypointName(int idx);

/**
 * Calculate distance between two keypoints in 3D space
 * @param kp1 First keypoint
 * @param kp2 Second keypoint
 * @return Distance in millimeters (returns -1 if either keypoint is invalid)
 */
float Calculate3DDistance(const KeyPoint& kp1, const KeyPoint& kp2);

/**
 * Calculate approximate body height from pose
 * @param pose Pose result with 3D coordinates
 * @return Estimated height in millimeters (returns -1 if cannot calculate)
 */
float EstimateBodyHeight(const PoseResult& pose);

#endif  // POSE_UTILS_H_
