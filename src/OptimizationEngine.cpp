#include "OptimizationEngine.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <map>
#include <iostream>
#include <filesystem>

void OptimizationEngine::optimize_registrations(
    const std::string& source_directory,
    const std::vector<std::vector<std::string>>& img_name_grid,
    std::vector<std::vector<float>>& Tx_west,
    std::vector<std::vector<float>>& Ty_west,
    std::vector<std::vector<float>>& Tx_north,
    std::vector<std::vector<float>>& Ty_north,
    const std::vector<std::vector<float>>& valid_translations_west,
    const std::vector<std::vector<float>>& valid_translations_north) 
{
    int height = img_name_grid.size();
    int width = img_name_grid[0].size();

    // 1. Calculate North repeatability
    float rx_north = 1.0f;
    float ry_north = 1.0f;
    {
        std::vector<float> valid_tx;
        for (int r = 1; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                if (!std::isnan(valid_translations_north[r][c]) && !std::isnan(Tx_north[r][c])) {
                    valid_tx.push_back(Tx_north[r][c]);
                }
            }
        }
        if (!valid_tx.empty()) {
            auto [min_it, max_it] = std::minmax_element(valid_tx.begin(), valid_tx.end());
            rx_north = std::ceil((*max_it - *min_it) / 2.0f);
        }

        float max_row_ry = 0.0f;
        for (int r = 1; r < height; ++r) {
            std::vector<float> row_ty;
            for (int c = 0; c < width; ++c) {
                if (!std::isnan(valid_translations_north[r][c]) && !std::isnan(Ty_north[r][c])) {
                    row_ty.push_back(Ty_north[r][c]);
                }
            }
            if (!row_ty.empty()) {
                auto [min_it, max_it] = std::minmax_element(row_ty.begin(), row_ty.end());
                float row_ry = std::ceil((*max_it - *min_it) / 2.0f);
                if (row_ry > max_row_ry) max_row_ry = row_ry;
            }
        }
        ry_north = max_row_ry;
    }
    float repeatability1 = std::max(rx_north, ry_north);

    // 2. Calculate West repeatability
    float rx_west = 1.0f;
    float ry_west = 1.0f;
    {
        std::vector<float> valid_ty;
        for (int r = 0; r < height; ++r) {
            for (int c = 1; c < width; ++c) {
                if (!std::isnan(valid_translations_west[r][c]) && !std::isnan(Ty_west[r][c])) {
                    valid_ty.push_back(Ty_west[r][c]);
                }
            }
        }
        if (!valid_ty.empty()) {
            auto [min_it, max_it] = std::minmax_element(valid_ty.begin(), valid_ty.end());
            ry_west = std::ceil((*max_it - *min_it) / 2.0f);
        }

        float max_col_rx = 0.0f;
        for (int c = 1; c < width; ++c) {
            std::vector<float> col_tx;
            for (int r = 0; r < height; ++r) {
                if (!std::isnan(valid_translations_west[r][c]) && !std::isnan(Tx_west[r][c])) {
                    col_tx.push_back(Tx_west[r][c]);
                }
            }
            if (!col_tx.empty()) {
                auto [min_it, max_it] = std::minmax_element(col_tx.begin(), col_tx.end());
                float col_rx = std::ceil((*max_it - *min_it) / 2.0f);
                if (col_rx > max_col_rx) max_col_rx = col_rx;
            }
        }
        rx_west = max_col_rx;
    }
    float repeatability2 = std::max(rx_west, ry_west);

    // 3. Repeatability search range: 2 * max(r, 1) + 1
    float r = std::max(repeatability1, repeatability2);
    if (std::isnan(r) || r < 1.0f) r = 1.0f;
    int search_r = 2 * std::max(static_cast<int>(r), 1) + 1;

    std::cout << "CC Hill Climb Search Range r: " << r << ", bounds width: " << search_r << std::endl;

    // 4. Build bounds and perform hill climbing
    for (int j = 0; j < width; ++j) {
        for (int i = 0; i < height; ++i) {
            // West neighbor optimization
            if (j > 0 && !img_name_grid[i][j - 1].empty() && !img_name_grid[i][j].empty()) {
                std::vector<float> bounds = {
                    Ty_west[i][j] - search_r, Ty_west[i][j] + search_r,
                    Tx_west[i][j] - search_r, Tx_west[i][j] + search_r
                };
                auto [ny, nx] = cross_correlation_hill_climb(source_directory, img_name_grid[i][j - 1], img_name_grid[i][j], bounds, Tx_west[i][j], Ty_west[i][j]);
                Tx_west[i][j] = nx;
                Ty_west[i][j] = ny;
            }

            // North neighbor optimization
            if (i > 0 && !img_name_grid[i - 1][j].empty() && !img_name_grid[i][j].empty()) {
                std::vector<float> bounds = {
                    Ty_north[i][j] - search_r, Ty_north[i][j] + search_r,
                    Tx_north[i][j] - search_r, Tx_north[i][j] + search_r
                };
                auto [ny, nx] = cross_correlation_hill_climb(source_directory, img_name_grid[i - 1][j], img_name_grid[i][j], bounds, Tx_north[i][j], Ty_north[i][j]);
                Tx_north[i][j] = nx;
                Ty_north[i][j] = ny;
            }
        }
    }
}

