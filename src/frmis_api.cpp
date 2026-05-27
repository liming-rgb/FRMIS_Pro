#include "frmis_api.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <cmath>
#include <cctype>
#include <cstring>
#include <chrono>
#include <iomanip>
#include "ImagePreprocessor.h"
#include "RegistrationEngine.h"
#include "GlobalOptimizer.h"
#include "OptimizationEngine.h"
#include "ImageBlender.h"

namespace {
    // Natural Sort string comparison helper
    bool natural_compare(const std::string& a, const std::string& b) {
        auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (is_digit(a[i]) && is_digit(b[j])) {
                // Compare numerical parts
                std::string num_a = "", num_b = "";
                while (i < a.size() && is_digit(a[i])) num_a += a[i++];
                while (j < b.size() && is_digit(b[j])) num_b += b[j++];
                
                // Remove leading zeros
                size_t start_a = num_a.find_first_not_of('0');
                size_t start_b = num_b.find_first_not_of('0');
                std::string sa = (start_a == std::string::npos) ? "0" : num_a.substr(start_a);
                std::string sb = (start_b == std::string::npos) ? "0" : num_b.substr(start_b);
                
                if (sa.size() != sb.size()) {
                    return sa.size() < sb.size();
                }
                if (sa != sb) {
                    return sa < sb;
                }
            } else {
                char ca = std::tolower(static_cast<unsigned char>(a[i]));
                char cb = std::tolower(static_cast<unsigned char>(b[j]));
                if (ca != cb) {
                    return ca < cb;
                }
                i++; j++;
            }
        }
        return a.size() < b.size();
    }
}

