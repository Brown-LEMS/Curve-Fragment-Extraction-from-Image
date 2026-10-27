// \file
// \author Zach Mahan (zachary_mahan@brown.edu)
// \brief Edge detection (CPU Third-Order Edge Detector, src/toed) + symbolic
//        edge linking (dbdet_sel_process), writing a .cem boundary fragment
//        map - the third-order counterpart of ocv_edt_link.cxx.
//
//        Usage:
//          main <input_image> <output.cem> [output.edg] [threshold] [sigma]
//
//        input_image             - any image cv::imread can read (converted
//                                  to grayscale)
//        output.cem              - where the linked curve fragment map goes
//        output.edg              - optional, the detected third-order edgels
//        threshold               - optional gradient-magnitude cutoff
//                                  (not normalized; intensities are 0-255),
//                                  default 10
//        sigma                   - optional Gaussian scale of the detector,
//                                  default 2
//
//        A step edge of contrast h has peak gradient h / (sigma*sqrt(2*pi)),
//        so the defaults keep steps of roughly 50+ gray levels. Raising sigma
//        suppresses texture far better than raising the threshold alone.
//
//        The detector is multithreaded with OpenMP when available; set
//        OMP_NUM_THREADS to limit the number of threads.
//
// \verbatim
//   Modifications
//     Swaps ocv_edt_link.cxx's structured forest detector + subpixel TO
//     correction for the ThirdOrderEdgeDetectionCPU detector, which
//     produces subpixel third-order edgels directly.
// \endverbatim

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vcl_iostream.h>
#include <vcl_vector.h>
#include <vector>
#include <vul/vul_timer.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "opencv_conversion.hpp"
#include "toed/cpu_toed.hpp"
#include "toed/toed_omp.hpp"

#include "core/dbdet_edgemap_storage.h"
#include "core/dbdet_save_cem_process.h"
#include "core/dbdet_sel_process.h"

// runs the CPU third-order edge detector on a grayscale 8-bit image, returning
// the subpixel edgels as [x, y, orientation(tangent), strength]
[[nodiscard]] static std::vector<dbdet_cv_bridge::edg_token>
detect_edges_third_order(const cv::Mat& gray, double threshold, double sigma) {
    const int height = gray.rows;
    const int width = gray.cols;

    // filter support / NMS border follow the MATLAB margin of ceil(4*sigma),
    // but never shrink below the toed_config.hpp defaults
    const int margin = static_cast<int>(std::ceil(4.0 * sigma));
    const int kernel_size = std::max(toed_cfg::kernel_size, (2 * margin) + 1);
    const int nms_border = std::max(toed_cfg::nms_border, margin + 2);
    const int nthreads = omp_get_max_threads();

    vcl_cout << "TOED config: sigma=" << sigma << " kernel_size=" << kernel_size
             << " grad_mag_thresh=" << threshold << " nms_border=" << nms_border
             << " threads=" << nthreads << '\n';

    ThirdOrderEdgeDetectionCPU<double> toed(height, width, sigma, kernel_size,
                                            nthreads);
    toed.set_grad_mag_thresh(threshold);
    toed.set_nms_border(nms_border);

    // at most one edgel per interpolated (2x) pixel, 4 values each
    std::vector<double> toed_edges(static_cast<size_t>(2 * height) *
                                       static_cast<size_t>(2 * width) * 4,
                                   0.0);
    toed.preprocessing(gray);
    toed.convolve_img();
    const int edge_num = toed.non_maximum_suppresion(toed_edges.data());

    std::vector<dbdet_cv_bridge::edg_token> edginfo;
    edginfo.reserve(edge_num);
    for (int i = 0; i < edge_num; ++i) {
        const double* e = &toed_edges[static_cast<size_t>(i) * 4];
        edginfo.emplace_back(e[0], e[1], e[2], e[3]);
    }
    return edginfo;
}

