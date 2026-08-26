#ifndef THIRD_ORDER_SUBPIX_CORRECTION_HPP
#define THIRD_ORDER_SUBPIX_CORRECTION_HPP
///
/// C++/OpenCV4 translation of MATLAB TO subpixel
/// edge/orientation correction:
///    subpix_TO_correction.m  (+ NMS_token.m, resampled_filter_2d.m,
///                              Gx_2d_op.m, Gy_2d_op.m, Gxx_2d_op.m,
///                              Gxy_2d_op.m, Gyy_2d_op.m)
///
/// Remaining implementation notes:
///
///   1) Only the n == 0 (no super-sampling) branch of resampled_filter_2d is
///      implemented
///
///   2) imfilter(sig, h, 'conv', 'circular') is TRUE convolution (kernel
///      rotated 180 degrees) with wrap-around border handling. OpenCV's
///      cv::filter2D performs correlation, so the kernel is flipped with
///      cv::flip(..., -1) before calling filter2D with cv::BORDER_WRAP
///
///   3) MATLAB's interp2(..., 'cubic') is replaced with a standard 4x4
///      cubic-convolution sampler (Keys, a = -0.5), which is what MATLAB's
///      'cubic' method for interp2 implements. Border samples are clamped
///      to the image edge (MATLAB would return NaN outside the sampled
///      grid; edgels found by NMS_token are always `margin` pixels from the
///      border so this should not matter in practice, but the clamp is a
///      safety net).

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace subpix_to_detail {

/// 1D Gaussian and its derivatives (helpers shared by the five kernel
/// generators below). `t` is already the shifted coordinate (e.g. x - dx).
static inline double gauss1d(double t, double sigma) {
    return std::exp(-(t * t) / (2.0 * sigma * sigma)) /
           (std::sqrt(2.0 * CV_PI) * sigma);
}
static inline double dgauss1d(double t, double sigma) {
    // -t * exp(-t^2/(2*sigma^2)) / (sqrt(2*pi) * sigma^3)
    return -t * std::exp(-(t * t) / (2.0 * sigma * sigma)) /
           (std::sqrt(2.0 * CV_PI) * sigma * sigma * sigma);
}
static inline double d2gauss1d(double t, double sigma) {
    // (t^2 - sigma^2) * exp(-t^2/(2*sigma^2)) / (sqrt(2*pi) * sigma^5)
    return ((t * t) - (sigma * sigma)) *
           std::exp(-(t * t) / (2.0 * sigma * sigma)) /
           (std::sqrt(2.0 * CV_PI) * sigma * sigma * sigma * sigma * sigma);
}

/// window half-size, exactly: w = ceil(sigma*4 + max(|dx|,|dy|))
static inline int kernel_half_width(double sigma, double dx, double dy) {
    return static_cast<int>(
        std::ceil((sigma * 4.0) + std::max(std::abs(dx), std::abs(dy))));
}

/// dG/dx operator with operator shift (dx, dy), literal port of Gx_2d_op.m
static inline cv::Mat Gx_2d_op(double sigma, double dx = 0.0, double dy = 0.0) {
    const int w = kernel_half_width(sigma, dx, dy);
    const int size = (2 * w) + 1;
    cv::Mat kernel(size, size, CV_32F);
    for (int r = 0; r < size; ++r) {
        const double y = (r - w);
        const double G_y = gauss1d(y - dy, sigma);
        for (int c = 0; c < size; ++c) {
            const double x = (c - w);
            const double dG_x = dgauss1d(x - dx, sigma);
            kernel.at<float>(r, c) = static_cast<float>(dG_x * G_y);
        }
    }
    return kernel;
}

/// dG/dy operator with operator shift (dx, dy), literal port of Gy_2d_op.m
static inline cv::Mat Gy_2d_op(double sigma, double dx = 0.0, double dy = 0.0) {
    const int w = kernel_half_width(sigma, dx, dy);
    const int size = (2 * w) + 1;
    cv::Mat kernel(size, size, CV_32F);
    for (int r = 0; r < size; ++r) {
        const double y = (r - w);
        const double dG_y = dgauss1d(y - dy, sigma);
        for (int c = 0; c < size; ++c) {
            const double x = (c - w);
            const double G_x = gauss1d(x - dx, sigma);
            kernel.at<float>(r, c) = static_cast<float>(G_x * dG_y);
        }
    }
    return kernel;
}

/// d^2G/dx^2 operator with operator shift (dx, dy), literal port of
/// Gxx_2d_op.m
static inline cv::Mat Gxx_2d_op(double sigma, double dx = 0.0,
                                double dy = 0.0) {
    const int w = kernel_half_width(sigma, dx, dy);
    const int size = (2 * w) + 1;
    cv::Mat kernel(size, size, CV_32F);
    for (int r = 0; r < size; ++r) {
        const double y = (r - w);
        const double G_y = gauss1d(y - dy, sigma);
        for (int c = 0; c < size; ++c) {
            const double x = (c - w);
            const double d2G_x = d2gauss1d(x - dx, sigma);
            kernel.at<float>(r, c) = static_cast<float>(d2G_x * G_y);
        }
    }
    return kernel;
}

/// d^2G/dxdy operator with operator shift (dx, dy), literal port of
/// Gxy_2d_op.m
static inline cv::Mat Gxy_2d_op(double sigma, double dx = 0.0,
                                double dy = 0.0) {
    const int w = kernel_half_width(sigma, dx, dy);
    const int size = (2 * w) + 1;
    cv::Mat kernel(size, size, CV_32F);
    for (int r = 0; r < size; ++r) {
        const double y = (r - w);
        const double dG_y = dgauss1d(y - dy, sigma);
        for (int c = 0; c < size; ++c) {
            const double x = (c - w);
            const double dG_x = dgauss1d(x - dx, sigma);
            kernel.at<float>(r, c) = static_cast<float>(dG_x * dG_y);
        }
    }
    return kernel;
}

/// d^2G/dy^2 operator with operator shift (dx, dy), literal port of
/// Gyy_2d_op.m
static inline cv::Mat Gyy_2d_op(double sigma, double dx = 0.0,
                                double dy = 0.0) {
    const int w = kernel_half_width(sigma, dx, dy);
    const int size = (2 * w) + 1;
    cv::Mat kernel(size, size, CV_32F);
    for (int r = 0; r < size; ++r) {
        const double y = (r - w);
        const double d2G_y = d2gauss1d(y - dy, sigma);
        for (int c = 0; c < size; ++c) {
            const double x = (c - w);
            const double G_x = gauss1d(x - dx, sigma);
            kernel.at<float>(r, c) = static_cast<float>(G_x * d2G_y);
        }
    }
    return kernel;
}

/// resampled_filter_2d, n == 0 case only (see implementation note #1 above):
/// imfilter(sig, filt_fun_2d(sigma,0,0), 'conv', 'circular')
static inline cv::Mat resampled_filter_2d_n0(const cv::Mat& sig,
                                             const cv::Mat& kernel) {
    cv::Mat flipped;
    cv::flip(kernel, flipped, -1); // 180-degree rotation: conv, not correlation
    cv::Mat dst;
    cv::filter2D(sig, dst, CV_32F, flipped, cv::Point(-1, -1), 0.0,
                 cv::BORDER_WRAP); // 'circular' boundary
    return dst;
}

/// Cubic-convolution interpolation (Keys, a = -0.5), matches MATLAB's
/// interp2(..., 'cubic'). (x, y) are in the same (col, row) pixel-index
/// units as the image (0-indexed here, vs. MATLAB's 1-indexed; internally
/// consistent since NMS_token below also produces 0-indexed coordinates).
static inline double cubic_weight(double t) {
    const double a = -0.5;
    t = std::abs(t);
    if (t <= 1.0) {
        return ((a + 2.0) * t * t * t) - ((a + 3.0) * t * t) + 1.0;
    }
    if (t < 2.0) {
        return (a * t * t * t) - (5.0 * a * t * t) + (8.0 * a * t) - (4.0 * a);
    }
    return 0.0;
}

static inline double bicubic_sample(const cv::Mat& img, double x, double y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const double fx = x - x0;
    const double fy = y - y0;

    double result = 0.0;
    for (int m = -1; m <= 2; ++m) {
        const double wy = cubic_weight(m - fy);
        const int yi = std::clamp(y0 + m, 0, img.rows - 1);
        for (int k = -1; k <= 2; ++k) {
            const double wx = cubic_weight(k - fx);
            const int xi = std::clamp(x0 + k, 0, img.cols - 1);
            result += wx * wy * static_cast<double>(img.at<float>(yi, xi));
        }
    }
    return result;
}

/// NMS_token: non-maximal suppression with subpixel (parabolic) fit.
///
///   Gx, Gy - direction field used to decide which of the 8 "faces" to
///            interpolate along (dirx = cos(theta), diry = sin(theta) at
///            the call site, NOT necessarily the gradient magnitude).
///   G      - the surface being suppressed (edge magnitude).
///   mask   - nonzero where a pixel is even eligible to be tested (G>thresh).
///   margin - border pixels to skip.
///
struct EdgelToken {
    double x, y;           // subpixel location
    double dir_x, dir_y;   // edgel orientation (tangent) vector
    double grad_x, grad_y; // gradient vector at the maxima
};

static inline void NMS_token(const cv::Mat& Gx, const cv::Mat& Gy,
                             const cv::Mat& G, const cv::Mat& mask, int margin,
                             std::vector<EdgelToken>& tokens) {
    tokens.clear();

    const int size_y = G.rows;
    const int size_x = G.cols;

    const int x_lo = margin + 1;
    const int x_hi = size_x - margin - 3;
    const int y_lo = margin + 1;
    const int y_hi = size_y - margin - 3;

    for (int x = x_lo; x <= x_hi; ++x) {
        for (int y = y_lo; y <= y_hi; ++y) {
            if (mask.at<uchar>(y, x) == 0)
                continue;

            const double gx = Gx.at<float>(y, x);
            const double gy = Gy.at<float>(y, x);

            if (std::abs(gx) < 1e-5 &&
                std::abs(gy) < 1e-5) /// invalid direction
                continue;

            int face;
            if (gx >= 0 && gy >= 0) { // first quadrant
                face = (gx >= gy) ? 1 : 2;
            } else if (gx < 0 && gy >= 0) {
                face = (std::abs(gx) < gy) ? 3 : 4;
            } else if (gx < 0 && gy < 0) {
                face = (std::abs(gx) >= std::abs(gy)) ? 5 : 6;
            } else { // gx >= 0 && gy < 0
                face = (gx < std::abs(gy)) ? 7 : 8;
            }

            double dirx = gx;
            double diry = gy;
            const double norm = std::sqrt((dirx * dirx) + (diry * diry));
            dirx /= norm;
            diry /= norm;

            const double f = G.at<float>(y, x);
            double d1;
            double fp;
            double fm;

            switch (face) {
            case 1:
                d1 = diry / dirx;
                fp = (G.at<float>(y, x + 1) * (1 - d1)) +
                     (G.at<float>(y + 1, x + 1) * d1);
                fm = (G.at<float>(y, x - 1) * (1 - d1)) +
                     (G.at<float>(y - 1, x - 1) * d1);
                break;
            case 2:
                d1 = dirx / diry;
                fp = (G.at<float>(y + 1, x) * (1 - d1)) +
                     (G.at<float>(y + 1, x + 1) * d1);
                fm = (G.at<float>(y - 1, x) * (1 - d1)) +
                     (G.at<float>(y - 1, x - 1) * d1);
                break;
            case 3:
                d1 = -dirx / diry;
                fp = (G.at<float>(y + 1, x) * (1 - d1)) +
                     (G.at<float>(y + 1, x - 1) * d1);
                fm = (G.at<float>(y - 1, x) * (1 - d1)) +
                     (G.at<float>(y - 1, x + 1) * d1);
                break;
            case 4:
                d1 = -diry / dirx;
                fp = (G.at<float>(y, x - 1) * (1 - d1)) +
                     (G.at<float>(y + 1, x - 1) * d1);
                fm = (G.at<float>(y, x + 1) * (1 - d1)) +
                     (G.at<float>(y - 1, x + 1) * d1);
                break;
            case 5:
                d1 = diry / dirx;
                fp = (G.at<float>(y, x - 1) * (1 - d1)) +
                     (G.at<float>(y - 1, x - 1) * d1);
                fm = (G.at<float>(y, x + 1) * (1 - d1)) +
                     (G.at<float>(y + 1, x + 1) * d1);
                break;
            case 6:
                d1 = dirx / diry;
                fp = (G.at<float>(y - 1, x) * (1 - d1)) +
                     (G.at<float>(y - 1, x - 1) * d1);
                fm = (G.at<float>(y + 1, x) * (1 - d1)) +
                     (G.at<float>(y + 1, x + 1) * d1);
                break;
            case 7:
                d1 = -dirx / diry;
                fp = (G.at<float>(y - 1, x) * (1 - d1)) +
                     (G.at<float>(y - 1, x + 1) * d1);
                fm = (G.at<float>(y + 1, x) * (1 - d1)) +
                     (G.at<float>(y + 1, x - 1) * d1);
                break;
            case 8:
            default:
                d1 = -diry / dirx;
                fp = (G.at<float>(y, x + 1) * (1 - d1)) +
                     (G.at<float>(y - 1, x + 1) * d1);
                fm = (G.at<float>(y, x - 1) * (1 - d1)) +
                     (G.at<float>(y + 1, x - 1) * d1);
                break;
            }

            const double s = std::sqrt(1.0 + (d1 * d1));

            const bool is_max = (f > fm && f > fp) ||  // abs max
                                (f > fm && f >= fp) || // relaxed max
                                (f >= fm && f > fp);
            if (!is_max)
                continue;

            // fit parabola
            const double A = (fm + fp - (2 * f)) / (2 * s * s);
            const double B = (fp - fm) / (2 * s);
            const double C = f;

            const double s_star = -B / (2 * A); // location of max
            const double max_f = (A * s_star * s_star) + (B * s_star) + C;

            if (std::abs(s_star) <=
                std::sqrt(2.0)) { // significant max within 1px
                EdgelToken tok;
                tok.x = x + (s_star * dirx);
                tok.y = y + (s_star * diry);
                tok.dir_x = -diry;
                tok.dir_y = dirx;
                tok.grad_x = max_f * dirx;
                tok.grad_y = max_f * diry;
                tokens.push_back(tok);
            }
        }
    }
}

} // namespace subpix_to_detail

