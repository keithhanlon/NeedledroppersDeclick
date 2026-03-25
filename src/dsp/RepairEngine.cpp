#include "dsp/RepairEngine.h"

namespace needledropper {

RepairEngine::RepairEngine(const WaveletEngine& engine)
    : engine_(engine) {}

RepairResult RepairEngine::repair_mono(const double*, int,
                                        const ChannelDetection&,
                                        std::atomic<bool>&) const {
    return RepairResult{};
}

void RepairEngine::repair_stereo(const double*, const double*, int,
                                  const ChannelDetection&,
                                  const ChannelDetection&,
                                  RepairResult&, RepairResult&,
                                  std::atomic<bool>&) const {}

std::vector<double> RepairEngine::fit_ar(const double*, int, int) const { return {}; }

void RepairEngine::ar_predict_forward(const double*, int,
                                       const std::vector<double>&,
                                       double*, int) const {}

void RepairEngine::repair_gap(const double*, int, const double*, int,
                               double*, int, int) const {}

void RepairEngine::repair_gap_stereo(const double*, int, const double*, int,
                                      double*, int, const double*,
                                      const bool*, int) const {}

double RepairEngine::estimate_cross_gain(const double*, const double*, int) const { return 1.0; }

double RepairEngine::cross_channel_weight(const double*, const double*, int) const { return 0.0; }

} // namespace needledropper
