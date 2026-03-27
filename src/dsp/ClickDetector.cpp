#include "dsp/ClickDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdio>

namespace needledropper {

ClickDetector::ClickDetector(const WaveletEngine& engine)
    : engine_(engine) {}

// ─── MAD sigma (kept for stereo refinement) ──────────────────────────────────

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

// ─── Time-domain AR prediction error detection ────────────────────────────────
//
// For each sample, fit an AR model to the preceding context window and
// predict the current sample. Large prediction errors that are short-duration
// and isolated are clicks. Musical transients are predictable by the AR model
// because it adapts to local spectral content — cymbals, sibilants, drums all
// produce small prediction errors relative to their own energy.

static std::vector<double> fit_ar_simple(const double* samples, int n, int order)
{
    if (n < order + 1) order = std::max(1, n - 1);
    std::vector<double> r(order + 1, 0.0);
    for (int lag = 0; lag <= order; ++lag)
        for (int i = lag; i < n; ++i)
            r[lag] += samples[i] * samples[i - lag];
    if (r[0] < 1e-12) return std::vector<double>(order + 1, 0.0);

    std::vector<double> a(order + 1, 0.0);
    std::vector<double> a_prev(order + 1, 0.0);
    double err = r[0];
    for (int m = 1; m <= order; ++m) {
        double lambda = 0.0;
        for (int k = 1; k < m; ++k)
            lambda += a_prev[k] * r[m - k];
        lambda = (r[m] - lambda) / err;
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

static std::vector<bool> detect_ar(const double* samples, int n,
                                    double sensitivity,
                                    double sample_rate)
{
    std::vector<bool> damaged(n, false);

    // AR order: enough to model the local spectral character
    const int AR_ORDER   = 64;
    // Context window: ~10ms of audio for AR fitting
    const int CTX_LEN    = static_cast<int>(sample_rate * 0.010);
    // Step size: process every sample but fit AR every STEP samples
    const int FIT_STEP   = 32;
    // Max click duration: 20 samples at any sample rate.
    // Real vinyl clicks are 2-20 samples. Drum transients are 200-2000+ samples.
    const int MAX_CLICK  = 20;
    // (MIN_GAP removed — gap filling disabled)

    // k threshold: sensitivity 0=very conservative, 100=aggressive
    // Maps to k in range [8.0, 3.0]
    const double k = 8.0 - (sensitivity / 100.0) * 5.0;

    // Compute prediction errors across the whole signal
    std::vector<double> pred_error(n, 0.0);
    std::vector<double> ar_coeffs;
    int last_fit = -FIT_STEP;

    for (int i = CTX_LEN; i < n; ++i) {
        // Re-fit AR model every FIT_STEP samples
        if (i - last_fit >= FIT_STEP) {
            ar_coeffs = fit_ar_simple(samples + i - CTX_LEN, CTX_LEN, AR_ORDER);
            last_fit = i;
        }
        if (ar_coeffs.empty()) continue;

        // Predict sample i from preceding samples
        double pred = 0.0;
        const int order = static_cast<int>(ar_coeffs.size()) - 1;
        for (int k2 = 1; k2 <= order && k2 <= i; ++k2)
            pred += ar_coeffs[k2] * samples[i - k2];

        pred_error[i] = std::abs(samples[i] - pred);
    }

    // Compute local noise floor using 20th percentile of prediction errors
    // over a wide window. Using 20th percentile (not median) means the floor
    // is set by the quiet valleys between transients, not the transients
    // themselves. This keeps the threshold low during loud drum/cymbal passages
    // so genuine clicks (which spike far above the floor) still get caught.
    const int NOISE_WIN = static_cast<int>(sample_rate * 0.200); // 200ms window
    std::vector<double> local_noise(n, 0.0);

    for (int i = CTX_LEN; i < n; ++i) {
        const int lo = std::max(CTX_LEN, i - NOISE_WIN / 2);
        const int hi = std::min(n - 1,   i + NOISE_WIN / 2);
        std::vector<double> samples_win;
        samples_win.reserve((hi - lo) / 16 + 1);
        for (int j = lo; j <= hi; j += 16)
            samples_win.push_back(pred_error[j]);
        if (samples_win.empty()) continue;
        // 20th percentile — use the quiet valleys as the noise floor
        const int p20 = static_cast<int>(samples_win.size() * 0.20);
        std::nth_element(samples_win.begin(),
                         samples_win.begin() + p20,
                         samples_win.end());
        local_noise[i] = samples_win[p20];
    }

    // Flag samples where prediction error >> local noise floor
    for (int i = CTX_LEN; i < n; ++i) {
        const double noise = local_noise[i];
        if (noise < 1e-12) continue;
        damaged[i] = pred_error[i] > k * noise;
    }

    // Duration filtering: remove runs longer than MAX_CLICK samples
    // (sustained transients — cymbals, sibilants — that survived the AR test)
    {
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
    }

    // Gap filling disabled — creates false positives between drum hits

    return damaged;
}

// ─── Map time-domain damaged mask to wavelet coefficient masks ────────────────

static void time_to_wavelet_masks(const std::vector<bool>& time_damaged,
                                   int n_levels,
                                   int original_length,
                                   std::vector<std::vector<bool>>& wv_masks)
{
    wv_masks.resize(n_levels);
    for (int lv = 0; lv < n_levels; ++lv) {
        const int stride  = 1 << (lv + 1);
        const int n_coeffs = (original_length / stride);
        wv_masks[lv].assign(n_coeffs, false);
        for (int i = 0; i < n_coeffs; ++i) {
            const int s_start = i * stride;
            const int s_end   = std::min(s_start + stride, original_length);
            for (int s = s_start; s < s_end; ++s) {
                if (s < (int)time_damaged.size() && time_damaged[s]) {
                    wv_masks[lv][i] = true;
                    break;
                }
            }
        }
    }
}

// ─── Map wavelet positions to sample-domain click events ─────────────────────

void ClickDetector::map_to_clicks(ChannelDetection& ch,
                                   int original_length) const
{
    ch.clicks.clear();
    const auto& dmg = ch.time_damaged;
    if (dmg.empty()) return;

    // Build click events directly from the time-domain damaged mask.
    // This gives exact sample positions without any wavelet scaling ambiguity.
    int i = 0;
    while (i < original_length && i < (int)dmg.size()) {
        if (!dmg[i]) { ++i; continue; }

        const int run_start = i;
        while (i < original_length && i < (int)dmg.size() && dmg[i]) ++i;
        const int run_end = i;

        // Confidence based on peak amplitude relative to neighbors
        double peak = 0.0;
        double ctx_rms = 0.0;
        int ctx_count = 0;
        const int ctx_lo = std::max(0, run_start - 64);
        const int ctx_hi = std::min(original_length, run_end + 64);
        for (int j = run_start; j < run_end; ++j)
            peak = std::max(peak, std::abs(ch.decomp.approx.empty() ? 0.0 :
                            j < (int)ch.decomp.approx.size() ? ch.decomp.approx[j] : 0.0));
        for (int j = ctx_lo; j < ctx_hi; ++j) {
            if (j >= run_start && j < run_end) continue;
            if (j < (int)ch.sigma.size())
                ctx_rms += ch.sigma[0];
            ++ctx_count;
        }
        const double conf = std::min(1.0, ctx_count > 0 ? 0.7 : 0.5);

        ch.clicks.push_back({ run_start, run_end, 0, conf });
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
    // Used only for stereo refinement pass — not primary detection
    const int n = static_cast<int>(detail.size());
    mask.assign(n, false);
    const double thresh = k * sigma;
    for (int i = 0; i < n; ++i)
        mask[i] = std::abs(detail[i]) > thresh;
}

void ClickDetector::refine_stereo(ChannelDetection& L,
                                   ChannelDetection& R,
                                   double k_base) const
{
    const int n_levels = static_cast<int>(L.decomp.detail.size());
    for (int lv = 0; lv < n_levels; ++lv) {
        const int len = static_cast<int>(L.decomp.detail[lv].size());
        if (len == 0) continue;
        for (int i = 0; i < len; ++i) {
            const double mag_L = std::abs(L.decomp.detail[lv][i]);
            const double mag_R = std::abs(R.decomp.detail[lv][i]);
            const double conf_L = stereo_confidence(mag_L, mag_R, R.sigma[lv]);
            const double conf_R = stereo_confidence(mag_R, mag_L, L.sigma[lv]);
            const double k_min = 2.0, k_max = 10.0;
            const double k_L = k_max - conf_L * (k_max - k_min);
            const double k_R = k_max - conf_R * (k_max - k_min);
            if (L.damaged[lv][i])
                L.damaged[lv][i] = mag_L > k_L * L.sigma[lv];
            if (R.damaged[lv][i])
                R.damaged[lv][i] = mag_R > k_R * R.sigma[lv];
            if (conf_L > 0.75 && !R.damaged[lv][i])
                R.damaged[lv][i] = mag_R > 2.0 * R.sigma[lv];
            if (conf_R > 0.75 && !L.damaged[lv][i])
                L.damaged[lv][i] = mag_L > 2.0 * L.sigma[lv];
        }
    }
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

    // Compute sigma for all levels (needed for repair confidence)
    for (int lv = 0; lv < n_levels; ++lv)
        ch.sigma[lv] = mad_sigma(ch.decomp.detail[lv]);

    if (cancel.load()) return ch;

    // Time-domain AR detection
    const double sr = sample_rate;
    ch.time_damaged = detect_ar(samples, n, sensitivity, sr);

    int flagged = 0;
    for (bool b : ch.time_damaged) if (b) ++flagged;
    fprintf(stderr, "[Repair] total flagged samples: %d out of %d (%.2f%%)\n",
            flagged, n, 100.0f * flagged / n);

    // Map to wavelet domain masks (used for confidence scoring only)
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

    // Time-domain AR detection on each channel
    const double sr = sample_rate;
    L.time_damaged = detect_ar(left,  n, sensitivity, sr);
    R.time_damaged = detect_ar(right, n, sensitivity, sr);

    time_to_wavelet_masks(L.time_damaged, n_levels, n, L.damaged);
    time_to_wavelet_masks(R.time_damaged, n_levels, n, R.damaged);

    map_to_clicks(L, n);
    map_to_clicks(R, n);
}

} // namespace needledropper