/// subpix_TO_correction
///
///   input_edgemap - CV_32F edge-probability/magnitude surface (E)
///   input_theta   - CV_32F edge-normal orientation in radians (O)
///   threshold     - NMS eligibility threshold (edgemap > threshold)
///   sigma         - Gaussian-derivative scale for the third-order fit
///   out_to_edgemap, out_to_thetamap - CV_32F, same size as input, filled
///        in-place (resized/allocated as needed) with the corrected
///        confidence / orientation at each detected subpixel edgel's
///        rounded integer location.
///   out_edginfo (optional) - if non-null, receives the raw subpixel edgel
///        list [x, y, orientation, confidence] before rounding
///        (equivalent to MATLAB's `edginfo` return value)
///
/// Only the n == 0 branch of the MATLAB function is implemented (no
/// super-sampling)
static inline void
subpix_TO_correction(cv::Mat& out_to_edgemap, cv::Mat& out_to_thetamap,
                     cv::Mat& input_edgemap, cv::Mat& input_theta,
                     double threshold, double sigma,
                     std::vector<cv::Vec4d>* out_edginfo = nullptr) {
    using namespace subpix_to_detail;

    CV_Assert(input_edgemap.type() == CV_32F);
    CV_Assert(input_theta.size() == input_edgemap.size());
    CV_Assert(input_theta.type() == CV_32F);

    const int h = input_edgemap.rows;
    const int w = input_edgemap.cols;
    const int margin = 3;

    /// dirx = cos(mod(theta,pi)); diry = sin(mod(theta,pi));
    cv::Mat dirx(h, w, CV_32F);
    cv::Mat diry(h, w, CV_32F);
    for (int y = 0; y < h; ++y) {
        const float* trow = input_theta.ptr<float>(y);
        auto* dxrow = dirx.ptr<float>(y);
        auto* dyrow = diry.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            double t = std::fmod(static_cast<double>(trow[x]), CV_PI);
            if (t < 0)
                t += CV_PI;
            dxrow[x] = static_cast<float>(std::cos(t));
            dyrow[x] = static_cast<float>(std::sin(t));
        }
    }

    cv::Mat mask;
    cv::compare(input_edgemap, threshold, mask, cv::CMP_GT); // CV_8U 0/255

    // non-max suppression -> subpixel edgel tokens
    std::vector<EdgelToken> tokens;
    NMS_token(dirx, diry, input_edgemap, mask, margin, tokens);

    if (tokens.empty()) {
        out_to_edgemap = cv::Mat::zeros(h, w, CV_32F);
        out_to_thetamap = cv::Mat::zeros(h, w, CV_32F);
        if (out_edginfo != nullptr)
            out_edginfo->clear();
        return;
    }

    // Hx = edgemap .* dirx; Hy = edgemap .* diry
    cv::Mat Hx = input_edgemap.mul(dirx);
    cv::Mat Hy = input_edgemap.mul(diry);

    // Derivative-of-Gaussian filters, n == 0 (no supersampling)
    cv::Mat Px = resampled_filter_2d_n0(input_edgemap, Gx_2d_op(sigma));
    cv::Mat Py = resampled_filter_2d_n0(input_edgemap, Gy_2d_op(sigma));
    cv::Mat Pxx = resampled_filter_2d_n0(input_edgemap, Gxx_2d_op(sigma));
    cv::Mat Pxy = resampled_filter_2d_n0(input_edgemap, Gxy_2d_op(sigma));
    cv::Mat Pyy = resampled_filter_2d_n0(input_edgemap, Gyy_2d_op(sigma));

    cv::Mat Hxx = resampled_filter_2d_n0(Hx, Gx_2d_op(sigma));
    cv::Mat Hyy = resampled_filter_2d_n0(Hy, Gy_2d_op(sigma));
    cv::Mat Hxy = resampled_filter_2d_n0(Hy, Gx_2d_op(sigma));

    out_to_edgemap = cv::Mat::zeros(h, w, CV_32F);
    out_to_thetamap = cv::Mat::zeros(h, w, CV_32F);
    if (out_edginfo != nullptr)
        out_edginfo->clear();

    for (const EdgelToken& tok : tokens) {
        const double mag_e =
            std::sqrt((tok.grad_x * tok.grad_x) + (tok.grad_y * tok.grad_y));

        const double Px_e = bicubic_sample(Px, tok.x, tok.y);
        const double Py_e = bicubic_sample(Py, tok.x, tok.y);
        const double Pxx_e = bicubic_sample(Pxx, tok.x, tok.y);
        const double Pyy_e = bicubic_sample(Pyy, tok.x, tok.y);
        const double Pxy_e = bicubic_sample(Pxy, tok.x, tok.y);
        const double Hx_e = bicubic_sample(Hx, tok.x, tok.y);
        const double Hy_e = bicubic_sample(Hy, tok.x, tok.y);
        const double Hxx_e = bicubic_sample(Hxx, tok.x, tok.y);
        const double Hxy_e = bicubic_sample(Hxy, tok.x, tok.y);
        const double Hyy_e = bicubic_sample(Hyy, tok.x, tok.y);

        double Fx_e =
            (Px_e * Hxx_e) + (Pxx_e * Hx_e) + (Py_e * Hxy_e) + (Pxy_e * Hy_e);
        double Fy_e =
            (Px_e * Hxy_e) + (Pxy_e * Hx_e) + (Py_e * Hyy_e) + (Pyy_e * Hy_e);

        const double F_mag = std::sqrt((Fx_e * Fx_e) + (Fy_e * Fy_e));
        if (F_mag < 1e-12)
            continue; // degenerate: undefined direction, matches MATLAB NaN
                      // case
        Fx_e /= F_mag;
        Fy_e /= F_mag;

        // edge tangent is orthogonal to the (normalized) third-order gradient
        const double dir_x2 = -Fy_e;
        const double dir_y2 = Fx_e;

        const double ori = std::atan2(dir_y2, dir_x2);
        const double conf = mag_e;

        if (out_edginfo != nullptr)
            out_edginfo->emplace_back(tok.x, tok.y, ori, conf);

        int X = static_cast<int>(std::lround(tok.x));
        int Y = static_cast<int>(std::lround(tok.y));
        X = std::clamp(X, 0, w - 1);
        Y = std::clamp(Y, 0, h - 1);

        out_to_edgemap.at<float>(Y, X) = static_cast<float>(conf);
        out_to_thetamap.at<float>(Y, X) = static_cast<float>(ori);
    }
}

#endif
