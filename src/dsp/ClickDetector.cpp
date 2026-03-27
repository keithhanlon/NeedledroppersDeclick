#include "dsp/ClickDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace needledropper {

ClickDetector::ClickDetector(const WaveletEngine& engine)
    : engine_(engine) {}

// ─── MAD sigma ────────────────────────────────────────────────────────────────

double ClickDetector::mad_sigma(const std::vector<double>& detail) const
{
    if (detail.empty()) return 1.0;
    std::vector<double> abs_vals(detail.size());
    for (size_t i = 0; i < detail.size(); ++i)
        abs_vals[i] = std::abs(detail[i]);
    const size_t mid = abs_vals.size() / 2;
    std::nth_element(abs_vals.begin(), abs_vals.begin() + mid, abs_vals.end());
    return 1.4826 * abs_vals[mid];
}

// ─── Levinson-Durbin AR fitting ───────────────────────────────────────────────

static std::vector<double> fit_ar(const double* samples, int n, int order)
{
    if (n < order + 1) order = std::max(1, n - 1);
    std::vector<double> r(order + 1, 0.0);
    for (int lag = 0; lag <= order; ++lag)
        for (int i = lag; i < n; ++i)
            r[lag] += samples[i] * samples[i - lag];
    for (auto& x : r) x /= n;

    if (r[0] < 1e-12) return std::vector<double>(order, 0.0);

    std::vector<double> a(order, 0.0), a_prev(order, 0.0);
    double err = r[0];
    for (int m = 0; m < order; ++m) {
        double lm = r[m + 1];
        for (int k = 0; k < m; ++k) lm -= a_prev[k] * r[m - k];
        lm /= err;
        lm = std::clamp(lm, -0.999, 0.999);
        a[m] = lm;
        for (int k = 0; k < m; ++k)
            a[k] = a_prev[k] - lm * a_prev[m - 1 - k];
        err *= (1.0 - lm * lm);
        if (err < 1e-12) break;
        a_prev = a;
    }
    return a;
}

// ─── Prediction-error click detection ────────────────────────────────────────
//
// Conceptual approach inspired by Davies (ClickRepair):
//   For each sample, compute the AR forward prediction error.
//   Maintain a slow exponential running average of prediction error.
//   Flag when error >> running_average (a sudden spike stands out
//   regardless of the signal's overall amplitude level).
//
// This correctly handles loud musical passages: the AR model learns the
// local spectral character, so cymbals/drums have LOW prediction error
// relative to their own running average. Only genuine anomalies spike.

static std::vector<bool> detect_prediction_error(const double* samples, int n,
                                                   double threshold,
                                                   double sample_rate)
{
    std::vector<bool> damaged(n, false);

    // AR order: enough to model local spectral character
    const int AR_ORDER = 10;
    // Refit AR model every BLOCK samples using CTX samples of context
    const int BLOCK = 256;
    const int CTX   = 512;
    // Maximum click run length in samples
    // Vinyl clicks: 1-30 samples. Broader events (crackle) handled separately.
    const int MAX_CLICK = 30;
    // Minimum absolute amplitude — don't flag near-silence
    const double MIN_AMP = 1e-4;

    // Slow exponential average of prediction error.
    // Time constant ≈ 1/(1-lambda) samples.
    // At 48kHz: lambda=0.999 → ~1000 sample memory (~21ms)
    // This is slow enough that brief clicks don't corrupt the average,
    // but fast enough to track gradual level changes across the file.
    const double lambda = 0.999;

    // Warmup: skip first 100ms while running average stabilises
    const int WARMUP = static_cast<int>(sample_rate * 0.10);

    // Compute per-sample prediction errors
    std::vector<double> pred_error(n, 0.0);
    std::vector<double> ar_coeffs(AR_ORDER, 0.0);
    int last_fit = -BLOCK;

    for (int i = AR_ORDER; i < n; ++i) {
        // Refit AR model periodically
        if (i - last_fit >= BLOCK) {
            const int ctx_start = std::max(0, i - CTX);
            const int ctx_len   = i - ctx_start;
            if (ctx_len >= AR_ORDER + 1)
                ar_coeffs = fit_ar(samples + ctx_start, ctx_len, AR_ORDER);
            last_fit = i;
        }
        // Forward prediction error
        double pred = 0.0;
        for (int k = 0; k < AR_ORDER; ++k)
            pred += ar_coeffs[k] * samples[i - 1 - k];
        pred_error[i] = std::abs(samples[i] - pred);
    }

    // Compute running exponential average of prediction error
    std::vector<double> running_avg(n, 0.0);
    double avg = 0.001;  // seed with small non-zero value
    for (int i = AR_ORDER; i < n; ++i) {
        avg = lambda * avg + (1.0 - lambda) * pred_error[i];
        running_avg[i] = avg;
    }

    // Detection pass: flag anomalous prediction errors after warmup
    for (int i = WARMUP; i < n; ++i) {
        if (running_avg[i] < 1e-9) continue;
        if (std::abs(samples[i]) < MIN_AMP) continue;
        if (pred_error[i] > threshold * running_avg[i])
            damaged[i] = true;
    }

    // Duration filter: reject runs longer than MAX_CLICK samples
    // Longer runs are musical transients the AR model didn't predict well
    int i = 0;
    while (i < n) {
        if (!damaged[i]) { ++i; continue; }
        const int run_start = i;
        while (i < n && damaged[i]) ++i;
        const int run_len = i - run_start;
        if (run_len > MAX_CLICK)
            for (int j = run_start; j < i; ++j)
                damaged[j] = false;
    }

    return damaged;
}