int RunStitchingPipeline(const StitchingParams* params) {
    auto t_total_start = std::chrono::high_resolution_clock::now();
    if (params == nullptr || params->dataset_dir == nullptr) {
        std::cerr << "Invalid parameter pointer passed to stitching pipeline." << std::endl;
        return -1;
    }

    std::string dataset_dir = params->dataset_dir;
    std::string dataset_name = params->dataset_name ? params->dataset_name : "";
    std::string img_type = params->img_type ? params->img_type : ".tif";
    std::string modality = params->modality ? params->modality : "phase&Fluorescent";
    
    // 定义允许的模态选项数组
    std::vector<std::string> allowed_modalities = {"BrightField", "phase&Fluorescent", "customize"};
    // 校验传入的 modality 是否有效，若无效则默认设为 "phase&Fluorescent"
    if (std::find(allowed_modalities.begin(), allowed_modalities.end(), modality) == allowed_modalities.end()) {
        std::cout << "[Warning] Invalid modality '" << modality << "', defaulting to 'phase&Fluorescent'\n";
        modality = "phase&Fluorescent";
    }

    std::string optimization = params->optimization ? params->optimization : "False";
    std::string global_registration = params->global_registration ? params->global_registration : "MST";
    std::string blend_method = params->blend_method ? params->blend_method : "Linear";

    int width = params->width;
    int height = params->height;
    int img_num = params->img_num > 0 ? params->img_num : (width * height);
    int sort_type = params->sort_type;
    
    // 根据模态自动覆盖或使用自定义阈值
    int threshold_metric = params->threshold_metric;
    if (modality == "BrightField") {
        threshold_metric = 1000;
    } else if (modality == "phase&Fluorescent") {
        threshold_metric = 1;
    } else if (modality == "customize") {
        threshold_metric = params->threshold_metric;
        if (threshold_metric <= 0) {
            std::cout << "[Warning] Invalid threshold_metric (" << threshold_metric 
                      << ") for 'customize' modality. Defaulting to 1.\n";
            threshold_metric = 1;
        }
    }

    float overlap_w = params->overlap_w;
    float overlap_h = params->overlap_h;
    float overlap_error = params->overlap_error;
    float alpha = params->alpha;

    bool use_fast_stitch = (params->use_fast_stitch != 0);
    bool use_surf = (params->use_surf != 0);
    bool use_pc = (params->use_pc != 0);

    std::cout << "=========================================================================\n";
    std::cout << " FRMIS_C Stitching Program (MATLAB 1:1 Parity Mode)\n";
    std::cout << "=========================================================================\n";
    std::cout << "[Dataset Path]   " << dataset_dir << "\n";
    std::cout << "[Dataset Prefix] " << dataset_name << "\n";
    std::cout << "[Image Type]     " << img_type << "\n";
    std::cout << "[Modality]       " << modality << "\n";
    std::cout << "[Grid Size]      " << width << " (cols) x " << height << " (rows)\n";
    std::cout << "[Total Images]   " << img_num << "\n";
    std::cout << "[West Overlap]   " << overlap_w << "%\n";
    std::cout << "[North Overlap]  " << overlap_h << "%\n";
    std::cout << "[Overlap Error]  " << overlap_error << "%\n";
    std::cout << "[Optimization]   " << optimization << "\n";
    std::cout << "[Global Reg]     " << global_registration << "\n";
    std::cout << "[Blend Method]   " << blend_method << "\n";
    std::cout << "[Alpha (Linear)] " << alpha << "\n";
    std::cout << "[Fast Stitch]    " << (use_fast_stitch ? "Yes" : "No") << "\n";
    std::cout << "[SURF Enabled]   " << (use_surf ? "Yes" : "No") << "\n";
    std::cout << "[PC Enabled]     " << (use_pc ? "Yes" : "No") << "\n";
    std::cout << "-------------------------------------------------------------------------\n";

    try {
        // 1. Scan and Match Files
        std::vector<std::string> all_files;
        for (const auto& entry : std::filesystem::directory_iterator(dataset_dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string ext = entry.path().extension().string();
                
                // Case-insensitive match for extension
                std::string ext_lower = ext;
                std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
                std::string target_ext = img_type;
                std::transform(target_ext.begin(), target_ext.end(), target_ext.begin(), ::tolower);
                
                // Prefix match
                bool prefix_matches = true;
                if (!dataset_name.empty()) {
                    prefix_matches = (filename.rfind(dataset_name, 0) == 0);
                }
                
                if (ext_lower == target_ext && prefix_matches) {
                    all_files.push_back(filename);
                }
            }
        }

        // Sort files naturally
        std::sort(all_files.begin(), all_files.end(), natural_compare);

        if (all_files.size() < (size_t)img_num) {
            std::cerr << "ERROR: Not enough images found. Found: " << all_files.size() 
                      << ", expected: " << img_num << std::endl;
            return -1;
        }

        std::vector<std::string> files(all_files.begin(), all_files.begin() + img_num);

        // 2. Build img_name_grid and index_matrix
        std::vector<std::vector<std::string>> img_name_grid(height, std::vector<std::string>(width));
        std::vector<std::vector<int>> index_matrix(height, std::vector<int>(width));

        int file_idx = 0;
        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                img_name_grid[r][c] = files[file_idx];
                index_matrix[r][c] = file_idx + 1; // 1-based index
                file_idx++;
            }
        }

        if (sort_type == 2) {
            std::reverse(img_name_grid.begin(), img_name_grid.end());
            std::reverse(index_matrix.begin(), index_matrix.end());
        }

        // 3. Preload and Preprocess images if algorithms are enabled
        std::filesystem::path first_img_path = std::filesystem::path(dataset_dir) / img_name_grid[0][0];
        cv::Mat first_img = cv::imread(first_img_path.string(), cv::IMREAD_UNCHANGED);
        if (first_img.empty()) {
            std::cerr << "Failed to read first image: " << first_img_path.string() << std::endl;
            return -1;
        }
        int M = first_img.rows;
        int N = first_img.cols;

        std::vector<std::vector<cv::Mat>> images_cache(height, std::vector<cv::Mat>(width));
        if (use_surf || use_pc) {
            std::cout << "Preloading and Preprocessing images..." << std::endl;
            for (int r = 0; r < height; ++r) {
                for (int c = 0; c < width; ++c) {
                    std::filesystem::path ip = std::filesystem::path(dataset_dir) / img_name_grid[r][c];
                    cv::Mat img = cv::imread(ip.string(), cv::IMREAD_UNCHANGED);
                    if (img.empty()) {
                        std::cerr << "Failed to read image: " << ip.string() << std::endl;
                        return -1;
                    }
                    
                    // Grayscale conversion
                    cv::Mat gray;
                    if (img.channels() == 3) {
                        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
                    } else {
                        gray = img;
                    }

                    images_cache[r][c] = ImagePreprocessor::Process(gray);
                }
            }
            std::cout << "Preloading done!" << std::endl;
        }

        // 4. Pairwise Alignment loop
        auto t_pairwise_start = std::chrono::high_resolution_clock::now();
        RegistrationEngine engine(threshold_metric, overlap_error);

        std::vector<std::vector<float>> Tx_north(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> Ty_north(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> matchedNumb_north(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> valid_translations_north(height, std::vector<float>(width, NAN));

        std::vector<std::vector<float>> Tx_west(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> Ty_west(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> matchedNumb_west(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> valid_translations_west(height, std::vector<float>(width, NAN));

        std::cout << "Performing pairwise alignments";
        for (int c = 0; c < width; ++c) {
            std::cout << ".";
            for (int r = 0; r < height; ++r) {
                cv::Mat I1 = (use_surf || use_pc) ? images_cache[r][c] : cv::Mat();

                // Compute North displacement (if not the first row)
                if (r > 0) {
                    cv::Mat I2 = (use_surf || use_pc) ? images_cache[r - 1][c] : cv::Mat();
                    TransformResult res = engine.ComputeNorthTransform(I1, I2, overlap_h, use_fast_stitch, use_surf, use_pc, index_matrix[r][c]);
                    
                    Tx_north[r][c] = res.tx;
                    Ty_north[r][c] = res.ty;
                    matchedNumb_north[r][c] = static_cast<float>(res.inliers);
                    if (res.status == 0) {
                        valid_translations_north[r][c] = 1.0f;
                    }
                }

                // Compute West displacement (if not the first column)
                if (c > 0) {
                    cv::Mat I2 = (use_surf || use_pc) ? images_cache[r][c - 1] : cv::Mat();
                    TransformResult res = engine.ComputeWestTransform(I1, I2, overlap_w, use_fast_stitch, use_surf, use_pc, index_matrix[r][c]);
                    
                    Tx_west[r][c] = res.tx;
                    Ty_west[r][c] = res.ty;
                    matchedNumb_west[r][c] = static_cast<float>(res.inliers);
                    if (res.status == 0) {
                        valid_translations_west[r][c] = 1.0f;
                    }
                }
            }
        }
        std::cout << " Done!\n";
        auto t_pairwise_end = std::chrono::high_resolution_clock::now();
        double time_pairwise = std::chrono::duration<double>(t_pairwise_end - t_pairwise_start).count();

        // 5. Post-filtering of translations (interpolating filtered outliers using row/col/global means)
        // Reset invalid (NaN in valid_translations) elements to NaN in Tx/Ty matrices first
        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                if (r > 0 && std::isnan(valid_translations_north[r][c])) {
                    Tx_north[r][c] = NAN;
                    Ty_north[r][c] = NAN;
                    matchedNumb_north[r][c] = NAN;
                }
                if (c > 0 && std::isnan(valid_translations_west[r][c])) {
                    Tx_west[r][c] = NAN;
                    Ty_west[r][c] = NAN;
                    matchedNumb_west[r][c] = NAN;
                }
            }
        }

        // Fill NaNs for North displacements
        for (int r = 1; r < height; ++r) {
            std::vector<float> row_tx, row_ty;
            for (int c = 0; c < width; ++c) {
                if (!std::isnan(Tx_north[r][c])) {
                    row_tx.push_back(Tx_north[r][c]);
                    row_ty.push_back(Ty_north[r][c]);
                }
            }

            if (row_tx.empty()) {
                // Entire row is NaN, use global mean of all valid North displacements
                float sum_g_tx = 0.0f, sum_g_ty = 0.0f;
                int count_g = 0;
                for (int gr = 1; gr < height; ++gr) {
                    for (int gc = 0; gc < width; ++gc) {
                        if (!std::isnan(Tx_north[gr][gc])) {
                            sum_g_tx += Tx_north[gr][gc];
                            sum_g_ty += Ty_north[gr][gc];
                            count_g++;
                        }
                    }
                }

                float final_tx = 0.0f;
                float final_ty = std::round(M * (100.0f - overlap_h) / 100.0f); // Nominal fallback
                if (count_g > 0) {
                    final_tx = std::round(sum_g_tx / count_g);
                    final_ty = std::round(sum_g_ty / count_g);
                }

                for (int c = 0; c < width; ++c) {
                    Tx_north[r][c] = final_tx;
                    Ty_north[r][c] = final_ty;
                }
            } else {
                // Row has some valid elements, use row mean
                float sum_tx = 0.0f, sum_ty = 0.0f;
                for (size_t k = 0; k < row_tx.size(); ++k) {
                    sum_tx += row_tx[k];
                    sum_ty += row_ty[k];
                }
                float mean_tx = std::floor(sum_tx / row_tx.size() + 0.5f);
                float mean_ty = std::floor(sum_ty / row_ty.size() + 0.5f);

                for (int c = 0; c < width; ++c) {
                    if (std::isnan(Tx_north[r][c])) {
                        Tx_north[r][c] = mean_tx;
                        Ty_north[r][c] = mean_ty;
                    }
                }
            }
        }

        // Fill NaNs for West displacements
        for (int c = 1; c < width; ++c) {
            std::vector<float> col_tx, col_ty;
            for (int r = 0; r < height; ++r) {
                if (!std::isnan(Tx_west[r][c])) {
                    col_tx.push_back(Tx_west[r][c]);
                    col_ty.push_back(Ty_west[r][c]);
                }
            }

            if (col_tx.empty()) {
                // Entire column is NaN, use global mean of all valid West displacements
                float sum_g_tx = 0.0f, sum_g_ty = 0.0f;
                int count_g = 0;
                for (int gr = 0; gr < height; ++gr) {
                    for (int gc = 1; gc < width; ++gc) {
                        if (!std::isnan(Tx_west[gr][gc])) {
                            sum_g_tx += Tx_west[gr][gc];
                            sum_g_ty += Ty_west[gr][gc];
                            count_g++;
                        }
                    }
                }

                float final_tx = std::round(N * (100.0f - overlap_w) / 100.0f); // Nominal fallback
                float final_ty = 0.0f;
                if (count_g > 0) {
                    final_tx = std::round(sum_g_tx / count_g);
                    final_ty = std::round(sum_g_ty / count_g);
                }

                for (int r = 0; r < height; ++r) {
                    Tx_west[r][c] = final_tx;
                    Ty_west[r][c] = final_ty;
                }
            } else {
                // Column has some valid elements, use column mean
                float sum_tx = 0.0f, sum_ty = 0.0f;
                for (size_t k = 0; k < col_tx.size(); ++k) {
                    sum_tx += col_tx[k];
                    sum_ty += col_ty[k];
                }
                float mean_tx = std::floor(sum_tx / col_tx.size() + 0.5f);
                float mean_ty = std::floor(sum_ty / col_ty.size() + 0.5f);

                for (int r = 0; r < height; ++r) {
                    if (std::isnan(Tx_west[r][c])) {
                        Tx_west[r][c] = mean_tx;
                        Ty_west[r][c] = mean_ty;
                    }
                }
            }
        }

        // 6. Compute graph weights for MST/SPT registration
        std::vector<std::vector<float>> N1(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> N2(height, std::vector<float>(width, NAN));
        float maxN = 0.0f;
        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                if (r > 0 && !std::isnan(matchedNumb_north[r][c]) && matchedNumb_north[r][c] > 0.0f) {
                    N1[r][c] = 1.0f / matchedNumb_north[r][c];
                    if (std::isnan(maxN) || N1[r][c] > maxN) maxN = N1[r][c];
                }
                if (c > 0 && !std::isnan(matchedNumb_west[r][c]) && matchedNumb_west[r][c] > 0.0f) {
                    N2[r][c] = 1.0f / matchedNumb_west[r][c];
                    if (std::isnan(maxN) || N2[r][c] > maxN) maxN = N2[r][c];
                }
            }
        }
        if (maxN <= 0.0f || std::isnan(maxN)) maxN = 1.0f;

        std::vector<std::vector<float>> weight_north(height, std::vector<float>(width, NAN));
        std::vector<std::vector<float>> weight_west(height, std::vector<float>(width, NAN));

        float max_valid_w_north = 0.0f;
        bool has_valid_w_north = false;
        for (int r = 1; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                if (!std::isnan(N1[r][c])) {
                    weight_north[r][c] = N1[r][c] / maxN;
                    if (!has_valid_w_north || weight_north[r][c] > max_valid_w_north) {
                        max_valid_w_north = weight_north[r][c];
                        has_valid_w_north = true;
                    }
                }
            }
        }

        float max_valid_w_west = 0.0f;
        bool has_valid_w_west = false;
        for (int r = 0; r < height; ++r) {
            for (int c = 1; c < width; ++c) {
                if (!std::isnan(N2[r][c])) {
                    weight_west[r][c] = N2[r][c] / maxN;
                    if (!has_valid_w_west || weight_west[r][c] > max_valid_w_west) {
                        max_valid_w_west = weight_west[r][c];
                        has_valid_w_west = true;
                    }
                }
            }
        }

        // Fill weight NaNs
        for (int r = 1; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                if (std::isnan(weight_north[r][c])) {
                    weight_north[r][c] = has_valid_w_north ? (max_valid_w_north + 0.1f) : 1.0f;
                }
            }
        }
        for (int r = 0; r < height; ++r) {
            for (int c = 1; c < width; ++c) {
                if (std::isnan(weight_west[r][c])) {
                    weight_west[r][c] = has_valid_w_west ? (max_valid_w_west + 0.1f) : 1.0f;
                }
            }
        }

        // 7. Perform Cross-Correlation Hill Climbing Optimization if requested
        if (optimization == "True") {
            std::cout << "Performing cross-correlation optimization..." << std::endl;
            OptimizationEngine::optimize_registrations(
                dataset_dir, img_name_grid, Tx_west, Ty_west, Tx_north, Ty_north, 
                valid_translations_west, valid_translations_north
            );
            std::cout << "CC Optimization completed!" << std::endl;
        }

        // 8. Global Registration Solver (MST vs SPT)
        auto t_global_start = std::chrono::high_resolution_clock::now();
        GridCoords coords;
        if (global_registration == "MST") {
            std::cout << "Solving global coordinates with Minimum Spanning Tree (MST)..." << std::endl;
            coords = GlobalOptimizer::PrimMST(Ty_north, Tx_north, Ty_west, Tx_west, weight_north, weight_west);
        } else {
            std::cout << "Solving global coordinates with Shortest Path Tree (SPT)..." << std::endl;
            coords = GlobalOptimizer::DijkstraSPT(Ty_north, Tx_north, Ty_west, Tx_west, weight_north, weight_west);
        }
        auto t_global_end = std::chrono::high_resolution_clock::now();
        double time_global_alignment = std::chrono::duration<double>(t_global_end - t_global_start).count();

        // 9. Blending and Merging
        auto t_assemble_start = std::chrono::high_resolution_clock::now();
        std::cout << "Assembling stitched image with method: " << blend_method << "..." << std::endl;
        cv::Mat blended = ImageBlender::blend(dataset_dir, img_name_grid, coords.y, coords.x, coords.coeff, blend_method, alpha);

        if (blended.empty()) {
            std::cerr << "ERROR: Assembled image is empty!" << std::endl;
            return -1;
        }
        auto t_assemble_end = std::chrono::high_resolution_clock::now();
        double time_assembling = std::chrono::duration<double>(t_assemble_end - t_assemble_start).count();

        // 10. Write final image to disk (placed under FRMIS_C/result/ directory with dynamic naming)
        std::filesystem::path result_dir = std::filesystem::current_path() / "result";
        std::filesystem::create_directories(result_dir);

        std::string dir_name = "result_stitched";
        try {
            std::filesystem::path dp = std::filesystem::path(dataset_dir).lexically_normal();
            std::vector<std::string> parts;
            for (const auto& part : dp) {
                std::string s = part.string();
                if (s != "/" && s != "\\" && s != "." && s != ".." && !s.empty()) {
                    parts.push_back(s);
                }
            }
            if (parts.size() >= 4) {
                dir_name = parts[parts.size() - 4] + "_" + 
                           parts[parts.size() - 3] + "_" + 
                           parts[parts.size() - 2] + "_" + 
                           parts[parts.size() - 1];
            } else if (!parts.empty()) {
                dir_name = parts.back();
            }
        } catch (...) {
            // fallback
        }

        std::string out_filename = dir_name + "_" + global_registration + img_type;
        std::filesystem::path out_path = result_dir / out_filename;
        std::cout << "Saving stitched image to: " << out_path.string() << std::endl;
        cv::imwrite(out_path.string(), blended);

        auto t_total_end = std::chrono::high_resolution_clock::now();
        double total_time = std::chrono::duration<double>(t_total_end - t_total_start).count();

        // Output MATLAB-style stats to the console
        std::cout << "\n---------------- MATLAB-Style Stitching Stats ----------------\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "            time_pairwise: " << time_pairwise << "\n";
        std::cout << "         tiling_indicator: [" << height << "x" << width << " double]\n";
        std::cout << "             tile_weights: [" << height << "x" << width << " double]\n";
        std::cout << "         global_y_img_pos: [" << height << "x" << width << " double]\n";
        std::cout << "         global_x_img_pos: [" << height << "x" << width << " double]\n";
        std::cout << "    time_global_alignment: " << time_global_alignment << "\n";
        std::cout << "          time_assembling: " << time_assembling << "\n";
        std::cout << "               total_time: " << total_time << "\n";
        std::cout << "--------------------------------------------------------------\n";

        // Print actual matrix contents for the user's quick reference
        auto print_matrix = [height, width](const std::string& name, const std::vector<std::vector<float>>& mat) {
            std::cout << "\n[ " << name << " (" << height << "x" << width << ") ]\n";
            for (const auto& row : mat) {
                for (const auto& val : row) {
                    if (std::isnan(val)) {
                        std::cout << std::setw(10) << "NaN" << " ";
                    } else {
                        std::cout << std::setw(10) << std::fixed << std::setprecision(4) << val << " ";
                    }
                }
                std::cout << "\n";
            }
        };

        auto print_int_matrix = [height, width](const std::string& name, const std::vector<std::vector<int>>& mat) {
            std::cout << "\n[ " << name << " (" << height << "x" << width << ") ]\n";
            for (const auto& row : mat) {
                for (const auto& val : row) {
                    std::cout << std::setw(6) << val << " ";
                }
                std::cout << "\n";
            }
        };

        // print_int_matrix("tiling_indicator", coords.tiling_indicator);
        // print_matrix("tile_weights (tiling_coeff)", coords.coeff);
        // print_matrix("global_y_img_pos", coords.y);
        // print_matrix("global_x_img_pos", coords.x);
        std::cout << "--------------------------------------------------------------\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception in RunStitchingPipeline: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown exception in RunStitchingPipeline." << std::endl;
        return -1;
    }
}
