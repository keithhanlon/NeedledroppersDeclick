#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace needledropper {

class ParameterPanel : public juce::Component,
                       public juce::Slider::Listener,
                       public juce::Button::Listener,
                       public juce::Timer {
public:
    ParameterPanel();
    ~ParameterPanel() override;

    double sensitivity()          const { return sensitivity_.load(); }
    double crackle_sensitivity()  const { return crackle_sensitivity_.load(); }
    bool   reverse_enabled()      const { return reverse_enabled_; }

    // Convenience getters for ProcessingThread
    double getSensitivity()         const;
    double getCrackleSensitivity()  const;

    // Callbacks — owner wires these up.
    std::function<void(double)> on_sensitivity_changed;
    std::function<void(bool)>   on_reverse_toggled;
    std::function<void()>       on_process_clicked;

    // juce::Component overrides.
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::Slider::Listener.
    void sliderValueChanged(juce::Slider* slider) override;

    // juce::Button::Listener.
    void buttonClicked(juce::Button* button) override;

    // juce::Timer — debounce for sensitivity slider.
    void timerCallback() override;

private:
    juce::Slider      sensitivity_slider_;
    juce::Label       sensitivity_label_;
    juce::Slider      crackle_slider_;
    juce::Label       crackle_label_;
    juce::ToggleButton reverse_toggle_   { "Reverse pass" };
    juce::TextButton  process_button_    { "Process" };

    std::atomic<double> sensitivity_         { 30.0 };
    std::atomic<double> crackle_sensitivity_ { 0.0 };
    bool                reverse_enabled_     { true };

    static constexpr int DEBOUNCE_MS = 150;
};

} // namespace needledropper
