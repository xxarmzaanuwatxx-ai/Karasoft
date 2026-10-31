#include "KaraokeEngine.h"

namespace kara
{

//==============================================================================
void TempoMap::build (const juce::MidiFile& file)
{
    points.clear();
    juce::MidiMessageSequence tempos;
    file.findAllTempoEvents (tempos);
    tempos.sort();

    const double ppq = file.getTimeFormat() > 0 ? file.getTimeFormat() : 480;
    double tick = 0.0, seconds = 0.0, spt = 0.5 / ppq; // 120 bpm default

    points.push_back ({ 0.0, 0.0, spt });

    for (auto* e : tempos)
    {
        const auto t = e->message.getTimeStamp();
        seconds += (t - tick) * spt;
        tick = t;
        spt = e->message.getTempoSecondsPerQuarterNote() / ppq;

        if (juce::exactlyEqual (points.back().tick, tick))
            points.back() = { tick, seconds, spt };
        else
            points.push_back ({ tick, seconds, spt });
    }
}

double TempoMap::tickToSeconds (double tick) const
{
    if (points.empty())
        return 0.0;

    auto it = std::upper_bound (points.begin(), points.end(), tick,
                                [] (double t, const Point& p) { return t < p.tick; });
    const auto& p = *std::prev (it == points.begin() ? std::next (it) : it);
    return p.seconds + (tick - p.tick) * p.secondsPerTick;
}

//==============================================================================
struct KaraokeEngine::SynthHolder
{
    fluid::Settings* settings = nullptr;
    fluid::Synth* synth = nullptr;

