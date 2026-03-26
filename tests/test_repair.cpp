#include "dsp/WaveletEngine.h"
#include "dsp/ClickDetector.h"
#include "dsp/RepairEngine.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <atomic>

static std::vector<double> make_sine(int n, double freq = 32.0) {
    std::vector<double> s(n);
    for (int i = 0; i < n; ++i)
        s[i] = 0.8 * std::sin(2.0 * M_PI * i / freq);
    return s;
}

static double rms_error(const std::vector<double>& a,
                         const std::vector<double>& b) {
    double e = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        double d = a[i] - b[i];
        e += d * d;
    }
    return std::sqrt(e / a.size());
}

int main() {
    needledropper::WaveletEngine engine(5);
    needledropper::ClickDetector detector(engine);
    needledropper::RepairEngine  repair(engine);
    std::atomic<bool>            cancel { false };

    printf("RepairEngine tests\n");
    printf("-------------------\n");

    {
        auto original = make_sine(1024);
        auto damaged  = original;
        for (int i = 400; i < 406; ++i)
            damaged[i] += (i % 2 == 0 ? 1.0 : -1.0) * 3.0;

        auto detection = detector.detect_mono(damaged.data(), 1024, 50.0, cancel);
        auto result    = repair.repair_mono(damaged.data(), 1024, detection, cancel);

        const double err_damaged  = rms_error(original, damaged);
        const double err_repaired = rms_error(original, result.audio);
        const bool   improved     = err_repaired < err_damaged;

        printf("  mono repair improves RMS error:      %s\n", improved ? "PASS" : "FAIL");
        printf("    damaged  RMS: %.6f\n", err_damaged);
        printf("    repaired RMS: %.6f\n", err_repaired);
        printf("    clicks repaired: %d\n", result.clicks_repaired);
    }

    {
        auto original  = make_sine(1024);
        auto detection = detector.detect_mono(original.data(), 1024, 50.0, cancel);
        auto result    = repair.repair_mono(original.data(), 1024, detection, cancel);

        const double err = rms_error(original, result.audio);
        const bool   unchanged = err < 1e-10;
        printf("  clean signal unchanged after repair: %s  (err=%.2e)\n",
               unchanged ? "PASS" : "FAIL", err);
    }

    {
        auto origL = make_sine(1024, 32.0);
        auto origR = make_sine(1024, 24.0);
        auto dmgL  = origL;
        auto dmgR  = origR;
        for (int i = 500; i < 506; ++i) {
            dmgL[i] += (i % 2 == 0 ? 1.0 : -1.0) * 3.0;
            dmgR[i] += (i % 2 == 0 ? 1.0 : -1.0) * 3.0;
        }

        needledropper::ChannelDetection lcd, rcd;
        detector.detect_stereo(dmgL.data(), dmgR.data(), 1024,
                                50.0, lcd, rcd, cancel);

        needledropper::RepairResult lres, rres;
        repair.repair_stereo(dmgL.data(), dmgR.data(), 1024,
                              lcd, rcd, lres, rres, cancel);

        const double errL    = rms_error(origL, lres.audio);
        const double errR    = rms_error(origR, rres.audio);
        const double dmgErrL = rms_error(origL, dmgL);
        const double dmgErrR = rms_error(origR, dmgR);

        const bool improved = errL < dmgErrL && errR < dmgErrR;
        printf("  stereo repair improves both channels: %s\n", improved ? "PASS" : "FAIL");
        printf("    L: damaged=%.6f repaired=%.6f\n", dmgErrL, errL);
        printf("    R: damaged=%.6f repaired=%.6f\n", dmgErrR, errR);
    }

    printf("-------------------\n");
    printf("Done.\n");
    return 0;
}
