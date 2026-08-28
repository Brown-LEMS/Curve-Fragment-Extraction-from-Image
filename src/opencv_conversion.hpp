#ifndef SRC_OPENCV_CONVERSION_HPP
#define SRC_OPENCV_CONVERSION_HPP

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "core/dbdet_edgel.h"
#include "core/dbdet_edgemap.h"
#include "core/dbdet_edgemap_sptr.h"

namespace dbdet_cv_bridge {

static inline std::ofstream create_file_recursive_and_open(
    const std::filesystem::path& file_path,
    std::ios_base::openmode mode = std::ios_base::out) {
    if (auto parent = file_path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    return std::ofstream(file_path, mode);
}

static inline void
create_file_recursive(const std::filesystem::path& file_path) {
    if (auto parent = file_path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

static inline double wrap_minus_pi_to_pi(double a) {
    a = std::fmod(a + CV_PI, 2.0 * CV_PI);
    if (a < 0)
        a += 2.0 * CV_PI;
    return a - CV_PI;
}

static inline double wrap_0_to_2pi(double a) {
    a = std::fmod(a, 2.0 * CV_PI);
    if (a < 0)
        a += 2.0 * CV_PI;
    return a;
}

/// a single subpixel edgel token, as produced by
/// third_order_subpix_correction.hpp's subpix_TO_correction():
///   [0] = x (subpixel column)
///   [1] = y (subpixel row)
///   [2] = orientation in radians (tangent)
///   [3] = confidence / edge strength (gradient magnitude at the maxima)
using edg_token = cv::Vec4d;

/// Writes a subpixel edgel list (as produced by subpix_TO_correction) to a
/// .edg file.
///
/// edginfo:              subpixel edgel tokens: [x, y, orientation, conf]
/// width, height:         size of the source edge map (for the file header
///                        and for clamping the integer pixel bucket)
/// orientation_is_normal: set true only if `edginfo`'s orientation field is
///                        the edge is normal
///
/// Returns false if the file couldn't be opened for writing.
static inline bool write_edg_v3(const std::string& filename,
                                const std::vector<edg_token>& edginfo,
                                int width, int height,
                                bool orientation_is_normal = false) {
    std::ofstream out{create_file_recursive_and_open(filename)};
    if (!out)
        return false;

    out << "# EDGE_MAP v3.0" << "\n\n";
    out << "# Format :  [Pixel_Pos]  Pixel_Dir Pixel_Conf  [Sub_Pixel_Pos] "
           "Sub_Pixel_Dir Sub_Pixel_Conf Sub_Pixel_Conf"
        << "\n\n";
    out << "WIDTH=" << width << "\n";
    out << "HEIGHT=" << height << "\n";
    out << "EDGE_COUNT=" << edginfo.size() << "\n\n\n";

    out << std::fixed << std::setprecision(6);
    for (const edg_token& e : edginfo) {
        const double px = e[0];
        const double py = e[1];
        double dir = e[2];
        if (orientation_is_normal)
            dir += CV_PI / 2.0;
        dir = wrap_minus_pi_to_pi(dir);
        const double conf = e[3];

        const int ix =
            std::clamp(static_cast<int>(std::lround(px)), 0, width - 1);
        const int iy =
            std::clamp(static_cast<int>(std::lround(py)), 0, height - 1);

        out << "[" << ix << ", " << iy << "]    " << dir << " " << conf << "  "
            << "[" << px << ", " << py << "]  " << dir << " " << conf << " "
            << 0.0 << "\n";
    }
    return true;
}

/// Builds a dbdet_edgemap directly from a subpixel edgel list, w/o file io
///
/// Same param/semantics as write_edg_v3, minus the file.
///
/// Intended for use inside the main pipeline that goes straight from
/// subpix_TO_correction's edginfo into dbdet_sel_process
static inline dbdet_edgemap_sptr
edgemap_from_opencv(const std::vector<edg_token>& edginfo, int width,
                    int height, bool orientation_is_normal = false) {
    dbdet_edgemap_sptr EM = new dbdet_edgemap(width, height);

    for (const edg_token& e : edginfo) {
        const double px = e[0];
        const double py = e[1];
        double dir = e[2];
        if (orientation_is_normal)
            dir += CV_PI / 2.0;
        dir = wrap_minus_pi_to_pi(dir);
        const double conf = e[3];

        const int ix =
            std::clamp(static_cast<int>(std::lround(px)), 0, width - 1);
        const int iy =
            std::clamp(static_cast<int>(std::lround(py)), 0, height - 1);

        auto* edgel = new dbdet_edgel(vgl_point_2d<double>(px, py), // sub-pixel
                                      dir, conf, 0.0, 0.0);

        EM->insert(edgel, ix, iy); // grid bucket stays at the integer pixel
    }
    return EM;
}

} // namespace dbdet_cv_bridge

#endif
