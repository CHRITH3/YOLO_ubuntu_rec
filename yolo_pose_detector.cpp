// Copyright 2020 Indemind Co., Ltd. All rights reserved.
//
// YOLO Pose Detector Implementation

#include "yolo_pose_detector.h"
#include <algorithm>
#include <cmath>
#include <iostream>

YOLOPoseDetector::YOLOPoseDetector(
    const std::string& model_path,
    int input_size,
    float conf_threshold,
    float iou_threshold)
    : model_path_(model_path),
      input_size_(input_size),
      conf_threshold_(conf_threshold),
      iou_threshold_(iou_threshold),
      env_(ORT_LOGGING_LEVEL_WARNING, "YOLOPose"),
      scale_factor_(1.0f),
      pad_w_(0),
      pad_h_(0),
      initialized_(false) {

    session_options_.SetIntraOpNumThreads(4);
    session_options_.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
}

YOLOPoseDetector::~YOLOPoseDetector() {
    // ONNX Runtime cleanup handled by smart pointers
}

bool YOLOPoseDetector::Init() {
    try {
        std::cout << "Initializing YOLO Pose Detector..." << std::endl;
        std::cout << "  Model: " << model_path_ << std::endl;
        std::cout << "  Input size: " << input_size_ << "x" << input_size_ << std::endl;

        // Create session
        session_ = std::make_unique<Ort::Session>(
            env_, model_path_.c_str(), session_options_);

        // Get input info
        size_t num_input_nodes = session_->GetInputCount();
        if (num_input_nodes != 1) {
            std::cerr << "Expected 1 input, got " << num_input_nodes << std::endl;
            return false;
        }

        Ort::TypeInfo input_type_info = session_->GetInputTypeInfo(0);
        auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        input_shape_ = tensor_info.GetShape();

        Ort::AllocatedStringPtr input_name_ptr = session_->GetInputNameAllocated(0, allocator_);
        input_names_.push_back(std::string(input_name_ptr.get()));
        input_names_ptrs_.push_back(input_names_[0].c_str());

        std::cout << "  Input name: " << input_names_[0] << std::endl;
        std::cout << "  Input shape: [";
        for (size_t i = 0; i < input_shape_.size(); i++) {
            std::cout << input_shape_[i];
            if (i < input_shape_.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;

        // Get output info
        size_t num_output_nodes = session_->GetOutputCount();
        if (num_output_nodes != 1) {
            std::cerr << "Expected 1 output, got " << num_output_nodes << std::endl;
            return false;
        }

        Ort::TypeInfo output_type_info = session_->GetOutputTypeInfo(0);
        auto output_tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
        output_shape_ = output_tensor_info.GetShape();

        Ort::AllocatedStringPtr output_name_ptr = session_->GetOutputNameAllocated(0, allocator_);
        output_names_.push_back(std::string(output_name_ptr.get()));
        output_names_ptrs_.push_back(output_names_[0].c_str());

        std::cout << "  Output name: " << output_names_[0] << std::endl;
        std::cout << "  Output shape: [";
        for (size_t i = 0; i < output_shape_.size(); i++) {
            std::cout << output_shape_[i];
            if (i < output_shape_.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;

        std::cout << "✓ YOLO Pose Detector initialized successfully" << std::endl;
        initialized_ = true;
        return true;

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
        return false;
    }
}

void YOLOPoseDetector::Preprocess(
    const cv::Mat& image,
    std::vector<float>& input_data) {

    // Letterbox resize (maintain aspect ratio)
    int img_h = image.rows;
    int img_w = image.cols;

    scale_factor_ = std::min(
        static_cast<float>(input_size_) / img_w,
        static_cast<float>(input_size_) / img_h);

    int new_w = static_cast<int>(img_w * scale_factor_);
    int new_h = static_cast<int>(img_h * scale_factor_);

    pad_w_ = (input_size_ - new_w) / 2;
    pad_h_ = (input_size_ - new_h) / 2;

    // Resize
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_w, new_h));

    // Add padding
    cv::Mat padded;
    cv::copyMakeBorder(
        resized, padded,
        pad_h_, input_size_ - new_h - pad_h_,
        pad_w_, input_size_ - new_w - pad_w_,
        cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

    // BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

    // Normalize to [0, 1] and convert to float
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    // HWC to CHW format
    input_data.resize(3 * input_size_ * input_size_);
    std::vector<cv::Mat> channels(3);
    cv::split(rgb, channels);

    int channel_size = input_size_ * input_size_;
    for (int c = 0; c < 3; c++) {
        std::memcpy(
            input_data.data() + c * channel_size,
            channels[c].data,
            channel_size * sizeof(float));
    }
}

std::vector<PoseResult> YOLOPoseDetector::Detect(const cv::Mat& image) {
    if (!initialized_) {
        std::cerr << "Detector not initialized! Call Init() first." << std::endl;
        return {};
    }

    if (image.empty()) {
        std::cerr << "Empty image!" << std::endl;
        return {};
    }

    // Preprocess
    std::vector<float> input_data;
    Preprocess(image, input_data);

    // Create input tensor
    std::vector<int64_t> input_shape = {1, 3, input_size_, input_size_};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_data.data(),
        input_data.size(),
        input_shape.data(),
        input_shape.size());

    // Run inference
    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr},
        input_names_ptrs_.data(),
        &input_tensor,
        1,
        output_names_ptrs_.data(),
        1);

    // Get output data
    float* output_data = output_tensors[0].GetTensorMutableData<float>();

    // Postprocess
    std::vector<PoseResult> results = Postprocess(output_data, image.size());

    return results;
}

