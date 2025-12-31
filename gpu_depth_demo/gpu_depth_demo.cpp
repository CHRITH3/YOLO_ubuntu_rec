/**
 * GPU Depth Calculation Demo
 *
 * This demo demonstrates how to use GPU-accelerated stereo matching
 * to compute depth maps from INDEMIND stereo camera.
 *
 * Expected performance improvement:
 *   - INDEMIND SDK (CPU): ~7 FPS
 *   - OpenCV CUDA StereoBM: 50-100 FPS
 *   - OpenCV CUDA StereoSGM: 30-60 FPS
 *
 * Requirements:
 *   - OpenCV compiled with CUDA support
 *   - NVIDIA GPU with CUDA
 *   - INDEMIND SDK
 */

#include "imrdata.h"
#include "imrsdk.h"
#include "types.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <queue>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <atomic>

// Try to include CUDA stereo headers
#ifdef HAVE_CUDA
#include <opencv2/cudastereo.hpp>
#include <opencv2/cudaimgproc.hpp>
#endif

using namespace indem;

// Configuration
#define MAX_QUEUE_SIZE 2
#define USE_SGBM 0  // Set to 1 for StereoSGBM (higher quality, slower)

// Global variables for camera parameters
static double g_baseline = 0.12;  // Default baseline in meters
static double g_fx = 0;           // Focal length

// Stereo matcher (CPU fallback)
static cv::Ptr<cv::StereoSGBM> cpu_sgbm;
static cv::Ptr<cv::StereoBM> cpu_bm;

#ifdef HAVE_CUDA
// GPU stereo matcher
static cv::Ptr<cv::cuda::StereoBM> gpu_bm;
static cv::Ptr<cv::cuda::StereoSGM> gpu_sgm;
#endif

// Statistics
static std::atomic<int> depth_count(0);
static std::atomic<int> dropped_depth(0);

/**
 * Initialize stereo matchers (both CPU and GPU versions)
 */
bool InitStereoMatchers(bool use_gpu) {
    // CPU fallback - always initialize
    int numDisparities = 128;  // Must be divisible by 16
    int blockSize = 9;

    cpu_bm = cv::StereoBM::create(numDisparities, blockSize);

    cpu_sgbm = cv::StereoSGBM::create(
        0,              // minDisparity
        numDisparities, // numDisparities
        blockSize,      // blockSize
        8 * blockSize * blockSize,   // P1
        32 * blockSize * blockSize,  // P2
        1,              // disp12MaxDiff
        63,             // preFilterCap
        10,             // uniquenessRatio
        100,            // speckleWindowSize
        32              // speckleRange
    );

#ifdef HAVE_CUDA
    if (use_gpu) {
        try {
            int cuda_devices = cv::cuda::getCudaEnabledDeviceCount();
            if (cuda_devices > 0) {
                std::cout << "Found " << cuda_devices << " CUDA device(s)" << std::endl;
                cv::cuda::printCudaDeviceInfo(0);

                // Initialize GPU stereo matchers
                gpu_bm = cv::cuda::createStereoBM(numDisparities, blockSize);

                // StereoSGM parameters: minDisparity, numDisparities, P1, P2, uniquenessRatio
                gpu_sgm = cv::cuda::createStereoSGM(0, numDisparities, 5, 8, 32);

                std::cout << "GPU stereo matchers initialized successfully!" << std::endl;
                return true;
            } else {
                std::cout << "No CUDA devices found, falling back to CPU" << std::endl;
                return false;
            }
        } catch (const cv::Exception& e) {
            std::cerr << "CUDA initialization error: " << e.what() << std::endl;
            std::cout << "Falling back to CPU stereo matching" << std::endl;
            return false;
        }
    }
#else
    (void)use_gpu;
    std::cout << "OpenCV not compiled with CUDA support" << std::endl;
    std::cout << "Using CPU stereo matching" << std::endl;
#endif
    return false;
}

/**
 * Compute disparity using GPU (if available) or CPU
 */
cv::Mat ComputeDisparity(const cv::Mat& left_gray, const cv::Mat& right_gray,
                         bool use_gpu, bool use_sgm) {
    cv::Mat disparity;

#ifdef HAVE_CUDA
    if (use_gpu && gpu_bm) {
        try {
            cv::cuda::GpuMat d_left, d_right, d_disparity;
            d_left.upload(left_gray);
            d_right.upload(right_gray);

            if (use_sgm && gpu_sgm) {
                gpu_sgm->compute(d_left, d_right, d_disparity);
            } else {
                gpu_bm->compute(d_left, d_right, d_disparity);
            }

            d_disparity.download(disparity);
            return disparity;
        } catch (const cv::Exception& e) {
            std::cerr << "GPU compute error: " << e.what() << std::endl;
            // Fall through to CPU
        }
    }
#else
    (void)use_gpu;
    (void)use_sgm;
#endif

    // CPU fallback
    if (use_sgm || USE_SGBM) {
        cpu_sgbm->compute(left_gray, right_gray, disparity);
    } else {
        cpu_bm->compute(left_gray, right_gray, disparity);
    }

    return disparity;
}

