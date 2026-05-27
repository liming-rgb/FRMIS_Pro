#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class ImageBlender {
public:
    static cv::Mat blend(
        const std::string& source_directory,
        const std::vector<std::vector<std::string>>& img_name_grid,
        const std::vector<std::vector<float>>& global_y_img_pos,
        const std::vector<std::vector<float>>& global_x_img_pos,
        const std::vector<std::vector<float>>& tile_weights,
        const std::string& fusion_method,
        float alpha
    );
};
