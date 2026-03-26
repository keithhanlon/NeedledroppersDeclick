#include "threading/DetectionThread.h"
#include <juce_events/juce_events.h>

namespace needledropper {

DetectionThread::DetectionThread(const WaveletEngine& engine,
                                 const ClickDetector& detector)
    : juce::Thread("DetectionThread")
    , engine_(engine)
    , detector_(detector)
{
    startThread();
}

DetectionThread::~DetectionThread()
{
    signalThreadShouldExit();
    work_ready_.signal();
    stopThread(2000);
}

void DetectionThread::submit(const std::vector<double>& left,
                             const std::vector<double>& right,
                             bool is_stereo,
                             double sensitivity,
                             CompletionCallback on_complete)
{
    {
        juce::ScopedLock sl(lock_);
        audio_left_    = left;
        audio_right_   = right;
        is_stereo_     = is_stereo;
        sensitivity_.store(sensitivity);
        on_complete_   = std::move(on_complete);
        work_pending_  = true;
    }
    cancel_.store(true);   // cancel any in-progress run
    work_ready_.signal();
}

void DetectionThread::update_sensitivity(double sensitivity,
                                         CompletionCallback on_complete)
{
    {
        juce::ScopedLock sl(lock_);
        sensitivity_.store(sensitivity);
        on_complete_  = std::move(on_complete);
        work_pending_ = true;
    }
    cancel_.store(true);
    work_ready_.signal();
}

void DetectionThread::run()
{
    while (!threadShouldExit()) {
        work_ready_.wait(-1);  // sleep until signalled

        if (threadShouldExit()) break;

        // Snapshot current work under lock
        std::vector<double> left, right;
        bool        is_stereo;
        double      sensitivity;
        CompletionCallback callback;

        {
            juce::ScopedLock sl(lock_);
            if (!work_pending_) continue;
            left        = audio_left_;
            right       = audio_right_;
            is_stereo   = is_stereo_;
            sensitivity = sensitivity_.load();
            callback    = on_complete_;
            work_pending_ = false;
        }

        // Reset cancellation flag for this run
        cancel_.store(false);

        DetectionResults results;
        results.sensitivity = sensitivity;
        results.is_stereo   = is_stereo;

        if (is_stereo && !right.empty()) {
            detector_.detect_stereo(left.data(), right.data(),
                                    static_cast<int>(left.size()),
                                    sensitivity,
                                    results.left, results.right,
                                    cancel_);
        } else {
            results.left = detector_.detect_mono(left.data(),
                                                  static_cast<int>(left.size()),
                                                  sensitivity,
                                                  cancel_);
        }

        // Only post results if not cancelled
        if (!cancel_.load() && !threadShouldExit() && callback) {
            juce::MessageManager::callAsync(
                [cb = std::move(callback), r = std::move(results)]() mutable {
                    cb(std::move(r));
                });
        }
    }
}

} // namespace needledropper
