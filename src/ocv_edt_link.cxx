// \file
// \brief Edge detection (OpenCV contrib structured edge detector) + symbolic
//        edge linking (dbdet_sel_process), writing a .cem boundary fragment
//        map - the OpenCV-native replacement for the old .edg-file-based
//        main.cxx.
//
//        Usage:
//          main <input_image> <structured_edge_model.yml.gz> <output.cem>
//          <output.edg> [threshold]
//
//        input_image             - any image cv::imread can read
//        structured_edge_model   - the pretrained model for
//                                    cv::ximgproc::createStructuredEdgeDetection()
//                                    (e.g. model.yml.gz from opencv_extra)
//        output.cem              - where the linked curve fragment map goes
//        threshold               - optional edge-strength cutoff in [0,1]
//
// \verbatim
//   Modifications
//     Ported from the .edg-file-loading main.cxx to consume an OpenCV
//     structured-edge detector directly, using dbdet_cv_bridge::
//     edgemap_from_opencv() to skip the text round-trip entirely.
// \endverbatim

#include <string>
#include <vcl_iostream.h>
#include <vcl_vector.h>
#include <vul/vul_timer.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/ximgproc.hpp>
#include <opencv2/ximgproc/edge_filter.hpp>
#include <opencv2/ximgproc/structured_edge_detection.hpp>

#include "opencv_conversion.hpp"

#include "core/dbdet_edgemap_storage.h"
#include "core/dbdet_save_cem_process.h"
#include "core/dbdet_sel_process.h"

bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size())
        return false;

    // Compare the end portion of 'str' with 'suffix'
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

[[nodiscard]] static cv::Mat detect_edges_multiscale(
    cv::Ptr<cv::ximgproc::StructuredEdgeDetection>& pDollar,
    const cv::Mat& image_float,
    const std::vector<double>& scales = {.5, .75, 1.0, 1.5, 2.0}) {

    cv::Mat accum = cv::Mat::zeros(image_float.size(), CV_32F);

    for (double s : scales) {
        cv::Mat scaled;
        cv::resize(image_float, scaled, cv::Size(), s, s,
                   s < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);

        cv::Mat e;
        pDollar->detectEdges(scaled, e);

        cv::Mat e_resized;
        cv::resize(e, e_resized, image_float.size(), 0, 0, cv::INTER_LINEAR);

        cv::max(accum, e_resized,
                accum); // max accum to try to get as much signal as possible
    }
    return accum;
}
[[nodiscard]] cv::Mat smooth_orientation(const cv::Mat& orientation,
                                         int ksize = 5) {
    CV_Assert(orientation.type() == CV_32F);

    cv::Mat mag = cv::Mat::ones(orientation.size(), CV_32F);
    cv::Mat cos_o;
    cv::Mat sin_o;
    cv::polarToCart(mag, orientation, cos_o,
                    sin_o); // vectorized angle->(cos,sin)

    cv::GaussianBlur(cos_o, cos_o, cv::Size(ksize, ksize), 0);
    cv::GaussianBlur(sin_o, sin_o, cv::Size(ksize, ksize), 0);

    cv::Mat mag_out;
    cv::Mat angle_out;
    cv::cartToPolar(
        cos_o, sin_o, mag_out,
        angle_out); // vectorized (cos,sin)->angle, radians in [0, 2pi)
    return angle_out;
}

