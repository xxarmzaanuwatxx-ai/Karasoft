#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/KaraokeEngine.h"

namespace kara
{

/** Shared look for the mixer panels. */
class MixerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MixerLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

/** Small vertical level meter. */
class LevelMeter : public juce::Component
{
public:
    void setLevel (float l) { if (std::abs (l - level) > 0.005f) { level = l; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float level = 0.0f;
};

/** One strip of the music panel: controls a single MIDI channel. */
class ChannelStripView : public juce::Component
{
public:
    ChannelStripView (KaraokeEngine&, int channel);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    KaraokeEngine& engine;
    int channel;
    juce::String instrument;
    juce::Slider volume { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider pan { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::TextButton mute { "M" }, solo { "S" };
    LevelMeter meter;
};

/** The 16-channel music panel that slides down from the top edge of the screen,
    with transport, key, tempo and melody controls. */
class ChannelPanel : public juce::Component
{
public:
    explicit ChannelPanel (KaraokeEngine&);
    ~ChannelPanel() override;

    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onOpenSong, onShowSynthMixer, onShowMenu, onPlayPause, onStop;

private:
    void updateLabels();

    KaraokeEngine& engine;
    MixerLookAndFeel lnf;
    juce::OwnedArray<ChannelStripView> strips;

    juce::TextButton openButton { juce::String::fromUTF8 ("เปิดเพลง") }, playButton { "Play" }, stopButton { "Stop" };
    juce::TextButton keyDown { "-" }, keyUp { "+" }, tempoDown { "-" }, tempoUp { "+" };
    juce::TextButton melodyButton { juce::String::fromUTF8 ("ทำนอง") }, mixerButton { "Synth Mixer" }, menuButton { juce::String::fromUTF8 ("ตั้งค่า") };
    juce::Label keyLabel, tempoLabel, timeLabel;
    juce::Slider positionSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider masterSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
};

} // namespace kara