int main(int argc, char* argv[]) {

    if (argc < 3) {
        vcl_cerr << "Usage: " << argv[0]
                 << " <input_image> <output.cem> [output.edg] "
                    "[threshold=8.0] [sigma=2.0]"
                 << '\n';
        return 1;
    }

    const std::string input_image_path = argv[1];
    const std::string output_cem_file = argv[2];
    std::string output_edg_file{};
    int next_arg = 3;
    if (argc > next_arg) {
        const std::string arg(argv[next_arg]);
        const std::string edg_suffix = ".edg";
        if (arg.size() >= edg_suffix.size() &&
            arg.compare(arg.size() - edg_suffix.size(), edg_suffix.size(),
                        edg_suffix) == 0) {
            output_edg_file = arg;
            ++next_arg;
        }
    }

    // defaults
    double threshold = 8.0;
    double sigma = 2.0;

    if (argc > next_arg) {
        threshold = atof(argv[next_arg++]);
    }
    if (argc > next_arg) {
        sigma = atof(argv[next_arg++]);
    }
    if (sigma <= 0.0) {
        vcl_cerr << "ERROR: sigma must be positive" << '\n';
        return 1;
    }

    // Let time how long this takes
    vul_timer t;

    //******************** Third-Order Edge Detection *********************
    vcl_cout << "************* Detect Edges (Third-Order) *********" << '\n';

    cv::Mat image = cv::imread(input_image_path, cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
        vcl_cerr << "ERROR: could not read input image " << input_image_path
                 << '\n';
        return 1;
    }

    // [x, y, orientation(tangent), confidence], already subpixel
    std::vector<dbdet_cv_bridge::edg_token> edginfo =
        detect_edges_third_order(image, threshold, sigma);

    vcl_cout << "subpixel edgels found: " << edginfo.size() << '\n';

    if (output_edg_file.length()) {
        if (!dbdet_cv_bridge::write_edg_v3(output_edg_file, edginfo, image.cols,
                                           image.rows,
                                           /*orientation_is_normal=*/false)) {
            vcl_cerr << "ERROR: failed to write " << output_edg_file << '\n';
        }
    }

    //******************** Build dbdet_edgemap in memory *******************
    vcl_cout << "************* Build dbdet_edgemap *********" << '\n';

    dbdet_edgemap_sptr EM = dbdet_cv_bridge::edgemap_from_opencv(
        edginfo, image.cols, image.rows, /*orientation_is_normal=*/false);

    vcl_cout << "N edgels: " << EM->num_edgels() << '\n';

    dbdet_edgemap_storage_sptr input_edgemap = dbdet_edgemap_storage_new();
    input_edgemap->set_edgemap(EM);

    vcl_vector<bpro1_storage_sptr> edge_det_results;
    edge_det_results.emplace_back(input_edgemap.as_pointer());

    //******************** Edge Linking *********************************
    vcl_vector<bpro1_storage_sptr> el_results;
    vcl_cout << "************ Symbolic Edge Linking     ************" << '\n';
    dbdet_sel_process sel_pro;

    sel_pro.parameters()->set_value("-badap_uncer", false);

    sel_pro.clear_input();
    sel_pro.clear_output();

    sel_pro.add_input(edge_det_results[0]);
    bool el_status = sel_pro.execute();
    sel_pro.finish();

    if (el_status) {
        el_results = sel_pro.get_output();
    }

    sel_pro.clear_input();
    sel_pro.clear_output();

    if (el_results.size() != 1) {
        vcl_cerr << "Process output does not contain a sel data structure"
                 << '\n';
        return 1;
    }

    //******************** Save Contours  *********************************
    vcl_cout << "************ Saving Contours  ************" << '\n';

    dbdet_cv_bridge::create_file_recursive_and_open(output_cem_file);

    bool write_status(false);
    vcl_cout << "output: " << output_cem_file << '\n';

    bpro1_filepath output(output_cem_file, ".cem");

    dbdet_save_cem_process save_cem_pro;
    save_cem_pro.parameters()->set_value("-cem_filename", output);

    save_cem_pro.clear_input();
    save_cem_pro.clear_output();

    save_cem_pro.add_input(el_results[0]);
    write_status = save_cem_pro.execute();
    save_cem_pro.finish();

    save_cem_pro.clear_input();
    save_cem_pro.clear_output();

    if (!write_status) {
        vcl_cerr << "ERROR: failed to write " << output_cem_file << '\n';
        return 1;
    }

    double total_time = static_cast<double>(t.real()) / 1000.0;
    t.mark();
    vcl_cout << '\n';
    vcl_cout << "************ Time taken: " << total_time << " sec" << '\n';

    return 0;
}