int main(int argc, char* argv[]) {

    if (argc < 4) {
        vcl_cerr << "Usage: " << argv[0]
                 << " <input_image> <structured_edge_model.yml.gz> "
                    "<output.cem> [output.edg] [threshold=0.1]"
                 << '\n';
        return 1;
    }

    const std::string input_image_path = argv[1];
    const std::string model_path = argv[2];
    const std::string output_cem_file = argv[3];
    std::string output_edg_file{};
    if (argc > 4 && ends_with(argv[4], ".edg")) {
        output_edg_file = argv[4];
    }

    double threshold = 0.1;

    if (output_edg_file.empty() && argc == 5) {
        threshold = atof(argv[4]);
    } else if (argc == 6) {
        threshold = atof(argv[5]);
    }

    // Let time how long this takes
    vul_timer t;

    //******************** OpenCV Structured Edge Detection ***************
    vcl_cout << "************* Detect Edges (OpenCV) *********" << '\n';

    cv::Mat image = cv::imread(input_image_path, cv::IMREAD_COLOR);
    if (image.empty()) {
        vcl_cerr << "ERROR: could not read input image " << input_image_path
                 << '\n';
        return 1;
    }
    if (model_path.empty()) {
        vcl_cerr << "ERROR: empty model name" << '\n';
        return 1;
    }

    // StructuredEdgeDetection expects CV_32FC3 scaled to [0,1] (in-place,
    // same as OpenCV's own structured_edge_detection sample -- no BGR->RGB
    // swap here, matching that sample's behavior)
    image.convertTo(image, CV_32F, 1.0 / 255.0);

    cv::Ptr<cv::ximgproc::StructuredEdgeDetection> p_dollar =
        cv::ximgproc::createStructuredEdgeDetection(model_path);
    if (p_dollar.empty()) {
        vcl_cerr << "ERROR: could not load structured edge model " << model_path
                 << '\n';
        return 1;
    }

    cv::Mat edges =
        detect_edges_multiscale(p_dollar, image); // CV_32FC1, values in [0,1]

    // boost faint edges
    // cv::pow(edges, 0.8, edges);

    // computes orientation from edge map
    cv::Mat orientation_map;
    p_dollar->computeOrientation(edges, orientation_map);
    orientation_map = smooth_orientation(orientation_map);

    // suppress edges -- thin the edge response down to (approximately)
    // single-pixel-wide ridges before handing it to the linker. Without
    // this, dbdet_sel_process's curvelet grouping chokes on blobs of
    // edgels a few pixels wide around every real edge.
    cv::Mat edges_nms;
    p_dollar->edgesNms(edges, orientation_map, edges_nms,
                       /* 2 might be better here */ 1, 0, 1, true);

    vcl_cout << "Edge detection done." << '\n';

    // diagnostics:
    vcl_cout << "edges > 0:              " << cv::countNonZero(edges > 0.0F)
             << '\n';
    vcl_cout << "edges_nms > 0:          " << cv::countNonZero(edges_nms > 0.0F)
             << '\n';
    vcl_cout << "edges_nms >= threshold: "
             << cv::countNonZero(edges_nms >= (float)threshold) << '\n';

    if (output_edg_file.length()) {
        dbdet_cv_bridge::write_edg_v3(output_edg_file, edges_nms,
                                      orientation_map, threshold,
                                      /*orientation_is_normal=*/true);
    }

    //******************** Build dbdet_edgemap in memory *******************
    vcl_cout << "************* Build dbdet_edgemap *********" << '\n';

    dbdet_edgemap_sptr EM = dbdet_cv_bridge::edgemap_from_opencv(
        edges_nms, orientation_map, threshold, /*orientation_is_normal=*/true);

    vcl_cout << "N edgels: " << EM->num_edgels() << '\n';

    dbdet_edgemap_storage_sptr input_edgemap = dbdet_edgemap_storage_new();
    input_edgemap->set_edgemap(EM);

    vcl_vector<bpro1_storage_sptr> edge_det_results;
    edge_det_results.emplace_back(input_edgemap.as_pointer());

    //******************** Edge Linking *********************************
    vcl_vector<bpro1_storage_sptr> el_results;
    vcl_cout << "************ Symbolic Edge Linking     ************" << '\n';
    dbdet_sel_process sel_pro;

    // the edgemap has no real per-edgel uncertainty (edgemap_from_opencv
    // sets uncertainty=0.0), so force fixed -dx/-dt tolerances instead
    // of "adaptive" per-edgel uncertainty.
    sel_pro.parameters()->set_value("-badap_uncer", false);

    // loosened for pixel-quantized OpenCV positions + noisier orientation,
    // vs. defaults tuned for subpixel third-order detector output.
    sel_pro.parameters()->set_value("-dx", 0.75);  // was 0.4
    sel_pro.parameters()->set_value("-dt", 25.0);  // was 20.0
    sel_pro.parameters()->set_value("-nrad", 7.0); // was 3.5
    sel_pro.parameters()->set_value("-gap", 6.0);  // was 3.0

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
