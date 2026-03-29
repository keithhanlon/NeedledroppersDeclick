// Needledropper's Declicker
// Copyright (c) 2025 Keith Hanlon
// SPDX-License-Identifier: AGPL-3.0-only
//
// This file is original work. No code has been copied or derived from
// any existing software including ClickRepair or any other application.
// Algorithms used are based on published academic literature.

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

static void detect_prediction_error(const double* samples, int n,
                                    double threshold,
                                    double sample_rate,
                                    std::vector<double>& pred_error,
                                    std::vector<double>& running_avg,
                                    std::vector<bool>& damaged_out)
{

    // AR order: enough to model local spectral character
    const int AR_ORDER = 10;

    // Refit AR model every ~5ms using ~10ms of context
    const int BLOCK = static_cast<int>(sample_rate * 0.005);
    const int CTX   = static_cast<int>(sample_rate * 0.011);

    // Maximum click run length: ~0.65ms (vinyl clicks are typically <0.5ms)
    const int MAX_CLICK = static_cast<int>(sample_rate * 0.00065);

    // Minimum absolute amplitude — don't flag near-silence
    const double MIN_AMP = 1e-4;

    // Slow exponential average of prediction error.
    // Time constant ~21ms regardless of sample rate.
    // lambda = 1 - 1/(sr * 0.021)
    const double lambda = 1.0 - 1.0 / (sample_rate * 0.021);

    // Warmup: skip first 100ms while running average stabilises
    const int WARMUP = static_cast<int>(sample_rate * 0.10);

    std::vector<bool>   damaged(n, false);
    pred_error.assign(n, 0.0);
    running_avg.assign(n, 0.0);

    // Compute per-sample prediction errors
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

    damaged_out = std::move(damaged);
}


// ─── Crackle detection ────────────────────────────────────────────────────────
//
// Crackle is sustained elevated prediction error — distributed noise rather
// than isolated spikes. The discriminator uses a 3-sample energy sum of
// prediction error compared to a very slow running average squared.
//
// A score accumulator (+10 when triggered, -1 otherwise) requires sustained
// elevated error to accumulate enough score to be flagged, preventing isolated
// click spikes (already handled by the click detector) from triggering crackle
// mode.

static std::vector<ClickEvent> detect_crackle_channel(
    const std::vector<double>& pred_error,
    const double* samples,
    int n, double kal_factor, double sample_rate)
{
    // Very slow running average: ~208ms time constant regardless of sample rate
    const double LAM      = 1.0 - 1.0 / (sample_rate * 0.208);
    const double SCORE_HIT  = 10.0;   // score increment when flagged
    const double SCORE_MISS =  1.0;   // score decrement otherwise
    const double SCORE_THRESH = 5.0;  // minimum score to mark as crackle
    const int    WARMUP   = static_cast<int>(sample_rate * 0.15);
    const int    PAD      = 3;

    // Build slow running average
    std::vector<double> crackle_avg(n, 0.0);
    double avg = 0.001;
    for (int i = 10; i < n; ++i) {
        avg = LAM * avg + (1.0 - LAM) * pred_error[i];
        crackle_avg[i] = avg;
    }

    // Signal amplitude gate: compute raw signal RMS in a short window.
    // Crackle lives in the quiet noise floor; drum hits are loud.
    // If the signal itself is loud relative to the long-term noise floor,
    // don't flag crackle there — that's a musical transient.
    const int RMS_WIN = static_cast<int>(sample_rate * 0.005);  // ~5ms
    std::vector<double> signal_rms(n, 0.0);
    {
        double sum_sq = 0.0;
        for (int i = 0; i < std::min(RMS_WIN, n); ++i)
            sum_sq += samples[i] * samples[i];
        for (int i = RMS_WIN; i < n; ++i) {
            sum_sq += samples[i]           * samples[i];
            sum_sq -= samples[i - RMS_WIN] * samples[i - RMS_WIN];
            signal_rms[i] = std::sqrt(sum_sq / RMS_WIN);
        }
    }
    // Long-term signal RMS for gating reference
    double signal_avg = 0.0;
    {
        double sum_sq = 0.0;
        for (int i = 0; i < n; ++i) sum_sq += samples[i] * samples[i];
        signal_avg = std::sqrt(sum_sq / n);
    }

    // Score accumulator pass
    std::vector<bool> damaged(n, false);
    double score = 0.0;
    for (int i = 11; i < n - 1; ++i) {
        const double e_sum = pred_error[i-1] * pred_error[i-1]
                           + pred_error[i+1] * pred_error[i+1]
                           + 2.0 * pred_error[i] * pred_error[i];
        const double avg_sq = crackle_avg[i] * crackle_avg[i];

        // Amplitude gate: if the signal is louder than 2x the long-term
        // average, this is a musical transient (drum hit, guitar attack).
        // Hard-reset score to zero so accumulated score doesn't bleed
        // into adjacent samples around the transient.
        const bool loud = (signal_rms[i] > 2.0 * signal_avg);

        if (avg_sq < 1e-18 || loud) {
            score = 0.0;  // hard reset, not gradual decay
        } else if (e_sum > kal_factor * avg_sq) {
            score += SCORE_HIT;
        } else {
            score = std::max(0.0, score - SCORE_MISS);
        }

        if (score > SCORE_THRESH && i >= WARMUP)
            damaged[i] = true;
    }

    // Maximum crackle run length: ~200 samples (~4ms at 48kHz).
    // Longer runs are drum hits or other transients the AR model struggled
    // to predict — not genuine crackle. Reject them.
    // Maximum crackle event length for repair.
    // Events longer than ~1ms risk overlapping drum attacks and other
    // transients — AR interpolation can't reconstruct those faithfully.
    // 48 samples ≈ 1ms at 48kHz.
    const int MAX_RUN = 48;

    // Duration filter
    int i = 0;
    while (i < n) {
        if (!damaged[i]) { ++i; continue; }
        const int run_start = i;
        while (i < n && damaged[i]) ++i;
        if (i - run_start > MAX_RUN)
            for (int j = run_start; j < i; ++j)
                damaged[j] = false;
    }

    // Collect events with padding
    std::vector<ClickEvent> events;
    i = 0;
    while (i < n) {
        if (!damaged[i]) { ++i; continue; }
        const int run_start = i;
        while (i < n && damaged[i]) ++i;
        const int ev_start = std::max(0, run_start - PAD);
        const int ev_end   = std::min(n, i + PAD);
        events.push_back({ ev_start, ev_end, 0, 0.7 });
    }
    return events;
}

