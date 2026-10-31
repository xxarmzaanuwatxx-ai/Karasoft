#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/MainComponent.h"

class KaraSoftApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
       #if JUCE_WINDOWS
        // Tahoma covers Thai and Latin, so menus and labels render consistently.
        juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName ("Tahoma");
       #endif
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
        openFromCommandLine (commandLine);
    }

    void shutdown() override { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        openFromCommandLine (commandLine);
    }

private:
    void openFromCommandLine (const juce::String& commandLine)
    {
        juce::Array<juce::File> files;
        double startSeconds = 0.0;

        for (auto& arg : juce::StringArray::fromTokens (commandLine, true))
        {
            if (arg.startsWith ("--start="))
            {
                startSeconds = arg.fromFirstOccurrenceOf ("=", false, false).getDoubleValue();
                continue;
            }

            juce::File f (arg.unquoted());
            if (f.existsAsFile())
                files.add (f);
        }

        if (! files.isEmpty())
            if (auto* mc = dynamic_cast<kara::MainComponent*> (mainWindow->getContentComponent()))
            {
                mc->enqueue (files, true);
                if (startSeconds > 0.0)
                    mc->seek (startSeconds);
            }
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new kara::MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            getContentComponent()->grabKeyboardFocus();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (KaraSoftApplication)
