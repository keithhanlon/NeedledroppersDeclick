#include "dsp/RepairEngine.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <cassert>

namespace needledropper {

RepairEngine::RepairEngine(const WaveletEngine& engine)
    : engine_(engine) {}

// ─── Levinson-Durbin AR fitting ───────────────────────────────────────────────

std::vector<double> RepairEngine::fit_ar(const double* samples,
                                          int n, int order) const
{
    if (n < order + 1) order = std::max(1, n - 1);

    // Step 1: autocorrelation r[0..order]
    std::vector<double> r(order + 1, 0.0);
    for (int lag = 0; lag <= order; ++lag)
        for (int i = lag; i < n; ++i)
            r[lag] += samples[i] * samples[i - lag];

    if (r[0] < 1e-12) return std::vector<double>(order + 1, 0.0);

    // Step 2: Levinson-Durbin recursion
    std::vector<double> a(order + 1, 0.0);
    std::vector<double> a_prev(order + 1, 0.0);
    double err = r[0];

    for (int m = 1; m <= order; ++m) {
        double lambda = 0.0;
        for (int k = 1; k < m; ++k)
            lambda += a_prev[k] * r[m - k];
        lambda = (r[m] - lambda) / err;

        // Clamp reflection coefficient for stability
        lambda = std::clamp(lambda, -0.999, 0.999);

        a[m] = lambda;
        for (int k = 1; k < m; ++k)
            a[k] = a_prev[k] - lambda * a_prev[m - k];

        err *= (1.0 - lambda * lambda);
        if (err < 1e-12) break;
        a_prev = a;
    }
    return a;
}

// ─── Forward AR prediction ────────────────────────────────────────────────────

void RepairEngine::ar_predict_forward(const double* context, int ctx_len,
                                       const std::vector<double>& coeffs,
                                       double* output, int gap_len) const
{
    const int order = static_cast<int>(coeffs.size()) - 1;

    // Seed buffer with last 'order' samples of context
    std::vector<double> buf(context + std::max(0, ctx_len - order),
                            context + ctx_len);
    buf.reserve(order + gap_len);

    for (int i = 0; i < gap_len; ++i) {
        double pred = 0.0;
        const int buf_size = static_cast<int>(buf.size());
        for (int k = 1; k <= order && k <= buf_size; ++k)
            pred += coeffs[k] * buf[buf_size - k];

        // Soft clip to prevent runaway prediction on long gaps
        pred = std::clamp(pred, -2.0, 2.0);
        output[i] = pred;
        buf.push_back(pred);
    }
}

// ─── Cross-channel gain estimate ──────────────────────────────────────────────

double RepairEngine::estimate_cross_gain(const double* ctx_A,
                                          const double* ctx_B,
                                          int n) const
{
    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; ++i) {
        num += ctx_A[i] * ctx_B[i];
        den += ctx_B[i] * ctx_B[i];
    }
    if (den < 1e-12) return 1.0;
    return std::clamp(num / den, 0.25, 4.0);
}

// ─── Cross-channel weight ─────────────────────────────────────────────────────

double RepairEngine::cross_channel_weight(const double* ctx_A,
                                           const double* ctx_B,
                                           int ctx_len) const
{
    if (ctx_len < 4) return 0.0;

    double mean_a = 0.0, mean_b = 0.0;
    for (int i = 0; i < ctx_len; ++i) {
        mean_a += ctx_A[i];
        mean_b += ctx_B[i];
    }
    mean_a /= ctx_len;
    mean_b /= ctx_len;

    double num = 0.0, den_a = 0.0, den_b = 0.0;
    for (int i = 0; i < ctx_len; ++i) {
        const double da = ctx_A[i] - mean_a;
        const double db = ctx_B[i] - mean_b;
        num   += da * db;
        den_a += da * da;
        den_b += db * db;
    }
    const double den = std::sqrt(den_a * den_b);
    if (den < 1e-12) return 0.0;

    const double r     = num / den;
    const double r_low = 0.40, r_high = 0.85;
    double t = (r - r_low) / (r_high - r_low);
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);  // smoothstep
}

// ─── Mono gap repair ──────────────────────────────────────────────────────────

