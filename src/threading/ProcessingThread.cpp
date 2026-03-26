#include "threading/ProcessingThread.h"
#include <juce_events/juce_events.h>

namespace needledropper {

ProcessingThread::ProcessingThread(const WaveletEngine& engine,
                                   const RepairEngine&  repair)
    : juce::Thread("ProcessingThread")
    , engine_(engine)
    , repair_(repair)
{
    startThread();
}

ProcessingThread::~ProcessingThread()
{
    signalThreadShouldExit();
    work_ready_.signal();
    stopThread(5000);
}

void ProcessingThread::submit_batch(std::vector<BatchItem> items,
                                    ProgressCallback       on_progress,
                                    CompletionCallback     on_complete)
{
    {
        juce::ScopedLock sl(lock_);
        pending_batch_ = std::move(items);
        on_progress_   = std::move(on_progress);
        on_complete_   = std::move(on_complete);
        work_pending_  = true;
    }
    cancel_.store(false);
    work_ready_.signal();
}

void ProcessingThread::cancel()
{
    cancel_.store(true);
}

void ProcessingThread::run()
{
    while (!threadShouldExit()) {
        work_ready_.wait(-1);

        if (threadShouldExit()) break;

        std::vector<BatchItem>  batch;
        ProgressCallback        on_progress;
        CompletionCallback      on_complete;

        {
            juce::ScopedLock sl(lock_);
            if (!work_pending_) continue;
            batch        = std::move(pending_batch_);
            on_progress  = on_progress_;
            on_complete  = on_complete_;
            work_pending_ = false;
        }

        const int total = static_cast<int>(batch.size());
        for (int i = 0; i < total; ++i) {
            if (cancel_.load() || threadShouldExit()) break;
            process_item(batch[i], i + 1, total, on_progress, on_complete);
        }
    }
}

void ProcessingThread::process_item(const BatchItem&    item,
                                    int                 index,
                                    int                 total,
                                    ProgressCallback&   on_progress,
                                    CompletionCallback& on_complete)
{
    ProcessingComplete result;
    result.source_file  = item.source_file;
    result.is_stereo    = item.is_stereo;
    result.success      = false;

    const int n = static_cast<int>(item.left.size());

    // Progress reporter — posts to UI thread
    auto report = [&](float pct) {
        if (on_progress) {
            ProcessingProgress prog;
            prog.current_file     = index;
            prog.total_files      = total;
            prog.file_progress    = pct;
            prog.clicks_repaired  = 0;
            juce::MessageManager::callAsync(
                [cb = on_progress, p = prog]() { cb(p); });
        }
    };

    report(0.0f);

    try {
        if (item.is_stereo && !item.right.empty()) {
            needledropper::RepairResult lres, rres;
            repair_.repair_stereo(item.left.data(),
                                  item.right.data(),
                                  n,
                                  item.left_detection,
                                  item.right_detection,
                                  lres, rres,
                                  cancel_);

            if (!cancel_.load()) {
                result.left            = std::move(lres.audio);
                result.right           = std::move(rres.audio);
                result.clicks_repaired = lres.clicks_repaired
                                       + rres.clicks_repaired;
                result.success         = true;
            }
        } else {
            needledropper::RepairResult res;
            res = repair_.repair_mono(item.left.data(), n,
                                      item.left_detection,
                                      cancel_);
            if (!cancel_.load()) {
                result.left            = std::move(res.audio);
                result.clicks_repaired = res.clicks_repaired;
                result.success         = true;
            }
        }
    } catch (const std::exception& e) {
        result.success       = false;
        result.error_message = e.what();
    }

    report(1.0f);

    if (!cancel_.load() && on_complete) {
        juce::MessageManager::callAsync(
            [cb = on_complete, r = std::move(result)]() mutable {
                cb(std::move(r));
            });
    }
}

} // namespace needledropper
