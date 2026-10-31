#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/KaraokeSong.h"

namespace kara
{

struct LyricStyle
{
    juce::String fontName { "Tahoma" };
    float sizeFraction = 0.11f;   // text height relative to view height
    bool bold = true;
    float outline = 0.07f;        // outline thickness relative to text height
    juce::Colour unsungFill { 0xffffffff }, unsungOutline { 0xff1d2a6b };
    juce::Colour sungFill { 0xff31d2ff }, sungOutline { 0xffffffff };
};

/** Full-screen karaoke lyric display: two alternating lines with a left-to-right colour
    wipe driven by the song position in MIDI ticks. */
class LyricView : public juce::Component
{
public:
    LyricView();

    void setSong (const KaraokeSong* song);
    void setPosition (double tick);
    void setStyle (const LyricStyle& s);
    const LyricStyle& getStyle() const { return style; }
    void setBackground (const juce::Image& image);
    void setStatusText (const juce::String& text);

    void paint (juce::Graphics&) override;
    void resized() override { layoutCache.clear(); }

private:
    struct LineLayout
    {
        std::vector<float> clusterX;   // left edge of each cluster at reference height, plus total width
    };

    juce::Font makeFont (float height) const;
    const LineLayout& layoutFor (int line);
    float wipeWidth (int line, const LineLayout&) const;
    void drawLine (juce::Graphics&, int line, juce::Rectangle<float> area, bool alignRight);
    void drawOutlinedText (juce::Graphics&, const juce::String& text, const juce::Font&, float x, float baseline,
                           juce::Colour fill, juce::Colour outline, float thickness);
    void paintBackground (juce::Graphics&);
    void paintTitleCard (juce::Graphics&);

    const KaraokeSong* song = nullptr;
    double tick = 0.0;
    LyricStyle style;
    juce::Image background;
    juce::String status;
    std::map<int, LineLayout> layoutCache;

    static constexpr float referenceHeight = 100.0f;
};

} // namespace kara
