// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// Pose Utilities Implementation

#include "pose_utils.h"
#include <cmath>
#include <sstream>
#include <iomanip>

std::vector<SkeletonConnection> GetCocoSkeleton() {
    // Define skeleton connections with colors
    // Format: {start_keypoint_idx, end_keypoint_idx, color}
    return {
        // Head (yellow)
        {NOSE, LEFT_EYE, cv::Scalar(0, 255, 255)},
        {NOSE, RIGHT_EYE, cv::Scalar(0, 255, 255)},
        {LEFT_EYE, LEFT_EAR, cv::Scalar(0, 255, 255)},
        {RIGHT_EYE, RIGHT_EAR, cv::Scalar(0, 255, 255)},

        // Torso (cyan)
        {LEFT_SHOULDER, RIGHT_SHOULDER, cv::Scalar(255, 255, 0)},
        {LEFT_SHOULDER, LEFT_HIP, cv::Scalar(255, 255, 0)},
        {RIGHT_SHOULDER, RIGHT_HIP, cv::Scalar(255, 255, 0)},
        {LEFT_HIP, RIGHT_HIP, cv::Scalar(255, 255, 0)},

        // Left arm (green)
        {LEFT_SHOULDER, LEFT_ELBOW, cv::Scalar(0, 255, 0)},
        {LEFT_ELBOW, LEFT_WRIST, cv::Scalar(0, 255, 0)},

        // Right arm (blue)
        {RIGHT_SHOULDER, RIGHT_ELBOW, cv::Scalar(255, 0, 0)},
        {RIGHT_ELBOW, RIGHT_WRIST, cv::Scalar(255, 0, 0)},

        // Left leg (magenta)
        {LEFT_HIP, LEFT_KNEE, cv::Scalar(255, 0, 255)},
        {LEFT_KNEE, LEFT_ANKLE, cv::Scalar(255, 0, 255)},

        // Right leg (orange)
        {RIGHT_HIP, RIGHT_KNEE, cv::Scalar(0, 165, 255)},
        {RIGHT_KNEE, RIGHT_ANKLE, cv::Scalar(0, 165, 255)}
    };
}

void MapPoseTo3D(
    std::vector<PoseResult>& poses,
    const cv::Mat& depth,
    const cv::Mat& camera_matrix) {

    if (depth.empty() || camera_matrix.empty()) {
        return;
    }

    // Extract camera intrinsics
    double fx = camera_matrix.at<double>(0, 0);
    double fy = camera_matrix.at<double>(1, 1);
    double cx = camera_matrix.at<double>(0, 2);
    double cy = camera_matrix.at<double>(1, 2);

    for (auto& pose : poses) {
        for (auto& kp : pose.keypoints) {
            // Skip if confidence too low
            if (kp.confidence < 0.3f) {
                continue;
            }

            int x = cvRound(kp.x);
            int y = cvRound(kp.y);

            // Check bounds
            if (x < 0 || x >= depth.cols || y < 0 || y >= depth.rows) {
                continue;
            }

            // Get depth value (in millimeters)
            ushort Z_mm = depth.at<ushort>(y, x);

            // Check if depth is valid
            if (Z_mm >= 10000 || Z_mm == 0) {
                continue;
            }

            // Convert to meters for calculations
            double Z = Z_mm / 1000.0;

            // Project to 3D (in left camera coordinate system)
            // X = (u - cx) * Z / fx
            // Y = (v - cy) * Z / fy
            // Z = Z
            double X = (x - cx) * Z / fx;
            double Y = (y - cy) * Z / fy;

            // Store 3D position (in millimeters)
            kp.pos3d.x = static_cast<float>(X * 1000.0);
            kp.pos3d.y = static_cast<float>(Y * 1000.0);
            kp.pos3d.z = static_cast<float>(Z * 1000.0);
        }
    }
}

void DrawPoses(
    cv::Mat& image,
    const std::vector<PoseResult>& poses,
    bool show_bbox,
    bool show_keypoints,
    bool show_skeleton,
    float keypoint_conf_threshold) {

    auto skeleton = GetCocoSkeleton();

    for (const auto& pose : poses) {
        // Draw bounding box
        if (show_bbox) {
            cv::rectangle(image, pose.bbox, cv::Scalar(0, 255, 0), 2);

            // Draw confidence
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << pose.box_confidence;
            cv::putText(
                image, ss.str(),
                cv::Point(pose.bbox.x, pose.bbox.y - 5),
                cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 255, 0), 2);
        }

        // Draw skeleton connections
        if (show_skeleton) {
            for (const auto& conn : skeleton) {
                const auto& kp1 = pose.keypoints[conn.start_idx];
                const auto& kp2 = pose.keypoints[conn.end_idx];

                if (kp1.confidence > keypoint_conf_threshold &&
                    kp2.confidence > keypoint_conf_threshold) {
                    cv::line(
                        image,
                        cv::Point(cvRound(kp1.x), cvRound(kp1.y)),
                        cv::Point(cvRound(kp2.x), cvRound(kp2.y)),
                        conn.color, 2);
                }
            }
        }

        // Draw keypoints
        if (show_keypoints) {
            for (size_t i = 0; i < pose.keypoints.size(); i++) {
                const auto& kp = pose.keypoints[i];

                if (kp.confidence > keypoint_conf_threshold) {
                    // Color based on confidence
                    cv::Scalar color;
                    if (kp.confidence > 0.8f) {
                        color = cv::Scalar(0, 0, 255);  // Red - high confidence
                    } else if (kp.confidence > 0.6f) {
                        color = cv::Scalar(0, 165, 255);  // Orange - medium
                    } else {
                        color = cv::Scalar(0, 255, 255);  // Yellow - low
                    }

                    cv::circle(
                        image,
                        cv::Point(cvRound(kp.x), cvRound(kp.y)),
                        5, color, -1);

                    // Optionally draw keypoint index
                    // cv::putText(image, std::to_string(i),
                    //     cv::Point(kp.x + 5, kp.y - 5),
                    //     cv::FONT_HERSHEY_SIMPLEX, 0.3, color, 1);
                }
            }
        }
    }
}

