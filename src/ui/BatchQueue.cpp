#include "ui/BatchQueue.h"

namespace needledropper {

BatchQueue::BatchQueue() {
    addAndMakeVisible(list_box_);
    list_box_.setModel(this);
}

BatchQueue::~BatchQueue() {}

void BatchQueue::add_files(const juce::Array<juce::File>& files) {
    for (auto& f : files) {
        bool exists = false;
        for (auto& item : items_)
            if (item.file == f) { exists = true; break; }
        if (!exists) items_.push_back({ f });
    }
    list_box_.updateContent();
    if (on_files_added) on_files_added();
}

void BatchQueue::set_item_state(int i, QueueItemState s)        { items_[i].state = s; list_box_.repaintRow(i); }
void BatchQueue::set_item_clicks(int i, int n)                  { items_[i].n_clicks = n; list_box_.repaintRow(i); }
void BatchQueue::set_item_progress(int i, float p)              { items_[i].progress = p; list_box_.repaintRow(i); }
void BatchQueue::set_item_error(int i, const juce::String& e)   { items_[i].error_message = e; list_box_.repaintRow(i); }
void BatchQueue::clear()                                        { items_.clear(); list_box_.updateContent(); }
void BatchQueue::paint(juce::Graphics& g)                       { g.fillAll(juce::Colours::darkgrey); }
void BatchQueue::resized()                                      { list_box_.setBounds(getLocalBounds()); }
int  BatchQueue::getNumRows()                                   { return (int)items_.size(); }

void BatchQueue::paintListBoxItem(int row, juce::Graphics& g,
                                  int w, int h, bool selected) {
    if (selected) g.fillAll(juce::Colours::darkblue);
    g.setColour(juce::Colours::white);
    g.drawText(items_[row].file.getFileName() + "  " +
               state_label(items_[row].state),
               4, 0, w - 4, h, juce::Justification::centredLeft);
}

void BatchQueue::selectedRowsChanged(int row) {
    if (on_selection_changed) on_selection_changed(row);
}

juce::String BatchQueue::state_label(QueueItemState s) const {
    switch (s) {
        case QueueItemState::Pending:    return "[pending]";
        case QueueItemState::Detecting:  return "[detecting...]";
        case QueueItemState::Ready:      return "[ready]";
        case QueueItemState::Processing: return "[processing...]";
        case QueueItemState::Done:       return "[done]";
        case QueueItemState::Error:      return "[error]";
        default:                         return "";
    }
}

} // namespace needledropper
