#include "ImageBlender.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <filesystem>

cv::Mat ImageBlender::blend(
    const std::string& source_directory,
    const std::vector<std::vector<std::string>>& img_name_grid,
    const std::vector<std::vector<float>>& global_y_img_pos,
    const std::vector<std::vector<float>>& global_x_img_pos,
    const std::vector<std::vector<float>>& tile_weights,
    const std::string& fusion_method,
    float alpha) 
{
    int height = img_name_grid.size();
    int width = img_name_grid[0].size();
    int num_tiles = height * width;

    // 1. Get the size and type of a single image
    std::filesystem::path first_path = std::filesystem::path(source_directory) / img_name_grid[0][0];
    cv::Mat tempI = cv::imread(first_path.string(), cv::IMREAD_UNCHANGED);
    if (tempI.empty()) {
        std::cerr << "Failed to read first image for size info: " << first_path.string() << std::endl;
        return cv::Mat();
    }

    int img_height = tempI.rows;
    int img_width = tempI.cols;
    int img_channels = tempI.channels();
    int img_type = tempI.type();

    // 2. Determine how big to make the canvas
    int max_x = 0;
    int max_y = 0;
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            int ox = static_cast<int>(std::round(global_x_img_pos[r][c])) - 1;
            int oy = static_cast<int>(std::round(global_y_img_pos[r][c])) - 1;
            max_x = std::max(max_x, ox + img_width);
            max_y = std::max(max_y, oy + img_height);
        }
    }

    int canvas_width = max_x;
    int canvas_height = max_y;

    std::cout << "Stitched Image Canvas Size: " << canvas_width << " x " << canvas_height << std::endl;

    // 3. Create the ordering vector so that images with higher correlation weights (worst edges)
    // are placed first. Images with lower weights (better matches) overwrite them later.
    struct TileInfo {
        int r;
        int c;
        float weight;
    };
    std::vector<TileInfo> tiles;
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            tiles.push_back({r, c, tile_weights[r][c]});
        }
    }

    std::sort(tiles.begin(), tiles.end(), [](const TileInfo& a, const TileInfo& b) {
        bool a_nan = std::isnan(a.weight);
        bool b_nan = std::isnan(b.weight);
        if (a_nan && b_nan) return false;
        if (a_nan) return true; // NaN goes first (lowest priority)
        if (b_nan) return false;
        return a.weight > b.weight; // Higher weight (worse match) goes first
    });

    std::string method = fusion_method;
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method == "overlay") {
        cv::Mat canvas = cv::Mat::zeros(canvas_height, canvas_width, img_type);
        for (const auto& tile : tiles) {
            std::filesystem::path p = std::filesystem::path(source_directory) / img_name_grid[tile.r][tile.c];
            cv::Mat current_image = cv::imread(p.string(), cv::IMREAD_UNCHANGED);
            if (current_image.empty()) continue;

            int ox = static_cast<int>(std::round(global_x_img_pos[tile.r][tile.c])) - 1;
            int oy = static_cast<int>(std::round(global_y_img_pos[tile.r][tile.c])) - 1;

            cv::Rect roi(ox, oy, img_width, img_height);
            current_image.copyTo(canvas(roi));
        }
        return canvas;
    } else { // Linear Blending
        int acc_type = (img_channels == 3) ? CV_32FC3 : CV_32FC1;
        cv::Mat acc = cv::Mat::zeros(canvas_height, canvas_width, acc_type);
        cv::Mat countsI = cv::Mat::zeros(canvas_height, canvas_width, acc_type);

        // Compute 2D pixel weights matrix w_mat
        cv::Mat w_mat_single(img_height, img_width, CV_32FC1);
        for (int r = 0; r < img_height; ++r) {
            float d_min_i = static_cast<float>(std::min(r + 1, img_height - r));
            for (int c = 0; c < img_width; ++c) {
                float d_min_j = static_cast<float>(std::min(c + 1, img_width - c));
                w_mat_single.at<float>(r, c) = std::pow(d_min_i * d_min_j, alpha);
            }
        }

        cv::Mat w_mat;
        if (img_channels == 3) {
            std::vector<cv::Mat> channels = {w_mat_single, w_mat_single, w_mat_single};
            cv::merge(channels, w_mat);
        } else {
            w_mat = w_mat_single;
        }

        for (const auto& tile : tiles) {
            std::filesystem::path p = std::filesystem::path(source_directory) / img_name_grid[tile.r][tile.c];
            cv::Mat current_image_raw = cv::imread(p.string(), cv::IMREAD_UNCHANGED);
            if (current_image_raw.empty()) continue;

            cv::Mat current_image;
            current_image_raw.convertTo(current_image, CV_32F);

            // Apply linear weights
            cv::multiply(current_image, w_mat, current_image);

            int ox = static_cast<int>(std::round(global_x_img_pos[tile.r][tile.c])) - 1;
            int oy = static_cast<int>(std::round(global_y_img_pos[tile.r][tile.c])) - 1;

            cv::Rect roi(ox, oy, img_width, img_height);
            
            acc(roi) += current_image;
            countsI(roi) += w_mat;
        }

        // Avoid division by zero
        cv::Mat mask = (countsI > 1e-10f);
        cv::Mat final_float = cv::Mat::zeros(acc.size(), acc.type());
        cv::divide(acc, countsI, final_float);

        cv::Mat canvas;
        final_float.convertTo(canvas, img_type);
        return canvas;
    }
}
