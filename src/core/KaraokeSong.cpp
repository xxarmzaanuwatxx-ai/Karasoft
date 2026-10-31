#include "KaraokeSong.h"

namespace kara
{

juce::String LyricLine::text() const
{
    juce::String s;
    for (auto& c : clusters)
        s << c.text;
    return s;
}

//==============================================================================
juce::String decodeThai (const void* data, size_t numBytes)
{
    auto* bytes = static_cast<const uint8_t*> (data);
    juce::String out;
    out.preallocateBytes (numBytes * 3);

    for (size_t i = 0; i < numBytes; ++i)
    {
        const auto b = bytes[i];
        juce::juce_wchar ch;

        if (b < 0x80)                       ch = b;
        else if (b >= 0xa1 && b <= 0xfb)    ch = 0x0e01 + (b - 0xa1);
        else if (b == 0xa0)                 ch = 0x00a0;
        else
        {
            switch (b)
            {
                case 0x80: ch = 0x20ac; break;
                case 0x85: ch = 0x2026; break;
                case 0x91: ch = 0x2018; break;
                case 0x92: ch = 0x2019; break;
                case 0x93: ch = 0x201c; break;
                case 0x94: ch = 0x201d; break;
                case 0x95: ch = 0x2022; break;
                case 0x96: ch = 0x2013; break;
                case 0x97: ch = 0x2014; break;
                default:   ch = '?';    break;
            }
        }

        out += juce::String::charToString (ch);
    }

    return out;
}

bool isThaiCombining (uint8_t b)
{
    return b == 0xd1 || (b >= 0xd4 && b <= 0xda) || (b >= 0xe7 && b <= 0xee);
}

//==============================================================================
static std::vector<juce::MemoryBlock> splitLines (const juce::MemoryBlock& data)
{
    std::vector<juce::MemoryBlock> lines;
    auto* p = static_cast<const char*> (data.getData());
    const auto n = data.getSize();
    size_t start = 0;

    for (size_t i = 0; i <= n; ++i)
    {
        if (i == n || p[i] == '\n')
        {
            auto end = i;
            if (end > start && p[end - 1] == '\r')
                --end;
            lines.emplace_back (p + start, end - start);
            start = i + 1;
        }
    }

    if (! lines.empty() && lines.back().isEmpty())
        lines.pop_back();

    return lines;
}

void buildLyrics (KaraokeSong& song, const juce::MemoryBlock& lyr, const juce::MemoryBlock& cur)
{
    auto rawLines = splitLines (lyr);

    auto header = [&] (size_t i)
    {
        return i < rawLines.size() ? decodeThai (rawLines[i].getData(), rawLines[i].getSize()).trim()
                                   : juce::String();
    };

    if (song.title.isEmpty())  song.title  = header (0);
    if (song.artist.isEmpty()) song.artist = header (1);
    if (song.key.isEmpty())    song.key    = header (2);

    auto* cursor = static_cast<const uint8_t*> (cur.getData());
    const auto numCursor = cur.getSize() / 2;
    const double tickScale = song.ppq / 24.0;
    size_t cursorIndex = 0;
    double lastTick = 0.0;

    auto tickAt = [&] (size_t idx)
    {
        if (idx < numCursor)
            lastTick = juce::jmax (lastTick, (cursor[idx * 2] | (cursor[idx * 2 + 1] << 8)) * tickScale);
        return lastTick;
    };

    song.lines.clear();

    for (size_t li = 4; li < rawLines.size(); ++li)
    {
        auto* bytes = static_cast<const uint8_t*> (rawLines[li].getData());
        const auto len = rawLines[li].getSize();
        LyricLine line;

        for (size_t i = 0; i < len;)
        {
            const auto clusterStart = i;
            const auto tick = tickAt (cursorIndex + i);
            ++i;
            while (i < len && isThaiCombining (bytes[i]))
                ++i;

            line.clusters.push_back ({ decodeThai (bytes + clusterStart, i - clusterStart), tick });
        }

        cursorIndex += len;

        if (! line.clusters.empty())
            song.lines.push_back (std::move (line));
    }
}

//==============================================================================
static bool readMidi (KaraokeSong& song, juce::String& error)
{
    juce::MemoryInputStream in (song.midiData, false);

    if (! song.midi.readFrom (in))
    {
        error = "Invalid MIDI data";
        return false;
    }

    song.ppq = song.midi.getTimeFormat() > 0 ? song.midi.getTimeFormat() : 480;
    return true;
}

static juce::File findSibling (const juce::File& base, const juce::String& folderName, const juce::String& ext)
{
    const auto name = base.getFileNameWithoutExtension();
    const auto dir = base.getParentDirectory();

    for (auto e : { ext.toLowerCase(), ext.toUpperCase() })
    {
        for (auto candidate : { dir.getChildFile (name + e),
                                dir.getSiblingFile (folderName).getChildFile (name + e),
                                dir.getSiblingFile (folderName.toUpperCase()).getChildFile (name + e) })
            if (candidate.existsAsFile())
                return candidate;
    }

    return {};
}

LoadResult loadNcn (const juce::File& file)
{
    LoadResult result;
    auto midFile = file.hasFileExtension ("mid") ? file : findSibling (file, "Song", ".mid");
    auto lyrFile = findSibling (midFile.existsAsFile() ? midFile : file, "Lyrics", ".lyr");
    auto curFile = findSibling (midFile.existsAsFile() ? midFile : file, "Cursor", ".cur");

    if (! midFile.existsAsFile())
    {
        result.error = "MIDI file not found for " + file.getFileName();
        return result;
    }

    auto song = std::make_unique<KaraokeSong>();
    song->sourceFile = midFile;
    song->code = midFile.getFileNameWithoutExtension();

    if (! midFile.loadFileAsData (song->midiData) || ! readMidi (*song, result.error))
    {
        if (result.error.isEmpty())
            result.error = "Cannot read " + midFile.getFullPathName();
        return result;
    }

    juce::MemoryBlock lyr, cur;
    if (lyrFile.existsAsFile()) lyrFile.loadFileAsData (lyr);
    if (curFile.existsAsFile()) curFile.loadFileAsData (cur);

    buildLyrics (*song, lyr, cur);
    result.song = std::move (song);
    return result;
}

//==============================================================================
namespace emk
{
    // EMK files are XOR-obfuscated with a repeating 8-byte key.
    static const uint8_t xorKey[8] = { 0xaf, 0xf2, 0x4c, 0x9c, 0xe9, 0xea, 0x99, 0x43 };

