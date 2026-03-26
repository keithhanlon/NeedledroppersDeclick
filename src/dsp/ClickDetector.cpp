#include "dsp/ClickDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace needledropper {

ClickDetector::ClickDetector(const WaveletEngine& engine)
    : engine_(engine) {}

// ─── MAD sigma ───────────────────────────────────────────────────────────────

double ClickDetector::mad_sigma(const std::vector<double>& detail) const
{
    if (detail.empty()) return 1.0;

    std::vector<double> abs_vals(detail.size());
    for (size_t i = 0; i < detail.size(); ++i)
        abs_vals[i] = std::abs(detail[i]);

    const size_t mid = abs_vals.size() / 2;
    std::nth_element(abs_vals.begin(),
                     abs_vals.begin() + mid,
                     abs_vals.end());
    const double median = abs_vals[mid];

    // 1.4826 makes MAD consistent with std dev for Gaussian data
    return 1.4826 * median;
}

// ─── Threshold and dilate ────────────────────────────────────────────────────

void ClickDetector::threshold(const std::vector<double>& detail,
                               double sigma, double k,
                               std::vector<bool>& mask) const
{
    const int n = static_cast<int>(detail.size());
    mask.assign(n, false);

    const double thresh = k * sigma;
    for (int i = 0; i < n; ++i)
        mask[i] = std::abs(detail[i]) > thresh;

    // One-sample dilation — extend each flagged region by one on each side
    // so we catch the full extent of click energy
    std::vector<bool> dilated = mask;
    for (int i = 1; i < n - 1; ++i)
        if (mask[i-1] || mask[i+1])
            dilated[i] = true;
    mask = std::move(dilated);
}

// ─── Stereo correlation helpers ──────────────────────────────────────────────

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

// ─── Stereo refinement ───────────────────────────────────────────────────────

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

            // Adjust effective threshold based on cross-channel confidence
            const double k_min = 2.0, k_max = 10.0;
            const double k_L = k_max - conf_L * (k_max - k_min);
            const double k_R = k_max - conf_R * (k_max - k_min);

            L.damaged[lv][i] = mag_L > k_L * L.sigma[lv];
            R.damaged[lv][i] = mag_R > k_R * R.sigma[lv];

            // Propagation: confident detection in one channel
            // lowers threshold for corresponding position in the other
            if (conf_L > 0.75 && !R.damaged[lv][i])
                R.damaged[lv][i] = mag_R > 2.0 * R.sigma[lv];
            if (conf_R > 0.75 && !L.damaged[lv][i])
                L.damaged[lv][i] = mag_L > 2.0 * L.sigma[lv];
        }
    }
}

// ─── Map wavelet positions back to sample domain ─────────────────────────────

void ClickDetector::map_to_clicks(ChannelDetection& ch,
                                   int original_length) const
{
    ch.clicks.clear();
    const int n_levels = static_cast<int>(ch.damaged.size());

    // Each detail coefficient at level lv spans 2^(lv+1) original samples.
    // Coefficient i covers samples [ i * 2^(lv+1) .. (i+1) * 2^(lv+1) ).
    for (int lv = 0; lv < n_levels; ++lv) {
        const int stride = 1 << (lv + 1);  // 2^(lv+1)
        const int len    = static_cast<int>(ch.damaged[lv].size());

        int i = 0;
        while (i < len) {
            if (!ch.damaged[lv][i]) { ++i; continue; }

            // Find the contiguous damaged run
            const int run_start = i;
            while (i < len && ch.damaged[lv][i]) ++i;
            const int run_end = i;

            // Map to sample positions
            const int sample_start = std::min(run_start * stride,
                                              original_length - 1);
            const int sample_end   = std::min(run_end   * stride,
                                              original_length);

            // Compute confidence as mean magnitude over the run
            // relative to sigma at this level
            double mean_mag = 0.0;
            for (int j = run_start; j < run_end; ++j)
                mean_mag += std::abs(ch.decomp.detail[lv][j]);
            mean_mag /= (run_end - run_start);
            const double conf = std::min(mean_mag / (ch.sigma[lv] + 1e-12)
                                         / 8.0, 1.0);

            ch.clicks.push_back({ sample_start, sample_end, lv, conf });
        }
    }

    // Sort by sample position, merge overlapping regions across levels
    std::sort(ch.clicks.begin(), ch.clicks.end(),
              [](const ClickEvent& a, const ClickEvent& b) {
                  return a.sample_start < b.sample_start;
              });

    // Merge overlapping or adjacent click events
    std::vector<ClickEvent> merged;
    for (auto& ev : ch.clicks) {
        if (!merged.empty() &&
            ev.sample_start <= merged.back().sample_end + 2) {
            // Extend the last event and take the higher confidence
            merged.back().sample_end  = std::max(merged.back().sample_end,
                                                  ev.sample_end);
            merged.back().confidence  = std::max(merged.back().confidence,
                                                  ev.confidence);
        } else {
            merged.push_back(ev);
        }
    }
    ch.clicks = std::move(merged);
}

// ─── Mono detection ──────────────────────────────────────────────────────────

ChannelDetection ClickDetector::detect_mono(const double* samples, int n,
                                             double sensitivity,
                                             std::atomic<bool>& cancel) const
{
    ChannelDetection ch;
    ch.decomp  = engine_.decompose(samples, n);
    const int n_levels = static_cast<int>(ch.decomp.detail.size());
    ch.sigma.resize(n_levels);
    ch.damaged.resize(n_levels);

    const double k = sensitivity_to_k(sensitivity);

    for (int lv = 0; lv < n_levels; ++lv) {
        if (cancel.load()) return ch;
        ch.sigma[lv] = mad_sigma(ch.decomp.detail[lv]);
        threshold(ch.decomp.detail[lv], ch.sigma[lv], k, ch.damaged[lv]);
    }

    map_to_clicks(ch, n);
    return ch;
}

// ─── Stereo detection ────────────────────────────────────────────────────────

void ClickDetector::detect_stereo(const double* left, const double* right,
                                   int n, double sensitivity,
                                   ChannelDetection& L, ChannelDetection& R,
                                   std::atomic<bool>& cancel) const
{
    // Decompose both channels
    L.decomp = engine_.decompose(left,  n);
    R.decomp = engine_.decompose(right, n);

    const int n_levels = static_cast<int>(L.decomp.detail.size());
    L.sigma.resize(n_levels);  R.sigma.resize(n_levels);
    L.damaged.resize(n_levels); R.damaged.resize(n_levels);

    const double k = sensitivity_to_k(sensitivity);

    // Pass 1: single-channel MAD detection
    for (int lv = 0; lv < n_levels; ++lv) {
        if (cancel.load()) return;
        L.sigma[lv] = mad_sigma(L.decomp.detail[lv]);
        R.sigma[lv] = mad_sigma(R.decomp.detail[lv]);
        threshold(L.decomp.detail[lv], L.sigma[lv], k, L.damaged[lv]);
        threshold(R.decomp.detail[lv], R.sigma[lv], k, R.damaged[lv]);
    }

    if (cancel.load()) return;

    // Pass 2: cross-channel refinement
    refine_stereo(L, R, k);

    // Map both channels to sample-domain click events
    map_to_clicks(L, n);
    map_to_clicks(R, n);
}

} // namespace needledropper
