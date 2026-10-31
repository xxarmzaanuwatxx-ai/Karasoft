#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../core/KaraokeSong.h"
#include "FluidApi.h"

namespace kara
{

/** Converts MIDI ticks to seconds using a file's tempo map. */
class TempoMap
{
public:
    void build (const juce::MidiFile& file);
    double tickToSeconds (double tick) const;

private:
    struct Point { double tick, seconds, secondsPerTick; };
    std::vector<Point> points;
};

/** One output strip of the synth mixer: gain, pan, mute/solo, meter and VST insert slots. */
struct MixerStrip
{
    static constexpr int numInsertSlots = 4;

    std::atomic<float> gain { 1.0f }, pan { 0.0f };
    std::atomic<bool> mute { false }, solo { false };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };

    std::array<std::unique_ptr<juce::AudioPluginInstance>, numInsertSlots> inserts;
    std::array<std::atomic<bool>, numInsertSlots> bypass {};
};

/** Per-MIDI-channel controls shown on the 16-channel music panel. */
struct ChannelControl
{
    std::atomic<float> gain { 1.0f };
    std::atomic<bool> mute { false }, solo { false };
};

/** SoundFont synth + MIDI sequencer + 32-strip mixer. Owned by the app and driven by an
    AudioSourcePlayer. All public setters are safe to call from the message thread. */
class KaraokeEngine : public juce::AudioSource
{
public:
    static constexpr int numMidiChannels = 16;
    static constexpr int numStrips = 32;
    static constexpr int drumChannel = 9;
    static constexpr int numDrumParts = numStrips - numMidiChannels;

    /** Strips 0-15 carry MIDI channels 1-16; strips 16-31 carry the drum kit split into parts. */
    static int stripMidiChannel (int strip) { return strip < numMidiChannels ? strip : drumChannel; }
    static juce::String getDrumPartName (int part);
    static int drumPartForNote (int note);

    KaraokeEngine();
    ~KaraokeEngine() override;

    /** Replaces the loaded SoundFonts. Later files take priority over earlier ones for
        presets they both define. Loads on a background thread; callback runs on the
        message thread when done. */
    void setSoundFonts (const juce::StringArray& paths, std::function<void (juce::String error)> onDone = {});
    juce::StringArray getSoundFonts() const { return soundFonts; }
    bool isSynthReady() const;

    void setSong (std::unique_ptr<KaraokeSong> newSong);
    const KaraokeSong* getSong() const { return song.get(); }
    double getLengthSeconds() const { return lengthSeconds; }

    void play();
    void pause();
    void stop();
    void seek (double seconds);
    bool isPlaying() const { return playing.load(); }
    bool hasFinished() const { return finished.load(); }
    double getPositionSeconds() const { return position.load(); }

    /** Current song position in MIDI ticks, for driving the lyric display. */
    double getPositionTicks() const;
    double ticksToSeconds (double tick) const { return tempoMap.tickToSeconds (tick); }

    void setTranspose (int semitones);
    int getTranspose() const { return transpose.load(); }
    void setSpeed (double ratio)  { speed = juce::jlimit (0.5, 2.0, ratio); }
    double getSpeed() const { return speed.load(); }
    void setGuideMelody (bool on);
    bool isGuideMelodyOn() const { return guideMelody.load(); }

    void setMasterGain (float g) { masterGain = g; }
    float getMasterGain() const { return masterGain.load(); }
    void setReverb (double room, double level);
    void setChorusLevel (double level);

    MixerStrip& getStrip (int index) { return strips[(size_t) index]; }
    ChannelControl& getChannel (int index) { return channels[(size_t) index]; }
    juce::String getChannelInstrumentName (int channel) const;
    float getChannelActivity (int channel) const { return channelActivity[(size_t) channel].load(); }

    /** Loads a VST3 (or other host-supported) plugin into a strip's insert slot. */
    bool loadInsert (int strip, int slot, const juce::PluginDescription& desc, juce::String& error);
    void clearInsert (int strip, int slot);
    juce::AudioPluginFormatManager& getPluginFormats() { return pluginFormats; }

    // AudioSource
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

private:
    struct SynthHolder;
    struct Event { double seconds; juce::MidiMessage msg; };

    void rebuildSynth();
    void sendToSynth (fluid::Synth*, const juce::MidiMessage&);
    void sendChannelMessage (fluid::Synth*, int ch, const juce::MidiMessage&);
    static void setupDrumChannels (fluid::Synth*);
    void renderSynth (fluid::Synth*, int startSample, int numSamples);
    void silenceSynth (fluid::Synth*);
    void restoreStateUpTo (fluid::Synth*, double seconds);

    juce::StringArray soundFonts;
    std::unique_ptr<SynthHolder> synthHolder;
    juce::CriticalSection synthLock;   // guards synthHolder swaps vs the audio thread
    juce::ThreadPool loader { 1 };

    std::unique_ptr<KaraokeSong> song;
    TempoMap tempoMap;
    std::vector<Event> events;
    size_t nextEvent = 0;
    double lengthSeconds = 0.0;
    juce::CriticalSection songLock;

    std::atomic<bool> playing { false }, finished { false }, needsSeek { false };
    std::atomic<double> position { 0.0 }, seekTarget { 0.0 }, speed { 1.0 };
    std::atomic<int> transpose { 0 };
    std::atomic<bool> guideMelody { true }, transposeChanged { false };
    std::atomic<float> masterGain { 0.7f };

    std::array<MixerStrip, numStrips> strips;
    std::array<ChannelControl, numMidiChannels> channels;
    std::array<std::atomic<float>, numMidiChannels> channelActivity {};
    std::array<std::array<int8_t, 128>, numStrips> soundingNote {}; // played note per source note, -1 = none

    juce::AudioPluginFormatManager pluginFormats;
    juce::CriticalSection insertLock;

    double sampleRate = 44100.0;
    int blockSize = 512;
    juce::AudioBuffer<float> stripBuffers, fxBuffers;
    juce::dsp::Limiter<float> limiter;
};

} // namespace kara
