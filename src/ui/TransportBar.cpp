#include "ui/TransportBar.h"

namespace needledropper {

TransportBar::TransportBar()
{
    addAndMakeVisible(play_button_);
    addAndMakeVisible(stop_button_);
    addAndMakeVisible(ab_toggle_);
    addAndMakeVisible(position_slider_);
    addAndMakeVisible(position_label_);

    play_button_.addListener(this);
    stop_button_.addListener(this);
    ab_toggle_.addListener(this);

    position_slider_.setRange(0.0, 1.0);
    position_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
    position_slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    position_slider_.addListener(this);

    position_label_.setFont(juce::FontOptions(12.0f));
    position_label_.setColour(juce::Label::textColourId,
                               juce::Colours::lightgrey);
    position_label_.setText("0:00 / 0:00", juce::dontSendNotification);

    ab_toggle_.setButtonText("A/B");
    ab_toggle_.setEnabled(false);

    stop_button_.setEnabled(false);
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::set_sources(juce::AudioFormatReaderSource* original,
                                juce::AudioFormatReaderSource* processed)
{
    // Sources are owned by MainComponent — we just reference them
    juce::ignoreUnused(original, processed);
}

void TransportBar::set_playback_position(double position_seconds,
                                          double total_seconds)
{
    if (total_seconds > 0.0)
        position_slider_.setValue(position_seconds / total_seconds,
                                  juce::dontSendNotification);

    // Format as M:SS / M:SS
    auto fmt = [](double s) -> juce::String {
        const int mins = static_cast<int>(s) / 60;
        const int secs = static_cast<int>(s) % 60;
        return juce::String(mins) + ":"
             + juce::String(secs).paddedLeft('0', 2);
    };

    position_label_.setText(fmt(position_seconds) + " / " +
                             fmt(total_seconds),
                             juce::dontSendNotification);
}

void TransportBar::set_processed_available(bool available)
{
    processed_available_ = available;
    ab_toggle_.setEnabled(available);
    if (!available)
        ab_toggle_.setToggleState(false, juce::dontSendNotification);
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff222222));
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(getLocalBounds(), 1);
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(4);

    play_button_.setBounds(area.removeFromLeft(56));
    area.removeFromLeft(4);
    stop_button_.setBounds(area.removeFromLeft(56));
    area.removeFromLeft(8);
    ab_toggle_.setBounds(area.removeFromLeft(48));
    area.removeFromLeft(8);
    position_label_.setBounds(area.removeFromRight(100));
    position_slider_.setBounds(area);
}

void TransportBar::buttonClicked(juce::Button* button)
{
    if (button == &play_button_) {
        is_playing_ = true;
        play_button_.setEnabled(false);
        stop_button_.setEnabled(true);
        startTimerHz(30);   // 30fps position updates
        if (on_play) on_play();
    }
    else if (button == &stop_button_) {
        is_playing_ = false;
        play_button_.setEnabled(true);
        stop_button_.setEnabled(false);
        stopTimer();
        if (on_stop) on_stop();
    }
    else if (button == &ab_toggle_) {
        if (on_ab_toggle)
            on_ab_toggle(ab_toggle_.getToggleState());
    }
}

void TransportBar::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &position_slider_)
        if (on_seek) on_seek(slider->getValue());
}

void TransportBar::timerCallback()
{
    // MainComponent drives position updates via set_playback_position()
    // Timer here is a hook for future direct transport polling if needed
}

} // namespace needledropper
