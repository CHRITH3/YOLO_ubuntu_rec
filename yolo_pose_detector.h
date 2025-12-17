// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// YOLO Pose Detector Header
// Implements YOLOv8-pose inference using ONNX Runtime

#ifndef YOLO_POSE_DETECTOR_H_
#define YOLO_POSE_DETECTOR_H_

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>

// COCO 17 keypoints definition
enum KeypointType {
    NOSE = 0,
    LEFT_EYE = 1,
    RIGHT_EYE = 2,
    LEFT_EAR = 3,
    RIGHT_EAR = 4,
    LEFT_SHOULDER = 5,
    RIGHT_SHOULDER = 6,
    LEFT_ELBOW = 7,
    RIGHT_ELBOW = 8,
    LEFT_WRIST = 9,
    RIGHT_WRIST = 10,
    LEFT_HIP = 11,
    RIGHT_HIP = 12,
    LEFT_KNEE = 13,
    RIGHT_KNEE = 14,
    LEFT_ANKLE = 15,
    RIGHT_ANKLE = 16
};

// Single keypoint structure
struct KeyPoint {
    float x;              // X coordinate in image
    float y;              // Y coordinate in image
    float confidence;     // Keypoint confidence [0-1]
    cv::Point3f pos3d;    // 3D position (filled after depth fusion)

    KeyPoint() : x(0), y(0), confidence(0), pos3d(0, 0, 0) {}
    KeyPoint(float x_, float y_, float conf_)
        : x(x_), y(y_), confidence(conf_), pos3d(0, 0, 0) {}
};

// Pose detection result for one person
struct PoseResult {
    cv::Rect bbox;                    // Bounding box
    float box_confidence;             // Detection confidence
    std::vector<KeyPoint> keypoints;  // 17 keypoints
    int person_id;                    // Person ID for tracking (optional)

    PoseResult() : box_confidence(0), person_id(-1) {
        keypoints.resize(17);  // COCO 17 keypoints
    }
};

// YOLOv8 Pose Detector class
class YOLOPoseDetector {
public:
    /**
     * Constructor
     * @param model_path Path to ONNX model file
     * @param input_size Input image size (default 640)
     * @param conf_threshold Confidence threshold (default 0.5)
     * @param iou_threshold NMS IoU threshold (default 0.45)
     */
    YOLOPoseDetector(
        const std::string& model_path,
        int input_size = 640,
        float conf_threshold = 0.5f,
        float iou_threshold = 0.45f,
        bool use_cuda = true);  // 添加 CUDA 参数，默认启用

    ~YOLOPoseDetector();

    /**
     * Initialize detector and load model
     * @return true if successful, false otherwise
     */
    bool Init();

    /**
     * Detect poses in image
     * @param image Input BGR image
     * @return Vector of detected poses
     */
    std::vector<PoseResult> Detect(const cv::Mat& image);

    /**
     * Set confidence threshold
     */
    void SetConfThreshold(float threshold) { conf_threshold_ = threshold; }

    /**
     * Set NMS IoU threshold
     */
    void SetIoUThreshold(float threshold) { iou_threshold_ = threshold; }

    /**
     * Get model input size
     */
    int GetInputSize() const { return input_size_; }

private:
    /**
     * Preprocess image for inference
     * BGR -> RGB, resize, normalize, HWC -> CHW
     * @param image Input BGR image
     * @param input_data Output float array
     */
    void Preprocess(const cv::Mat& image, std::vector<float>& input_data);

    /**
     * Postprocess model output
     * Parse output tensor, apply NMS, convert coordinates
     * @param output Raw output data from model
     * @param original_size Original image size
     * @return Vector of detected poses
     */
    std::vector<PoseResult> Postprocess(
        float* output,
        const cv::Size& original_size);

    /**
     * Apply Non-Maximum Suppression
     * @param proposals All detections before NMS
     * @return Filtered detections after NMS
     */
    std::vector<PoseResult> NonMaximumSuppression(
        std::vector<PoseResult>& proposals);

    /**
     * Calculate IoU between two bounding boxes
     */
    float CalculateIoU(const cv::Rect& box1, const cv::Rect& box2);

    /**
     * Rescale coordinates from model input size to original image size
     */
    void RescaleCoordinates(
        PoseResult& result,
        const cv::Size& original_size);

private:
    std::string model_path_;          // ONNX model path
    int input_size_;                  // Model input size (640x640)
    float conf_threshold_;            // Confidence threshold
    float iou_threshold_;             // NMS IoU threshold
    bool use_cuda_;                   // 是否使用 CUDA 加速

    // ONNX Runtime components
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
    Ort::AllocatorWithDefaultOptions allocator_;

    // Model info
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<const char*> input_names_ptrs_;
    std::vector<const char*> output_names_ptrs_;
    std::vector<int64_t> input_shape_;
    std::vector<int64_t> output_shape_;

    // Image preprocessing parameters
    float scale_factor_;              // Resize scale factor
    int pad_w_;                       // Padding width
    int pad_h_;                       // Padding height

    bool initialized_;
};

#endif  // YOLO_POSE_DETECTOR_H_
