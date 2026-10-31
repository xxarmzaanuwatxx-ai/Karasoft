// Renders a song through the KaraSoft engine to a WAV file: kararender <song> <soundfont.sf2> <out.wav> [seconds]
#include "../src/audio/KaraokeEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cout << "usage: kararender <song> <soundfont.sf2> <out.wav> [seconds]\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI init;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    auto loaded = kara::loadSong (cwd.getChildFile (argv[1]));
    if (loaded.song == nullptr)
    {
        std::cout << "ERROR: " << loaded.error << "\n";
        return 2;
    }

    constexpr double rate = 44100.0;
    constexpr int block = 512;
    kara::KaraokeEngine engine;
    engine.prepareToPlay (block, rate);

    std::atomic<bool> ready { false };
    juce::String sfError;
    engine.setSoundFonts ({ cwd.getChildFile (argv[2]).getFullPathName() }, [&] (juce::String e) { sfError = e; ready = true; });
    while (! ready)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    if (sfError.isNotEmpty())
    {
        std::cout << "ERROR: " << sfError << "\n";
        return 3;
    }

    engine.setSong (std::move (loaded.song));
    engine.play();

    const auto seconds = argc > 4 ? juce::String (argv[4]).getDoubleValue() : engine.getLengthSeconds();
    juce::AudioBuffer<float> buffer (2, block);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (
        new juce::FileOutputStream (cwd.getChildFile (argv[3])), rate, 2, 16, {}, 0));

    for (double t = 0; t < seconds && ! engine.hasFinished(); t += block / rate)
    {
        engine.getNextAudioBlock ({ &buffer, 0, block });
        writer->writeFromAudioSampleBuffer (buffer, 0, block);
    }

    std::cout << "rendered " << engine.getPositionSeconds() << " s; ch1 = " << engine.getChannelInstrumentName (0) << "\n";
    return 0;
}
