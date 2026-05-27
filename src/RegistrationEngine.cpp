#include "RegistrationEngine.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <cmath>
#include <random>
#include <algorithm>
#include <iostream>

RegistrationEngine::RegistrationEngine(int threshold, float overlap_err)
    : threshold_metric(threshold), overlap_error(overlap_err) {}

TransformResult RegistrationEngine::EstimateTranslationRANSAC(
    const std::vector<cv::Point2f>& pts1, 
    const std::vector<cv::Point2f>& pts2, 
    float max_distance, int max_trials, 
    double confidence) 
{
    TransformResult res;
    int n = pts1.size();
    if (n < 1) return res;

    float max_dist_sq = max_distance * max_distance;
    int best_inliers = 0;
    float best_tx = 0.0f;
    float best_ty = 0.0f;
    std::vector<uchar> best_mask(n, 0);

    // Limit trials to at most max_trials
    int trials = std::min(n, max_trials);
    
    // If n is small, evaluate all matches as translation hypotheses
    // If n is large, sample up to max_trials randomly
    bool run_random = (n > max_trials);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, n - 1);

    double conf_log = std::log(1.0 - confidence);
    int trial = 0;
    while (trial < trials) {
        int idx = run_random ? dist(rng) : trial;

        float tx = pts2[idx].x - pts1[idx].x;
        float ty = pts2[idx].y - pts1[idx].y;

        int inliers = 0;
        std::vector<uchar> mask(n, 0);
        for (int i = 0; i < n; ++i) {
            float dx = (pts1[i].x + tx) - pts2[i].x;
            float dy = (pts1[i].y + ty) - pts2[i].y;
            if (dx * dx + dy * dy <= max_dist_sq) {
                inliers++;
                mask[i] = 1;
            }
        }

        if (inliers > best_inliers) {
            best_inliers = inliers;
            best_tx = tx;
            best_ty = ty;
            best_mask = mask;

            // Dynamic early termination (RANSAC confidence formula for 1-point model)
            float w = static_cast<float>(best_inliers) / n;
            if (w > 0.0f) {
                if (w >= 0.999f) {
                    trials = 1; // Perfect fit, stop immediately
                } else {
                    double num = conf_log;
                    double den = std::log(1.0 - w);
                    if (std::abs(den) > 1e-6) {
                        int dynamic_trials = static_cast<int>(std::ceil(num / den));
                        if (dynamic_trials < trials) {
                            trials = std::max(5, dynamic_trials);
                        }
                    }
                }
            }
        }
        trial++;
    }

    // Re-estimate translation using the mean of all inliers
    if (best_inliers > 0) {
        double sum_tx = 0.0;
        double sum_ty = 0.0;
        for (int i = 0; i < n; ++i) {
            if (best_mask[i]) {
                sum_tx += (pts2[i].x - pts1[i].x);
                sum_ty += (pts2[i].y - pts1[i].y);
            }
        }
        res.tx = static_cast<float>(sum_tx / best_inliers);
        res.ty = static_cast<float>(sum_ty / best_inliers);
        res.inliers = best_inliers;
        res.status = (best_inliers >= 4) ? 0 : 2; // status 0: success, 2: not enough inliers
    } else {
        res.status = 2;
    }
    
    res.matched_count = n;
    return res;
}

