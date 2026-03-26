// Standalone roundtrip test for WaveletEngine.
// Compile and run independently of JUCE:
//   clang++ -std=c++17 -I../include test_wavelet.cpp ../src/dsp/WaveletEngine.cpp -o test_wavelet
//   ./test_wavelet

#include "dsp/WaveletEngine.h"
#include <cmath>
#include <cstdio>
#include <vector>

static bool roundtrip_test(int n_levels, int signal_length) {
    needledropper::WaveletEngine engine(n_levels);

    // Build a test signal: mix of two sine waves
    std::vector<double> original(signal_length);
    for (int i = 0; i < signal_length; ++i)
        original[i] = 0.7 * std::sin(2.0 * M_PI * i / 32.0)
                    + 0.3 * std::sin(2.0 * M_PI * i / 8.0);

    // Forward decompose
    auto decomp = engine.decompose(original.data(), signal_length);

    // Inverse reconstruct
    auto recovered = engine.reconstruct(decomp);

    // Measure max absolute error
    double max_err = 0.0;
    for (int i = 0; i < signal_length; ++i) {
        double err = std::abs(recovered[i] - original[i]);
        if (err > max_err) max_err = err;
    }

    bool passed = max_err < 1e-10;
    printf("  levels=%d  length=%d  max_error=%.2e  %s\n",
           n_levels, signal_length, max_err,
           passed ? "PASS" : "FAIL");
    return passed;
}

int main() {
    printf("WaveletEngine roundtrip tests\n");
    printf("------------------------------\n");

    bool all_passed = true;
    all_passed &= roundtrip_test(1, 256);
    all_passed &= roundtrip_test(3, 512);
    all_passed &= roundtrip_test(5, 1024);
    all_passed &= roundtrip_test(5, 4096);
    all_passed &= roundtrip_test(8, 8192);

    printf("------------------------------\n");
    printf("%s\n", all_passed ? "All tests passed." : "FAILURES detected.");
    return all_passed ? 0 : 1;
}
