#include "dsp/WaveletEngine.h"

namespace needledropper {

WaveletEngine::WaveletEngine(int n_levels)
    : n_levels_(n_levels) {}

DecompositionResult WaveletEngine::decompose(const double*, int) const {
    return DecompositionResult{};
}

std::vector<double> WaveletEngine::reconstruct(const DecompositionResult&) const {
    return {};
}

void WaveletEngine::dwt_level(const double*, int, double*, double*) const {}

void WaveletEngine::idwt_level(const double*, const double*, int, double*) const {}

} // namespace needledropper
