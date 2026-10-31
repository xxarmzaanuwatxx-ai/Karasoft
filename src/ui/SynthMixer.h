#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ChannelPanel.h"
#include "AppSettings.h"

namespace kara
{

class SynthMixer;

/** One of the 32 synth-mixer strips: fader, pan, mute/solo, meter and VST insert slots. */
class SynthStripView : public juce::Component
{
public:
    SynthStripView (SynthMixer&, KaraokeEngine&, int strip);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void showSlotMenu (int slot);
    void updateSlotButton (int slot);

    SynthMixer& owner;
    KaraokeEngine& engine;
    int strip;
    juce::String name;
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider pan { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::TextButton mute { "M" }, solo { "S" };
    juce::OwnedArray<juce::TextButton> slots;
    LevelMeter meterL, meterR;
};

/** 32-strip synth mixer with VST3 insert slots per strip. */
class SynthMixer : public juce::Component
{
public:
    SynthMixer (KaraokeEngine&, AppSettings&);
    ~SynthMixer() override;

    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::KnownPluginList& getKnownPlugins() { return knownPlugins; }
    void showPluginScanner();
    void openPluginEditor (int strip, int slot);
    void closePluginEditor (int strip, int slot);

private:
    void saveKnownPlugins();

    KaraokeEngine& engine;
    AppSettings& settings;
    MixerLookAndFeel lnf;
    juce::KnownPluginList knownPlugins;
    juce::OwnedArray<SynthStripView> strips;
    juce::Viewport viewport;
    juce::Component content;
    std::map<std::pair<int, int>, std::unique_ptr<juce::DocumentWindow>> editors;
};

} // namespace kara
