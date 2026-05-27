#pragma once
#include <opencv2/opencv.hpp>

class ImagePreprocessor {
public:
    static cv::Mat Process(const cv::Mat& input_image);
};
