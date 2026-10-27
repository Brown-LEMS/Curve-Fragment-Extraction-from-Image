#ifndef TOED_CONFIG_HPP
#define TOED_CONFIG_HPP
// =============================================================================
// Central configuration for Third-Order Edge Detection (CPU only; the GPU and
// curvelet code of the original TOED repo is not part of this port).
// Edit this file to control feature flags and algorithm parameters.
// =============================================================================

#include <cmath>

// ----------------------------------------------------------------------------
// Feature flags (compile-time)
// ----------------------------------------------------------------------------
#define OPENCV_SUPPORT                      (true)

// Write intermediate arrays (Ix, Iy, grad mag, etc.) for debugging
#define WriteDataToFile                     (0)

// Write third-order edges as an `.edg` file (EDGE_MAP v3.0, MATLAB-compatible)
#define WriteEdgFile                        (true)

// ----------------------------------------------------------------------------
// Algorithm / runtime parameters
// ----------------------------------------------------------------------------
namespace toed_cfg {

// ====================== Third-order edge detector =============================
// Gaussian scale. CPU filters are built from this value at runtime
constexpr int    sigma           = 1;

// Filter support. CPU kernels have length kernel_size (unshifted) and
// kernel_size+2 (half-pixel shift). Use 2*ceil(4*sigma)+1 (17 when sigma = 2)
constexpr int    kernel_size     = 17;

// Gradient-magnitude threshold in NMS (keep edges with mag > thresh)
// Matches MATLAB: grad_mag > threshold.
constexpr double grad_mag_thresh = 1.0;

// Border (in interpolated pixels) skipped during NMS.
// MATLAB uses (margin+2) with margin = ceil(4*sigma); for sigma = 2 that is 10
constexpr int    nms_border      = 10;

// ====================== CLI defaults (overridable by argv) ======================
constexpr int         default_nthreads   = 1;
constexpr const char* default_output_dir = "./output_files";

// ====================== Output filenames =====================================
constexpr const char* edges_txt_filename          = "TOED_edges.txt";
constexpr const char* edges_edg_filename          = "TOED_edges.edg";

} // namespace toed_cfg

#endif // TOED_CONFIG_HPP
