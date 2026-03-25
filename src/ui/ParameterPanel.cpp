#include "ui/ParameterPanel.h"

namespace needledropper {

ParameterPanel::ParameterPanel() {
    addAndMakeVisible(sensitivity_slider_);
    addAndMakeVisible(sensitivity_label_);
    addAndMakeVisible(reverse_toggle_);
    addAndMakeVisible(process_button_);
    sensitivity_slider_.addListener(this);
    reverse_toggle_.addListener(this);
    process_button_.addListener(this);
    sensitivity_slider_.setRange(0.0, 100.0);
    sensitivity_slider_.setValue(50.0);
}

ParameterPanel::~ParameterPanel() {}

void ParameterPanel::paint(juce::Graphics& g)        { g.fillAll(juce::Colours::darkgrey); }
void ParameterPanel::resized()                       {}
void ParameterPanel::sliderValueChanged(juce::Slider* s) {
    if (s == &sensitivity_slider_) {
        sensitivity_.store(s->getValue());
        startTimer(DEBOUNCE_MS);
    }
}
void ParameterPanel::buttonClicked(juce::Button* b) {
    if (b == &reverse_toggle_)  reverse_enabled_ = reverse_toggle_.getToggleState();
    if (b == &process_button_ && on_process_clicked) on_process_clicked();
}
void ParameterPanel::timerCallback() {
    stopTimer();
    if (on_sensitivity_changed) on_sensitivity_changed(sensitivity_.load());
}

} // namespace needledropper
