#pragma once
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

class OptimizationEngine {
public:
    static void optimize_registrations(
        const std::string& source_directory,
        const std::vector<std::vector<std::string>>& img_name_grid,
        std::vector<std::vector<float>>& Tx_west,
        std::vector<std::vector<float>>& Ty_west,
        std::vector<std::vector<float>>& Tx_north,
        std::vector<std::vector<float>>& Ty_north,
        const std::vector<std::vector<float>>& valid_translations_west,
        const std::vector<std::vector<float>>& valid_translations_north
    );

private:
    static std::pair<float, float> cross_correlation_hill_climb(
        const std::string& images_path,
        const std::string& I1_name,
        const std::string& I2_name,
        const std::vector<float>& bounds,
        float x_start,
        float y_start
    );
    
    static double get_ncc(const cv::Mat& I1, const cv::Mat& I2, int x, int y);
};
