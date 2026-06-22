#include "opencv_conversion.hpp"

vcl_vector<bpro1_storage_sptr> convert_matrix_rgb(cv::Mat& mat) {
    for (int i = 0; i < mat.rows; ++i) {
        for (int k = 0; k < mat.cols; ++k) {
            std::cout << mat.at<cv::Vec3b>(i, k)[0] << " "
                      << mat.at<cv::Vec3b>(i, k)[1] << " "
                      << mat.at<cv::Vec3b>(i, k)[2] << " " << '\n';
        }
    }
    // TODO
}

vcl_vector<bpro1_storage_sptr> convert_matrix_grayscale(cv::Mat& mat) {
    for (int i = 0; i < mat.rows; i++)
        for (int j = 0; j < mat.cols; j++)
            std::cout << mat.at<uchar>(i, j) << '\n'; // TODO
}
