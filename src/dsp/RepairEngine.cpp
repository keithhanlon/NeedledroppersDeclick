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

// ─── Time-domain gap repair ───────────────────────────────────────────────────

void RepairEngine::repair_gap_timedomain(const double* audio, int n,
                                          int gap_start, int gap_end,
                                          double* output) const
{
    const int gap_len = gap_end - gap_start;
    if (gap_len <= 0) return;

    int ctx   = std::min(CTX_LEN, gap_start);
    int rctx  = std::min(CTX_LEN, n - gap_end);
    int order = select_ar_order(gap_len, ctx);

    // Pitch protection: detect local pitch and use pitch-synchronous context
    // if a clear pitched region is found. This significantly improves repair
    // quality on sustained tones (strings, voice, bass) by ensuring the AR
    // model captures the full pitch cycle.
    const int center = (gap_start + gap_end) / 2;
    // Use a fixed sample rate estimate — the repair engine doesn't hold SR,
    // so we detect pitch using period-based heuristics only.
    // Look at both pre and post context for periodicity.
    {
        // Simple period estimate via autocorrelation on pre-gap context
        const int ac_win = std::min(ctx, 1024);
        const int ac_start = gap_start - ac_win;
        if (ac_start >= 0 && ac_win >= 64) {
            // Compute autocorrelation
            double r0 = 0.0;
            for (int i = ac_start; i < gap_start; ++i)
                r0 += audio[i] * audio[i];
            r0 /= ac_win;

            if (r0 > 1e-10) {
                // Search for period in 20-800 sample range (60Hz-2400Hz at 48kHz)
                int    best_lag = 0;
                double best_r   = 0.0;
                bool   in_peak  = false;
                double prev_r   = 0.0;

                for (int lag = 20; lag < std::min(800, ac_win / 2); ++lag) {
                    double r = 0.0;
                    for (int i = ac_start; i < gap_start - lag; ++i)
                        r += audio[i] * audio[i + lag];
                    r /= (ac_win - lag) * r0;

                    if (!in_peak && r > 0.35) in_peak = true;
                    if (in_peak && r > prev_r) { best_lag = lag; best_r = r; }
                    else if (in_peak && r < prev_r && best_lag > 0) break;
                    prev_r = r;
                }

                // If strong pitch found, use pitch-synchronous context
                if (best_lag > 0 && best_r > 0.45) {
                    const int pitch_order = std::min(best_lag, 64);
                    const int n_cycles    = std::max(2, 512 / best_lag);
                    const int pitch_ctx   = n_cycles * best_lag;

                    if (pitch_ctx <= gap_start) {
                        ctx   = pitch_ctx;
                        rctx  = std::min(pitch_ctx, n - gap_end);
                        order = pitch_order;
                    }
                }
            }
        }
    }

    repair_gap(audio + gap_start - ctx, ctx,
               audio + gap_end,          rctx,
               output + gap_start,       gap_len,
               order);
}

// ─── Mono repair ─────────────────────────────────────────────────────────────

