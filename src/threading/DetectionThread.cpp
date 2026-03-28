// Needledropper's Declicker
// Copyright (c) 2025 Keith Hanlon
// SPDX-License-Identifier: AGPL-3.0-only
//
// This file is original work. No code has been copied or derived from
// any existing software including ClickRepair or any other application.
// Algorithms used are based on published academic literature.

#include "threading/DetectionThread.h"
#include <juce_events/juce_events.h>
#include <algorithm>

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
                             double crackle_sensitivity,
                             double sample_rate,
                             bool reverse_enabled,
                             CompletionCallback on_complete)
{
    {
        juce::ScopedLock sl(lock_);
        audio_left_          = left;
        audio_right_         = right;
        is_stereo_           = is_stereo;
        sensitivity_.store(sensitivity);
        crackle_sensitivity_.store(crackle_sensitivity);
        sample_rate_.store(sample_rate);
        reverse_enabled_.store(reverse_enabled);
        on_complete_         = std::move(on_complete);
        work_pending_        = true;
    }
    cancel_.store(true);
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

// Merge click events from a reversed detection pass into a forward event list.
// rev_clicks are in reversed-audio coordinates — flip them back to forward.
static void merge_reversed_clicks(std::vector<ClickEvent>& fwd,
                                   const std::vector<ClickEvent>& rev_clicks,
                                   int n)
{
    for (const auto& ev : rev_clicks) {
        const int fwd_start = n - ev.sample_end;
        const int fwd_end   = n - ev.sample_start;
        if (fwd_start < 0 || fwd_end > n) continue;

        // Skip if already covered by a forward event
        bool covered = false;
        for (const auto& fev : fwd) {
            if (fev.sample_start <= fwd_end &&
                fev.sample_end   >= fwd_start) {
                covered = true;
                break;
            }
        }
        if (!covered)
            fwd.push_back({ fwd_start, fwd_end, ev.level, ev.confidence });
    }

    std::sort(fwd.begin(), fwd.end(),
        [](const ClickEvent& a, const ClickEvent& b) {
            return a.sample_start < b.sample_start;
        });
}

void DetectionThread::run()
{
    while (!threadShouldExit()) {
        work_ready_.wait(-1);

        if (threadShouldExit()) break;

        std::vector<double> left, right;
        bool   is_stereo;
        bool   reverse_enabled;
        double sensitivity;
        double crackle_sensitivity;
        double sample_rate;
        CompletionCallback callback;

        {
            juce::ScopedLock sl(lock_);
            if (!work_pending_) continue;
            left                = audio_left_;
            right               = audio_right_;
            is_stereo           = is_stereo_;
            sensitivity         = sensitivity_.load();
            crackle_sensitivity = crackle_sensitivity_.load();
            sample_rate         = sample_rate_.load();
            reverse_enabled     = reverse_enabled_.load();
            callback            = on_complete_;
            work_pending_       = false;
        }

        cancel_.store(false);

        DetectionResults results;
        results.sensitivity = sensitivity;
        results.is_stereo   = is_stereo;

        // Forward detection pass
        if (is_stereo && !right.empty()) {
            detector_.detect_stereo(left.data(), right.data(),
                                    static_cast<int>(left.size()),
                                    sensitivity, crackle_sensitivity,
                                    sample_rate,
                                    results.left, results.right,
                                    cancel_);
        } else {
            results.left = detector_.detect_mono(left.data(),
                                                  static_cast<int>(left.size()),
                                                  sensitivity, crackle_sensitivity,
                                                  sample_rate,
                                                  cancel_);
        }

        // Reverse pass: detect on time-reversed audio, flip positions back,
        // merge with forward results. Catches asymmetric clicks whose sharp
        // edge faces the wrong direction for the forward AR predictor.
        if (reverse_enabled && !cancel_.load()) {
            std::atomic<bool> rev_cancel { false };
            const int n_left  = static_cast<int>(left.size());
            const int n_right = static_cast<int>(right.size());

            // Reverse L channel and detect
            std::vector<double> rev_left(left.rbegin(), left.rend());
            ChannelDetection rev_left_det = detector_.detect_mono(
                rev_left.data(), n_left,
                sensitivity, crackle_sensitivity,
                sample_rate, rev_cancel);
            merge_reversed_clicks(results.left.clicks,
                                   rev_left_det.clicks, n_left);

            // Reverse R channel and detect (stereo only)
            if (is_stereo && !right.empty()) {
                std::vector<double> rev_right(right.rbegin(), right.rend());
                ChannelDetection rev_right_det = detector_.detect_mono(
                    rev_right.data(), n_right,
                    sensitivity, crackle_sensitivity,
                    sample_rate, rev_cancel);
                merge_reversed_clicks(results.right.clicks,
                                       rev_right_det.clicks, n_right);
            }
        }

        if (!cancel_.load() && !threadShouldExit() && callback) {
            juce::MessageManager::callAsync(
                [cb = std::move(callback), r = std::move(results)]() mutable {
                    cb(std::move(r));
                });
        }
    }
}

} // namespace needledropper
