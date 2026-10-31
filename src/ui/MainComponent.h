#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "../audio/KaraokeEngine.h"
#include "AppSettings.h"
#include "LyricView.h"
#include "ChannelPanel.h"
#include "SynthMixer.h"

namespace kara
{

/** The single karaoke screen: lyrics full-window, the 16-channel panel slides down when
    the mouse touches the top edge, and right-click anywhere opens the settings menu. */
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** Adds songs to the queue; plays the first immediately if nothing is playing. */
    void enqueue (const juce::Array<juce::File>& files, bool playNow);
    void seek (double seconds) { engine.seek (seconds); }

private:
    void timerCallback() override;
    void loadAndPlay (const juce::File& file);
    void playNext();
    void showMenu();
    void openSongDialog();
    void addSoundFontDialog();
    void applySoundFonts();
    void showFontDialog();
    void showAudioSettings();
    void showSynthMixer();
    void chooseBackground();
    void applyStyleFromSettings();
    void setPanelVisible (bool shouldShow);
    void toggleFullScreen();
    void updateStatus();

    AppSettings settings;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    KaraokeEngine engine;

    LyricView lyrics;
    ChannelPanel panel { engine };
    std::unique_ptr<juce::DocumentWindow> mixerWindow;
    SynthMixer* synthMixer = nullptr;
    juce::TooltipWindow tooltips { this };

    juce::Array<juce::File> queue;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String statusText;
    bool panelShown = false;
    double panelHideTime = 0.0;
    int frame = 0;
};

} // namespace kara
