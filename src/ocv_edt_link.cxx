// \file
// \brief Edge detection (OpenCV contrib structured edge detector) + symbolic
//        edge linking (dbdet_sel_process), writing a .cem boundary fragment
//        map -- the OpenCV-native replacement for the old .edg-file-based
//        main.cxx.
//
//        Usage:
//          main <input_image> <structured_edge_model.yml.gz> <output.cem>
//          [threshold]
//
//        input_image             -- any image cv::imread can read
//        structured_edge_model   -- the pretrained model for
//                                    cv::ximgproc::createStructuredEdgeDetection()
//                                    (e.g. model.yml.gz from opencv_extra)
//        output.cem              -- where the linked curve fragment map goes
//        threshold                -- optional edge-strength cutoff in [0,1],
//                                    default 0.1
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
    image.convertTo(image, cv::DataType<float>::type, 1.0 / 255.0);

    cv::Ptr<cv::ximgproc::StructuredEdgeDetection> pDollar =
        cv::ximgproc::createStructuredEdgeDetection(model_path);
    if (pDollar.empty()) {
        vcl_cerr << "ERROR: could not load structured edge model " << model_path
                 << '\n';
        return 1;
    }

    cv::Mat edges;
    pDollar->detectEdges(image, edges); // CV_32FC1, values in [0,1]

    // computes orientation from edge map
    cv::Mat orientation_map;
    pDollar->computeOrientation(edges, orientation_map);

    // suppress edges -- thin the edge response down to (approximately)
    // single-pixel-wide ridges before handing it to the linker. Without
    // this, dbdet_sel_process's curvelet grouping chokes on blobs of
    // edgels a few pixels wide around every real edge.
    cv::Mat edges_nms;
    pDollar->edgesNms(edges, orientation_map, edges_nms, 2, 0, 1, true);

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
                                      /*orientation_is_normal=*/false);
    }

    //******************** Build dbdet_edgemap in memory *******************
    vcl_cout << "************* Build dbdet_edgemap *********" << '\n';

    // TODO: computeOrientation() returns the normal (gradient) direction,
    // not the curve tangent that dbdet_edgel::tangent expects, so verify
    // this empirically against linked output and flip
    // orientation_is_normal if fragments come out rotated 90 degrees.
    dbdet_edgemap_sptr EM = dbdet_cv_bridge::edgemap_from_opencv(
        edges_nms, orientation_map, threshold, /*orientation_is_normal=*/false);

    vcl_cout << "N edgels: " << EM->num_edgels() << '\n';

    dbdet_edgemap_storage_sptr input_edgemap = dbdet_edgemap_storage_new();
    input_edgemap->set_edgemap(EM);

    vcl_vector<bpro1_storage_sptr> edge_det_results;
    edge_det_results.emplace_back(input_edgemap.as_pointer());

    //******************** Edge Linking *********************************
    vcl_vector<bpro1_storage_sptr> el_results;
    vcl_cout << "************ Symbolic Edge Linking     ************" << '\n';
    dbdet_sel_process sel_pro;

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
