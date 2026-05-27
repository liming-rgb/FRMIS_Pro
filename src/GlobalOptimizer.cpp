#include "GlobalOptimizer.h"
#include <cmath>
#include <limits>
#include <queue>
#include <algorithm>
#include <iostream>

namespace {
    constexpr int MST_CONNECTED_NORTH = 11;
    constexpr int MST_CONNECTED_SOUTH = 12;
    constexpr int MST_CONNECTED_LEFT  = 22;
    constexpr int MST_CONNECTED_RIGHT = 21;
}

GridCoords GlobalOptimizer::PrimMST(
    const std::vector<std::vector<float>>& Y1, 
    const std::vector<std::vector<float>>& X1, 
    const std::vector<std::vector<float>>& Y2, 
    const std::vector<std::vector<float>>& X2, 
    const std::vector<std::vector<float>>& W1, 
    const std::vector<std::vector<float>>& W2) 
{
    int height = Y1.size();
    int width = Y1[0].size();
    int num_tiles = height * width;

    std::vector<std::vector<int>> tiling_indicator(height, std::vector<int>(width, 0));
    std::vector<std::vector<float>> global_x(height, std::vector<float>(width, 0.0f));
    std::vector<std::vector<float>> global_y(height, std::vector<float>(width, 0.0f));
    std::vector<std::vector<float>> tiling_coeff(height, std::vector<float>(width, 0.0f));

    // 1. Find the starting tile (minimum weight edge in W1 or W2)
    int ii = 0, jj = 0;
    float start_val = std::numeric_limits<float>::max();
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (r > 0 && !std::isnan(W1[r][c]) && W1[r][c] < start_val) {
                start_val = W1[r][c];
                ii = r; jj = c;
            }
            if (c > 0 && !std::isnan(W2[r][c]) && W2[r][c] < start_val) {
                start_val = W2[r][c];
                ii = r; jj = c;
            }
        }
    }
    tiling_indicator[ii][jj] = 10; // MST_START_TILE
    tiling_coeff[ii][jj] = start_val;

    // 2. Prim's Algorithm loop
    for (int step = 1; step < num_tiles; ++step) {
        float mst_min = std::numeric_limits<float>::max();
        int best_p_r = -1, best_p_c = -1;
        int best_c_r = -1, best_c_c = -1;
        int best_stitching_index = 0;

        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c) {
                if (tiling_indicator[r][c] == 0) continue; // Unconnected

                // Check neighbor below (South) -> matches MST_CONNECTED_NORTH
                if (r < height - 1 && tiling_indicator[r + 1][c] == 0) {
                    float w = W1[r + 1][c];
                    if (!std::isnan(w) && w < mst_min) {
                        mst_min = w;
                        best_stitching_index = MST_CONNECTED_NORTH;
                        best_p_r = r; best_p_c = c;
                        best_c_r = r + 1; best_c_c = c;
                    }
                }

                // Check neighbor above (North) -> matches MST_CONNECTED_SOUTH
                if (r > 0 && tiling_indicator[r - 1][c] == 0) {
                    float w = W1[r][c];
                    if (!std::isnan(w) && w < mst_min) {
                        mst_min = w;
                        best_stitching_index = MST_CONNECTED_SOUTH;
                        best_p_r = r; best_p_c = c;
                        best_c_r = r - 1; best_c_c = c;
                    }
                }

                // Check neighbor to the right (East) -> matches MST_CONNECTED_LEFT
                if (c < width - 1 && tiling_indicator[r][c + 1] == 0) {
                    float w = W2[r][c + 1];
                    if (!std::isnan(w) && w < mst_min) {
                        mst_min = w;
                        best_stitching_index = MST_CONNECTED_LEFT;
                        best_p_r = r; best_p_c = c;
                        best_c_r = r; best_c_c = c + 1;
                    }
                }

                // Check neighbor to the left (West) -> matches MST_CONNECTED_RIGHT
                if (c > 0 && tiling_indicator[r][c - 1] == 0) {
                    float w = W2[r][c];
                    if (!std::isnan(w) && w < mst_min) {
                        mst_min = w;
                        best_stitching_index = MST_CONNECTED_RIGHT;
                        best_p_r = r; best_p_c = c;
                        best_c_r = r; best_c_c = c - 1;
                    }
                }
            }
        }

        if (best_c_r == -1) {
            // Graph is disconnected, but we must still process remaining nodes
            // Find any unconnected node and connect it nominally
            bool found = false;
            for (int r = 0; r < height && !found; ++r) {
                for (int c = 0; c < width && !found; ++c) {
                    if (tiling_indicator[r][c] == 0) {
                        // Connect nominally to any neighbor
                        if (r > 0 && tiling_indicator[r - 1][c] > 0) {
                            tiling_indicator[r][c] = MST_CONNECTED_NORTH;
                            global_y[r][c] = global_y[r - 1][c] + Y1[r][c];
                            global_x[r][c] = global_x[r - 1][c] + X1[r][c];
                            tiling_coeff[r][c] = 1.0f;
                            found = true;
                        } else if (c > 0 && tiling_indicator[r][c - 1] > 0) {
                            tiling_indicator[r][c] = MST_CONNECTED_LEFT;
                            global_y[r][c] = global_y[r][c - 1] + Y2[r][c];
                            global_x[r][c] = global_x[r][c - 1] + X2[r][c];
                            tiling_coeff[r][c] = 1.0f;
                            found = true;
                        }
                    }
                }
            }
            if (!found) break;
            continue;
        }

        tiling_indicator[best_c_r][best_c_c] = best_stitching_index;
        tiling_coeff[best_c_r][best_c_c] = mst_min;

        if (best_stitching_index == MST_CONNECTED_NORTH) {
            global_y[best_c_r][best_c_c] = global_y[best_p_r][best_p_c] + Y1[best_c_r][best_c_c];
            global_x[best_c_r][best_c_c] = global_x[best_p_r][best_p_c] + X1[best_c_r][best_c_c];
        } else if (best_stitching_index == MST_CONNECTED_SOUTH) {
            global_y[best_c_r][best_c_c] = global_y[best_p_r][best_p_c] - Y1[best_p_r][best_p_c];
            global_x[best_c_r][best_c_c] = global_x[best_p_r][best_p_c] - X1[best_p_r][best_p_c];
        } else if (best_stitching_index == MST_CONNECTED_LEFT) {
            global_y[best_c_r][best_c_c] = global_y[best_p_r][best_p_c] + Y2[best_c_r][best_c_c];
            global_x[best_c_r][best_c_c] = global_x[best_p_r][best_p_c] + X2[best_c_r][best_c_c];
        } else if (best_stitching_index == MST_CONNECTED_RIGHT) {
            global_y[best_c_r][best_c_c] = global_y[best_p_r][best_p_c] - Y2[best_p_r][best_p_c];
            global_x[best_c_r][best_c_c] = global_x[best_p_r][best_p_c] - X2[best_p_r][best_p_c];
        }
    }

    // 3. Normalize coordinates (minimum x and y mapped to 1)
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (global_x[r][c] < min_x) min_x = global_x[r][c];
            if (global_y[r][c] < min_y) min_y = global_y[r][c];
        }
    }

    GridCoords out;
    out.x.assign(height, std::vector<float>(width));
    out.y.assign(height, std::vector<float>(width));
    out.coeff = tiling_coeff;
    out.tiling_indicator = tiling_indicator;

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            out.x[r][c] = std::round(global_x[r][c] - min_x) + 1.0f;
            out.y[r][c] = std::round(global_y[r][c] - min_y) + 1.0f;
        }
    }

    return out;
}

