#ifndef cf_params_h_
#define cf_params_h_

// Curve-fragment extraction parameters.
//
// Edit the constants in this file and rebuild to change algorithm behavior.
// Every value below is the default that the pipeline actually uses.
// Command-line arguments still override the entries noted as CLI:
//   MSEL_img2CFs     nContours, e_sigma, e_thresh
//   MSEL_edges2CFs   nContours
//   ocv_compute_curve_frags   edge-strength threshold
//
// Choice parameters are integer indices. The legal values are listed
// on the line above each constant.

namespace cf_params {

// ---------------------------------------------------------------------------
// Third-order color edge detector (MSEL_img2CFs)
// ---------------------------------------------------------------------------

// 0 = RGB, 1 = IHS, 2 = Lab, 3 = Luv
constexpr unsigned edge_color_space = 2;
// 0 = Gaussian, 1 = h0-operator, 2 = h1-operator
constexpr unsigned edge_grad_op = 0;
// 0 = 2-D convolution, 1 = 1-D convolution
constexpr unsigned edge_conv_algo = 0;
// 0 = 3-point parabola fit, 1 = 9-point parabola fit
constexpr unsigned edge_parabola_fit = 0;

constexpr bool edge_load_component_images = false;
constexpr double edge_sigma = 1.0;          // Gaussian sigma; CLI e_sigma overrides
constexpr double edge_thresh = 2.0;         // gradient magnitude; CLI e_thresh overrides
constexpr int edge_int_factor = 1;          // interpolation factor 2^N
constexpr bool edge_reduce_tokens = false;

// ---------------------------------------------------------------------------
// Symbolic edge linking (every executable)
// ---------------------------------------------------------------------------

constexpr double sel_nrad = 3.5;            // grouping neighborhood radius (pixels)
constexpr double sel_gap = 3.0;             // max gap to complete (pixels)
constexpr bool sel_badap_uncer = true;      // read position/orientation uncertainty from edgels
constexpr double sel_dx = 0.4;              // position uncertainty (pixels), if not adaptive
constexpr double sel_dt_deg = 20.0;         // orientation uncertainty (degrees), if not adaptive

// 0 simple linear, 1 linear, 2 circular arc without perturbations,
// 3 circular arc with k classes, 4 circular arc with perturbations,
// 5 circular arc 3d bundle, 6 Euler spiral without perturbations,
// 7 Euler spiral with perturbations
constexpr unsigned sel_curve_model = 5;

constexpr double sel_token_len = 1.0;       // edgel token length (curvature bound)
constexpr double sel_max_k = 0.2;           // max curvature in the curve bundle
constexpr double sel_max_gamma = 0.05;      // max curvature derivative in the curve bundle

// 0 combinatorial, 1 hierarchical, 2 greedy local, 3 very greedy local
constexpr unsigned sel_grouping_algo = 2;
// 0 anchor centered, 1 anchor centered / bidirectional,
// 2 anchor leading / bidirectional, 3 ENO style around anchor
constexpr unsigned sel_cvlet_type = 0;
// 0 do not use appearance, 1 local comparison, 2 compare against reference
constexpr unsigned sel_app_usage = 0;
constexpr double sel_app_thresh = 0.2;

constexpr unsigned sel_max_size_to_group = 7;
constexpr bool sel_form_complete_cvlet_map = false;
constexpr bool sel_form_link_graph = true;

// 0 do not link, 1 from the link graph, 2 regular contours
constexpr unsigned sel_linking_algo = 2;
constexpr unsigned sel_num_link_iters = 7;
constexpr bool sel_get_final_contours = true;
constexpr bool sel_resolve_junctions = true;

// Applied inside dbdet_sel_process::get_parameters (not registered as
// process parameters, so this header is the only way to change them).
constexpr unsigned sel_linkgraph_algo = 0;
constexpr unsigned sel_min_size_to_link = 4;
constexpr bool sel_use_all_cvlets = false;
constexpr bool sel_centered_grouping = true;

// ocv_compute_curve_frags forces this value after the SEL defaults above.
constexpr bool ocv_sel_badap_uncer = false;

// ---------------------------------------------------------------------------
// OpenCV structured-edge front end (ocv_compute_curve_frags)
// ---------------------------------------------------------------------------

constexpr double ocv_edge_threshold = 0.1;  // CLI threshold overrides
constexpr double ocv_subpix_sigma = 2.0;    // third-order subpixel correction scale

constexpr double ocv_scales[] = {0.5, 1.0, 2.0};
constexpr int ocv_num_scales =
    static_cast<int>(sizeof(ocv_scales) / sizeof(ocv_scales[0]));

// Combined multiscale map: (avg_weight * mean + max_weight * max) / blend_norm
constexpr double ocv_scale_avg_weight = 9.0;
constexpr double ocv_scale_max_weight = 1.0;
constexpr double ocv_scale_blend_norm = 8.0;

// ---------------------------------------------------------------------------
// Contour ranker
// ---------------------------------------------------------------------------
constexpr int rank_nfrags = 0;              // top contours to keep; 0 = all. CLI nContours overrides
constexpr double rank_thresh = 0.0;
constexpr int rank_minlen = 0;              // minimum contour length in edgels

// Trained logistic weights. Index 0 is the constant feature.
//   0 bias, 1 bg gradient, 2 saturation gradient, 3 hue gradient,
//   4 |curvature|, 5 edge sparsity, 6 wiggle, 7 length, 8 mean confidence
constexpr double rank_fmean[9] = {
    0.0000000e+00, 1.2100564e-01, 1.1583408e-01, 8.5717724e-02, 1.5083448e-01,
    1.1946507e-01, 2.2051502e+00, 7.6632618e-01, 9.1834344e-01};
constexpr double rank_fstd[9] = {
    1.0000000e+00, 1.1751265e-01, 1.0735821e-01, 1.3995593e-01, 1.4768553e-01,
    1.0967336e-01, 1.6652428e+00, 3.4109466e-01, 4.1954811e-01};
constexpr double rank_beta[9] = {
    -1.7658682e-01, -2.4023619e-01, -2.8814772e-01, -2.8576244e-01, 4.9989011e-02,
    -1.5149038e-01, -4.1761749e-01, 9.2396348e-01, 1.5311652e-02};

// ---------------------------------------------------------------------------
// Geometric contour breaker
// ---------------------------------------------------------------------------

constexpr int break_max_iterations = 2;
constexpr int break_ref_table_nbr_range = 2;  // search window around reference endpoints
constexpr unsigned break_local_dist = 1;      // samples along the curve normal
constexpr unsigned break_nbr_width = 3;       // lateral neighborhood for sparsity / texture

// Geometric (2-feature) logistic model. Index 0 is the constant feature.
constexpr double break_fmean[2] = {0.0000000e+00, 7.6632618e-01};
constexpr double break_fstd[2] = {1.0000000e+00, 3.4109466e-01};
constexpr double break_beta[2] = {-1.7658682e-01, 1.0618483e+00};

// ---------------------------------------------------------------------------
// Graphical-model contour merge
// ---------------------------------------------------------------------------

constexpr unsigned merge_tex_nbr_dist = 3;    // texture histogram samples along the normal

// Appearance (9-feature) logistic model. Same feature order as the ranker.
constexpr double merge_fmean[9] = {
    0.0000000e+00, 1.2100564e-01, 1.1583408e-01, 8.5717724e-02, 1.5083448e-01,
    1.1946507e-01, 2.2051502e+00, 7.6632618e-01, 9.1834344e-01};
constexpr double merge_fstd[9] = {
    1.0000000e+00, 1.1751265e-01, 1.0735821e-01, 1.3995593e-01, 1.4768553e-01,
    1.0967336e-01, 1.6652428e+00, 3.4109466e-01, 4.1954811e-01};
constexpr double merge_beta[9] = {
    -1.7658682e-01, -2.4023619e-01, -2.8814772e-01, -2.8576244e-01, 4.9989011e-02,
    -1.5149038e-01, -4.1761749e-01, 9.2396348e-01, 1.5311652e-02};

// Geometric (2-feature) logistic model used inside the merge.
constexpr double merge_geom_fmean[2] = {0.0000000e+00, 7.6632618e-01};
constexpr double merge_geom_fstd[2] = {1.0000000e+00, 3.4109466e-01};
constexpr double merge_geom_beta[2] = {-1.7658682e-01, 1.0618483e+00};

// ---------------------------------------------------------------------------
// Shared break / merge geometry thresholds (dbdet_yuliang_const)
// ---------------------------------------------------------------------------

constexpr double diag_of_train = 578.275;     // diagonal of the training images
constexpr unsigned nbr_num_edges = 15;        // edgels examined near a junction
constexpr unsigned nbr_len_th = 5;            // curves shorter than this use the geometric rule
constexpr double merge_th_sem = 0.2;          // semantic merge probability threshold
constexpr double merge_th_geom = 0.5;         // geometric merge probability threshold

// ---------------------------------------------------------------------------
// Curve-fragment cue sampling
// ---------------------------------------------------------------------------

constexpr unsigned cue_local_dist = 2;        // samples along the curve normal
constexpr unsigned cue_nbr_width = 3;         // lateral neighborhood for edge sparsity

}  // namespace cf_params

#endif  // cf_params_h_