void DrawPoseInfo(
    cv::Mat& image,
    const std::vector<PoseResult>& poses,
    bool show_depth) {

    int y_offset = 30;
    int line_height = 25;

    for (size_t i = 0; i < poses.size(); i++) {
        const auto& pose = poses[i];

        // Person label
        std::ostringstream ss;
        ss << "Person " << (i + 1);
        cv::putText(
            image, ss.str(),
            cv::Point(10, y_offset),
            cv::FONT_HERSHEY_SIMPLEX, 0.6,
            cv::Scalar(255, 255, 255), 2);
        y_offset += line_height;

        // Confidence
        ss.str("");
        ss << "  Conf: " << std::fixed << std::setprecision(2)
           << pose.box_confidence;
        cv::putText(
            image, ss.str(),
            cv::Point(10, y_offset),
            cv::FONT_HERSHEY_SIMPLEX, 0.5,
            cv::Scalar(200, 200, 200), 1);
        y_offset += line_height;

        // 3D depth information
        if (show_depth) {
            // Calculate average depth from visible keypoints
            float sum_depth = 0;
            int count = 0;
            for (const auto& kp : pose.keypoints) {
                if (kp.confidence > 0.5f && kp.pos3d.z > 0) {
                    sum_depth += kp.pos3d.z;
                    count++;
                }
            }

            if (count > 0) {
                float avg_depth = sum_depth / count;
                ss.str("");
                ss << "  Depth: " << std::fixed << std::setprecision(2)
                   << (avg_depth / 1000.0) << " m";
                cv::putText(
                    image, ss.str(),
                    cv::Point(10, y_offset),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(200, 200, 200), 1);
                y_offset += line_height;
            }

            // Estimate height
            float height = EstimateBodyHeight(pose);
            if (height > 0) {
                ss.str("");
                ss << "  Height: " << std::fixed << std::setprecision(0)
                   << height << " mm";
                cv::putText(
                    image, ss.str(),
                    cv::Point(10, y_offset),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(200, 200, 200), 1);
                y_offset += line_height;
            }
        }

        y_offset += 10;  // Space between people
    }
}

std::string GetKeypointName(int idx) {
    static const std::vector<std::string> names = {
        "Nose", "Left Eye", "Right Eye", "Left Ear", "Right Ear",
        "Left Shoulder", "Right Shoulder", "Left Elbow", "Right Elbow",
        "Left Wrist", "Right Wrist", "Left Hip", "Right Hip",
        "Left Knee", "Right Knee", "Left Ankle", "Right Ankle"
    };

    if (idx >= 0 && idx < static_cast<int>(names.size())) {
        return names[idx];
    }
    return "Unknown";
}

float Calculate3DDistance(const KeyPoint& kp1, const KeyPoint& kp2) {
    // Check if both keypoints have valid 3D coordinates
    if (kp1.pos3d.z <= 0 || kp2.pos3d.z <= 0) {
        return -1.0f;
    }

    // Euclidean distance in 3D
    float dx = kp1.pos3d.x - kp2.pos3d.x;
    float dy = kp1.pos3d.y - kp2.pos3d.y;
    float dz = kp1.pos3d.z - kp2.pos3d.z;

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float EstimateBodyHeight(const PoseResult& pose) {
    // Try multiple methods to estimate height

    // Method 1: Top of head to ankle
    const auto& nose = pose.keypoints[NOSE];
    const auto& left_ankle = pose.keypoints[LEFT_ANKLE];
    const auto& right_ankle = pose.keypoints[RIGHT_ANKLE];

    float height = -1.0f;

    // Use the ankle with higher confidence
    const KeyPoint* ankle = nullptr;
    if (left_ankle.confidence > right_ankle.confidence &&
        left_ankle.confidence > 0.5f) {
        ankle = &left_ankle;
    } else if (right_ankle.confidence > 0.5f) {
        ankle = &right_ankle;
    }

    if (ankle != nullptr && nose.confidence > 0.5f) {
        height = Calculate3DDistance(nose, *ankle);
    }

    // Method 2: If ankles not visible, try knee to head
    if (height < 0) {
        const auto& left_knee = pose.keypoints[LEFT_KNEE];
        const auto& right_knee = pose.keypoints[RIGHT_KNEE];

        const KeyPoint* knee = nullptr;
        if (left_knee.confidence > right_knee.confidence &&
            left_knee.confidence > 0.5f) {
            knee = &left_knee;
        } else if (right_knee.confidence > 0.5f) {
            knee = &right_knee;
        }

        if (knee != nullptr && nose.confidence > 0.5f) {
            float partial_height = Calculate3DDistance(nose, *knee);
            if (partial_height > 0) {
                // Estimate full height (knee to head is roughly 70% of full height)
                height = partial_height / 0.7f;
            }
        }
    }

    return height;
}
