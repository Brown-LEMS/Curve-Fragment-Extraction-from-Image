#ifndef SRC_OPENCV_CONVERSION_HPP
#define SRC_OPENCV_CONVERSION_HPP

#include <opencv2/opencv.hpp>

#include <cmath>
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

/// helper that fits a parabola through three samples of edges along the local
/// normal direction, and returns the offset from (x, y) to the true sub-pixel
/// peak.
///
/// raw_edges should be the edge map BEFORE edgesNms
static inline cv::Point2d subpixel_offset(const cv::Mat& raw_edges, int x,
                                          int y, double normal_dir) {
    double ndx = std::cos(normal_dir);
    double ndy = std::sin(normal_dir);

    const auto sample = [&raw_edges, x, y](double ox, double oy) -> double {
        int sx = cvRound(x + ox);
        int sy = cvRound(y + oy);
        if (sx < 0 || sy < 0 || sx >= raw_edges.cols || sy >= raw_edges.rows)
            return 0.0;
        return static_cast<double>(raw_edges.at<float>(sy, sx));
    };

    double fm = sample(-ndx, -ndy);
    double f0 = sample(0.0, 0.0);
    double fp = sample(ndx, ndy);

    double denom = fm - (2.0 * f0) + fp;
    double offset = 0.0;
    if (std::abs(denom) > 1e-6) {
        offset = 0.5 * (fm - fp) / denom;
        offset = std::clamp(offset, -1.0, 1.0); // guard against a bad fit
    }
    return {offset * ndx, offset * ndy};
}

/// Writes edges to a .edg file
/// edges:       edgemap from OpenCV
/// orientation: orientation map
/// threshold:   minimum edge strength to keep a pixel as an
/// edgel.
//
// Returns false if the file couldn't be opened for writing.
static inline bool write_edg_v3(const std::string& filename,
                                const cv::Mat& edges_nms,
                                const cv::Mat& raw_edges = cv::Mat(),
                                const cv::Mat& orientation = cv::Mat(),
                                double threshold = 0.1,
                                bool orientation_is_normal = true) {
    CV_Assert(edges_nms.type() == CV_32F);
    CV_Assert(orientation.empty() || (orientation.type() == CV_32F &&
                                      orientation.size() == raw_edges.size()));

    struct E {
        int ix, iy;
        double x, y, dir, conf;
    };
    std::vector<E> pts;
    pts.reserve(static_cast<size_t>(edges_nms.rows) * edges_nms.cols / 20);

    for (int y = 0; y < edges_nms.rows; ++y) {
        const auto* erow = edges_nms.ptr<float>(y);
        const float* orow =
            orientation.empty() ? nullptr : orientation.ptr<float>(y);
        for (int x = 0; x < edges_nms.cols; ++x) {
            float conf = erow[x];
            if (conf < threshold)
                continue;

            double dir = 0.0;
            if (orow != nullptr) {
                dir = orow[x];
                if (orientation_is_normal)
                    dir += CV_PI / 2.0;
            }
            dir = wrap_minus_pi_to_pi(dir);

            double px = x;
            double py = y;
            if (!raw_edges.empty()) {
                double normal_dir = dir - CV_PI / 2.0;
                cv::Point2d off = subpixel_offset(raw_edges, x, y, normal_dir);
                px += off.x;
                py += off.y;
            }

            pts.push_back({.ix = x,
                           .iy = y,
                           .x = px,
                           .y = py,
                           .dir = dir,
                           .conf = static_cast<double>(conf)});
        }
    }
    std::ofstream out{create_file_recursive_and_open(filename)};
    if (!out)
        return false;

    out << "# EDGE_MAP v3.0" << "\n\n";
    out << "# Format :  [Pixel_Pos]  Pixel_Dir Pixel_Conf  [Sub_Pixel_Pos] "
           "Sub_Pixel_Dir Sub_Pixel_Conf Sub_Pixel_Conf"
        << "\n\n";
    out << "WIDTH=" << raw_edges.cols << "\n";
    out << "HEIGHT=" << raw_edges.rows << "\n";
    out << "EDGE_COUNT=" << pts.size() << "\n\n\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& e : pts) {
        out << "[" << e.ix << ", " << e.iy << "]    " << e.dir << " " << e.conf
            << "  " << "[" << e.x << ", " << e.y << "]  " << e.dir << " "
            << e.conf << " " << 0.0 << "\n";
    }
    return true;
}

/// build a dbdet_edgemap directly, no file I/O
///
/// Same parameters/semantics as write_edg_v3, minus the file. This is the
/// call you want inline in a pipeline that goes straight from OpenCV
/// detection into dbdet_sel_process.
static inline dbdet_edgemap_sptr
edgemap_from_opencv(const cv::Mat& edges_nms, const cv::Mat& raw_edges,
                    const cv::Mat& orientation = cv::Mat(),
                    double threshold = 0.1, bool orientation_is_normal = true) {
    CV_Assert(edges_nms.type() == CV_32F);
    CV_Assert(raw_edges.empty() || (raw_edges.type() == CV_32F &&
                                    raw_edges.size() == edges_nms.size()));
    CV_Assert(orientation.empty() || (orientation.type() == CV_32F &&
                                      orientation.size() == edges_nms.size()));

    dbdet_edgemap_sptr EM = new dbdet_edgemap(edges_nms.cols, edges_nms.rows);

    for (int y = 0; y < edges_nms.rows; ++y) {
        const auto* erow = edges_nms.ptr<float>(y);
        const float* orow =
            orientation.empty() ? nullptr : orientation.ptr<float>(y);
        for (int x = 0; x < edges_nms.cols; ++x) {
            float conf = erow[x];
            if (conf < threshold)
                continue;

            double dir = 0.0;
            if (orow != nullptr) {
                dir = orow[x];
                if (orientation_is_normal)
                    dir += CV_PI / 2.0;
            }

            double px = x;
            double py = y;
            if (!raw_edges.empty()) {
                // normal = tangent - 90deg; sign doesn't matter, fit is
                // symmetric
                double normal_dir = dir - CV_PI / 2.0;
                cv::Point2d off = subpixel_offset(raw_edges, x, y, normal_dir);
                px += off.x;
                py += off.y;
            }

            auto* e = new dbdet_edgel(
                vgl_point_2d<double>(px, py), // now genuinely sub-pixel
                dir, static_cast<double>(conf), 0.0, 0.0);

            EM->insert(e, x, y); // grid bucket stays at the integer pixel
        }
    }
    return EM;
}

} // namespace dbdet_cv_bridge

#endif