    ~SynthHolder()
    {
        auto& f = fluid::api();
        if (synth != nullptr)    f.delete_synth (synth);
        if (settings != nullptr) f.delete_settings (settings);
    }
};

KaraokeEngine::KaraokeEngine()
{
    pluginFormats.addDefaultFormats();

    for (auto& ch : soundingNote)
        ch.fill (-1);
}

KaraokeEngine::~KaraokeEngine()
{
    loader.removeAllJobs (true, 30000);
}

bool KaraokeEngine::isSynthReady() const
{
    const juce::ScopedLock sl (synthLock);
    return synthHolder != nullptr;
}

void KaraokeEngine::setSoundFonts (const juce::StringArray& paths, std::function<void (juce::String)> onDone)
{
    soundFonts = paths;
    const auto rate = sampleRate;
    const auto room = 0.6, level = 0.5;

    loader.addJob ([this, paths, rate, room, level, onDone]
    {
        auto& f = fluid::api();
        juce::String error;

        if (! f.load (error))
        {
            juce::MessageManager::callAsync ([onDone, error] { if (onDone) onDone (error); });
            return;
        }

        auto holder = std::make_unique<SynthHolder>();
        holder->settings = f.new_settings();
        f.settings_setnum (holder->settings, "synth.sample-rate", rate);
        f.settings_setint (holder->settings, "synth.midi-channels", numStrips);
        f.settings_setint (holder->settings, "synth.audio-channels", numStrips);
        f.settings_setint (holder->settings, "synth.audio-groups", numStrips);
        f.settings_setint (holder->settings, "synth.effects-groups", 1);
        f.settings_setint (holder->settings, "synth.polyphony", 1024);
        f.settings_setint (holder->settings, "synth.threadsafe-api", 1);
        f.settings_setnum (holder->settings, "synth.gain", 0.4);
        f.settings_setnum (holder->settings, "synth.reverb.room-size", room);
        f.settings_setnum (holder->settings, "synth.reverb.level", level);
        f.settings_setnum (holder->settings, "synth.reverb.width", 0.8);
        f.settings_setnum (holder->settings, "synth.reverb.damp", 0.3);
        f.settings_setnum (holder->settings, "synth.chorus.level", 1.2);
        holder->synth = f.new_synth (holder->settings);

        if (holder->synth == nullptr)
            error = "Cannot create synthesizer";

        juce::StringArray failed;
        for (auto& p : paths)
            if (holder->synth != nullptr && f.sfload (holder->synth, p.toRawUTF8(), 1) < 0)
                failed.add (juce::File (p).getFileName());

        if (holder->synth != nullptr)
        {
            for (int ch = 0; ch < numStrips; ++ch)
                f.set_interp_method (holder->synth, ch, fluid::interpHighest);

            setupDrumChannels (holder->synth);
        }

        if (! failed.isEmpty())
            error = "Cannot load SoundFont: " + failed.joinIntoString (", ");

        if (holder->synth != nullptr)
        {
            std::unique_ptr<SynthHolder> old;
            {
                const juce::ScopedLock sl (synthLock);
                old = std::move (synthHolder);
                synthHolder = std::move (holder);
                needsSeek = true;
                seekTarget = position.load();
            }
        }

        juce::MessageManager::callAsync ([onDone, error] { if (onDone) onDone (error); });
    });
}

void KaraokeEngine::prepareToPlay (int samplesPerBlockExpected, double newRate)
{
    blockSize = juce::jmax (samplesPerBlockExpected, 64);
    stripBuffers.setSize (numStrips * 2, blockSize * 2);
    fxBuffers.setSize (4, blockSize * 2);

    limiter.prepare ({ newRate, (juce::uint32) (blockSize * 2), 2 });
    limiter.setThreshold (-1.0f);
    limiter.setRelease (120.0f);

    {
        const juce::ScopedLock sl (insertLock);
        for (auto& s : strips)
            for (auto& p : s.inserts)
                if (p != nullptr)
                    p->prepareToPlay (newRate, blockSize * 2);
    }

    if (std::abs (newRate - sampleRate) > 1.0 || ! isSynthReady())
    {
        sampleRate = newRate;
        if (! soundFonts.isEmpty())
            setSoundFonts (soundFonts);
    }
}

//==============================================================================
void KaraokeEngine::setSong (std::unique_ptr<KaraokeSong> newSong)
{
    std::vector<Event> newEvents;
    TempoMap newMap;
    double newLength = 0.0;

    if (newSong != nullptr)
    {
        newMap.build (newSong->midi);

        for (int t = 0; t < newSong->midi.getNumTracks(); ++t)
            for (auto* e : *newSong->midi.getTrack (t))
            {
                const auto& m = e->message;
                if (m.isMetaEvent())
                    continue;

                const auto secs = newMap.tickToSeconds (m.getTimeStamp());
                newEvents.push_back ({ secs, m });
                newLength = juce::jmax (newLength, secs);
            }

        std::stable_sort (newEvents.begin(), newEvents.end(),
                          [] (const Event& a, const Event& b) { return a.seconds < b.seconds; });
    }

    playing = false;
    {
        const juce::ScopedLock sl (songLock);
        song = std::move (newSong);
        tempoMap = newMap;
        events = std::move (newEvents);
        lengthSeconds = newLength + 1.0;
        nextEvent = 0;
        position = 0.0;
        finished = false;
        seekTarget = 0.0;
        needsSeek = true;
    }
}

void KaraokeEngine::play()
{
    if (song != nullptr)
    {
        if (finished)
            seek (0.0);
        playing = true;
    }
}

void KaraokeEngine::pause() { playing = false; }

void KaraokeEngine::stop()
{
    playing = false;
    seek (0.0);
}

void KaraokeEngine::seek (double seconds)
{
    seekTarget = juce::jlimit (0.0, lengthSeconds, seconds);
    position = seekTarget.load();
    finished = false;
    needsSeek = true;
}

double KaraokeEngine::getPositionTicks() const
{
    const juce::ScopedLock sl (songLock);
    if (song == nullptr)
        return 0.0;

    // Invert tickToSeconds by bisection; tempo maps are monotonic.
    const auto target = position.load();
    double lo = 0.0, hi = 1.0;
    while (tempoMap.tickToSeconds (hi) < target && hi < 1.0e9)
        hi *= 2.0;
    for (int i = 0; i < 40; ++i)
    {
        const auto mid = 0.5 * (lo + hi);
        (tempoMap.tickToSeconds (mid) < target ? lo : hi) = mid;
    }
    return lo;
}

void KaraokeEngine::setTranspose (int semitones)
{
    transpose = juce::jlimit (-12, 12, semitones);
    transposeChanged = true;
}

void KaraokeEngine::setGuideMelody (bool on) { guideMelody = on; }

void KaraokeEngine::setReverb (double room, double level)
{
    const juce::ScopedLock sl (synthLock);
    if (synthHolder != nullptr)
    {
        auto& f = fluid::api();
        f.set_reverb_group_roomsize (synthHolder->synth, -1, room);
        f.set_reverb_group_level (synthHolder->synth, -1, level);
    }
}

void KaraokeEngine::setChorusLevel (double level)
{
    const juce::ScopedLock sl (synthLock);
    if (synthHolder != nullptr)
        fluid::api().set_chorus_group_level (synthHolder->synth, -1, level);
}

juce::String KaraokeEngine::getChannelInstrumentName (int channel) const
{
    const juce::ScopedLock sl (synthLock);
    if (synthHolder == nullptr)
        return {};

    auto& f = fluid::api();
    if (auto* preset = f.get_channel_preset (synthHolder->synth, channel))
        if (auto* name = f.preset_get_name (preset))
            return juce::String::fromUTF8 (name).trim();

    return {};
}

//==============================================================================
bool KaraokeEngine::loadInsert (int strip, int slot, const juce::PluginDescription& desc, juce::String& error)
{
    auto instance = pluginFormats.createPluginInstance (desc, sampleRate, blockSize * 2, error);
    if (instance == nullptr)
        return false;

    instance->setPlayConfigDetails (2, 2, sampleRate, blockSize * 2);
    instance->prepareToPlay (sampleRate, blockSize * 2);

    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (insertLock);
        old = std::move (strips[(size_t) strip].inserts[(size_t) slot]);
        strips[(size_t) strip].inserts[(size_t) slot] = std::move (instance);
    }
    return true;
}

