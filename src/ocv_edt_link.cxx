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

inline bool
equal(const double lhs, const double rhs,
      const double epsilon = std::numeric_limits<double>::epsilon()) {
    return std::abs(lhs - rhs) <=
           epsilon * std::max(std::abs(lhs), std::abs(rhs));
}

// detects edges on multiple scales averaging probs across scales
[[nodiscard]] static cv::Mat
detect_edges_multiscale(cv::Ptr<cv::ximgproc::StructuredEdgeDetection>& pDollar,
                        const cv::Mat& image_float) {
    const std::array scales = {.25, .5, 1.0, 2.0, 4.0};
    cv::Mat av_accum = cv::Mat::zeros(image_float.size(), CV_32F);
    cv::Mat max_accum = cv::Mat::zeros(image_float.size(), CV_32F);
    for (double s : scales) {
        cv::Mat scaled;
        cv::resize(image_float, scaled, cv::Size(), s, s,
                   s < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);
        cv::Mat e;
        pDollar->detectEdges(scaled, e);
        cv::Mat e_resized;
        cv::resize(e, e_resized, image_float.size(), 0, 0, cv::INTER_LINEAR);

        cv::max(max_accum, e_resized,
                max_accum);    // max accum for scales to try to get as
                               // much signal as possible
        av_accum += e_resized; // sum across scales, averaged below
    }
    av_accum /= static_cast<float>(scales.size());
    return (av_accum + max_accum * 2) /
           static_cast<float>(3); // weighted average these
}

[[nodiscard]] static cv::Mat denoise_and_sharpen(const cv::Mat& edges) {
    CV_Assert(edges.type() == CV_32F);

    cv::Mat denoised;
    cv::medianBlur(edges, denoised, 3);

    cv::Mat blurred;
    cv::GaussianBlur(denoised, blurred, cv::Size(0, 0), 1.0);
    cv::Mat sharpened = denoised + 1.0F * (denoised - blurred);
    cv::threshold(sharpened, sharpened, 0.0, 0.0,
                  cv::THRESH_TOZERO); // clip negative overshoot

    return sharpened;
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
    if (argc > 4 && std::string(argv[4]).ends_with(".edg")) {
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

    // StructuredEdgeDetection expects CV_32FC3 scaled to [0,1]
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

    // computes orientation from edge map
    cv::Mat orientation_map;
    p_dollar->computeOrientation(edges, orientation_map);

    cv::Mat edges_nms;
    p_dollar->edgesNms(edges, orientation_map, edges_nms,
                       /* r=2 by default */ 5, /*s=0 by default*/ 17, 1, true);

    vcl_cout << "edges_amplified >= 0.8:  "
             << cv::countNonZero(edges_nms >= 0.8F) << '\n';

    vcl_cout << "Edge detection done." << '\n';

    // diagnostics:
    vcl_cout << "edges > 0:              " << cv::countNonZero(edges > 0.0F)
             << '\n';
    vcl_cout << "edges_nms > 0:          " << cv::countNonZero(edges_nms > 0.0F)
             << '\n';
    vcl_cout << "edges_nms >= threshold: "
             << cv::countNonZero(edges_nms >= (float)threshold) << '\n';

    if (output_edg_file.length()) {
        dbdet_cv_bridge::write_edg_v3(output_edg_file, edges_nms, edges,
                                      orientation_map, threshold,
                                      /*orientation_is_normal=*/true);
    }

    //******************** Build dbdet_edgemap in memory *******************
    vcl_cout << "************* Build dbdet_edgemap *********" << '\n';

    dbdet_edgemap_sptr EM = dbdet_cv_bridge::edgemap_from_opencv(
        edges_nms, edges, orientation_map, threshold,
        /*orientation_is_normal=*/true);

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
    // sel_pro.parameters()->set_value("-gap", 5.0); // default = 3.0
    //  sel_pro.parameters()->set_value("-nrad", 5.0);            // default 2.5
    // sel_pro.parameters()->set_value("-max_size_to_group", 10); // default 7

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
