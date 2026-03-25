#include "ui/TransportBar.h"

namespace needledropper {

TransportBar::TransportBar() {
    addAndMakeVisible(play_button_);
    addAndMakeVisible(stop_button_);
    addAndMakeVisible(ab_toggle_);
    addAndMakeVisible(position_slider_);
    addAndMakeVisible(position_label_);
    play_button_.addListener(this);
    stop_button_.addListener(this);
    ab_toggle_.addListener(this);
}

TransportBar::~TransportBar() {}

void TransportBar::set_sources(juce::AudioFormatReaderSource*,
                               juce::AudioFormatReaderSource*) {}
void TransportBar::set_playback_position(double, double)     {}
void TransportBar::set_processed_available(bool a)           { processed_available_ = a; }
void TransportBar::paint(juce::Graphics& g)                  { g.fillAll(juce::Colours::darkgrey); }
void TransportBar::resized()                                 {}
void TransportBar::buttonClicked(juce::Button*)              {}
void TransportBar::timerCallback()                           {}

} // namespace needledropper
