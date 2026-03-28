#include "MainComponent.h"

namespace needledropper {

MainComponent::MainComponent()
    : wavelet_engine_(5)
    , click_detector_(wavelet_engine_)
    , repair_engine_(wavelet_engine_)
    , detection_thread_(wavelet_engine_, click_detector_)
    , processing_thread_(wavelet_engine_, repair_engine_)
{
    setSize(960, 620);

    addAndMakeVisible(waveform_view_);
    addAndMakeVisible(transport_bar_);
    addAndMakeVisible(parameter_panel_);
    addAndMakeVisible(batch_queue_);
    addAndMakeVisible(status_bar_);

    format_manager_.registerBasicFormats();
    device_manager_.initialiseWithDefaultDevices(0, 2);
    source_player_.setSource(&transport_source_);
    device_manager_.addAudioCallback(&source_player_);
    startTimerHz(30);  // 30fps playhead updates

    connect_callbacks();
}

MainComponent::~MainComponent()
{
    // Disconnect transport first, before any sources are destroyed
    transport_source_.stop();
    transport_source_.setSource(nullptr);
    device_manager_.removeAudioCallback(&source_player_);
    source_player_.setSource(nullptr);

    // Explicitly release sources in reverse construction order
    delta_source_.reset();
    processed_source_.reset();
    original_source_.reset();
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Status bar at bottom
    status_bar_.setBounds(area.removeFromBottom(24));

    // Transport bar above status
    transport_bar_.setBounds(area.removeFromBottom(44));

    // Right panel: parameter panel + batch queue
    auto right = area.removeFromRight(220);
    parameter_panel_.setBounds(right.removeFromTop(180));
    batch_queue_.setBounds(right);

    // Remaining area: waveform view
    waveform_view_.setBounds(area);
}

// ─── File drag and drop ───────────────────────────────────────────────────────

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files) {
        juce::File file(f);
        if (file.hasFileExtension("wav;aiff;aif;flac;caf"))
            return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> audio_files;
    for (auto& f : files) {
        juce::File file(f);
        if (file.hasFileExtension("wav;aiff;aif;flac;caf"))
            audio_files.add(file);
    }

    if (audio_files.isEmpty()) return;

    batch_queue_.add_files(audio_files);

    // Load the first dropped file into the waveform view
    load_file(audio_files[0]);
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

void MainComponent::connect_callbacks()
{
    // Sensitivity slider triggers detection re-run
    parameter_panel_.on_sensitivity_changed = [this](double s) {
        if (audio_left_.empty()) return;

        // Fast recount from cached prediction errors if available
        if (!last_detection_.left.pred_error.empty()) {
            const double threshold = needledropper::ClickDetector::sensitivity_to_k(s);
            const int warmup = static_cast<int>(sample_rate_ * 0.10);
            const int left_count  = needledropper::ClickDetector::recount_clicks(
                last_detection_.left,  threshold, warmup);
            const int right_count = is_stereo_
                ? needledropper::ClickDetector::recount_clicks(
                    last_detection_.right, threshold, warmup)
                : 0;
            status_bar_.set_click_count(left_count + right_count);
        }

        // Full detection run (debounced) to update waveform markers
        status_bar_.set_message("Detecting...");
        detection_thread_.submit(
            audio_left_, audio_right_, is_stereo_,
            s,
            parameter_panel_.crackle_sensitivity(),
            sample_rate_,
            parameter_panel_.reverse_enabled(),
            [this](DetectionResults r) {
                on_detection_complete(std::move(r));
            });
    };

    // Process button triggers repair
    parameter_panel_.on_process_clicked = [this] {
        if (audio_left_.empty()) return;

        // Build batch from queue items that are Ready
        std::vector<BatchItem> batch;
        for (int i = 0; i < batch_queue_.size(); ++i) {
            const auto& item = batch_queue_.item(i);
            if (item.state == QueueItemState::Ready ||
                item.state == QueueItemState::Detecting ||
                item.state == QueueItemState::Done) {
                BatchItem bi;
                bi.source_file      = item.file;
                bi.left             = audio_left_;
                bi.right            = audio_right_;
                bi.is_stereo        = is_stereo_;
                bi.sensitivity      = parameter_panel_.sensitivity();
                bi.left_detection   = last_detection_.left;
                bi.right_detection  = last_detection_.right;
                bi.mono_output      = parameter_panel_.getMonoOutput();
                bi.sample_rate      = sample_rate_;
                batch_queue_.set_item_state(i, QueueItemState::Processing);
                batch.push_back(std::move(bi));
            }
        }

        if (batch.empty()) return;

        status_bar_.set_message("Processing...");
        status_bar_.set_progress(0.0f);

        processing_thread_.submit_batch(
            std::move(batch),
            [this](ProcessingProgress p) {
                on_processing_progress(p);
            },
            [this](ProcessingComplete c) {
                on_processing_complete(std::move(c));
            });
    };

    // Waveform seek
    waveform_view_.on_seek = [this](double pos) {
        transport_source_.setPosition(pos * transport_source_.getLengthInSeconds());
    };

    // Transport play/stop
    transport_bar_.on_play = [this] {
        // If at end of file, restart from beginning
        if (transport_source_.getLengthInSeconds() > 0 &&
            transport_source_.getCurrentPosition() >=
            transport_source_.getLengthInSeconds() - 0.1)
            transport_source_.setPosition(0.0);
        transport_source_.start();
    };
    transport_bar_.on_stop = [this] {
        transport_source_.stop();
        transport_source_.setPosition(0.0);
    };

    // A / B / Delta mode switching
    transport_bar_.on_mode_changed = [this](TransportBar::Mode mode) {
        // Guard: don't switch to a mode whose source doesn't exist yet
        if (mode == TransportBar::Mode::Processed && !processed_source_) return;
        if (mode == TransportBar::Mode::Delta    && !delta_source_)     return;
        if (mode == TransportBar::Mode::Original && !original_source_)  return;

        const bool was_playing = transport_source_.isPlaying();
        // Disconnect completely before querying or switching anything
        transport_source_.stop();
        transport_source_.setSource(nullptr, 0, nullptr, sample_rate_);

        waveform_view_.show_processed(mode == TransportBar::Mode::Processed);

        if (mode == TransportBar::Mode::Processed)
            transport_source_.setSource(processed_source_.get(),
                                        0, nullptr, sample_rate_);
        else if (mode == TransportBar::Mode::Delta)
            transport_source_.setSource(delta_source_.get(),
                                        0, nullptr, sample_rate_);
        else
            transport_source_.setSource(original_source_.get(),
                                        0, nullptr, sample_rate_);

        // Always restart from beginning when switching modes
        transport_source_.setPosition(0.0);
        if (was_playing) transport_source_.start();
    };

    // Batch queue selection — load selected file into waveform view
    batch_queue_.on_selection_changed = [this](int index) {
        if (index >= 0 && index < batch_queue_.size())
            load_file(batch_queue_.item(index).file);
    };
}

// ─── File loading ─────────────────────────────────────────────────────────────

void MainComponent::load_file(const juce::File& file)
{
    auto* reader = format_manager_.createReaderFor(file);
    if (!reader) {
        status_bar_.set_message("Could not read: " + file.getFileName());
        return;
    }

    const int n  = static_cast<int>(reader->lengthInSamples);
    sample_rate_ = reader->sampleRate;
    bit_depth_   = reader->bitsPerSample;
    is_stereo_   = reader->numChannels >= 2;

    // Read audio data into double buffers
    juce::AudioBuffer<float> buf(reader->numChannels, n);
    reader->read(&buf, 0, n, 0, true, true);

    audio_left_.resize(n);
    for (int i = 0; i < n; ++i)
        audio_left_[i] = buf.getSample(0, i);

    if (is_stereo_) {
        audio_right_.resize(n);
        for (int i = 0; i < n; ++i)
            audio_right_[i] = buf.getSample(1, i);
    } else {
        audio_right_.clear();
    }

    // Display waveform
    waveform_view_.set_source(audio_left_, audio_right_,
                               is_stereo_, sample_rate_);

    // Update status bar
    status_bar_.set_sample_rate(sample_rate_);
    status_bar_.set_bit_depth(bit_depth_);
    status_bar_.set_message("Loaded: " + file.getFileName());

    // Set up playback — reader now owned by AudioFormatReaderSource
    original_source_ = std::make_unique<juce::AudioFormatReaderSource>(
        reader, true);
    transport_source_.setSource(original_source_.get(),
                                0, nullptr, sample_rate_);
    transport_bar_.set_playback_position(0.0,
        static_cast<double>(n) / sample_rate_);
    transport_bar_.set_processed_available(false);

    // Mark queue item as Detecting
    for (int i = 0; i < batch_queue_.size(); ++i)
        if (batch_queue_.item(i).file == file)
            batch_queue_.set_item_state(i, QueueItemState::Detecting);

    // Run initial detection at current sensitivity
    status_bar_.set_message("Detecting...");
    detection_thread_.submit(
        audio_left_, audio_right_, is_stereo_,
        parameter_panel_.sensitivity(),
        parameter_panel_.crackle_sensitivity(),
        sample_rate_,
        parameter_panel_.reverse_enabled(),
        [this](DetectionResults r) {
            on_detection_complete(std::move(r));
        });
}

// ─── Detection complete ───────────────────────────────────────────────────────

void MainComponent::on_detection_complete(DetectionResults results)
{
    last_detection_ = results;
    // Update waveform with click markers
    waveform_view_.set_clicks(results.left.clicks, results.right.clicks);

    // Count total clicks
    const int click_total = static_cast<int>(
        results.left.clicks.size() + results.right.clicks.size());
    const int crackle_total = static_cast<int>(
        results.left.crackle_clicks.size() + results.right.crackle_clicks.size());
    status_bar_.set_click_count(click_total + crackle_total);
    status_bar_.set_progress(-1.0f);

    // Mark current queue item as Ready
    for (int i = 0; i < batch_queue_.size(); ++i)
        if (batch_queue_.item(i).state == QueueItemState::Detecting)
            batch_queue_.set_item_state(i, QueueItemState::Ready);

    status_bar_.set_message(juce::String(click_total + crackle_total) + " clicks detected");
}

// ─── Processing progress ──────────────────────────────────────────────────────

void MainComponent::on_processing_progress(ProcessingProgress progress)
{
    status_bar_.set_progress(progress.file_progress);
    status_bar_.set_message("Processing file "
        + juce::String(progress.current_file)
        + " of "
        + juce::String(progress.total_files)
        + "...");
}

// ─── Processing complete ──────────────────────────────────────────────────────

void MainComponent::on_processing_complete(ProcessingComplete result)
{
    if (!result.success) {
        status_bar_.set_message("Error: " + result.error_message);
        return;
    }

    // Update queue item state
    for (int i = 0; i < batch_queue_.size(); ++i)
        if (batch_queue_.item(i).file == result.source_file) {
            batch_queue_.set_item_state(i, QueueItemState::Done);
            batch_queue_.set_item_clicks(i, result.clicks_repaired);
        }

    // Write output file
    if (write_output(result)) {
        status_bar_.set_message("Done — "
            + juce::String(result.clicks_repaired)
            + " clicks repaired: "
            + result.source_file.getFileName());
    }

    // Load repaired file into processed_source_ for A/B audition
    const juce::File out_file = result.source_file.getSiblingFile(
        result.source_file.getFileNameWithoutExtension()
        + "-repaired."
        + result.source_file.getFileExtension().trimCharactersAtStart("."));

    auto* proc_reader = format_manager_.createReaderFor(out_file);
    if (proc_reader) {
        processed_source_ = std::make_unique<juce::AudioFormatReaderSource>(
            proc_reader, true);
    }

    // Disconnect transport before resetting any sources to avoid dangling pointers
    transport_source_.stop();
    transport_source_.setSource(nullptr, 0, nullptr, sample_rate_);

    // Build delta (original - repaired) and write to temp file
    delta_source_.reset();
    processed_source_.reset();
    if (result.success && !audio_left_.empty() && !result.left.empty()) {
        const int n_delta = static_cast<int>(
            std::min(audio_left_.size(), result.left.size()));
        const juce::File delta_file = result.source_file.getSiblingFile(
            result.source_file.getFileNameWithoutExtension()
            + "-delta."
            + result.source_file.getFileExtension().trimCharactersAtStart("."));

        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::WavAudioFormat wav;
        if (auto out = std::unique_ptr<juce::OutputStream>(
                delta_file.createOutputStream())) {
            auto* w = wav.createWriterFor(
                out.get(),
                sample_rate_,
                is_stereo_ ? 2 : 1,
                24, {}, 0);
            if (w) { out.release(); writer.reset(w); }
        }
        if (writer) {
            juce::AudioBuffer<float> delta_buf(is_stereo_ ? 2 : 1, n_delta);
            const auto& right_orig    = is_stereo_ ? audio_right_ : audio_left_;
            const auto& right_repair  = is_stereo_ ? result.right  : result.left;
            for (int i = 0; i < n_delta; ++i) {
                delta_buf.setSample(0, i,
                    static_cast<float>(audio_left_[i] - result.left[i]));
                if (is_stereo_ && i < (int)right_orig.size()
                               && i < (int)right_repair.size())
                    delta_buf.setSample(1, i,
                        static_cast<float>(right_orig[i] - right_repair[i]));
            }
            writer->writeFromAudioSampleBuffer(delta_buf, 0, n_delta);
            writer.reset();

            auto* delta_reader = format_manager_.createReaderFor(delta_file);
            if (delta_reader)
                delta_source_ = std::make_unique<juce::AudioFormatReaderSource>(
                    delta_reader, true);
        }
    }

    // Reconnect transport to original source
    if (original_source_)
        transport_source_.setSource(original_source_.get(),
                                    0, nullptr, sample_rate_);
    transport_source_.setPosition(0.0);

    // Enable A/B/Delta audition
    transport_bar_.set_processed_available(proc_reader != nullptr);
    status_bar_.set_progress(-1.0f);
}

// ─── Write output ─────────────────────────────────────────────────────────────

bool MainComponent::write_output(const ProcessingComplete& result)
{
    // Output file: same folder as source, "-repaired" suffix
    const juce::File out_file = result.source_file.getSiblingFile(
        result.source_file.getFileNameWithoutExtension()
        + "-repaired."
        + result.source_file.getFileExtension().trimCharactersAtStart("."));

    juce::WavAudioFormat wav_format;
    std::unique_ptr<juce::OutputStream> fos(
        out_file.createOutputStream());
    if (!fos) return false;
    const int n_channels = result.is_stereo ? 2 : 1;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav_format.createWriterFor(
            fos,
            juce::AudioFormatWriterOptions{}
                .withSampleRate(sample_rate_)
                .withNumChannels(n_channels)
                .withBitsPerSample(bit_depth_)));
    if (!writer) return false;

    const int n = static_cast<int>(result.left.size());
    juce::AudioBuffer<float> buf(n_channels, n);

    for (int i = 0; i < n; ++i)
        buf.setSample(0, i, static_cast<float>(result.left[i]));

    if (result.is_stereo)
        for (int i = 0; i < n; ++i)
            buf.setSample(1, i, static_cast<float>(result.right[i]));

    writer->writeFromAudioSampleBuffer(buf, 0, n);
    return true;
}

void MainComponent::timerCallback()
{
    if (transport_source_.isPlaying()) {
        const double pos     = transport_source_.getCurrentPosition();
        const double total   = transport_source_.getLengthInSeconds();
        waveform_view_.set_playback_position(total > 0.0 ? pos / total : 0.0);
        transport_bar_.set_playback_position(pos, total);
    }
}

} // namespace needledropper
