#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class DeClickApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {
        return JUCE_APPLICATION_NAME_STRING;
    }
    const juce::String getApplicationVersion() override {
        return JUCE_APPLICATION_VERSION_STRING;
    }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override {
        main_window_.reset(new MainWindow(getApplicationName()));
    }
    void shutdown() override { main_window_ = nullptr; }

    class MainWindow : public juce::DocumentWindow {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                juce::Desktop::getInstance()
                    .getDefaultLookAndFeel()
                    .findColour(juce::ResizableWindow::backgroundColourId),
                DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new declick::MainComponent(), true);
            setResizable(true, true);
            centreWithSize(900, 600);
            setVisible(true);
        }
        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> main_window_;
};

START_JUCE_APPLICATION(DeClickApplication)