void RepairEngine::repair_gap(const double* left_ctx,  int left_len,
                               const double* right_ctx, int right_len,
                               double* gap, int gap_len, int ar_order) const
{
    if (gap_len <= 0) return;

    // Forward pass
    auto a_fwd = fit_ar(left_ctx, left_len, ar_order);
    std::vector<double> pred_fwd(gap_len);
    ar_predict_forward(left_ctx, left_len, a_fwd,
                       pred_fwd.data(), gap_len);

    // Backward pass — reverse right context, predict, re-reverse
    std::vector<double> right_rev(right_ctx, right_ctx + right_len);
    std::reverse(right_rev.begin(), right_rev.end());
    auto a_bwd = fit_ar(right_rev.data(), right_len, ar_order);
    std::vector<double> pred_bwd(gap_len);
    ar_predict_forward(right_rev.data(), right_len, a_bwd,
                       pred_bwd.data(), gap_len);
    std::reverse(pred_bwd.begin(), pred_bwd.end());

    // Weighted blend: linear crossfade
    for (int i = 0; i < gap_len; ++i) {
        const double w_fwd = (gap_len == 1)
            ? 0.5
            : 1.0 - static_cast<double>(i) / (gap_len - 1);
        gap[i] = w_fwd * pred_fwd[i] + (1.0 - w_fwd) * pred_bwd[i];
    }
}

// ─── Stereo gap repair ────────────────────────────────────────────────────────

void RepairEngine::repair_gap_stereo(const double* left_ctx_A,  int left_len,
                                      const double* right_ctx_A, int right_len,
                                      double* gap_A,   int gap_len,
                                      const double* gap_B,
                                      const char*   B_clean,
                                      int ar_order) const
{
    if (gap_len <= 0) return;

    // AR prediction from A's own context
    auto a_fwd = fit_ar(left_ctx_A, left_len, ar_order);
    std::vector<double> pred_fwd(gap_len);
    ar_predict_forward(left_ctx_A, left_len, a_fwd,
                       pred_fwd.data(), gap_len);

    std::vector<double> right_rev(right_ctx_A, right_ctx_A + right_len);
    std::reverse(right_rev.begin(), right_rev.end());
    auto a_bwd = fit_ar(right_rev.data(), right_len, ar_order);
    std::vector<double> pred_bwd(gap_len);
    ar_predict_forward(right_rev.data(), right_len, a_bwd,
                       pred_bwd.data(), gap_len);
    std::reverse(pred_bwd.begin(), pred_bwd.end());

    // Cross-channel parameters
    const double corr_w  = cross_channel_weight(left_ctx_A, gap_B,
                                                 std::min(left_len, gap_len));
    const double ab_gain = estimate_cross_gain(left_ctx_A, gap_B,
                                                std::min(left_len, gap_len));

    // Predictive consistency check
    double err_e = 0.0, pred_e = 0.0;
    for (int i = 0; i < gap_len; ++i) {
        if (!B_clean[i]) continue;
        const double diff = pred_fwd[i] - ab_gain * gap_B[i];
        err_e  += diff * diff;
        pred_e += pred_fwd[i] * pred_fwd[i];
    }
    double div_suppress = 1.0;
    if (pred_e > 1e-12) {
        const double div = std::sqrt(err_e / pred_e);
        double t = 1.0 - std::clamp(div / 0.5, 0.0, 1.0);
        div_suppress = t * t;
    }

    for (int i = 0; i < gap_len; ++i) {
        const double w_fwd = (gap_len == 1)
            ? 0.5
            : 1.0 - static_cast<double>(i) / (gap_len - 1);
        const double ar_pred = w_fwd * pred_fwd[i]
                             + (1.0 - w_fwd) * pred_bwd[i];

        if (B_clean[i]) {
            const double gap_depth =
                std::sin(M_PI * static_cast<double>(i) / (gap_len - 1 + 1));
            const double w_B  = corr_w * div_suppress * 0.6 * gap_depth;
            gap_A[i] = (1.0 - w_B) * ar_pred + w_B * ab_gain * gap_B[i];
        } else {
            gap_A[i] = ar_pred;
        }
    }
}

// ─── Mono repair ─────────────────────────────────────────────────────────────