/**
 * Convert disparity to depth map
 * Formula: depth = baseline * fx / disparity
 */
cv::Mat DisparityToDepth(const cv::Mat& disparity, double baseline, double fx) {
    cv::Mat depth(disparity.size(), CV_16U, cv::Scalar(0));

    // Disparity from OpenCV stereo matchers is in fixed-point format (multiplied by 16)
    double scale = 16.0;

    for (int y = 0; y < disparity.rows; y++) {
        for (int x = 0; x < disparity.cols; x++) {
            float d = static_cast<float>(disparity.at<short>(y, x)) / scale;

            if (d > 1.0f) {
                // depth = baseline * fx / disparity (in mm)
                float depth_val = (baseline * fx * 1000.0f) / d;

                // Clamp to valid range
                if (depth_val > 0 && depth_val < 10000) {
                    depth.at<ushort>(y, x) = static_cast<ushort>(depth_val);
                }
            }
        }
    }

    return depth;
}

/**
 * Create colorized depth visualization
 */
cv::Mat ColorizeDepth(const cv::Mat& depth, double min_depth = 200, double max_depth = 5000) {
    cv::Mat normalized;
    depth.convertTo(normalized, CV_8U, 255.0 / (max_depth - min_depth),
                    -255.0 * min_depth / (max_depth - min_depth));

    cv::Mat colored;
    cv::applyColorMap(normalized, colored, cv::COLORMAP_JET);

    // Set invalid depth (0) to black
    for (int y = 0; y < depth.rows; y++) {
        for (int x = 0; x < depth.cols; x++) {
            if (depth.at<ushort>(y, x) == 0) {
                colored.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 0);
            }
        }
    }

    return colored;
}

