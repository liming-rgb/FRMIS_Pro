#pragma once
#include <vector>

struct GridCoords {
    std::vector<std::vector<float>> x;     // global x position of each tile
    std::vector<std::vector<float>> y;     // global y position of each tile
    std::vector<std::vector<float>> coeff; // weight/distance coefficient for blending priority
    std::vector<std::vector<int>> tiling_indicator; // MST / SPT connectivity indicator
};

class GlobalOptimizer {
public:
    static GridCoords PrimMST(const std::vector<std::vector<float>>& Y1, 
                              const std::vector<std::vector<float>>& X1, 
                              const std::vector<std::vector<float>>& Y2, 
                              const std::vector<std::vector<float>>& X2, 
                              const std::vector<std::vector<float>>& W1, 
                              const std::vector<std::vector<float>>& W2);
                              
    static GridCoords DijkstraSPT(const std::vector<std::vector<float>>& Y1, 
                                  const std::vector<std::vector<float>>& X1, 
                                  const std::vector<std::vector<float>>& Y2, 
                                  const std::vector<std::vector<float>>& X2, 
                                  const std::vector<std::vector<float>>& W1, 
                                  const std::vector<std::vector<float>>& W2);
};