void KaraokeEngine::clearInsert (int strip, int slot)
{
    std::unique_ptr<juce::AudioPluginInstance> old;
    const juce::ScopedLock sl (insertLock);
    old = std::move (strips[(size_t) strip].inserts[(size_t) slot]);
}

//==============================================================================
juce::String KaraokeEngine::getDrumPartName (int part)
{
    static const char* names[numDrumParts] = { "Kick", "Snare", "Rim/Clap", "Closed HH", "Open HH", "Pedal HH",
                                               "Low Tom", "Mid Tom", "High Tom", "Crash", "Ride", "Splash/China",
                                               "Tambourine", "Conga/Bongo", "Timbale/Cowbell", "Perc" };
    return names[juce::jlimit (0, numDrumParts - 1, part)];
}

int KaraokeEngine::drumPartForNote (int note)
{
    switch (note)
    {
        case 35: case 36:                     return 0;
        case 38: case 40:                     return 1;
        case 37: case 39:                     return 2;
        case 42:                              return 3;
        case 46:                              return 4;
        case 44:                              return 5;
        case 41: case 43:                     return 6;
        case 45: case 47:                     return 7;
        case 48: case 50:                     return 8;
        case 49: case 57:                     return 9;
        case 51: case 53: case 59:            return 10;
        case 52: case 55:                     return 11;
        case 54: case 56: case 58:            return 12;
        case 60: case 61: case 62: case 63: case 64: return 13;
        case 65: case 66: case 67: case 68:   return 14;
        default:                              return 15;
    }
}

void KaraokeEngine::setupDrumChannels (fluid::Synth* s)
{
    auto& f = fluid::api();
    for (int ch = numMidiChannels; ch < numStrips; ++ch)
    {
        f.set_channel_type (s, ch, fluid::channelTypeDrum);
        f.program_change (s, ch, 0);
    }
}

