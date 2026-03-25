#include "dsp/ClickDetector.h"

namespace needledropper {

ClickDetector::ClickDetector(const WaveletEngine& engine)
    : engine_(engine) {}

ChannelDetection ClickDetector::detect_mono(const double*, int,
                                             double,
                                             std::atomic<bool>&) const {
    return ChannelDetection{};
}

void ClickDetector::detect_stereo(const double*, const double*, int,
                                   double,
                                   ChannelDetection&, ChannelDetection&,
                                   std::atomic<bool>&) const {}

double ClickDetector::mad_sigma(const std::vector<double>&) const { return 0.0; }

void ClickDetector::threshold(const std::vector<double>&, double,
                               double, std::vector<bool>&) const {}

double ClickDetector::stereo_confidence(double, double, double) const { return 0.0; }

double ClickDetector::cross_correlation(const double*, const double*, int) const { return 0.0; }

void ClickDetector::refine_stereo(ChannelDetection&, ChannelDetection&, double) const {}

void ClickDetector::map_to_clicks(ChannelDetection&, int) const {}

} // namespace needledropper
