#include "ui/TransportBar.h"

namespace needledropper {

TransportBar::TransportBar()
{
    addAndMakeVisible(play_button_);
    addAndMakeVisible(stop_button_);
    addAndMakeVisible(a_button_);
    addAndMakeVisible(b_button_);
    addAndMakeVisible(delta_button_);
    addAndMakeVisible(position_slider_);
    addAndMakeVisible(position_label_);

    play_button_.addListener(this);
    stop_button_.addListener(this);
    a_button_.addListener(this);
    b_button_.addListener(this);
    delta_button_.addListener(this);

    position_slider_.setRange(0.0, 1.0);
    position_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
    position_slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    position_slider_.addListener(this);

    position_label_.setFont(juce::FontOptions(12.0f));
    position_label_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    position_label_.setText("0:00 / 0:00", juce::dontSendNotification);

    // A/B/Delta act as a radio group using TextButton toggle state
    a_button_.setClickingTogglesState(true);
    b_button_.setClickingTogglesState(true);
    delta_button_.setClickingTogglesState(true);

    a_button_.setRadioGroupId(1);
    b_button_.setRadioGroupId(1);
    delta_button_.setRadioGroupId(1);

    a_button_.setToggleState(true, juce::dontSendNotification);

    b_button_.setEnabled(false);
    delta_button_.setEnabled(false);
    stop_button_.setEnabled(false);
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::set_sources(juce::AudioFormatReaderSource* original,
                                juce::AudioFormatReaderSource* processed)
{
    juce::ignoreUnused(original, processed);
}

void TransportBar::set_playback_position(double position_seconds,
                                          double total_seconds)
{
    if (total_seconds > 0.0)
        position_slider_.setValue(position_seconds / total_seconds,
                                  juce::dontSendNotification);

    auto fmt = [](double s) -> juce::String {
        const int mins = static_cast<int>(s) / 60;
        const int secs = static_cast<int>(s) % 60;
        return juce::String(mins) + ":"
             + juce::String(secs).paddedLeft('0', 2);
    };

    position_label_.setText(fmt(position_seconds) + " / " + fmt(total_seconds),
                             juce::dontSendNotification);
}

void TransportBar::set_processed_available(bool available)
{
    processed_available_ = available;
    b_button_.setEnabled(available);
    delta_button_.setEnabled(available);

    if (!available) {
        a_button_.setToggleState(true, juce::sendNotification);
        current_mode_ = Mode::Original;
    }
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

    // IN / OUT / NOISE buttons grouped together
    a_button_.setBounds(area.removeFromLeft(44));
    area.removeFromLeft(2);
    b_button_.setBounds(area.removeFromLeft(44));
    area.removeFromLeft(2);
    delta_button_.setBounds(area.removeFromLeft(56));
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
        startTimerHz(30);
        if (on_play) on_play();
    }
    else if (button == &stop_button_) {
        is_playing_ = false;
        play_button_.setEnabled(true);
        stop_button_.setEnabled(false);
        stopTimer();
        if (on_stop) on_stop();
    }
    else if (button == &a_button_) {
        current_mode_ = Mode::Original;
        if (on_mode_changed) on_mode_changed(current_mode_);
    }
    else if (button == &b_button_) {
        current_mode_ = Mode::Processed;
        if (on_mode_changed) on_mode_changed(current_mode_);
    }
    else if (button == &delta_button_) {
        current_mode_ = Mode::Delta;
        if (on_mode_changed) on_mode_changed(current_mode_);
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
}

} // namespace needledropper
