#include "threading/ProcessingThread.h"

namespace needledropper {

ProcessingThread::ProcessingThread(const WaveletEngine& engine,
                                   const RepairEngine&  repair)
    : juce::Thread("ProcessingThread")
    , engine_(engine)
    , repair_(repair) {}

ProcessingThread::~ProcessingThread() {
    signalThreadShouldExit();
    work_ready_.signal();
    stopThread(5000);
}

void ProcessingThread::submit_batch(std::vector<BatchItem>,
                                    ProgressCallback,
                                    CompletionCallback) {}

void ProcessingThread::cancel() {
    cancel_.store(true);
}

void ProcessingThread::run() {}

void ProcessingThread::process_item(const BatchItem&,
                                    int, int,
                                    ProgressCallback&,
                                    CompletionCallback&) {}

} // namespace needledropper
