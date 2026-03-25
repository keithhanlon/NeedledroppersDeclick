#pragma once
#include <vector>
#include <atomic>
#include "dsp/WaveletEngine.h"

namespace declick {

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
    std::vector<double>              sigma;     // MAD sigma per level
    std::vector<std::vector<bool>>   damaged;   // [level][coeff index]
    std::vector<ClickEvent>          clicks;    // mapped to sample domain
};

class ClickDetector {
public:
    explicit ClickDetector(const WaveletEngine& engine);

    // Detect clicks in a mono channel.
    // cancel flag is checked each level — set to abort mid-run.
    ChannelDetection detect_mono(const double*        samples,
                                 int                  n,
                                 double               sensitivity,
                                 std::atomic<bool>&   cancel) const;

    // Detect clicks in a stereo pair with cross-channel refinement.
    // Results are written into left and right in place.
    void detect_stereo(const double*        left,
                       const double*        right,
                       int                  n,
                       double               sensitivity,
                       ChannelDetection&    left_out,
                       ChannelDetection&    right_out,
                       std::atomic<bool>&   cancel) const;

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

    // Sensitivity (0-100) to k-sigma multiplier.
    static double sensitivity_to_k(double sensitivity) {
        return 8.0 - (sensitivity / 100.0) * 6.0;
    }
};

} // namespace declick
