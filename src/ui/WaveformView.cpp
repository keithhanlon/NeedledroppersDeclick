#include "ui/WaveformView.h"

namespace needledropper {

WaveformView::WaveformView()
    : thumbnail_cache_(5) {
    format_manager_.registerBasicFormats();
}

WaveformView::~WaveformView() {}

void WaveformView::set_source(const std::vector<double>&,
                              const std::vector<double>&,
                              bool, double) {}
void WaveformView::set_clicks(const std::vector<ClickEvent>&,
                              const std::vector<ClickEvent>&) {}
void WaveformView::clear()                               {}
void WaveformView::set_playback_position(double p)       { playback_position_ = p; repaint(); }
void WaveformView::show_processed(bool s)                { show_processed_ = s; repaint(); }
void WaveformView::paint(juce::Graphics& g)              { g.fillAll(juce::Colours::black); }
void WaveformView::resized()                             {}
void WaveformView::mouseDown(const juce::MouseEvent&)    {}
void WaveformView::mouseDrag(const juce::MouseEvent&)    {}
void WaveformView::changeListenerCallback(juce::ChangeBroadcaster*) {}
float WaveformView::sample_to_x(int) const               { return 0.0f; }
void WaveformView::draw_click_markers(juce::Graphics&, bool) {}
void WaveformView::draw_playhead(juce::Graphics&)        {}

} // namespace needledropper