std::pair<float, float> OptimizationEngine::cross_correlation_hill_climb(
    const std::string& images_path,
    const std::string& I1_name,
    const std::string& I2_name,
    const std::vector<float>& bounds,
    float x_start,
    float y_start) 
{
    std::filesystem::path p1 = std::filesystem::path(images_path) / I1_name;
    std::filesystem::path p2 = std::filesystem::path(images_path) / I2_name;
    
    cv::Mat I1_raw = cv::imread(p1.string(), cv::IMREAD_UNCHANGED);
    cv::Mat I2_raw = cv::imread(p2.string(), cv::IMREAD_UNCHANGED);

    if (I1_raw.empty() || I2_raw.empty()) {
        return {y_start, x_start};
    }

    // Convert to grayscale double matching MATLAB
    cv::Mat I1, I2;
    if (I1_raw.channels() == 3) cv::cvtColor(I1_raw, I1, cv::COLOR_BGR2GRAY); else I1 = I1_raw;
    if (I2_raw.channels() == 3) cv::cvtColor(I2_raw, I2, cv::COLOR_BGR2GRAY); else I2 = I2_raw;
    
    I1.convertTo(I1, CV_64F);
    I2.convertTo(I2, CV_64F);

    std::map<std::pair<int, int>, double> ncc_cache;

    auto get_ncc_val = [&](int cx, int cy) {
        if (cy < bounds[0] || cy > bounds[1] || cx < bounds[2] || cx > bounds[3]) {
            return -2.0; // out of bounds
        }
        auto key = std::make_pair(cx, cy);
        auto it = ncc_cache.find(key);
        if (it != ncc_cache.end()) {
            return it->second;
        }
        double val = get_ncc(I1, I2, cx, cy);
        ncc_cache[key] = val;
        return val;
    };

    int x = static_cast<int>(std::round(x_start));
    int y = static_cast<int>(std::round(y_start));
    
    double best_ncc = get_ncc_val(x, y);

    int dx_vals[] = {0, 0, 1, -1};
    int dy_vals[] = {-1, 1, 0, 0};

    bool done = false;
    while (!done) {
        int best_dx = 0;
        int best_dy = 0;
        double local_best_ncc = best_ncc;

        for (int k = 0; k < 4; ++k) {
            int nx = x + dx_vals[k];
            int ny = y + dy_vals[k];
            double val = get_ncc_val(nx, ny);
            if (val > local_best_ncc) {
                local_best_ncc = val;
                best_dx = dx_vals[k];
                best_dy = dy_vals[k];
            }
        }

        if (best_dx == 0 && best_dy == 0) {
            done = true;
        } else {
            x += best_dx;
            y += best_dy;
            best_ncc = local_best_ncc;
        }
    }

    return {static_cast<float>(y), static_cast<float>(x)};
}

double OptimizationEngine::get_ncc(const cv::Mat& I1, const cv::Mat& I2, int x, int y) {
    int w = I1.cols;
    int h = I1.rows;
    
    int abs_x = std::abs(x);
    int abs_y = std::abs(y);
    
    if (abs_x >= w || abs_y >= h) return -1.0;

    int x1 = (x >= 0) ? abs_x : 0;
    int y1 = (y >= 0) ? abs_y : 0;
    int x2 = (x < 0) ? abs_x : 0;
    int y2 = (y < 0) ? abs_y : 0;
    
    int crop_w = w - abs_x;
    int crop_h = h - abs_y;

    cv::Mat sub1 = I1(cv::Rect(x1, y1, crop_w, crop_h));
    cv::Mat sub2 = I2(cv::Rect(x2, y2, crop_w, crop_h));

    cv::Scalar mean1, stddev1;
    cv::meanStdDev(sub1, mean1, stddev1);
    cv::Scalar mean2, stddev2;
    cv::meanStdDev(sub2, mean2, stddev2);

    cv::Mat f1 = sub1 - mean1[0];
    cv::Mat f2 = sub2 - mean2[0];

    double num = f1.dot(f2);
    double den = std::sqrt(f1.dot(f1)) * std::sqrt(f2.dot(f2));

    if (den < 1e-10) return -1.0;
    double ncc = num / den;
    if (!std::isfinite(ncc)) return -1.0;
    return ncc;
}
