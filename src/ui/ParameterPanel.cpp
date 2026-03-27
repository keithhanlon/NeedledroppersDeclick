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
    sensitivity_slider_.setValue(30.0);
}

ParameterPanel::~ParameterPanel() {}

void ParameterPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff222222));
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Sensitivity", 8, 8, 100, 16, juce::Justification::centredLeft);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Sensitivity label + slider
    area.removeFromTop(24);  // space for label drawn in paint()
    sensitivity_slider_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    // Reverse toggle
    reverse_toggle_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    // Process button — prominent at bottom of panel
    process_button_.setBounds(area.removeFromTop(36));
}
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
