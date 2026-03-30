#include "ui/ParameterPanel.h"

namespace needledropper {

ParameterPanel::ParameterPanel() {
    addAndMakeVisible(sensitivity_label_);
    addAndMakeVisible(sensitivity_value_label_);
    addAndMakeVisible(sensitivity_slider_);
    addAndMakeVisible(crackle_label_);
    addAndMakeVisible(crackle_value_label_);
    addAndMakeVisible(crackle_slider_);
    addAndMakeVisible(reverse_toggle_);
    addAndMakeVisible(mono_toggle_);
    addAndMakeVisible(process_button_);

    sensitivity_slider_.addListener(this);
    crackle_slider_.addListener(this);
    reverse_toggle_.addListener(this);
    mono_toggle_.addListener(this);
    process_button_.addListener(this);

    sensitivity_slider_.setRange(0.0, 100.0, 1.0);
    sensitivity_slider_.setValue(30.0);
    sensitivity_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 20);

    crackle_slider_.setRange(0.0, 100.0, 1.0);
    crackle_slider_.setValue(0.0);
    crackle_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 20);

    auto setup_label = [](juce::Label& l, const juce::String& text, float size = 11.0f) {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::FontOptions(size));
        l.setColour(juce::Label::textColourId, juce::Colour(0xffbbbbbb));
        l.setJustificationType(juce::Justification::centredLeft);
    };

    setup_label(sensitivity_label_, "Click Sensitivity");
    setup_label(crackle_label_,     "Crackle Sensitivity");
    setup_label(sensitivity_value_label_, "Conservative", 10.0f);
    setup_label(crackle_value_label_,     "Off", 10.0f);

    sensitivity_value_label_.setJustificationType(juce::Justification::centredRight);
    crackle_value_label_.setJustificationType(juce::Justification::centredRight);
    sensitivity_value_label_.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    crackle_value_label_.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
}

ParameterPanel::~ParameterPanel() {}

void ParameterPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff222222));
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(getLocalBounds(), 1);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(6);

    // Click Sensitivity row
    auto label_row = area.removeFromTop(18);
    sensitivity_label_.setBounds(label_row.removeFromLeft(80));
    sensitivity_value_label_.setBounds(label_row);
    sensitivity_slider_.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    // Crackle Sensitivity row
    label_row = area.removeFromTop(18);
    crackle_label_.setBounds(label_row.removeFromLeft(80));
    crackle_value_label_.setBounds(label_row);
    crackle_slider_.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    reverse_toggle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(2);
    mono_toggle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    process_button_.setBounds(area.removeFromBottom(32));
}

static juce::String sensitivity_descriptor(double v) {
    if (v < 15)  return "Very conservative";
    if (v < 30)  return "Conservative";
    if (v < 50)  return "Moderate";
    if (v < 70)  return "Aggressive";
    return "Very aggressive";
}

static juce::String crackle_descriptor(double v) {
    if (v == 0)  return "Off";
    if (v < 25)  return "Light";
    if (v < 50)  return "Moderate";
    if (v < 75)  return "Heavy";
    return "Maximum";
}

void ParameterPanel::sliderValueChanged(juce::Slider* s)
{
    if (s == &sensitivity_slider_) {
        sensitivity_.store(s->getValue());
        sensitivity_value_label_.setText(
            sensitivity_descriptor(s->getValue()), juce::dontSendNotification);
        startTimer(DEBOUNCE_MS);
    }
    if (s == &crackle_slider_) {
        crackle_sensitivity_.store(s->getValue());
        crackle_value_label_.setText(
            crackle_descriptor(s->getValue()), juce::dontSendNotification);
        startTimer(DEBOUNCE_MS);
    }
}

void ParameterPanel::buttonClicked(juce::Button* b)
{
    if (b == &reverse_toggle_) {
        reverse_enabled_ = reverse_toggle_.getToggleState();
        if (on_sensitivity_changed)
            on_sensitivity_changed(sensitivity_.load());
    }
    if (b == &mono_toggle_)
        mono_output_ = mono_toggle_.getToggleState();
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
