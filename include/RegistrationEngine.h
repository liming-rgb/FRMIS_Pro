#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

struct TransformResult {
    float tx = 0.0f;
    float ty = 0.0f;
    int status = 1; // 0: success, 1: failed
    int inliers = 0;
    int matched_count = 0;
};

class RegistrationEngine {
public:
    RegistrationEngine(int threshold, float overlap_err);
    
    TransformResult ComputeNorthTransform(const cv::Mat& I1, const cv::Mat& I2, 
                                          float overlap_north, bool use_fast_stitch, 
                                          bool use_surf, bool use_pc, int tile_index);
                                          
    TransformResult ComputeWestTransform(const cv::Mat& I1, const cv::Mat& I2, 
                                         float overlap_west, bool use_fast_stitch, 
                                         bool use_surf, bool use_pc, int tile_index);
                                         
private:
    int threshold_metric;
    float overlap_error;
    
    TransformResult TrySURF(const cv::Mat& I1, const cv::Mat& I2, 
                            cv::Rect roi1, cv::Rect roi2, 
                            float tx_min, float tx_max, float ty_min, float ty_max,
                            int max_trials);
                            
    TransformResult TryPhaseCorrelation(const cv::Mat& I1, const cv::Mat& I2, 
                                        cv::Rect roi1, cv::Rect roi2, 
                                        float tx_min, float tx_max, float ty_min, float ty_max);
                                        
    TransformResult EstimateTranslationRANSAC(const std::vector<cv::Point2f>& pts1, 
                                              const std::vector<cv::Point2f>& pts2, 
                                              float max_distance, int max_trials, 
                                              double confidence);
};
