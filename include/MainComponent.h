#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/WaveletEngine.h"
#include "dsp/ClickDetector.h"
#include "dsp/RepairEngine.h"
#include "threading/DetectionThread.h"
#include "threading/ProcessingThread.h"
#include "ui/WaveformView.h"
#include "ui/TransportBar.h"
#include "ui/ParameterPanel.h"
#include "ui/BatchQueue.h"
#include "ui/StatusBar.h"

namespace declick {

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget {
public:
    MainComponent();
    ~MainComponent() override;

    // juce::Component overrides.
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::FileDragAndDropTarget overrides.
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files,
                      int x, int y) override;

private:
    // DSP layer — created first, passed by reference downstream.
    WaveletEngine    wavelet_engine_;
    ClickDetector    click_detector_;
    RepairEngine     repair_engine_;

    // Threading layer.
    DetectionThread  detection_thread_;
    ProcessingThread processing_thread_;

    // UI layer.
    WaveformView     waveform_view_;
    TransportBar     transport_bar_;
    ParameterPanel   parameter_panel_;
    BatchQueue       batch_queue_;
    StatusBar        status_bar_;

    // Playback.
    juce::AudioDeviceManager          device_manager_;
    juce::AudioFormatManager          format_manager_;
    juce::AudioTransportSource        transport_source_;
    std::unique_ptr<juce::AudioFormatReaderSource> original_source_;
    std::unique_ptr<juce::AudioFormatReaderSource> processed_source_;

    // Currently loaded audio — kept in memory for detection/repair.
    std::vector<double> audio_left_;
    std::vector<double> audio_right_;
    bool                is_stereo_   { false };
    double              sample_rate_ { 44100.0 };
    int                 bit_depth_   { 24 };

    // Wire up all inter-component callbacks.
    void connect_callbacks();

    // Load a file into the waveform view and trigger detection.
    void load_file(const juce::File& file);

    // Called by DetectionThread on completion.
    void on_detection_complete(DetectionResults results);

    // Called by ProcessingThread on progress update.
    void on_processing_progress(ProcessingProgress progress);

    // Called by ProcessingThread on file completion.
    void on_processing_complete(ProcessingComplete result);

    // Write repaired audio to disk next to source file.
    bool write_output(const ProcessingComplete& result);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace declick