TransformResult RegistrationEngine::TrySURF(
    const cv::Mat& I1, const cv::Mat& I2, 
    cv::Rect roi1, cv::Rect roi2, 
    float tx_min, float tx_max, float ty_min, float ty_max,
    int max_trials) 
{
    TransformResult res;
    
    cv::Mat img1 = I1(roi1);
    cv::Mat img2 = I2(roi2);
    
    // Create SURF detector matching MATLAB: nOctaves=4, nOctaveLayers=6
    cv::Ptr<cv::Feature2D> detector;
    int norm_type = cv::NORM_L2;
    std::string det_type = "SURF";
    try {
        detector = cv::xfeatures2d::SURF::create(
            static_cast<double>(threshold_metric), // hessianThreshold
            4,                                    // nOctaves
            6,                                    // nOctaveLayers
            false,                                // extended
            false                                 // upright
        );
        norm_type = cv::NORM_L2;
        det_type = "SURF";
    } catch (...) {
        try {
            detector = cv::SIFT::create();
            norm_type = cv::NORM_L2;
            det_type = "SIFT";
        } catch (...) {
            detector = cv::ORB::create();
            norm_type = cv::NORM_HAMMING;
            det_type = "ORB";
        }
    }
    
    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;
    
    cv::Mat img1_8u, img2_8u;
    img1.convertTo(img1_8u, CV_8U, 255.0);
    img2.convertTo(img2_8u, CV_8U, 255.0);

    try {
        if (!detector) return res;
        detector->detectAndCompute(img1_8u, cv::noArray(), kp1, desc1);
        detector->detectAndCompute(img2_8u, cv::noArray(), kp2, desc2);
    } catch (...) {
        std::cout << " [Try" << det_type << "] Exception during feature detection/computation." << std::endl;
        return res;
    }
    
    if (kp1.size() < 4 || kp2.size() < 4 || desc1.empty() || desc2.empty()) {
        std::cout << " [Try" << det_type << "] Not enough keypoints or descriptors (kp1=" << kp1.size() << ", kp2=" << kp2.size() << ")" << std::endl;
        return res;
    }
    
    // Unique matching using crossCheck=true
    cv::BFMatcher matcher(norm_type, true);
    std::vector<cv::DMatch> matches;
    try {
        matcher.match(desc2, desc1, matches);
    } catch (...) {
        std::cout << " [Try" << det_type << "] Exception during feature matching." << std::endl;
        return res;
    }
    
    if (matches.size() < 4) {
        std::cout << " [Try" << det_type << "] Not enough matches (matches=" << matches.size() << ")" << std::endl;
        return res;
    }
    
    std::vector<cv::Point2f> pts1, pts2;
    for (const auto& match : matches) {
        cv::Point2f p1 = kp1[match.trainIdx].pt;
        cv::Point2f p2 = kp2[match.queryIdx].pt;
        // Offset to original image coordinates
        p1.x += roi1.x; p1.y += roi1.y;
        p2.x += roi2.x; p2.y += roi2.y;
        pts1.push_back(p1);
        pts2.push_back(p2);
    }
    
    res = EstimateTranslationRANSAC(pts1, pts2, 1.5f, max_trials, 0.9999);
    
    if (res.status == 0) {
        // Validate against physical bounds constraints
        if (res.tx >= tx_min && res.tx <= tx_max && res.ty >= ty_min && res.ty <= ty_max) {
            std::cout << " [Try" << det_type << "] RANSAC SUCCEEDED. Inliers: " << res.inliers << "/" << res.matched_count 
                      << ", Tx: " << res.tx << ", Ty: " << res.ty << std::endl;
        } else {
            std::cout << " [Try" << det_type << "] RANSAC OUT OF BOUNDS. Tx: " << res.tx << " (limit [" << tx_min << ", " << tx_max 
                      << "]), Ty: " << res.ty << " (limit [" << ty_min << ", " << ty_max << "])" << std::endl;
            res.status = 1; // Mark failed due to bounds
        }
    } else {
        std::cout << " [Try" << det_type << "] RANSAC FAILED (status=" << res.status << ", matched=" << res.matched_count << ")" << std::endl;
    }
    
    return res;
}

TransformResult RegistrationEngine::TryPhaseCorrelation(
    const cv::Mat& I1, const cv::Mat& I2, 
    cv::Rect roi1, cv::Rect roi2, 
    float tx_min, float tx_max, float ty_min, float ty_max) 
{
    TransformResult res;
    
    cv::Mat img1 = I1(roi1);
    cv::Mat img2 = I2(roi2);
    
    cv::Mat gray1 = img1, gray2 = img2;
    cv::Mat f1, f2;
    gray1.convertTo(f1, CV_64F);
    gray2.convertTo(f2, CV_64F);
    
    cv::Mat window;
    cv::createHanningWindow(window, f1.size(), CV_64F);
    
    double response = 0.0;
    cv::Point2d shift;
    try {
        shift = cv::phaseCorrelate(f2, f1, window, &response);
    } catch (...) {
        return res;
    }
    
    float tx = static_cast<float>(roi2.x - roi1.x - shift.x);
    float ty = static_cast<float>(roi2.y - roi1.y - shift.y);
    
    if (tx >= tx_min && tx <= tx_max && ty >= ty_min && ty <= ty_max) {
        res.tx = tx;
        res.ty = ty;
        res.status = 0;
        res.inliers = 10; // Virtual inlier count matching MATLAB
        res.matched_count = 10;
    }
    
    return res;
}

