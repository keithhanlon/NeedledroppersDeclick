#include "ui/StatusBar.h"

namespace needledropper {

StatusBar::StatusBar() {
    addAndMakeVisible(message_label_);
    addAndMakeVisible(stats_label_);
    addAndMakeVisible(click_count_label_);

    message_label_.setFont(juce::FontOptions(11.0f));
    message_label_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    stats_label_.setFont(juce::FontOptions(11.0f));
    stats_label_.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    stats_label_.setJustificationType(juce::Justification::centredRight);

    click_count_label_.setFont(juce::FontOptions(11.0f));
    click_count_label_.setColour(juce::Label::textColourId, juce::Colour(0xffff8888));
    click_count_label_.setJustificationType(juce::Justification::centredRight);
}

void StatusBar::set_message(const juce::String& m) {
    message_ = m;
    message_label_.setText(m, juce::dontSendNotification);
}

void StatusBar::set_click_count(int n) {
    click_count_ = n;
    click_count_label_.setText(juce::String(n) + " clicks",
                                juce::dontSendNotification);
}

void StatusBar::set_progress(float p)  { progress_ = p; repaint(); }

void StatusBar::set_sample_rate(double sr) {
    sample_rate_ = sr;
    update_stats_label();
}

void StatusBar::set_bit_depth(int b) {
    bit_depth_ = b;
    update_stats_label();
}
void StatusBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111));
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(getLocalBounds(), 1);
}

void StatusBar::resized()
{
    auto area = getLocalBounds().reduced(4, 0);
    click_count_label_.setBounds(area.removeFromRight(120));
    stats_label_.setBounds(area.removeFromRight(160));
    message_label_.setBounds(area);
}

void StatusBar::update_stats_label() {
    if (sample_rate_ > 0.0) {
        juce::String s = juce::String(static_cast<int>(sample_rate_ / 1000))
                       + "kHz / "
                       + juce::String(bit_depth_)
                       + "-bit";
        stats_label_.setText(s, juce::dontSendNotification);
    }
}

} // namespace needledropper
