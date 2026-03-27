#include "ui/ParameterPanel.h"

namespace needledropper {

ParameterPanel::ParameterPanel() {
    addAndMakeVisible(sensitivity_label_);
    addAndMakeVisible(sensitivity_slider_);
    addAndMakeVisible(crackle_label_);
    addAndMakeVisible(crackle_slider_);
    addAndMakeVisible(reverse_toggle_);
    addAndMakeVisible(process_button_);

    sensitivity_slider_.addListener(this);
    crackle_slider_.addListener(this);
    reverse_toggle_.addListener(this);
    process_button_.addListener(this);

    sensitivity_slider_.setRange(0.0, 100.0, 1.0);
    sensitivity_slider_.setValue(30.0);
    sensitivity_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);

    crackle_slider_.setRange(0.0, 100.0, 1.0);
    crackle_slider_.setValue(0.0);
    crackle_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
}

ParameterPanel::~ParameterPanel() {}

void ParameterPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff222222));
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("DeClick",   8,  8, 100, 16, juce::Justification::centredLeft);
    g.drawText("DeCrackle", 8, 52, 100, 16, juce::Justification::centredLeft);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    area.removeFromTop(24);
    sensitivity_slider_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    area.removeFromTop(16);
    crackle_slider_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    reverse_toggle_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    process_button_.setBounds(area.removeFromTop(36));
}

void ParameterPanel::sliderValueChanged(juce::Slider* s)
{
    if (s == &sensitivity_slider_) {
        sensitivity_.store(s->getValue());
        startTimer(DEBOUNCE_MS);
    }
    if (s == &crackle_slider_) {
        crackle_sensitivity_.store(s->getValue());
        startTimer(DEBOUNCE_MS);
    }
}

void ParameterPanel::buttonClicked(juce::Button* b)
{
    if (b == &reverse_toggle_)
        reverse_enabled_ = reverse_toggle_.getToggleState();
    if (b == &process_button_ && on_process_clicked)
        on_process_clicked();
}

void ParameterPanel::timerCallback()
{
    stopTimer();
    if (on_sensitivity_changed)
        on_sensitivity_changed(sensitivity_.load());
}

double ParameterPanel::getSensitivity() const
{
    return sensitivity_slider_.getValue();
}

double ParameterPanel::getCrackleSensitivity() const
{
    return crackle_slider_.getValue();
}

} // namespace needledropper
