#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include "dsp/WaveletEngine.h"
#include "dsp/ClickDetector.h"

namespace needledropper {

// Results posted back to the UI thread after a detection run.
struct DetectionResults {
    ChannelDetection left;
    ChannelDetection right;
    double           sensitivity;    // sensitivity used for this run
    bool             is_stereo;
};

class DetectionThread : public juce::Thread {
public:
    // Callback invoked on the UI thread when detection completes.
    using CompletionCallback = std::function<void(DetectionResults)>;

    DetectionThread(const WaveletEngine& engine,
                    const ClickDetector& detector);
    ~DetectionThread() override;

    // Submit new audio for detection.
    // Safe to call from UI thread at any time — cancels any in-progress run.
    void submit(const std::vector<double>& left,
                const std::vector<double>& right,
                bool                       is_stereo,
                double                     sensitivity,
                CompletionCallback         on_complete);

    // Update sensitivity only — re-runs detection on current audio.
    // Debounced by caller (ParameterPanel timer).
    void update_sensitivity(double             sensitivity,
                            CompletionCallback on_complete);

    // juce::Thread entry point.
    void run() override;

private:
    const WaveletEngine& engine_;
    const ClickDetector& detector_;

    // Shared state — written from UI thread, read from worker thread.
    juce::CriticalSection       lock_;
    std::vector<double>         audio_left_;
    std::vector<double>         audio_right_;
    bool                        is_stereo_    { false };
    std::atomic<double>         sensitivity_  { 50.0 };
    CompletionCallback          on_complete_;
    bool                        work_pending_ { false };

    // Cancellation flag — set from UI thread, checked in detection loop.
    std::atomic<bool>           cancel_       { false };

    // Wakes the thread when new work arrives.
    juce::WaitableEvent         work_ready_;
};

} // namespace needledropper