GridCoords GlobalOptimizer::DijkstraSPT(
    const std::vector<std::vector<float>>& Y1, 
    const std::vector<std::vector<float>>& X1, 
    const std::vector<std::vector<float>>& Y2, 
    const std::vector<std::vector<float>>& X2, 
    const std::vector<std::vector<float>>& W1, 
    const std::vector<std::vector<float>>& W2) 
{
    int height = Y1.size();
    int width = Y1[0].size();
    int num_tiles = height * width;

    float best_cost = std::numeric_limits<float>::max();
    
    std::vector<std::vector<float>> best_distance;
    std::vector<std::vector<int>> best_tiling_indicator;
    std::vector<std::vector<std::pair<int, int>>> best_parent;
    std::pair<int, int> best_start = {0, 0};

    // 1. Dijkstra from every node as source
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            std::vector<std::vector<int>> visited(height, std::vector<int>(width, 0));
            std::vector<std::vector<float>> distance(height, std::vector<float>(width, std::numeric_limits<float>::max()));
            std::vector<std::vector<std::pair<int, int>>> parent(height, std::vector<std::pair<int, int>>(width, {-1, -1}));
            std::vector<std::vector<int>> tiling_indicator(height, std::vector<int>(width, 0));

            distance[i][j] = 0.0f;

            for (int count = 0; count < num_tiles - 1; ++count) {
                // Find node with minimum distance among unvisited
                float min_d = std::numeric_limits<float>::max();
                int u_r = -1, u_c = -1;
                for (int r = 0; r < height; ++r) {
                    for (int c = 0; c < width; ++c) {
                        if (visited[r][c] == 0 && distance[r][c] < min_d) {
                            min_d = distance[r][c];
                            u_r = r; u_c = c;
                        }
                    }
                }

                if (u_r == -1) break; // Disconnected remaining nodes
                visited[u_r][u_c] = 1;

                // Update neighbor to the right (East) -> W2[row][col+1]
                if (u_c < width - 1 && visited[u_r][u_c + 1] == 0) {
                    float w = W2[u_r][u_c + 1];
                    if (!std::isnan(w) && distance[u_r][u_c] + w < distance[u_r][u_c + 1]) {
                        distance[u_r][u_c + 1] = distance[u_r][u_c] + w;
                        parent[u_r][u_c + 1] = {u_r, u_c};
                        tiling_indicator[u_r][u_c + 1] = MST_CONNECTED_LEFT;
                    }
                }

                // Update neighbor to the left (West) -> W2[row][col]
                if (u_c > 0 && visited[u_r][u_c - 1] == 0) {
                    float w = W2[u_r][u_c];
                    if (!std::isnan(w) && distance[u_r][u_c] + w < distance[u_r][u_c - 1]) {
                        distance[u_r][u_c - 1] = distance[u_r][u_c] + w;
                        parent[u_r][u_c - 1] = {u_r, u_c};
                        tiling_indicator[u_r][u_c - 1] = MST_CONNECTED_RIGHT;
                    }
                }

                // Update neighbor below (South) -> W1[row+1][col]
                if (u_r < height - 1 && visited[u_r + 1][u_c] == 0) {
                    float w = W1[u_r + 1][u_c];
                    if (!std::isnan(w) && distance[u_r][u_c] + w < distance[u_r + 1][u_c]) {
                        distance[u_r + 1][u_c] = distance[u_r][u_c] + w;
                        parent[u_r + 1][u_c] = {u_r, u_c};
                        tiling_indicator[u_r + 1][u_c] = MST_CONNECTED_NORTH;
                    }
                }

                // Update neighbor above (North) -> W1[row][col]
                if (u_r > 0 && visited[u_r - 1][u_c] == 0) {
                    float w = W1[u_r][u_c];
                    if (!std::isnan(w) && distance[u_r][u_c] + w < distance[u_r - 1][u_c]) {
                        distance[u_r - 1][u_c] = distance[u_r][u_c] + w;
                        parent[u_r - 1][u_c] = {u_r, u_c};
                        tiling_indicator[u_r - 1][u_c] = MST_CONNECTED_SOUTH;
                    }
                }
            }

            float cost = 0.0f;
            for (int r = 0; r < height; ++r) {
                for (int c = 0; c < width; ++c) {
                    if (distance[r][c] != std::numeric_limits<float>::max()) {
                        cost += distance[r][c];
                    }
                }
            }

            if (cost < best_cost) {
                best_cost = cost;
                best_distance = distance;
                best_tiling_indicator = tiling_indicator;
                best_parent = parent;
                best_start = {i, j};
            }
        }
    }

    // 2. Traversal using BFS from best_start to build coordinates
    std::vector<std::vector<float>> global_x(height, std::vector<float>(width, 0.0f));
    std::vector<std::vector<float>> global_y(height, std::vector<float>(width, 0.0f));
    std::vector<std::vector<int>> visited(height, std::vector<int>(width, 0));
    std::vector<std::vector<float>> tiling_coeff(height, std::vector<float>(width, 0.0f));

    std::queue<std::pair<int, int>> q;
    q.push(best_start);
    visited[best_start.first][best_start.second] = 1;
    tiling_coeff[best_start.first][best_start.second] = best_distance[best_start.first][best_start.second];

    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();

        // Check all 4 neighbors to see if their parent is (r, c)
        // Right neighbor
        if (c < width - 1 && visited[r][c + 1] == 0 && best_parent[r][c + 1] == std::make_pair(r, c)) {
            global_y[r][c + 1] = global_y[r][c] + Y2[r][c + 1];
            global_x[r][c + 1] = global_x[r][c] + X2[r][c + 1];
            tiling_coeff[r][c + 1] = best_distance[r][c + 1];
            visited[r][c + 1] = 1;
            q.push({r, c + 1});
        }
        // Left neighbor
        if (c > 0 && visited[r][c - 1] == 0 && best_parent[r][c - 1] == std::make_pair(r, c)) {
            global_y[r][c - 1] = global_y[r][c] - Y2[r][c];
            global_x[r][c - 1] = global_x[r][c] - X2[r][c];
            tiling_coeff[r][c - 1] = best_distance[r][c - 1];
            visited[r][c - 1] = 1;
            q.push({r, c - 1});
        }
        // Below neighbor
        if (r < height - 1 && visited[r + 1][c] == 0 && best_parent[r + 1][c] == std::make_pair(r, c)) {
            global_y[r + 1][c] = global_y[r][c] + Y1[r + 1][c];
            global_x[r + 1][c] = global_x[r][c] + X1[r + 1][c];
            tiling_coeff[r + 1][c] = best_distance[r + 1][c];
            visited[r + 1][c] = 1;
            q.push({r + 1, c});
        }
        // Above neighbor
        if (r > 0 && visited[r - 1][c] == 0 && best_parent[r - 1][c] == std::make_pair(r, c)) {
            global_y[r - 1][c] = global_y[r][c] - Y1[r][c];
            global_x[r - 1][c] = global_x[r][c] - X1[r][c];
            tiling_coeff[r - 1][c] = best_distance[r - 1][c];
            visited[r - 1][c] = 1;
            q.push({r - 1, c});
        }
    }

    // 3. Normalize coordinates (minimum x and y mapped to 1)
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (global_x[r][c] < min_x) min_x = global_x[r][c];
            if (global_y[r][c] < min_y) min_y = global_y[r][c];
        }
    }

    GridCoords out;
    out.x.assign(height, std::vector<float>(width));
    out.y.assign(height, std::vector<float>(width));
    out.coeff = tiling_coeff;
    out.tiling_indicator = best_tiling_indicator;

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            out.x[r][c] = std::round(global_x[r][c] - min_x) + 1.0f;
            out.y[r][c] = std::round(global_y[r][c] - min_y) + 1.0f;
        }
    }

    return out;
}
