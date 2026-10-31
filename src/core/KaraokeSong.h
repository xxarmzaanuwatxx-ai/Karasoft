#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace kara
{

/** One display unit of lyric text. Thai combining marks (upper/lower vowels and
    tone marks) are grouped with their base consonant so they wipe together. */
struct LyricCluster
{
    juce::String text;  // UTF-8
    double tick = 0.0;  // MIDI tick at which the highlight reaches this cluster
};

struct LyricLine
{
    std::vector<LyricCluster> clusters;
    juce::String text() const;
    double startTick() const { return clusters.empty() ? 0.0 : clusters.front().tick; }
};

/** A karaoke song decoded from either an NCN set (.mid + .lyr + .cur) or an .emk file. */
struct KaraokeSong
{
    juce::String code, title, artist, key;
    juce::File sourceFile;
    int vocalChannel = 8;  // zero-based MIDI channel of the guide melody (NCN/EMK default: ch 9)

    juce::MemoryBlock midiData;
    juce::MidiFile midi;
    int ppq = 480;

    std::vector<LyricLine> lines;

    bool isValid() const { return midi.getNumTracks() > 0; }
};

struct LoadResult
{
    std::unique_ptr<KaraokeSong> song;
    juce::String error;
};

/** Loads any supported file. For NCN pass the .mid (or .lyr/.cur); siblings are located
    automatically, including the standard NCN Song/Lyrics/Cursor folder layout. */
LoadResult loadSong (const juce::File& file);

LoadResult loadNcn (const juce::File& midFile);
LoadResult loadEmk (const juce::File& emkFile);

/** Decodes TIS-620 / Windows-874 bytes to a Unicode string. */
juce::String decodeThai (const void* data, size_t numBytes);

/** True for TIS-620 bytes that combine with the preceding character. */
bool isThaiCombining (uint8_t tisByte);

/** Builds lyric lines from raw .lyr bytes and the cursor table (one u16 per lyric byte,
    in units of ppq/24 ticks). Fills title/artist/key from the 4-line header. */
void buildLyrics (KaraokeSong& song, const juce::MemoryBlock& lyr, const juce::MemoryBlock& cur);

} // namespace kara