    struct Entry
    {
        juce::String tag;
        uint64_t start = 0, end = 0;
    };

    /** Reads a typed value from the directory: 2=u8, 3=u16, 4=u32, 6=string. */
    struct DirReader
    {
        const uint8_t* p;
        size_t size, pos = 0;

        bool readTagged (int& type, uint64_t& value, juce::String& str)
        {
            if (pos >= size) return false;
            type = p[pos++];
            value = 0;

            auto readInt = [&] (int numBytes)
            {
                if (pos + (size_t) numBytes > size) return false;
                for (int i = 0; i < numBytes; ++i)
                    value |= (uint64_t) p[pos + (size_t) i] << (8 * i);
                pos += (size_t) numBytes;
                return true;
            };

            switch (type)
            {
                case 2: return readInt (1);
                case 3: return readInt (2);
                case 4: return readInt (4);
                case 6:
                {
                    if (pos >= size) return false;
                    const auto len = p[pos++];
                    if (pos + len > size) return false;
                    str = juce::String::fromUTF8 (reinterpret_cast<const char*> (p + pos), len);
                    pos += len;
                    return true;
                }
                default: return false;
            }
        }
    };

    static std::vector<Entry> parseDirectory (const juce::MemoryBlock& data)
    {
        std::vector<Entry> entries;
        auto* d = static_cast<const uint8_t*> (data.getData());
        const auto n = data.getSize();

        if (n < 0x32)
            return entries;

        uint64_t dirStart = 0, dirEnd = 0;
        for (int i = 7; i >= 0; --i)
        {
            dirStart = (dirStart << 8) | d[0x22 + i];
            dirEnd   = (dirEnd   << 8) | d[0x2a + i];
        }

        if (dirStart >= n || dirEnd > n || dirEnd <= dirStart)
            return entries;

        // Each entry: "SFDS" tag-string uncompressed-size ? start end ... md5[16] ...
        for (auto pos = dirStart; pos + 4 < dirEnd; ++pos)
        {
            if (std::memcmp (d + pos, "SFDS", 4) != 0)
                continue;

            DirReader r { d + pos + 4, (size_t) (dirEnd - pos - 4) };
            int type; uint64_t v; juce::String s;
            Entry e;
            std::vector<uint64_t> ints;

            if (! r.readTagged (type, v, s) || type != 6)
                continue;

            e.tag = s;
            while (ints.size() < 4 && r.readTagged (type, v, s) && type != 6)
                ints.push_back (v);

            if (ints.size() == 4)
            {
                e.start = ints[2];
                e.end = ints[3];
                if (e.start < e.end && e.end <= n)
                    entries.push_back (e);
            }
        }

        return entries;
    }

