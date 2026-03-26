#include "dsp/WaveletEngine.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace needledropper {

WaveletEngine::WaveletEngine(int n_levels)
    : n_levels_(n_levels)
{
    if (n_levels < 1 || n_levels > 10)
        throw std::invalid_argument("n_levels must be 1-10");
}

void WaveletEngine::dwt_level(const double* in, int n,
                               double* approx, double* detail) const
{
    const int half = n / 2;
    for (int i = 0; i < half; ++i) {
        double a = 0.0, d = 0.0;
        for (int k = 0; k < 4; ++k) {
            const double s = in[(2*i + k) % n];
            a += H[k] * s;
            d += G[k] * s;
        }
        approx[i] = a;
        detail[i]  = d;
    }
}

void WaveletEngine::idwt_level(const double* approx, const double* detail,
                                int half_n, double* out) const
{
    const int n = 2 * half_n;
    for (int j = 0; j < n; ++j) out[j] = 0.0;

    // Transpose of the forward transform.
    // For orthogonal Daubechies wavelets this gives perfect reconstruction:
    // out[(2i+k) % n] += approx[i]*H[k] + detail[i]*G[k]
    for (int i = 0; i < half_n; ++i) {
        for (int k = 0; k < 4; ++k) {
            const int idx = (2*i + k) % n;
            out[idx] += approx[i] * H[k] + detail[i] * G[k];
        }
    }
}

DecompositionResult WaveletEngine::decompose(const double* samples, int n) const
{
    DecompositionResult result;
    result.n_levels        = n_levels_;
    result.original_length = n;
    result.detail.resize(n_levels_);

    // Start with the input signal as the first approximation
    std::vector<double> current(samples, samples + n);

    for (int lv = 0; lv < n_levels_; ++lv) {
        const int len      = static_cast<int>(current.size());
        // Ensure even length for downsampling — odd lengths are rare
        // for audio but handled safely by ignoring the last sample
        const int even_len = len & ~1;
        const int half     = even_len / 2;

        std::vector<double> next_approx(half);
        std::vector<double> next_detail(half);

        dwt_level(current.data(), even_len,
                  next_approx.data(), next_detail.data());

        result.detail[lv] = std::move(next_detail);
        current           = std::move(next_approx);
    }

    result.approx = std::move(current);
    return result;
}

std::vector<double> WaveletEngine::reconstruct(const DecompositionResult& decomp) const
{
    // Begin at the deepest approximation and work back up
    std::vector<double> current = decomp.approx;

    for (int lv = decomp.n_levels - 1; lv >= 0; --lv) {
        const auto& detail = decomp.detail[lv];
        const int   half_n = static_cast<int>(detail.size());

        std::vector<double> rebuilt(2 * half_n);
        idwt_level(current.data(), detail.data(),
                   half_n, rebuilt.data());

        current = std::move(rebuilt);
    }

    // Trim to original length in case of any padding
    current.resize(decomp.original_length);
    return current;
}

} // namespace needledropper
