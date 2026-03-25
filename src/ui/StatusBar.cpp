#include "ui/StatusBar.h"

namespace declick {

StatusBar::StatusBar() {
    addAndMakeVisible(message_label_);
    addAndMakeVisible(stats_label_);
    addAndMakeVisible(click_count_label_);
}

void StatusBar::set_message(const juce::String& m)  { message_ = m; repaint(); }
void StatusBar::set_click_count(int n)               { click_count_ = n; repaint(); }
void StatusBar::set_progress(float p)                { progress_ = p; repaint(); }
void StatusBar::set_sample_rate(double sr)           { sample_rate_ = sr; repaint(); }
void StatusBar::set_bit_depth(int b)                 { bit_depth_ = b; repaint(); }
void StatusBar::paint(juce::Graphics& g)             { g.fillAll(juce::Colours::black); }
void StatusBar::resized()                            {}

} // namespace declick