RepairResult RepairEngine::repair_mono(const double* samples, int n,
                                        const ChannelDetection& detection,
                                        std::atomic<bool>& cancel) const
{
    RepairResult result;
    result.audio.assign(samples, samples + n);
    result.clicks_repaired = 0;

    // Merge click and crackle events, sorted by start position
    std::vector<ClickEvent> all_events = detection.clicks;
    all_events.insert(all_events.end(),
                      detection.crackle_clicks.begin(),
                      detection.crackle_clicks.end());
    std::sort(all_events.begin(), all_events.end(),
              [](const ClickEvent& a, const ClickEvent& b) {
                  return a.sample_start < b.sample_start;
              });

    for (const auto& ev : all_events) {
        if (cancel.load()) break;
        const int gap_start = std::max(0, ev.sample_start);
        const int gap_end   = std::min(n, ev.sample_end);
        if (gap_end <= gap_start) continue;

        repair_gap_timedomain(result.audio.data(), n,
                              gap_start, gap_end,
                              result.audio.data());
        ++result.clicks_repaired;
    }

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
    left_out.audio.assign(left,  left  + n);
    right_out.audio.assign(right, right + n);
    left_out.clicks_repaired  = 0;
    right_out.clicks_repaired = 0;

    // Merge click and crackle events for L
    std::vector<ClickEvent> left_events = left_det.clicks;
    left_events.insert(left_events.end(),
                       left_det.crackle_clicks.begin(),
                       left_det.crackle_clicks.end());
    std::sort(left_events.begin(), left_events.end(),
              [](const ClickEvent& a, const ClickEvent& b) {
                  return a.sample_start < b.sample_start;
              });

    // Merge click and crackle events for R
    std::vector<ClickEvent> right_events = right_det.clicks;
    right_events.insert(right_events.end(),
                        right_det.crackle_clicks.begin(),
                        right_det.crackle_clicks.end());
    std::sort(right_events.begin(), right_events.end(),
              [](const ClickEvent& a, const ClickEvent& b) {
                  return a.sample_start < b.sample_start;
              });

    // Repair L
    for (const auto& ev : left_events) {
        if (cancel.load()) break;
        const int gap_start = std::max(0, ev.sample_start);
        const int gap_end   = std::min(n, ev.sample_end);
        const int gap_len   = gap_end - gap_start;
        if (gap_len <= 0) continue;

        // Check if R is clean at this position
        bool r_clean = true;
        for (const auto& rev : right_events) {
            if (rev.sample_start < gap_end && rev.sample_end > gap_start) {
                r_clean = false; break;
            }
        }

        if (r_clean) {
            const int ctx   = std::min(CTX_LEN, gap_start);
            const int rctx  = std::min(CTX_LEN, n - gap_end);
            const int order = select_ar_order(gap_len, ctx);
            std::vector<char> B_clean(gap_len, 1);
            repair_gap_stereo(
                left_out.audio.data()  + gap_start - ctx, ctx,
                left_out.audio.data()  + gap_end,         rctx,
                left_out.audio.data()  + gap_start,       gap_len,
                right_out.audio.data() + gap_start,
                B_clean.data(), order);
        } else {
            repair_gap_timedomain(left_out.audio.data(), n,
                                  gap_start, gap_end,
                                  left_out.audio.data());
        }
        ++left_out.clicks_repaired;
    }

    // Repair R
    for (const auto& ev : right_events) {
        if (cancel.load()) break;
        const int gap_start = std::max(0, ev.sample_start);
        const int gap_end   = std::min(n, ev.sample_end);
        const int gap_len   = gap_end - gap_start;
        if (gap_len <= 0) continue;

        bool l_clean = true;
        for (const auto& lev : left_events) {
            if (lev.sample_start < gap_end && lev.sample_end > gap_start) {
                l_clean = false; break;
            }
        }

        if (l_clean) {
            const int ctx   = std::min(CTX_LEN, gap_start);
            const int rctx  = std::min(CTX_LEN, n - gap_end);
            const int order = select_ar_order(gap_len, ctx);
            std::vector<char> B_clean(gap_len, 1);
            repair_gap_stereo(
                right_out.audio.data() + gap_start - ctx, ctx,
                right_out.audio.data() + gap_end,         rctx,
                right_out.audio.data() + gap_start,       gap_len,
                left_out.audio.data()  + gap_start,
                B_clean.data(), order);
        } else {
            repair_gap_timedomain(right_out.audio.data(), n,
                                  gap_start, gap_end,
                                  right_out.audio.data());
        }
        ++right_out.clicks_repaired;
    }
}


// ─── Pitch detection (NSDF) ───────────────────────────────────────────────────
//
// Uses the Normalized Square Difference Function — more robust than raw
// autocorrelation for pitch detection on mixed audio. Returns the first
// clear periodic peak in the range 60Hz-1000Hz.