// ─── Fast threshold recount ───────────────────────────────────────────────────

int ClickDetector::recount_clicks(const ChannelDetection& ch, double threshold,
                                   int warmup_samples, int max_click)
{
    const auto& pe  = ch.pred_error;
    const auto& avg = ch.running_avg;
    const int n     = static_cast<int>(pe.size());
    if (n == 0 || avg.empty()) return 0;

    std::vector<bool> damaged(n, false);
    for (int i = warmup_samples; i < n; ++i) {
        if (avg[i] < 1e-9) continue;
        if (pe[i] > threshold * avg[i])
            damaged[i] = true;
    }
    // Duration filter
    int i = 0;
    while (i < n) {
        if (!damaged[i]) { ++i; continue; }
        const int rs = i;
        while (i < n && damaged[i]) ++i;
        if (i - rs > max_click)
            for (int j = rs; j < i; ++j) damaged[j] = false;
    }
    // Count events
    int count = 0; i = 0;
    while (i < n) {
        if (!damaged[i]) { ++i; continue; }
        ++count;
        while (i < n && damaged[i]) ++i;
    }
    return count;
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

static std::vector<double> compute_pred_errors(const double* samples, int n)
{
    const int AR_ORDER = 10, BLOCK = 256, CTX = 512;
    std::vector<double> pred_err(n, 0.0);
    std::vector<double> ar(AR_ORDER, 0.0);
    int last_fit = -BLOCK;
    for (int i = AR_ORDER; i < n; ++i) {
        if (i - last_fit >= BLOCK) {
            const int cs = std::max(0, i - CTX);
            if (i - cs >= AR_ORDER + 1)
                ar = fit_ar(samples + cs, i - cs, AR_ORDER);
            last_fit = i;
        }
        double pred = 0.0;
        for (int k = 0; k < AR_ORDER; ++k) pred += ar[k] * samples[i - 1 - k];
        pred_err[i] = std::abs(samples[i] - pred);
    }
    return pred_err;
}

// ─── Mono detection ───────────────────────────────────────────────────────────

ChannelDetection ClickDetector::detect_mono(const double* samples, int n,
                                             double sensitivity,
                                             double crackle_sensitivity,
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
    detect_prediction_error(samples, n, threshold, sample_rate,
                             ch.pred_error, ch.running_avg, ch.time_damaged);

    time_to_wavelet_masks(ch.time_damaged, n_levels, n, ch.damaged);
    map_to_clicks(ch, n);
    if (crackle_sensitivity > 0.0) {
        const double kal = 250.0 - (crackle_sensitivity / 100.0) * 200.0;
        auto pred_err = compute_pred_errors(samples, n);
        ch.crackle_clicks = detect_crackle_channel(pred_err, samples, n, kal, sample_rate);
    }
    return ch;
}

// ─── Stereo detection ─────────────────────────────────────────────────────────

void ClickDetector::detect_stereo(const double* left, const double* right,
                                   int n, double sensitivity,
                                   double crackle_sensitivity,
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
    detect_prediction_error(left,  n, threshold, sample_rate,
                             L.pred_error, L.running_avg, L.time_damaged);
    detect_prediction_error(right, n, threshold, sample_rate,
                             R.pred_error, R.running_avg, R.time_damaged);

    time_to_wavelet_masks(L.time_damaged, n_levels, n, L.damaged);
    time_to_wavelet_masks(R.time_damaged, n_levels, n, R.damaged);

    map_to_clicks(L, n);
    map_to_clicks(R, n);
    if (crackle_sensitivity > 0.0) {
        const double kal = 250.0 - (crackle_sensitivity / 100.0) * 200.0;
        L.crackle_clicks = detect_crackle_channel(compute_pred_errors(left,  n), left,  n, kal, sample_rate);
        R.crackle_clicks = detect_crackle_channel(compute_pred_errors(right, n), right, n, kal, sample_rate);
    }
}

} // namespace needledropper
