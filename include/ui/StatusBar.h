#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace declick {

class StatusBar : public juce::Component {
public:
    StatusBar();

    // Update display.
    void set_message(const juce::String& message);
    void set_click_count(int count);
    void set_progress(float progress);   // 0.0 - 1.0, <0 = hide bar
    void set_sample_rate(double sr);
    void set_bit_depth(int bits);

    // juce::Component overrides.
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label  message_label_;
    juce::Label  stats_label_;
    juce::Label  click_count_label_;

    juce::String message_;
    int          click_count_  { 0 };
    float        progress_     { -1.0f };
    double       sample_rate_  { 0.0 };
    int          bit_depth_    { 0 };
};

} // namespace declick
