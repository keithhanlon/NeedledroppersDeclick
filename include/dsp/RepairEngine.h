#pragma once
#include <vector>
#include <atomic>
#include "dsp/WaveletEngine.h"
#include "dsp/ClickDetector.h"

namespace needledropper {

// Result of a full repair operation on one channel.
struct RepairResult {
    std::vector<double> audio;          // repaired audio samples
    int                 clicks_repaired; // number of gaps filled
};

class RepairEngine {
public:
    explicit RepairEngine(const WaveletEngine& engine);

    // Repair a mono channel using its detection results.
    // cancel flag is checked between gaps.
    RepairResult repair_mono(const double*            samples,
                             int                      n,
                             const ChannelDetection&  detection,
                             std::atomic<bool>&       cancel) const;

    // Repair a stereo pair using cross-channel constraint where valid.
    void repair_stereo(const double*            left,
                       const double*            right,
                       int                      n,
                       const ChannelDetection&  left_det,
                       const ChannelDetection&  right_det,
                       RepairResult&            left_out,
                       RepairResult&            right_out,
                       std::atomic<bool>&       cancel) const;

private:
    const WaveletEngine& engine_;

    // Fit AR model of order p to samples using Levinson-Durbin.
    std::vector<double> fit_ar(const double* samples,
                               int           n,
                               int           order) const;

    // Forward AR prediction into gap.
    void ar_predict_forward(const double*                 context,
                            int                           ctx_len,
                            const std::vector<double>&    coeffs,
                            double*                       output,
                            int                           gap_len) const;

    // Repair a single gap using bidirectional AR interpolation.
    void repair_gap(const double* left_ctx,
                    int           left_len,
                    const double* right_ctx,
                    int           right_len,
                    double*       gap,
                    int           gap_len,
                    int           ar_order) const;

    // Repair a gap with cross-channel constraint.
    // B_clean indicates which positions in gap_B are undamaged.
    void repair_gap_stereo(const double* left_ctx_A,
                           int           left_len,
                           const double* right_ctx_A,
                           int           right_len,
                           double*       gap_A,
                           int           gap_len,
                           const double* gap_B,
                           const char*   B_clean,
                           int           ar_order) const;

    // Estimate least-squares gain between two context windows.
    double estimate_cross_gain(const double* ctx_A,
                               const double* ctx_B,
                               int           n) const;

    // Correlation-based weight for cross-channel assist.
    double cross_channel_weight(const double* ctx_A,
                                const double* ctx_B,
                                int           ctx_len) const;

    // AR order selection based on gap length and context available.
    static int select_ar_order(int gap_len, int ctx_len) {
        int max_order = std::min(64, ctx_len / 2);
        int min_order = 10;
        return std::clamp(gap_len * 2, min_order, max_order);
    }

    // Context window length for AR fitting.
    static constexpr int CTX_LEN = 256;
};

} // namespace needledropper