void KaraokeEngine::sendToSynth (fluid::Synth* s, const juce::MidiMessage& m)
{
    const auto ch = m.getChannel() - 1;

    if (m.isSysEx())
    {
        fluid::api().sysex (s, reinterpret_cast<const char*> (m.getSysExData()), m.getSysExDataSize(),
                            nullptr, nullptr, nullptr, 0);
        setupDrumChannels (s);  // a GS/XG reset restores default channel types
        return;
    }

    if (ch == drumChannel)
    {
        if (m.isNoteOnOrOff() || m.isAftertouch())
        {
            sendChannelMessage (s, numMidiChannels + drumPartForNote (m.getNoteNumber()), m);
            if (m.isNoteOn())
                channelActivity[(size_t) ch] = juce::jmax (channelActivity[(size_t) ch].load(), m.getFloatVelocity());
            return;
        }

        // Controllers, programs and bends apply to every drum part.
        for (int part = numMidiChannels; part < numStrips; ++part)
            sendChannelMessage (s, part, m);
    }

    sendChannelMessage (s, ch, m);
}

void KaraokeEngine::sendChannelMessage (fluid::Synth* s, int ch, const juce::MidiMessage& m)
{
    auto& f = fluid::api();

    if (m.isNoteOn())
    {
        const auto src = m.getNoteNumber();
        const auto note = (ch == drumChannel || ch >= numMidiChannels) ? src : juce::jlimit (0, 127, src + transpose.load());
        if (soundingNote[(size_t) ch][(size_t) src] >= 0)
            f.noteoff (s, ch, soundingNote[(size_t) ch][(size_t) src]);
        soundingNote[(size_t) ch][(size_t) src] = (int8_t) note;
        f.noteon (s, ch, note, m.getVelocity());
        if (ch < numMidiChannels)
            channelActivity[(size_t) ch] = juce::jmax (channelActivity[(size_t) ch].load(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        auto& sounding = soundingNote[(size_t) ch][(size_t) m.getNoteNumber()];
        if (sounding >= 0)
            f.noteoff (s, ch, sounding);
        sounding = -1;
    }
    else if (m.isController())        f.cc (s, ch, m.getControllerNumber(), m.getControllerValue());
    else if (m.isProgramChange())     f.program_change (s, ch, m.getProgramChangeNumber());
    else if (m.isPitchWheel())        f.pitch_bend (s, ch, m.getPitchWheelValue());
    else if (m.isChannelPressure())   f.channel_pressure (s, ch, m.getChannelPressureValue());
    else if (m.isAftertouch())        f.key_pressure (s, ch, m.getNoteNumber(), m.getAfterTouchValue());
}

void KaraokeEngine::silenceSynth (fluid::Synth* s)
{
    auto& f = fluid::api();
    for (int ch = 0; ch < numStrips; ++ch)
        f.all_sounds_off (s, ch);
    for (auto& ch : soundingNote)
        ch.fill (-1);
}

void KaraokeEngine::restoreStateUpTo (fluid::Synth* s, double seconds)
{
    auto& f = fluid::api();
    silenceSynth (s);
    f.system_reset (s);
    setupDrumChannels (s);

    nextEvent = 0;
    while (nextEvent < events.size() && events[nextEvent].seconds < seconds)
    {
        const auto& m = events[nextEvent].msg;
        if (! m.isNoteOnOrOff())
            sendToSynth (s, m);
        ++nextEvent;
    }
}

void KaraokeEngine::renderSynth (fluid::Synth* s, int start, int num)
{
    if (num <= 0)
        return;

    float* out[numStrips * 2];
    for (int i = 0; i < numStrips * 2; ++i)
        out[i] = stripBuffers.getWritePointer (i, start);

    float* fx[4];
    for (int i = 0; i < 4; ++i)
        fx[i] = fxBuffers.getWritePointer (i, start);

    fluid::api().process (s, num, 4, fx, numStrips * 2, out);
}

void KaraokeEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    const auto n = juce::jmin (info.numSamples, stripBuffers.getNumSamples());

    const juce::ScopedTryLock synthTry (synthLock);
    if (! synthTry.isLocked() || synthHolder == nullptr)
        return;

    const juce::ScopedTryLock songTry (songLock);
    if (! songTry.isLocked())
        return;

    auto* s = synthHolder->synth;
    stripBuffers.clear();
    fxBuffers.clear();

    if (needsSeek.exchange (false))
    {
        restoreStateUpTo (s, seekTarget.load());
        position = seekTarget.load();
    }

    if (transposeChanged.exchange (false))
        for (int ch = 0; ch < numMidiChannels; ++ch)
            if (ch != drumChannel)
            {
                for (auto& note : soundingNote[(size_t) ch])
                    if (note >= 0) { fluid::api().noteoff (s, ch, note); note = -1; }
            }

    // Sequence events with sample accuracy, rendering the synth between them.
    int rendered = 0;
    if (playing)
    {
        const auto rate = speed.load();
        const auto startPos = position.load();
        const auto endPos = startPos + n / sampleRate * rate;

        while (nextEvent < events.size() && events[nextEvent].seconds < endPos)
        {
            const auto offset = juce::jlimit (rendered, n, (int) ((events[nextEvent].seconds - startPos) / rate * sampleRate));
            renderSynth (s, rendered, offset - rendered);
            rendered = offset;
            sendToSynth (s, events[nextEvent].msg);
            ++nextEvent;
        }

        position = endPos;
        if (nextEvent >= events.size() && endPos >= lengthSeconds)
        {
            playing = false;
            finished = true;
        }
    }
    renderSynth (s, rendered, n - rendered);

    // Mix strips into the stereo output. Solo works independently on the music panel
    // (MIDI channels) and on the synth mixer (strips).
    bool anyStripSolo = false, anyChannelSolo = false;
    for (auto& st : strips)    anyStripSolo = anyStripSolo || st.solo.load();
    for (auto& c : channels)   anyChannelSolo = anyChannelSolo || c.solo.load();

    const auto vocal = song != nullptr ? song->vocalChannel : -1;
    const juce::ScopedTryLock insertTry (insertLock);
    juce::MidiBuffer noMidi;

    auto* outL = info.buffer->getWritePointer (0, info.startSample);
    auto* outR = info.buffer->getNumChannels() > 1 ? info.buffer->getWritePointer (1, info.startSample) : nullptr;

    for (int i = 0; i < numStrips; ++i)
    {
        auto& st = strips[(size_t) i];
        const int midiChannel = stripMidiChannel (i);
        auto& chan = channels[(size_t) midiChannel];
        const bool silent = st.mute || (anyStripSolo && ! st.solo)
                         || chan.mute || (anyChannelSolo && ! chan.solo)
                         || (! guideMelody && midiChannel == vocal);

        juce::AudioBuffer<float> pair (stripBuffers.getArrayOfWritePointers() + i * 2, 2, n);

        if (insertTry.isLocked())
            for (size_t slot = 0; slot < st.inserts.size(); ++slot)
                if (auto& p = st.inserts[slot]; p != nullptr && ! st.bypass[slot])
                    p->processBlock (pair, noMidi);

        const auto g = silent ? 0.0f : st.gain.load() * chan.gain.load();
        const auto p = st.pan.load();
        const auto gl = g * std::min (1.0f, 1.0f - p), gr = g * std::min (1.0f, 1.0f + p);

        st.peakL = juce::jmax (st.peakL.load() * 0.85f, pair.getMagnitude (0, 0, n) * gl);
        st.peakR = juce::jmax (st.peakR.load() * 0.85f, pair.getMagnitude (1, 0, n) * gr);

        juce::FloatVectorOperations::addWithMultiply (outL, pair.getReadPointer (0), gl, n);
        if (outR != nullptr)
            juce::FloatVectorOperations::addWithMultiply (outR, pair.getReadPointer (1), gr, n);
    }

    // Reverb and chorus returns.
    for (int fx = 0; fx < 4; fx += 2)
    {
        juce::FloatVectorOperations::add (outL, fxBuffers.getReadPointer (fx), n);
        if (outR != nullptr)
            juce::FloatVectorOperations::add (outR, fxBuffers.getReadPointer (fx + 1), n);
    }

    info.buffer->applyGain (info.startSample, n, masterGain.load());

    if (outR != nullptr)
    {
        juce::dsp::AudioBlock<float> block (info.buffer->getArrayOfWritePointers(), 2, (size_t) info.startSample, (size_t) n);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::clip (info.buffer->getWritePointer (c, info.startSample),
                                               info.buffer->getReadPointer (c, info.startSample), -1.0f, 1.0f, n);
    }

    for (auto& a : channelActivity)
        a = a.load() * 0.92f;
}

} // namespace kara
