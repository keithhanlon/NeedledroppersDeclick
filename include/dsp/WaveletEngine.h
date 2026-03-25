#pragma once
#include <vector>
#include <functional>

namespace declick {

// Results of a full wavelet decomposition.
// detail[level] contains the detail coefficients at that level.
// approx contains the final approximation coefficients.
struct DecompositionResult {
    std::vector<std::vector<double>> detail;
    std::vector<double>              approx;
    int                              n_levels;
    int                              original_length;
};

class WaveletEngine {
public:
    explicit WaveletEngine(int n_levels = 5);

    // Forward DWT — returns decomposition of input signal.
    // Input length should be a power of 2, or will be padded.
    DecompositionResult decompose(const double* samples,
                                  int            n) const;

    // Inverse DWT — reconstructs signal from decomposition.
    // Output length matches original_length from decomposition.
    std::vector<double> reconstruct(
        const DecompositionResult& decomp) const;

    // Single level forward transform.
    // approx and detail must each be pre-allocated to n/2 samples.
    void dwt_level(const double* in,
                   int            n,
                   double*        approx,
                   double*        detail) const;

    // Single level inverse transform.
    // out must be pre-allocated to n samples (2 * approx/detail length).
    void idwt_level(const double* approx,
                    const double* detail,
                    int           half_n,
                    double*       out) const;

    int n_levels() const { return n_levels_; }

private:
    int n_levels_;

    // D4 Daubechies filter coefficients
    static constexpr double H[4] = {
         0.4829629131445341,
         0.8365163037378079,
         0.2241438680420134,
        -0.1294095225512604
    };
    static constexpr double G[4] = {
        -H[3],  H[2], -H[1],  H[0]
    };
    // Time-reversed filters for inverse transform
    static constexpr double HT[4] = {
         H[2],  H[3],  H[0],  H[1]
    };
    static constexpr double GT[4] = {
         G[2],  G[3],  G[0],  G[1]
    };
};

} // namespace declick