RepairResult RepairEngine::repair_mono(const double* samples, int n,
                                        const ChannelDetection& detection,
                                        std::atomic<bool>& cancel) const
{
    RepairResult result;
    result.audio.assign(samples, samples + n);
    result.clicks_repaired = 0;

    // Work in the wavelet domain — modify detail coefficients in place
    DecompositionResult decomp = detection.decomp;
    const int n_levels = static_cast<int>(decomp.detail.size());

    for (int lv = 0; lv < n_levels; ++lv) {
        if (cancel.load()) break;

        auto& detail   = decomp.detail[lv];
        const auto& damaged = detection.damaged[lv];
        const int len  = static_cast<int>(detail.size());

        int i = 0;
        while (i < len) {
            if (!damaged[i]) { ++i; continue; }

            const int gap_start = i;
            while (i < len && damaged[i]) ++i;
            const int gap_end = i;
            const int gap_len = gap_end - gap_start;

            const int ctx = std::min(CTX_LEN, gap_start);
            const int rctx = std::min(CTX_LEN, len - gap_end);
            const int order = select_ar_order(gap_len, ctx);

            repair_gap(detail.data() + gap_start - ctx, ctx,
                       detail.data() + gap_end,          rctx,
                       detail.data() + gap_start,        gap_len,
                       order);

            ++result.clicks_repaired;
        }
    }

    // Reconstruct from modified wavelet coefficients
    result.audio = engine_.reconstruct(decomp);
    result.audio.resize(n);
    return result;
}

// ─── Stereo repair ────────────────────────────────────────────────────────────

void RepairEngine::repair_stereo(const double* left,  const double* right,
                                  int n,
                                  const ChannelDetection& left_det,
                                  const ChannelDetection& right_det,
                                  RepairResult& left_out,
                                  RepairResult& right_out,
                                  std::atomic<bool>& cancel) const
{
    // Work on copies of the decompositions
    DecompositionResult decomp_L = left_det.decomp;
    DecompositionResult decomp_R = right_det.decomp;
    left_out.clicks_repaired  = 0;
    right_out.clicks_repaired = 0;

    const int n_levels = static_cast<int>(decomp_L.detail.size());

    for (int lv = 0; lv < n_levels; ++lv) {
        if (cancel.load()) break;

        auto& dL = decomp_L.detail[lv];
        auto& dR = decomp_R.detail[lv];
        const auto& dmgL = left_det.damaged[lv];
        const auto& dmgR = right_det.damaged[lv];
        const int len = static_cast<int>(dL.size());

        // Repair L using R as cross-channel reference where available
        int i = 0;
        while (i < len) {
            if (!dmgL[i]) { ++i; continue; }

            const int gap_start = i;
            while (i < len && dmgL[i]) ++i;
            const int gap_end = i;
            const int gap_len = gap_end - gap_start;

            const int ctx   = std::min(CTX_LEN, gap_start);
            const int rctx  = std::min(CTX_LEN, len - gap_end);
            const int order = select_ar_order(gap_len, ctx);

            // Build B_clean mask for R at these positions
            std::vector<char> B_clean(gap_len);
            for (int j = 0; j < gap_len; ++j)
                B_clean[j] = !dmgR[gap_start + j];

            repair_gap_stereo(dL.data() + gap_start - ctx, ctx,
                              dL.data() + gap_end,          rctx,
                              dL.data() + gap_start,        gap_len,
                              dR.data() + gap_start,
                              B_clean.data(),
                              order);

            ++left_out.clicks_repaired;
        }

        // Repair R using L as cross-channel reference
        i = 0;
        while (i < len) {
            if (!dmgR[i]) { ++i; continue; }

            const int gap_start = i;
            while (i < len && dmgR[i]) ++i;
            const int gap_end = i;
            const int gap_len = gap_end - gap_start;

            const int ctx   = std::min(CTX_LEN, gap_start);
            const int rctx  = std::min(CTX_LEN, len - gap_end);
            const int order = select_ar_order(gap_len, ctx);

            std::vector<char> B_clean(gap_len);
            for (int j = 0; j < gap_len; ++j)
                B_clean[j] = !dmgL[gap_start + j];

            repair_gap_stereo(dR.data() + gap_start - ctx, ctx,
                              dR.data() + gap_end,          rctx,
                              dR.data() + gap_start,        gap_len,
                              dL.data() + gap_start,
                              B_clean.data(),
                              order);

            ++right_out.clicks_repaired;
        }
    }

    // Reconstruct both channels
    left_out.audio  = engine_.reconstruct(decomp_L);
    right_out.audio = engine_.reconstruct(decomp_R);
    left_out.audio.resize(n);
    right_out.audio.resize(n);
}

} // namespace needledropper
