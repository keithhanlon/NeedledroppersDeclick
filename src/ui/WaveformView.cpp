#include "ui/WaveformView.h"

namespace needledropper {

WaveformView::WaveformView()
    : thumbnail_cache_(5)
{
    format_manager_.registerBasicFormats();
    thumbnail_ = std::make_unique<juce::AudioThumbnail>(
        512, format_manager_, thumbnail_cache_);
    thumbnail_->addChangeListener(this);
}

WaveformView::~WaveformView()
{
    thumbnail_->removeChangeListener(this);
}

void WaveformView::set_source(const std::vector<double>& left,
                               const std::vector<double>& right,
                               bool is_stereo, double sample_rate)
{
    sample_rate_   = sample_rate;
    total_samples_ = static_cast<int>(left.size());
    is_stereo_     = is_stereo;

    // Convert double samples to float buffer for AudioThumbnail
    const int n_channels = is_stereo ? 2 : 1;
    juce::AudioBuffer<float> buf(n_channels, total_samples_);

    for (int i = 0; i < total_samples_; ++i)
        buf.setSample(0, i, static_cast<float>(left[i]));

    if (is_stereo && !right.empty())
        for (int i = 0; i < total_samples_; ++i)
            buf.setSample(1, i, static_cast<float>(right[i]));

    thumbnail_->reset(n_channels, sample_rate_, total_samples_);
    thumbnail_->addBlock(0, buf, 0, total_samples_);

    repaint();
}

void WaveformView::set_clicks(const std::vector<ClickEvent>& left_clicks,
                               const std::vector<ClickEvent>& right_clicks)
{
    left_clicks_  = left_clicks;
    right_clicks_ = right_clicks;
    repaint();
}

void WaveformView::clear()
{
    thumbnail_->clear();
    left_clicks_.clear();
    right_clicks_.clear();
    total_samples_ = 0;
    playback_position_ = 0.0;
    repaint();
}

void WaveformView::set_playback_position(double position)
{
    playback_position_ = position;
    repaint();
}

void WaveformView::show_processed(bool show)
{
    show_processed_ = show;
    repaint();
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void WaveformView::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Background
    g.fillAll(juce::Colour(0xff1a1a1a));

    if (thumbnail_->getTotalLength() <= 0.0) {
        g.setColour(juce::Colour(0xff444444));
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("Drop audio files here or use File menu",
                   bounds, juce::Justification::centred);
        return;
    }

    const double total_secs = thumbnail_->getTotalLength();

    if (is_stereo_) {
        // Draw L channel in top half, R channel in bottom half
        const auto top_half = bounds.withHeight(bounds.getHeight() * 0.5f);
        const auto bot_half = bounds.withY(bounds.getHeight() * 0.5f)
                                    .withHeight(bounds.getHeight() * 0.5f);

        g.setColour(juce::Colour(0xff2a6a9a));
        thumbnail_->drawChannel(g, top_half.toNearestInt(),
                                0.0, total_secs, 0, 1.0f);

        g.setColour(juce::Colour(0xff2a8a5a));
        thumbnail_->drawChannel(g, bot_half.toNearestInt(),
                                0.0, total_secs, 1, 1.0f);

        // Channel labels
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("L", top_half.withWidth(20), juce::Justification::centred);
        g.drawText("R", bot_half.withWidth(20), juce::Justification::centred);

        // Click markers
        draw_click_markers(g, true);
        draw_click_markers(g, false);
    } else {
        g.setColour(juce::Colour(0xff2a6a9a));
        thumbnail_->drawChannel(g, bounds.toNearestInt(),
                                0.0, total_secs, 0, 1.0f);
        draw_click_markers(g, true);
    }

    // A/B indicator
    if (show_processed_) {
        g.setColour(juce::Colour(0xff44aa44).withAlpha(0.15f));
        g.fillRect(bounds);
        g.setColour(juce::Colour(0xff44aa44));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("REPAIRED", bounds.withWidth(70).withX(bounds.getRight() - 72),
                   juce::Justification::centred);
    }

    draw_playhead(g);

    // Border
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(bounds, 1.0f);
}

void WaveformView::draw_click_markers(juce::Graphics& g, bool is_left)
{
    if (total_samples_ == 0) return;

    const auto& clicks = is_left ? left_clicks_ : right_clicks_;
    if (clicks.empty()) return;

    const float h = static_cast<float>(getHeight());
    const float channel_y = (!is_stereo_ || is_left) ? 0.0f : h * 0.5f;
    const float channel_h = is_stereo_ ? h * 0.5f : h;

    for (const auto& ev : clicks) {
        const float x1 = sample_to_x(ev.sample_start);
        const float x2 = sample_to_x(ev.sample_end);
        const float width = std::max(1.0f, x2 - x1);

        // Fill: red with opacity scaled by confidence
        const float alpha = 0.3f + static_cast<float>(ev.confidence) * 0.4f;
        g.setColour(juce::Colour(0xffff4444).withAlpha(alpha));
        g.fillRect(x1, channel_y, width, channel_h);

        // Border line at start
        g.setColour(juce::Colour(0xffff6666).withAlpha(0.8f));
        g.drawVerticalLine(static_cast<int>(x1),
                           channel_y, channel_y + channel_h);
    }
}

void WaveformView::draw_playhead(juce::Graphics& g)
{
    if (playback_position_ <= 0.0) return;
    const float x = static_cast<float>(getWidth()) *
                    static_cast<float>(playback_position_);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawVerticalLine(static_cast<int>(x), 0.0f,
                       static_cast<float>(getHeight()));
}

float WaveformView::sample_to_x(int sample) const
{
    if (total_samples_ == 0) return 0.0f;
    return static_cast<float>(getWidth()) *
           static_cast<float>(sample) /
           static_cast<float>(total_samples_);
}

void WaveformView::resized() {}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    if (total_samples_ == 0) return;
    const double pos = static_cast<double>(e.x) / getWidth();
    if (on_seek) on_seek(std::clamp(pos, 0.0, 1.0));
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    if (total_samples_ == 0) return;
    const double pos = static_cast<double>(e.x) / getWidth();
    if (on_seek) on_seek(std::clamp(pos, 0.0, 1.0));
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}

} // namespace needledropper
