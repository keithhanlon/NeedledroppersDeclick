#include "threading/DetectionThread.h"

namespace needledropper {

DetectionThread::DetectionThread(const WaveletEngine& engine,
                                 const ClickDetector& detector)
    : juce::Thread("DetectionThread")
    , engine_(engine)
    , detector_(detector) {}

DetectionThread::~DetectionThread() {
    signalThreadShouldExit();
    work_ready_.signal();
    stopThread(2000);
}

void DetectionThread::submit(const std::vector<double>&,
                             const std::vector<double>&,
                             bool, double, CompletionCallback) {}

void DetectionThread::update_sensitivity(double, CompletionCallback) {}

void DetectionThread::run() {}

} // namespace needledropper
