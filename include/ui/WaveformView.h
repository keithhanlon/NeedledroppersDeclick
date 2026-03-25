#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include "dsp/ClickDetector.h"

namespace declick {

class WaveformView : public juce::Component,
                     public juce::ChangeListener {
public:
    WaveformView();
    ~WaveformView() override;

    // Load audio into the waveform display.
    void set_source(const std::vector<double>& left,
                    const std::vector<double>& right,
                    bool                       is_stereo,
                    double                     sample_rate);

    // Update click markers from detection results.
    void set_clicks(const std::vector<ClickEvent>& left_clicks,
                    const std::vector<ClickEvent>& right_clicks);

    // Clear audio and markers.
    void clear();

    // Playback position 0.0 - 1.0 — drives the playhead indicator.
    void set_playback_position(double position);

    // Toggle between original and processed waveform display.
    void show_processed(bool show);

    // juce::Component overrides.
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // Called when user scrubs — owner should seek playback.
    std::function<void(double position)> on_seek;

    // juce::ChangeListener — AudioThumbnail notifies when ready.
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    juce::AudioFormatManager                format_manager_;
    juce::AudioThumbnailCache               thumbnail_cache_;
    std::unique_ptr<juce::AudioThumbnail>   thumbnail_;

    std::vector<ClickEvent>  left_clicks_;
    std::vector<ClickEvent>  right_clicks_;
    double                   playback_position_ { 0.0 };
    double                   sample_rate_       { 44100.0 };
    int                      total_samples_     { 0 };
    bool                     show_processed_    { false };
    bool                     is_stereo_         { false };

    // Convert sample index to x pixel coordinate.
    float sample_to_x(int sample) const;

    // Draw click markers as vertical overlays.
    void draw_click_markers(juce::Graphics& g, bool is_left_channel);

    // Draw playhead line.
    void draw_playhead(juce::Graphics& g);
};

} // namespace declick
