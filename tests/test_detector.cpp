// Standalone click detector test.
// clang++ -std=c++17 -I../include test_detector.cpp
//     ../src/dsp/WaveletEngine.cpp ../src/dsp/ClickDetector.cpp
//     -o test_detector && ./test_detector

#include "dsp/WaveletEngine.h"
#include "dsp/ClickDetector.h"
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

static void inject_click(std::vector<double>& s, int pos, int width = 4) {
    for (int i = pos; i < pos + width && i < (int)s.size(); ++i)
        s[i] += (i % 2 == 0 ? 1.0 : -1.0) * 3.0;
}

int main() {
    needledropper::WaveletEngine  engine(5);
    needledropper::ClickDetector  detector(engine);
    std::atomic<bool>             cancel { false };

    printf("ClickDetector tests\n");
    printf("--------------------\n");

    // Test 1: single click detected in mono signal
    {
        auto sig = make_sine(1024);
        inject_click(sig, 400, 4);
        auto ch = detector.detect_mono(sig.data(), 1024, 50.0, cancel);
        bool found = !ch.clicks.empty();
        printf("  mono single click (sensitivity=50):  %s  (%d events)\n",
               found ? "PASS" : "FAIL", (int)ch.clicks.size());
    }

    // Test 2: no clicks on clean signal
    {
        auto sig = make_sine(1024);
        auto ch = detector.detect_mono(sig.data(), 1024, 50.0, cancel);
        bool clean = ch.clicks.empty();
        printf("  mono clean signal (no false pos):     %s  (%d events)\n",
               clean ? "PASS" : "FAIL", (int)ch.clicks.size());
    }

    // Test 3: multiple clicks detected
    {
        auto sig = make_sine(2048);
        inject_click(sig, 200,  3);
        inject_click(sig, 800,  5);
        inject_click(sig, 1500, 4);
        auto ch = detector.detect_mono(sig.data(), 2048, 50.0, cancel);
        bool found = ch.clicks.size() >= 3;
        printf("  mono three clicks detected:           %s  (%d events)\n",
               found ? "PASS" : "FAIL", (int)ch.clicks.size());
    }

    // Test 4: stereo — click in both channels at same position
    {
        auto L = make_sine(1024);
        auto R = make_sine(1024, 16.0);
        inject_click(L, 500, 4);
        inject_click(R, 500, 4);
        needledropper::ChannelDetection lcd, rcd;
        detector.detect_stereo(L.data(), R.data(), 1024, 50.0,
                                lcd, rcd, cancel);
        bool both = !lcd.clicks.empty() && !rcd.clicks.empty();
        printf("  stereo symmetric click:               %s  (L:%d R:%d)\n",
               both ? "PASS" : "FAIL",
               (int)lcd.clicks.size(), (int)rcd.clicks.size());
    }

    // Test 5: sensitivity — high sensitivity catches more
    {
        auto sig = make_sine(1024);
        inject_click(sig, 400, 2);  // small click
        auto ch_lo = detector.detect_mono(sig.data(), 1024, 20.0, cancel);
        auto ch_hi = detector.detect_mono(sig.data(), 1024, 80.0, cancel);
        bool sens_works = ch_hi.clicks.size() >= ch_lo.clicks.size();
        printf("  sensitivity scaling works:            %s  (lo:%d hi:%d)\n",
               sens_works ? "PASS" : "FAIL",
               (int)ch_lo.clicks.size(), (int)ch_hi.clicks.size());
    }

    printf("--------------------\n");
    printf("Done.\n");
    return 0;
}