std::pair<int,double> RepairEngine::detect_pitch(const double* samples, int n,
                                                   int center, double sample_rate)
{
    const int WIN  = 2048;
    const int lo   = std::max(0, center - WIN / 2);
    const int hi   = std::min(n, lo + WIN);
    const int seg_n = hi - lo;
    if (seg_n < 128) return {0, 0.0};

    // Zero-mean and normalize
    double mean = 0.0;
    for (int i = lo; i < hi; ++i) mean += samples[i];
    mean /= seg_n;

    std::vector<double> seg(seg_n);
    double rms = 0.0;
    for (int i = 0; i < seg_n; ++i) {
        seg[i] = samples[lo + i] - mean;
        rms += seg[i] * seg[i];
    }
    rms = std::sqrt(rms / seg_n);
    if (rms < 1e-6) return {0, 0.0};
    for (auto& x : seg) x /= rms;

    const int min_lag = static_cast<int>(sample_rate / 1000.0);  // 1000Hz max
    const int max_lag = std::min(static_cast<int>(sample_rate / 60.0),
                                  seg_n / 3);                     // 60Hz min

    // NSDF: normalized square difference function
    // Peak finding: find first peak above threshold
    const double THRESHOLD = 0.30;
    int    best_lag = 0;
    double best_val = 0.0;
    double prev_val = 0.0;
    bool   in_peak  = false;

    for (int lag = min_lag; lag < max_lag; ++lag) {
        double num = 0.0, den = 0.0;
        for (int i = 0; i < seg_n - lag; ++i) {
            num += seg[i] * seg[i + lag];
            den += seg[i] * seg[i] + seg[i + lag] * seg[i + lag];
        }
        const double val = (den > 1e-12) ? 2.0 * num / den : 0.0;

        if (!in_peak && val > THRESHOLD)
            in_peak = true;

        if (in_peak && val > prev_val) {
            best_lag = lag;
            best_val = val;
        } else if (in_peak && val < prev_val && best_lag > 0) {
            break;  // Past the first peak
        }
        prev_val = val;
    }

    const double confidence = std::clamp(best_val, 0.0, 1.0);
    return {best_lag, confidence};
}

// ─── Mono merge ──────────────────────────────────────────────────────────────
//
// Dynamically equalises L and R channel levels using a slow exponential RMS,
// then sums them to mono. The time constant is chosen so that gain changes
// are sub-20Hz and therefore below auditory perception limits.
// Output is written to both channels so the file remains stereo-format.

void RepairEngine::apply_mono_merge(std::vector<double>& left,
                                     std::vector<double>& right,
                                     double sample_rate)
{
    const int n = static_cast<int>(std::min(left.size(), right.size()));
    if (n == 0) return;

    // λ for ~20Hz time constant: λ = 1 - 1/(sr/20)
    const double lam = 1.0 - 20.0 / sample_rate;

    // Compute global mean RMS for both channels (used as target level)
    double sum_L = 0.0, sum_R = 0.0;
    for (int i = 0; i < n; ++i) {
        sum_L += left[i]  * left[i];
        sum_R += right[i] * right[i];
    }
    const double mean_rms_L = std::sqrt(sum_L / n);
    const double mean_rms_R = std::sqrt(sum_R / n);
    const double target = (mean_rms_L + mean_rms_R) * 0.5;

    if (target < 1e-9) return;  // silence

    // Forward pass: compute slow RMS envelope for each channel
    std::vector<double> rms_L(n), rms_R(n);
    double rL = mean_rms_L, rR = mean_rms_R;
    for (int i = 0; i < n; ++i) {
        rL = lam * rL + (1.0 - lam) * std::abs(left[i]);
        rR = lam * rR + (1.0 - lam) * std::abs(right[i]);
        rms_L[i] = std::max(rL, 1e-9);
        rms_R[i] = std::max(rR, 1e-9);
    }

    // Apply dynamic gains and merge
    const double MAX_GAIN = 4.0;
    const double MIN_GAIN = 0.25;
    for (int i = 0; i < n; ++i) {
        const double gL = std::clamp(target / rms_L[i], MIN_GAIN, MAX_GAIN);
        const double gR = std::clamp(target / rms_R[i], MIN_GAIN, MAX_GAIN);
        const double mono = 0.5 * (gL * left[i] + gR * right[i]);
        left[i]  = mono;
        right[i] = mono;
    }
}

} // namespace needledropper
