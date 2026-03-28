#pragma once
#include <vector>
#include <atomic>
#include "dsp/WaveletEngine.h"

namespace needledropper {

// Represents a single detected click in the original sample domain.
struct ClickEvent {
    int    sample_start;   // first damaged sample in original signal
    int    sample_end;     // first clean sample after damaged region
    int    level;          // wavelet level where detection occurred
    double confidence;     // 0.0 - 1.0, higher = more certain
};

// Per-channel detection state, holds decomposition and masks.
struct ChannelDetection {
    DecompositionResult              decomp;
    std::vector<double>              sigma;          // MAD sigma per level
    std::vector<std::vector<bool>>   damaged;        // [level][coeff index]
    std::vector<ClickEvent>          clicks;         // click events (sample domain)
    std::vector<ClickEvent>          crackle_clicks; // crackle events (sample domain)
    std::vector<bool>                time_damaged;   // raw time-domain mask
    // Cached for fast threshold-only recount (avoids re-running AR fitting)
    std::vector<double>              pred_error;     // per-sample AR prediction error
    std::vector<double>              running_avg;    // slow exponential average of pred_error
};

class ClickDetector {
public:
    explicit ClickDetector(const WaveletEngine& engine);

    // Detect clicks in a mono channel.
    // cancel flag is checked each level — set to abort mid-run.
    ChannelDetection detect_mono(const double*        samples,
                                 int                  n,
                                 double               sensitivity,
                                 double               crackle_sensitivity,
                                 double               sample_rate,
                                 std::atomic<bool>&   cancel) const;

    // Detect clicks in a stereo pair with cross-channel refinement.
    // Results are written into left and right in place.
    void detect_stereo(const double*        left,
                       const double*        right,
                       int                  n,
                       double               sensitivity,
                       double               crackle_sensitivity,
                       double               sample_rate,
                       ChannelDetection&    left_out,
                       ChannelDetection&    right_out,
                       std::atomic<bool>&   cancel) const;


    // Sensitivity (0-100) to k-sigma multiplier.
    static double sensitivity_to_k(double sensitivity) {
        return 8.0 - (sensitivity / 100.0) * 6.0;
    }

    // Fast recount: reapply threshold to cached pred_error/running_avg
    // without re-running AR fitting. Used for real-time slider feedback.
    static int recount_clicks(const ChannelDetection& ch,
                              double threshold,
                              int    warmup_samples,
                              int    max_click = 30);

private:
    const WaveletEngine& engine_;

    // Compute MAD-based sigma for a detail level.
    double mad_sigma(const std::vector<double>& detail) const;

    // Apply threshold and dilation to produce damaged mask.
    void threshold(const std::vector<double>& detail,
                   double                     sigma,
                   double                     k,
                   std::vector<bool>&         mask) const;

    // Cross-channel confidence score.
    double stereo_confidence(double mag_a,
                             double mag_b,
                             double sigma_b) const;

    // Normalized cross-correlation over context window.
    double cross_correlation(const double* a,
                             const double* b,
                             int           n) const;

    // Refine damaged masks using cross-channel evidence.
    void refine_stereo(ChannelDetection& L,
                       ChannelDetection& R,
                       double            k_base) const;

    // Map wavelet-domain damaged masks back to sample positions.
    void map_to_clicks(ChannelDetection& ch,
                       int               original_length) const;

};

} // namespace needledropper
