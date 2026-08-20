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

// Wrap an angle into [0, 2*pi) -- matches dbdet_angle0To2Pi() convention
// used internally by dbdet_edgel's constructor for `tangent`.
static inline double wrap0to2pi(double a) {
    a = std::fmod(a, 2.0 * CV_PI);
    if (a < 0)
        a += 2.0 * CV_PI;
    return a;
}

// Writes edges to a .edg file
// edges:       edgemap from OpenCV
// orientation:
// threshold:   minimum edge strength to keep a pixel as an
// edgel.
//
// Returns false if the file couldn't be opened for writing.
static inline bool write_edg_v3(const std::string& filename,
                                const cv::Mat& edges,
                                const cv::Mat& orientation = cv::Mat(),
                                double threshold = 0.1,
                                bool orientation_is_normal = true) {
    CV_Assert(edges.type() == CV_32F);
    CV_Assert(orientation.empty() || (orientation.type() == CV_32F &&
                                      orientation.size() == edges.size()));

    struct E {
        int ix, iy;
        double dir, conf;
    };
    std::vector<E> pts;
    pts.reserve(static_cast<size_t>(edges.rows) * edges.cols /
                20); // rough guess

    for (int y = 0; y < edges.rows; ++y) {
        const auto* erow = edges.ptr<float>(y);
        const float* orow =
            orientation.empty() ? nullptr : orientation.ptr<float>(y);
        for (int x = 0; x < edges.cols; ++x) {
            float conf = erow[x];
            if (conf < threshold)
                continue;

            double dir = 0.0;
            if (orow != nullptr) {
                dir = orow[x];
                if (orientation_is_normal)
                    dir += CV_PI / 2.0;
            }
            dir = wrap0to2pi(dir);

            pts.push_back({x, y, dir, static_cast<double>(conf)});
        }
    }

    std::ofstream out{create_file_recursive_and_open(filename)};
    if (!out)
        return false;

    out << "# EDGE_MAP v3.0" << "\n\n";
    out << "# Format :  [Pixel_Pos]  Pixel_Dir Pixel_Conf  [Sub_Pixel_Pos] "
           "Sub_Pixel_Dir Sub_Pixel_Conf Sub_Pixel_Conf"
        << "\n\n";
    out << "WIDTH=" << edges.cols << "\n";
    out << "HEIGHT=" << edges.rows << "\n";
    out << "EDGE_COUNT=" << pts.size() << "\n\n\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& e : pts) {
        // No true sub-pixel localization is done here (x,y == ix,iy).
        out << "[" << e.ix << ", " << e.iy << "]    " << e.dir << " " << e.conf
            << "  "
            << "[" << static_cast<double>(e.ix) << ", "
            << static_cast<double>(e.iy) << "]  " << e.dir << " " << e.conf
            << " " << 0.0 << "\n";
    }
    return true;
}

// build a dbdet_edgemap directly, no file I/O
//
// Same parameters/semantics as write_edg_v3, minus the file. This is the
// call you want inline in a pipeline that goes straight from OpenCV
// detection into dbdet_sel_process.
static inline dbdet_edgemap_sptr
edgemap_from_opencv(const cv::Mat& edges,
                    const cv::Mat& orientation = cv::Mat(),
                    double threshold = 0.1, bool orientation_is_normal = true) {
    CV_Assert(edges.type() == CV_32F);
    CV_Assert(orientation.empty() || (orientation.type() == CV_32F &&
                                      orientation.size() == edges.size()));

    dbdet_edgemap_sptr EM = new dbdet_edgemap(edges.cols, edges.rows);

    for (int y = 0; y < edges.rows; ++y) {
        const auto* erow = edges.ptr<float>(y);
        const float* orow =
            orientation.empty() ? nullptr : orientation.ptr<float>(y);
        for (int x = 0; x < edges.cols; ++x) {
            float conf = erow[x];
            if (conf < threshold)
                continue;

            double dir = 0.0;
            if (orow != nullptr) {
                dir = orow[x];
                if (orientation_is_normal)
                    dir += CV_PI / 2.0;
            }
            // dbdet_edgel's constructor already calls dbdet_angle0To2Pi()
            // on the tangent argument, so no need to wrap dir here.

            auto* e = new dbdet_edgel(
                vgl_point_2d<double>(static_cast<double>(x),
                                     static_cast<double>(y)), // sub-pixel pos
                dir,                                          // tangent
                static_cast<double>(conf),                    // strength
                0.0,                                          // deriv
                0.0);                                         // uncertainty

            // insert(e, ix, iy) buckets it into the grid cell, appends to
            // EM->edgels, assigns e->id, and sets e->gpt -- all at once.
            EM->insert(e, x, y);
        }
    }
    return EM;
}

} // namespace dbdet_cv_bridge

#endif
