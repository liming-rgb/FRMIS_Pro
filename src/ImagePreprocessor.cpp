#include "ImagePreprocessor.h"

namespace {
    constexpr int FILTER_SIZE = 3;
    constexpr float THRESHOLD_MULTIPLIER = 2.5f;
    constexpr double EPSILON = 1e-5;
    constexpr double GAMMA_FACTOR = 0.7;
    constexpr double GAUSSIAN_SIGMA = 0.75;
}

cv::Mat ImagePreprocessor::Process(const cv::Mat& input_image) {
    cv::Mat float_img;
    input_image.convertTo(float_img, CV_32F);

    cv::medianBlur(float_img, float_img, FILTER_SIZE);

    cv::Scalar mu, sigma;
    cv::meanStdDev(float_img, mu, sigma);
    float thresh = static_cast<float>(mu[0] + THRESHOLD_MULTIPLIER * sigma[0]);

    double minVal, maxVal;
    cv::minMaxLoc(float_img, &minVal, &maxVal);
    
    if (maxVal > thresh + EPSILON) {
        cv::subtract(float_img, cv::Scalar(thresh), float_img);
        cv::max(float_img, 0.0, float_img);
        float_img.convertTo(float_img, -1, 1.0 / (maxVal - thresh));
        cv::pow(float_img, GAMMA_FACTOR, float_img);
    } else {
        return cv::Mat::zeros(input_image.size(), CV_32F);
    }

    cv::GaussianBlur(float_img, float_img, cv::Size(7, 7), GAUSSIAN_SIGMA);
    return float_img;
}
