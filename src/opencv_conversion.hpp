#ifndef SRC_OPENCV_CONVERSION_HPP
#define SRC_OPENCV_CONVERSION_HPP

#include "core/bpro1_storage_sptr.h"
#include <opencv2/opencv.hpp>

[[nodiscard]] vcl_vector<bpro1_storage_sptr> convert_matrix_rgb(cv::Mat& mat);

[[nodiscard]] vcl_vector<bpro1_storage_sptr>
convert_matrix_grayscale(cv::Mat& mat);

#endif