// ─── Map time-domain mask to wavelet coefficient masks ────────────────────────

static void time_to_wavelet_masks(const std::vector<bool>& time_damaged,
                                   int n_levels, int original_length,
                                   std::vector<std::vector<bool>>& wv_masks)
{
    wv_masks.resize(n_levels);
    for (int lv = 0; lv < n_levels; ++lv) {
        const int stride   = 1 << (lv + 1);
        const int n_coeffs = original_length / stride;
        wv_masks[lv].assign(n_coeffs, false);
        for (int ci = 0; ci < n_coeffs; ++ci) {
            const int s_start = ci * stride;
            const int s_end   = std::min(s_start + stride, original_length);
            for (int s = s_start; s < s_end; ++s) {
                if (s < (int)time_damaged.size() && time_damaged[s]) {
                    wv_masks[lv][ci] = true;
                    break;
                }
            }
        }
    }
}

// ─── Map time-domain mask to click events ─────────────────────────────────────

void ClickDetector::map_to_clicks(ChannelDetection& ch,
                                   int original_length) const
{
    ch.clicks.clear();
    const auto& dmg = ch.time_damaged;
    if (dmg.empty()) return;

    int i = 0;
    while (i < original_length && i < (int)dmg.size()) {
        if (!dmg[i]) { ++i; continue; }
        const int run_start = i;
        while (i < original_length && i < (int)dmg.size() && dmg[i]) ++i;
        const int run_end = i;

        // Pad each event slightly to catch spillover energy
        const int PAD      = 3;
        const int ev_start = std::max(0, run_start - PAD);
        const int ev_end   = std::min(original_length, run_end + PAD);
        ch.clicks.push_back({ ev_start, ev_end, 0, 0.8 });
    }
}

// ─── Stereo helpers ───────────────────────────────────────────────────────────

double ClickDetector::stereo_confidence(double mag_a, double mag_b,
                                         double sigma_b) const
{
    const double ratio = mag_b / (sigma_b + 1e-12);
    return ratio / (1.0 + ratio);
}

double ClickDetector::cross_correlation(const double* a, const double* b,
                                         int n) const
{
    if (n < 2) return 0.0;
    double mean_a = 0.0, mean_b = 0.0;
    for (int i = 0; i < n; ++i) { mean_a += a[i]; mean_b += b[i]; }
    mean_a /= n; mean_b /= n;
    double num = 0.0, den_a = 0.0, den_b = 0.0;
    for (int i = 0; i < n; ++i) {
        const double da = a[i] - mean_a;
        const double db = b[i] - mean_b;
        num   += da * db;
        den_a += da * da;
        den_b += db * db;
    }
    const double den = std::sqrt(den_a * den_b);
    if (den < 1e-12) return 0.0;
    return num / den;
}

void ClickDetector::threshold(const std::vector<double>& detail,
                               double sigma, double k,
                               std::vector<bool>& mask) const
{
    const int n = static_cast<int>(detail.size());
    mask.assign(n, false);
    const double thresh = k * sigma;
    for (int i = 0; i < n; ++i)
        mask[i] = std::abs(detail[i]) > thresh;
}

void ClickDetector::refine_stereo(ChannelDetection& L, ChannelDetection& R,
                                   double k_base) const
{
    (void)L; (void)R; (void)k_base;
}

// ─── Mono detection ───────────────────────────────────────────────────────────

ChannelDetection ClickDetector::detect_mono(const double* samples, int n,
                                             double sensitivity,
                                             double sample_rate,
                                             std::atomic<bool>& cancel) const
{
    ChannelDetection ch;
    ch.decomp = engine_.decompose(samples, n);
    const int n_levels = static_cast<int>(ch.decomp.detail.size());
    ch.sigma.resize(n_levels);
    ch.damaged.resize(n_levels);

    for (int lv = 0; lv < n_levels; ++lv)
        ch.sigma[lv] = mad_sigma(ch.decomp.detail[lv]);

    if (cancel.load()) return ch;

    const double threshold = sensitivity_to_k(sensitivity);
    ch.time_damaged = detect_prediction_error(samples, n, threshold, sample_rate);

    time_to_wavelet_masks(ch.time_damaged, n_levels, n, ch.damaged);
    map_to_clicks(ch, n);
    return ch;
}

// ─── Stereo detection ─────────────────────────────────────────────────────────

void ClickDetector::detect_stereo(const double* left, const double* right,
                                   int n, double sensitivity,
                                   double sample_rate,
                                   ChannelDetection& L, ChannelDetection& R,
                                   std::atomic<bool>& cancel) const
{
    L.decomp = engine_.decompose(left,  n);
    R.decomp = engine_.decompose(right, n);

    const int n_levels = static_cast<int>(L.decomp.detail.size());
    L.sigma.resize(n_levels); R.sigma.resize(n_levels);
    L.damaged.resize(n_levels); R.damaged.resize(n_levels);

    for (int lv = 0; lv < n_levels; ++lv) {
        L.sigma[lv] = mad_sigma(L.decomp.detail[lv]);
        R.sigma[lv] = mad_sigma(R.decomp.detail[lv]);
    }

    if (cancel.load()) return;

    const double threshold = sensitivity_to_k(sensitivity);
    L.time_damaged = detect_prediction_error(left,  n, threshold, sample_rate);
    R.time_damaged = detect_prediction_error(right, n, threshold, sample_rate);

    time_to_wavelet_masks(L.time_damaged, n_levels, n, L.damaged);
    time_to_wavelet_masks(R.time_damaged, n_levels, n, R.damaged);

    map_to_clicks(L, n);
    map_to_clicks(R, n);
}

} // namespace needledropper
