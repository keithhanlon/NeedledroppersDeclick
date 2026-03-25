#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace needledropper {

// State of a single item in the batch queue.
enum class QueueItemState {
    Pending,       // loaded, not yet detected
    Detecting,     // detection thread running
    Ready,         // detection complete, awaiting process
    Processing,    // processing thread running
    Done,          // repair complete, output written
    Error          // something went wrong
};

struct QueueItem {
    juce::File      file;
    QueueItemState  state    { QueueItemState::Pending };
    int             n_clicks { 0 };       // detected click count
    float           progress { 0.0f };    // 0.0 - 1.0 during processing
    juce::String    error_message;
};

class BatchQueue : public juce::Component,
                   public juce::ListBoxModel {
public:
    BatchQueue();
    ~BatchQueue() override;

    // Add files — duplicates ignored.
    void add_files(const juce::Array<juce::File>& files);

    // Update state of a specific item.
    void set_item_state(int index, QueueItemState state);
    void set_item_clicks(int index, int n_clicks);
    void set_item_progress(int index, float progress);
    void set_item_error(int index, const juce::String& message);

    // Access items.
    const QueueItem& item(int index) const { return items_[index]; }
    int              size()          const { return items_.size(); }
    void             clear();

    // Callbacks.
    std::function<void(int index)>  on_selection_changed;
    std::function<void()>           on_files_added;

    // juce::Component overrides.
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::ListBoxModel overrides.
    int  getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g,
                          int width, int height,
                          bool selected) override;
    void selectedRowsChanged(int last_row_selected) override;

private:
    juce::ListBox            list_box_;
    std::vector<QueueItem>   items_;

    juce::String state_label(QueueItemState state) const;
};

} // namespace needledropper
