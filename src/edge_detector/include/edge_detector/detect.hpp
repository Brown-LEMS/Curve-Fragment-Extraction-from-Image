#ifndef DETECT_HPP
#define DETECT_HPP

#include <opencv2/opencv.hpp>
#include <string>

namespace edt {

using namespace cv;

/// conver and image matrix to grayscale
[[nodiscard]] Mat grayscale(const Mat& img_orig);

/// blur an image matrix for smoother edges
[[nodiscard]] Mat blur(const Mat& img_orig);

/// enum modeling possible directions for Sobel edge detect
enum class SobelDir : uint8_t { dx, dy, dxdy };

/// detect Sobel edges
[[nodiscard]] Mat sobel(const Mat& img_orig, SobelDir dir);

/// detect Canny edges
[[nodiscard]] Mat canny(const Mat& img_orig);

/// display an image on a specified window, and then wait:
/// - image can be closed by pressing any key
void display_and_wait_on_window(const std::string& win_name, const Mat& img);

/// runs the demo for sobel & canny edge detection
void sobel_canny_demo(const char* img_path);

/// runs the demo for structured forest edges
int structured_edges_demo(int argc, char** argv);

} // namespace edt

#endif