TransformResult RegistrationEngine::ComputeNorthTransform(
    const cv::Mat& I1, const cv::Mat& I2, 
    float overlap_north, bool use_fast_stitch, 
    bool use_surf, bool use_pc, int tile_index) 
{
    TransformResult res;
    if (I1.empty() || I2.empty()) return res;
    
    int H = I1.rows;
    int W = I1.cols;
    
    // 1. Default physical translation
    float dy_nominal = std::round(H * (100.0f - overlap_north) / 100.0f);
    float dx_nominal = 0.0f;
    
    if (!use_surf && !use_pc) {
        res.tx = dx_nominal;
        res.ty = dy_nominal;
        res.status = 0;
        res.inliers = 1;
        res.matched_count = 1;
        return res;
    }
    
    // Physical range constraints
    float ty_min = H - (overlap_north + overlap_error) * H / 100.0f;
    float ty_max = H - (overlap_north - overlap_error) * H / 100.0f;
    float tx_min = -overlap_error * W / 100.0f;
    float tx_max = overlap_error * W / 100.0f;
    
    // Stage 1: Try fast stitch with 5% overlap if enabled
    if (use_fast_stitch && overlap_north > 5.0f) {
        float overlap_y = 5.0f;
        int y_pixel = static_cast<int>(std::round(H * overlap_y / 100.0f));
        cv::Rect roi1(0, 0, W, y_pixel);
        cv::Rect roi2(0, static_cast<int>(std::round(H * (100.0f - overlap_north) / 100.0f)), W, y_pixel);
        
        if (use_surf) {
            res = TrySURF(I1, I2, roi1, roi2, tx_min, tx_max, ty_min, ty_max, 2000);
            if (res.status == 0) {
                std::cout << "Tile " << tile_index << " (North): SURF SUCCEEDED (Fast). Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
                return res;
            }
        }
    }
    
    // Stage 2: Try full overlap SURF
    {
        int y_pixel = static_cast<int>(std::round(H * overlap_north / 100.0f));
        cv::Rect roi1(0, 0, W, y_pixel);
        cv::Rect roi2(0, static_cast<int>(std::round(H * (100.0f - overlap_north) / 100.0f)), W, y_pixel);
        
        if (use_surf) {
            res = TrySURF(I1, I2, roi1, roi2, tx_min, tx_max, ty_min, ty_max, 2000);
            if (res.status == 0) {
                std::cout << "Tile " << tile_index << " (North): SURF SUCCEEDED (Full). Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
                return res;
            }
        }
        
        // Stage 3: Phase Correlation (PC) fallback
        if (use_pc) {
            res = TryPhaseCorrelation(I1, I2, roi1, roi2, tx_min, tx_max, ty_min, ty_max);
            if (res.status == 0) {
                std::cout << "Tile " << tile_index << " (North): SURF failed. Phase Correlation SUCCEEDED. Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
                return res;
            }
        }
    }
    
    // Stage 4: Nominal fallback
    res.tx = dx_nominal;
    res.ty = dy_nominal;
    res.status = 1; // Mark status = 1 to indicate default fallback for post-filtering
    res.inliers = 1;
    res.matched_count = 1;
    std::cout << "Tile " << tile_index << " (North): SURF & PC failed. Using Nominal Physical Translation. Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
    return res;
}

TransformResult RegistrationEngine::ComputeWestTransform(
    const cv::Mat& I1, const cv::Mat& I2, 
    float overlap_west, bool use_fast_stitch, 
    bool use_surf, bool use_pc, int tile_index) 
{
    TransformResult res;
    if (I1.empty() || I2.empty()) return res;
    
    int H = I1.rows;
    int W = I1.cols;
    
    // 1. Default physical translation
    float dx_nominal = std::round(W * (100.0f - overlap_west) / 100.0f);
    float dy_nominal = 0.0f;
    
    if (!use_surf && !use_pc) {
        res.tx = dx_nominal;
        res.ty = dy_nominal;
        res.status = 0;
        res.inliers = 1;
        res.matched_count = 1;
        return res;
    }
    
    // Physical range constraints
    float tx_min = W - (overlap_west + overlap_error) * W / 100.0f;
    float tx_max = W - (overlap_west - overlap_error) * W / 100.0f;
    float ty_min = -overlap_error * H / 100.0f;
    float ty_max = overlap_error * H / 100.0f;
    
    // Stage 1: Try fast stitch with 5% overlap if enabled
    if (use_fast_stitch && overlap_west > 5.0f) {
        float overlap_x = 5.0f;
        int x_pixel = static_cast<int>(std::round(W * overlap_x / 100.0f));
        cv::Rect roi1(0, 0, x_pixel, H);
        cv::Rect roi2(static_cast<int>(std::round(W * (100.0f - overlap_west) / 100.0f)), 0, x_pixel, H);
        
        if (use_surf) {
            res = TrySURF(I1, I2, roi1, roi2, tx_min, tx_max, ty_min, ty_max, 10000);
            if (res.status == 0) {
                std::cout << "Tile " << tile_index << " (West): SURF SUCCEEDED (Fast). Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
                return res;
            }
        }
    }
    
    // Stage 2: Try full overlap SURF
    {
        int x_pixel = static_cast<int>(std::round(W * overlap_west / 100.0f));
        cv::Rect roi1(0, 0, x_pixel, H);
        cv::Rect roi2(static_cast<int>(std::round(W * (100.0f - overlap_west) / 100.0f)), 0, x_pixel, H);
        
        if (use_surf) {
            res = TrySURF(I1, I2, roi1, roi2, tx_min, tx_max, ty_min, ty_max, 10000);
            if (res.status == 0) {
                std::cout << "Tile " << tile_index << " (West): SURF SUCCEEDED (Full). Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
                return res;
            }
        }
        
        // Stage 3: Phase Correlation (PC) fallback
        if (use_pc) {
            res = TryPhaseCorrelation(I1, I2, roi1, roi2, tx_min, tx_max, ty_min, ty_max);
            if (res.status == 0) {
                std::cout << "Tile " << tile_index << " (West): SURF failed. Phase Correlation SUCCEEDED. Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
                return res;
            }
        }
    }
    
    // Stage 4: Nominal fallback
    res.tx = dx_nominal;
    res.ty = dy_nominal;
    res.status = 1; // Mark status = 1 to indicate default fallback for post-filtering
    res.inliers = 1;
    res.matched_count = 1;
    std::cout << "Tile " << tile_index << " (West): SURF & PC failed. Using Nominal Physical Translation. Tx=" << res.tx << ", Ty=" << res.ty << std::endl;
    return res;
}