    static juce::MemoryBlock inflate (const juce::MemoryBlock& data, size_t start, size_t end)
    {
        juce::MemoryInputStream raw (static_cast<const char*> (data.getData()) + start, end - start, false);
        juce::GZIPDecompressorInputStream z (&raw, false, juce::GZIPDecompressorInputStream::zlibFormat);
        juce::MemoryBlock out;
        z.readIntoMemoryBlock (out);
        return out;
    }
}

LoadResult loadEmk (const juce::File& file)
{
    LoadResult result;
    juce::MemoryBlock data;

    if (! file.loadFileAsData (data))
    {
        result.error = "Cannot read " + file.getFullPathName();
        return result;
    }

    auto* d = static_cast<uint8_t*> (data.getData());
    for (size_t i = 0; i < data.getSize(); ++i)
        d[i] ^= emk::xorKey[i % 8];

    if (data.getSize() < 5 || std::memcmp (d, ".SFDS", 5) != 0)
    {
        result.error = "Not an EMK file: " + file.getFileName();
        return result;
    }

    auto song = std::make_unique<KaraokeSong>();
    song->sourceFile = file;
    song->code = file.getFileNameWithoutExtension();
    juce::MemoryBlock lyr, cur;

    for (auto& e : emk::parseDirectory (data))
    {
        auto block = emk::inflate (data, (size_t) e.start, (size_t) e.end);

        if (e.tag == "SONG_INFO")
        {
            juce::StringArray infoLines;
            infoLines.addLines (decodeThai (block.getData(), block.getSize()));

            for (auto& l : infoLines)
            {
                auto k = l.upToFirstOccurrenceOf ("=", false, false).trim();
                auto v = l.fromFirstOccurrenceOf ("=", false, false).trim();

                if (k == "CODE")               song->code = v;
                else if (k == "TITLE")         song->title = v;
                else if (k == "ARTIST")        song->artist = v;
                else if (k == "KEY")           song->key = v;
                else if (k == "VOCAL_CHANNEL" && v.getIntValue() > 0)
                    song->vocalChannel = v.getIntValue() - 1;
            }
        }
        else if (e.tag == "MIDI_DATA")   song->midiData = std::move (block);
        else if (e.tag == "LYRIC_DATA")  lyr = std::move (block);
        else if (e.tag == "CURSOR_DATA") cur = std::move (block);
    }

    if (song->midiData.isEmpty())
    {
        result.error = "EMK has no MIDI data: " + file.getFileName();
        return result;
    }

    if (! readMidi (*song, result.error))
        return result;

    buildLyrics (*song, lyr, cur);
    result.song = std::move (song);
    return result;
}

LoadResult loadSong (const juce::File& file)
{
    if (file.hasFileExtension ("emk"))
        return loadEmk (file);

    return loadNcn (file);
}

} // namespace kara
