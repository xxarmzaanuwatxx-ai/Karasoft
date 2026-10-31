// Prints what KaraSoft reads from a song file: header, MIDI summary and timed lyrics.
#include "../src/core/KaraokeSong.h"

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: karaparse <song.mid|song.emk> [lines]\n";
        return 1;
    }

    auto result = kara::loadSong (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    if (result.song == nullptr)
    {
        std::cout << "ERROR: " << result.error << "\n";
        return 2;
    }

    auto& s = *result.song;
    const int maxLines = argc > 2 ? juce::String (argv[2]).getIntValue() : 8;

    std::cout << "code:   " << s.code << "\n"
              << "title:  " << s.title << "\n"
              << "artist: " << s.artist << "\n"
              << "key:    " << s.key << "\n"
              << "ppq:    " << s.ppq << "  tracks: " << s.midi.getNumTracks()
              << "  vocal ch: " << s.vocalChannel + 1 << "\n"
              << "lines:  " << (int) s.lines.size() << "\n";

    for (int i = 0; i < juce::jmin (maxLines, (int) s.lines.size()); ++i)
    {
        auto& l = s.lines[(size_t) i];
        std::cout << juce::String (l.startTick(), 0).paddedLeft (' ', 7) << "  " << l.text()
                  << "  [" << (int) l.clusters.size() << " clusters]\n";
    }

    return 0;
}