int main(int argc, char** argv) {
    std::cout << "\n=== GPU Depth Calculation Demo ===" << std::endl;
    std::cout << "This demo compares GPU vs CPU stereo matching performance\n" << std::endl;

    // Parse command line arguments
    bool use_gpu = true;
    bool use_sgm = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--cpu") {
            use_gpu = false;
        } else if (arg == "--sgm") {
            use_sgm = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --cpu    Use CPU stereo matching (default: GPU)" << std::endl;
            std::cout << "  --sgm    Use SGM algorithm (default: BM)" << std::endl;
            std::cout << "  --help   Show this help message" << std::endl;
            return 0;
        }
    }

    // Initialize INDEMIND SDK
    auto m_pSDK = new CIMRSDK();
    MRCONFIG config = {0};
    config.bSlam = false;
    config.imgResolution = IMG_640;
    config.imgFrequency = 50;
    config.imuFrequency = 0;

    std::cout << "Initializing INDEMIND camera..." << std::endl;
    if (!m_pSDK->Init(config)) {
        std::cerr << "ERROR: Failed to initialize INDEMIND SDK!" << std::endl;
        delete m_pSDK;
        return -1;
    }

    // Get camera parameters
    auto module_params = m_pSDK->GetModuleParams();
    auto param = module_params._left_camera[RESOLUTION::RES_640X400];

    g_baseline = module_params._baseline;
    g_fx = param._K[0];

    std::cout << "\nCamera Parameters:" << std::endl;
    std::cout << "  Baseline: " << g_baseline << " m" << std::endl;
    std::cout << "  Focal length (fx): " << g_fx << " pixels" << std::endl;
    std::cout << "  Resolution: 640x400" << std::endl;

    // Initialize stereo matchers
    bool gpu_available = InitStereoMatchers(use_gpu);
    use_gpu = use_gpu && gpu_available;

    std::cout << "\nStereo matching mode: "
              << (use_gpu ? "GPU" : "CPU") << " "
              << (use_sgm ? "SGM" : "BM") << std::endl;

    // Queues for synchronized frames
    std::queue<std::pair<cv::Mat, cv::Mat>> rect_queue;  // left, right rectified pairs
    std::mutex mutex_rect;

    // Enable rectified image processor
    if (!m_pSDK->EnableRectifyProcessor()) {
        std::cerr << "ERROR: Failed to enable rectify processor!" << std::endl;
        delete m_pSDK;
        return -1;
    }

    std::cout << "Rectify processor enabled." << std::endl;

    // Register rectified image callback
    m_pSDK->RegistRectifiedImgCallback([&](double time, cv::Mat left_rect, cv::Mat right_rect) {
        (void)time;
        if (!left_rect.empty() && !right_rect.empty()) {
            std::unique_lock<std::mutex> lock(mutex_rect);
            if (rect_queue.size() < MAX_QUEUE_SIZE) {
                rect_queue.push(std::make_pair(left_rect.clone(), right_rect.clone()));
            } else {
                ++dropped_depth;
            }
        }
    });

    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "  q/ESC : Quit" << std::endl;
    std::cout << "  g     : Toggle GPU/CPU mode" << std::endl;
    std::cout << "  s     : Toggle SGM/BM algorithm" << std::endl;
    std::cout << "  SPACE : Save current depth frame\n" << std::endl;

    // FPS calculation
    auto start_time = std::chrono::steady_clock::now();
    auto last_fps_time = start_time;
    int fps_frame_count = 0;
    double current_fps = 0;
    int frame_save_count = 0;

    cv::Mat depth_display;

    // Main loop
    while (true) {
        cv::Mat left_rect, right_rect;

        // Get rectified image pair
        {
            std::unique_lock<std::mutex> lock(mutex_rect);
            if (!rect_queue.empty()) {
                auto& pair = rect_queue.front();
                left_rect = pair.first;
                right_rect = pair.second;
                rect_queue.pop();
            }
        }

        if (!left_rect.empty() && !right_rect.empty()) {
            auto compute_start = std::chrono::steady_clock::now();

            // Convert to grayscale
            cv::Mat left_gray, right_gray;
            if (left_rect.channels() == 3) {
                cv::cvtColor(left_rect, left_gray, cv::COLOR_BGR2GRAY);
                cv::cvtColor(right_rect, right_gray, cv::COLOR_BGR2GRAY);
            } else {
                left_gray = left_rect;
                right_gray = right_rect;
            }

            // Compute disparity
            cv::Mat disparity = ComputeDisparity(left_gray, right_gray, use_gpu, use_sgm);

            // Convert to depth
            cv::Mat depth = DisparityToDepth(disparity, g_baseline, g_fx);

            auto compute_end = std::chrono::steady_clock::now();
            double compute_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                compute_end - compute_start).count();

            ++depth_count;
            ++fps_frame_count;

            // Calculate FPS
            auto current_time = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - last_fps_time).count() / 1000.0;

            if (elapsed >= 1.0) {
                current_fps = fps_frame_count / elapsed;
                fps_frame_count = 0;
                last_fps_time = current_time;
            }

            // Create visualization
            cv::Mat depth_colored = ColorizeDepth(depth);

            // Convert left image to color for display
            cv::Mat left_display;
            if (left_rect.channels() == 1) {
                cv::cvtColor(left_rect, left_display, cv::COLOR_GRAY2BGR);
            } else {
                left_display = left_rect.clone();
            }

            // Add info overlay
            std::ostringstream ss;
            ss << (use_gpu ? "GPU" : "CPU") << " " << (use_sgm ? "SGM" : "BM");
            cv::putText(depth_colored, ss.str(), cv::Point(10, 25),
                        cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(255, 255, 255), 2);

            ss.str("");
            ss << "FPS: " << std::fixed << std::setprecision(1) << current_fps;
            cv::putText(depth_colored, ss.str(), cv::Point(10, 50),
                        cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(0, 255, 0), 2);

            ss.str("");
            ss << "Compute: " << std::fixed << std::setprecision(1) << compute_time << " ms";
            cv::putText(depth_colored, ss.str(), cv::Point(10, 75),
                        cv::FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(0, 255, 255), 1);

            // Side-by-side display
            cv::hconcat(left_display, depth_colored, depth_display);

            cv::imshow("GPU Depth Demo - Left | Depth", depth_display);
        }

        // Handle keyboard
        char key = static_cast<char>(cv::waitKey(1));
        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        } else if (key == 'g' || key == 'G') {
#ifdef HAVE_CUDA
            if (gpu_bm) {
                use_gpu = !use_gpu;
                std::cout << "Switched to " << (use_gpu ? "GPU" : "CPU") << " mode" << std::endl;
            } else {
                std::cout << "GPU not available" << std::endl;
            }
#else
            std::cout << "GPU not available (OpenCV not compiled with CUDA)" << std::endl;
#endif
        } else if (key == 's' || key == 'S') {
            use_sgm = !use_sgm;
            std::cout << "Switched to " << (use_sgm ? "SGM" : "BM") << " algorithm" << std::endl;
        } else if (key == ' ') {
            if (!depth_display.empty()) {
                std::ostringstream filename;
                filename << "gpu_depth_" << std::setfill('0') << std::setw(4)
                         << frame_save_count++ << ".jpg";
                cv::imwrite(filename.str(), depth_display);
                std::cout << "Saved: " << filename.str() << std::endl;
            }
        }
    }

    // Calculate statistics
    auto end_time = std::chrono::steady_clock::now();
    double total_time = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();

    delete m_pSDK;
    cv::destroyAllWindows();

    // Print statistics
    std::cout << "\n=== Performance Statistics ===" << std::endl;
    std::cout << "Total runtime: " << total_time << " seconds" << std::endl;
    std::cout << "Total depth frames: " << depth_count << std::endl;
    std::cout << "Dropped frames: " << dropped_depth << std::endl;
    if (total_time > 0) {
        std::cout << "Average depth FPS: " << (depth_count / total_time) << std::endl;
    }
    std::cout << "==============================\n" << std::endl;

    return 0;
}
