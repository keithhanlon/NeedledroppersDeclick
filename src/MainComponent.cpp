#include "MainComponent.h"

namespace declick {

MainComponent::MainComponent()
    : wavelet_engine_(5)
    , click_detector_(wavelet_engine_)
    , repair_engine_(wavelet_engine_)
    , detection_thread_(wavelet_engine_, click_detector_)
    , processing_thread_(wavelet_engine_, repair_engine_)
{
    setSize(900, 600);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey);
}

void MainComponent::resized() {}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray&) {
    return true;
}

void MainComponent::filesDropped(const juce::StringArray&, int, int) {}

void MainComponent::connect_callbacks() {}
void MainComponent::load_file(const juce::File&) {}
void MainComponent::on_detection_complete(DetectionResults) {}
void MainComponent::on_processing_progress(ProcessingProgress) {}
void MainComponent::on_processing_complete(ProcessingComplete) {}
bool MainComponent::write_output(const ProcessingComplete&) { return false; }

} // namespace declick