std::vector<PoseResult> YOLOPoseDetector::Postprocess(
    float* output,
    const cv::Size& original_size) {

    // YOLOv8-pose output format: [1, 56, 8400]
    // 56 = 4 (bbox: cx, cy, w, h) + 1 (confidence) + 17*3 (keypoints: x, y, visibility)
    // Note: YOLOv8-pose uses visibility (0-1) not confidence for keypoints
    // 8400 = detection proposals

    int num_proposals = output_shape_[2];  // 8400
    int num_elements = output_shape_[1];   // 56

    std::vector<PoseResult> proposals;

    // Transpose from [1, 56, 8400] to [8400, 56] for easier access
    std::vector<float> transposed(num_proposals * num_elements);
    for (int i = 0; i < num_proposals; i++) {
        for (int j = 0; j < num_elements; j++) {
            transposed[i * num_elements + j] = output[j * num_proposals + i];
        }
    }

    // Parse each proposal
    for (int i = 0; i < num_proposals; i++) {
        float* ptr = transposed.data() + i * num_elements;

        // Extract bbox (center_x, center_y, width, height)
        float cx = ptr[0];
        float cy = ptr[1];
        float w = ptr[2];
        float h = ptr[3];

        // Extract confidence (person detection confidence)
        float conf = ptr[4];

        if (conf < conf_threshold_) {
            continue;
        }

        // Create result
        PoseResult result;
        result.box_confidence = conf;

        // Convert bbox from center format to corner format
        float x1 = cx - w / 2;
        float y1 = cy - h / 2;
        result.bbox = cv::Rect(
            static_cast<int>(x1),
            static_cast<int>(y1),
            static_cast<int>(w),
            static_cast<int>(h));

        // Extract 17 keypoints (x, y, visibility)
        // Keypoints start at index 5 (not 6!)
        // Format: x, y, visibility (where visibility is 0-1, similar to confidence)
        for (int k = 0; k < 17; k++) {
            float kp_x = ptr[5 + k * 3 + 0];
            float kp_y = ptr[5 + k * 3 + 1];
            float kp_visibility = ptr[5 + k * 3 + 2];

            result.keypoints[k] = KeyPoint(kp_x, kp_y, kp_visibility);
        }

        proposals.push_back(result);
    }

    // Apply NMS
    std::vector<PoseResult> nms_results = NonMaximumSuppression(proposals);

    // Rescale coordinates to original image size
    for (auto& result : nms_results) {
        RescaleCoordinates(result, original_size);
    }

    return nms_results;
}

std::vector<PoseResult> YOLOPoseDetector::NonMaximumSuppression(
    std::vector<PoseResult>& proposals) {

    if (proposals.empty()) {
        return {};
    }

    // Sort by confidence (descending)
    std::sort(proposals.begin(), proposals.end(),
        [](const PoseResult& a, const PoseResult& b) {
            return a.box_confidence > b.box_confidence;
        });

    std::vector<bool> suppressed(proposals.size(), false);
    std::vector<PoseResult> results;

    for (size_t i = 0; i < proposals.size(); i++) {
        if (suppressed[i]) {
            continue;
        }

        results.push_back(proposals[i]);

        // Suppress overlapping boxes
        for (size_t j = i + 1; j < proposals.size(); j++) {
            if (suppressed[j]) {
                continue;
            }

            float iou = CalculateIoU(proposals[i].bbox, proposals[j].bbox);
            if (iou > iou_threshold_) {
                suppressed[j] = true;
            }
        }
    }

    return results;
}

float YOLOPoseDetector::CalculateIoU(
    const cv::Rect& box1,
    const cv::Rect& box2) {

    int x1 = std::max(box1.x, box2.x);
    int y1 = std::max(box1.y, box2.y);
    int x2 = std::min(box1.x + box1.width, box2.x + box2.width);
    int y2 = std::min(box1.y + box1.height, box2.y + box2.height);

    int intersection_area = std::max(0, x2 - x1) * std::max(0, y2 - y1);
    int box1_area = box1.width * box1.height;
    int box2_area = box2.width * box2.height;
    int union_area = box1_area + box2_area - intersection_area;

    return static_cast<float>(intersection_area) / union_area;
}

void YOLOPoseDetector::RescaleCoordinates(
    PoseResult& result,
    const cv::Size& original_size) {

    // Remove padding and rescale
    auto rescale = [&](float& x, float& y) {
        x = (x - pad_w_) / scale_factor_;
        y = (y - pad_h_) / scale_factor_;

        // Clamp to image bounds
        x = std::max(0.0f, std::min(x, static_cast<float>(original_size.width - 1)));
        y = std::max(0.0f, std::min(y, static_cast<float>(original_size.height - 1)));
    };

    // Rescale bbox
    float x1 = result.bbox.x;
    float y1 = result.bbox.y;
    float x2 = result.bbox.x + result.bbox.width;
    float y2 = result.bbox.y + result.bbox.height;

    rescale(x1, y1);
    rescale(x2, y2);

    result.bbox = cv::Rect(
        static_cast<int>(x1),
        static_cast<int>(y1),
        static_cast<int>(x2 - x1),
        static_cast<int>(y2 - y1));

    // Rescale keypoints
    for (auto& kp : result.keypoints) {
        rescale(kp.x, kp.y);
    }
}
