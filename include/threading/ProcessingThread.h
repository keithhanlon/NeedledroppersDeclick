#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include "dsp/WaveletEngine.h"
#include "dsp/ClickDetector.h"
#include "dsp/RepairEngine.h"

namespace needledropper {

// Progress update posted to UI thread during processing.
struct ProcessingProgress {
    int    current_file;     // 1-based index into batch queue
    int    total_files;
    float  file_progress;    // 0.0 - 1.0 within current file
    int    clicks_repaired;  // running total for current file
};

// Final result for one file posted to UI thread on completion.
struct ProcessingComplete {
    std::vector<double> left;
    std::vector<double> right;
    bool                is_stereo;
    int                 clicks_repaired;
    juce::File          source_file;
    bool                success;
    juce::String        error_message;  // populated if !success
};

// One item in the processing batch.
struct BatchItem {
    juce::File          source_file;
    std::vector<double> left;
    std::vector<double> right;
    bool                is_stereo;
    ChannelDetection    left_detection;
    ChannelDetection    right_detection;
    double              sensitivity;
};

class ProcessingThread : public juce::Thread {
public:
    using ProgressCallback   = std::function<void(ProcessingProgress)>;
    using CompletionCallback = std::function<void(ProcessingComplete)>;

    ProcessingThread(const WaveletEngine& engine,
                     const RepairEngine&  repair);
    ~ProcessingThread() override;

    // Submit a batch for processing.
    // Safe to call from UI thread — replaces any pending batch.
    void submit_batch(std::vector<BatchItem> items,
                      ProgressCallback       on_progress,
                      CompletionCallback     on_complete);

    // Cancel current processing run.
    void cancel();

    // juce::Thread entry point.
    void run() override;

private:
    const WaveletEngine& engine_;
    const RepairEngine&  repair_;

    juce::CriticalSection   lock_;
    std::vector<BatchItem>  pending_batch_;
    ProgressCallback        on_progress_;
    CompletionCallback      on_complete_;
    bool                    work_pending_ { false };

    std::atomic<bool>       cancel_      { false };
    juce::WaitableEvent     work_ready_;

    void process_item(const BatchItem&   item,
                      int                index,
                      int                total,
                      ProgressCallback&  on_progress,
                      CompletionCallback& on_complete);
};

} // namespace needledropper
