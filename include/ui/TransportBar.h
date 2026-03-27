#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace needledropper {

class TransportBar : public juce::Component,
                     public juce::Button::Listener,
                     public juce::Slider::Listener,
                     public juce::Timer {
public:
    TransportBar();
    ~TransportBar() override;

    // Bind audio sources for A/B/Delta toggle.
    void set_sources(juce::AudioFormatReaderSource* original,
                     juce::AudioFormatReaderSource* processed);

    // Called by owner to update position display.
    void set_playback_position(double position_seconds,
                               double total_seconds);

    // Enable/disable processed source availability.
    void set_processed_available(bool available);

    // Playback mode
    enum class Mode { Original, Processed, Delta };

    // Callbacks — owner wires these up.
    std::function<void()>          on_play;
    std::function<void()>          on_stop;
    std::function<void(Mode)>      on_mode_changed;
    std::function<void(double)>    on_seek;

    // juce::Component overrides.
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::Button::Listener.
    void buttonClicked(juce::Button* button) override;

    // juce::Timer — updates position display during playback.
    void timerCallback() override;
    void sliderValueChanged(juce::Slider* slider) override;

private:
    juce::TextButton   play_button_          { "Play" };
    juce::TextButton   stop_button_          { "Stop" };
    juce::ToggleButton a_button_             { "A" };
    juce::ToggleButton b_button_             { "B" };
    juce::ToggleButton delta_button_         { "Δ" };
    juce::Slider       position_slider_;
    juce::Label        position_label_;

    bool               is_playing_           { false };
    bool               processed_available_  { false };
    Mode               current_mode_         { Mode::Original };
};

} // namespace needledropper
