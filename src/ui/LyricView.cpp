#include "LyricView.h"

namespace kara
{

LyricView::LyricView()
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
}

void LyricView::setSong (const KaraokeSong* s)
{
    song = s;
    tick = 0.0;
    layoutCache.clear();
    repaint();
}

void LyricView::setPosition (double newTick)
{
    if (! juce::exactlyEqual (newTick, tick))
    {
        tick = newTick;
        repaint();
    }
}

void LyricView::setStyle (const LyricStyle& s)
{
    style = s;
    layoutCache.clear();
    repaint();
}

void LyricView::setBackground (const juce::Image& image)
{
    background = image;
    repaint();
}

void LyricView::setStatusText (const juce::String& text)
{
    status = text;
    repaint();
}

juce::Font LyricView::makeFont (float height) const
{
    return juce::Font (juce::FontOptions (style.fontName, height, style.bold ? juce::Font::bold : juce::Font::plain));
}

const LyricView::LineLayout& LyricView::layoutFor (int line)
{
    if (auto it = layoutCache.find (line); it != layoutCache.end())
        return it->second;

    LineLayout layout;
    const auto font = makeFont (referenceHeight);
    juce::String prefix;
    layout.clusterX.push_back (0.0f);

    // Measure prefixes so Thai shaping across clusters is taken into account.
    for (auto& c : song->lines[(size_t) line].clusters)
    {
        prefix << c.text;
        layout.clusterX.push_back (juce::GlyphArrangement::getStringWidth (font, prefix));
    }

    return layoutCache[line] = std::move (layout);
}

float LyricView::wipeWidth (int line, const LineLayout& layout) const
{
    const auto& clusters = song->lines[(size_t) line].clusters;
    if (clusters.empty() || tick < clusters.front().tick)
        return 0.0f;

    const double maxClusterTicks = song->ppq;  // a single syllable never wipes slower than one beat
    const double lineEndTick = (size_t) line + 1 < song->lines.size() ? song->lines[(size_t) line + 1].startTick()
                                                                      : clusters.back().tick + maxClusterTicks;

    for (size_t i = 0; i < clusters.size(); ++i)
    {
        const auto start = clusters[i].tick;
        auto end = i + 1 < clusters.size() ? clusters[i + 1].tick : lineEndTick;
        end = juce::jmin (end, start + maxClusterTicks);

        if (tick < (i + 1 < clusters.size() ? clusters[i + 1].tick : lineEndTick))
        {
            const auto frac = end > start ? juce::jlimit (0.0, 1.0, (tick - start) / (end - start)) : 1.0;
            return layout.clusterX[i] + (float) frac * (layout.clusterX[i + 1] - layout.clusterX[i]);
        }
    }

    return layout.clusterX.back();
}

void LyricView::drawOutlinedText (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                                  float x, float baseline, juce::Colour fill, juce::Colour outline, float thickness)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, x, baseline);
    juce::Path path;
    ga.createPath (path);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (path, juce::AffineTransform::translation (thickness * 0.6f, thickness * 0.8f));

    g.setColour (outline);
    g.strokePath (path, juce::PathStrokeType (thickness * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (fill);
    g.fillPath (path);
}

void LyricView::drawLine (juce::Graphics& g, int line, juce::Rectangle<float> area, bool alignRight)
{
    const auto& layout = layoutFor (line);
    const auto text = song->lines[(size_t) line].text();
    const auto refWidth = layout.clusterX.back();

    auto height = area.getHeight();
    const auto margin = getWidth() * 0.05f;
    if (refWidth * height / referenceHeight > getWidth() - 2 * margin)
        height = (getWidth() - 2 * margin) * referenceHeight / refWidth;

    const auto scale = height / referenceHeight;
    const auto font = makeFont (height);
    const auto width = refWidth * scale;
    const auto x = alignRight ? getWidth() - margin - width : margin;
    const auto baseline = area.getBottom() - area.getHeight() * 0.22f;
    const auto thickness = juce::jmax (1.5f, height * style.outline);

    drawOutlinedText (g, text, font, x, baseline, style.unsungFill, style.unsungOutline, thickness);

    const auto wipe = wipeWidth (line, layout) * scale;
    if (wipe > 0.0f)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<float> (x - thickness * 2, area.getY() - height, wipe + thickness * 2, height * 3)
                                .getSmallestIntegerContainer());
        drawOutlinedText (g, text, font, x, baseline, style.sungFill, style.sungOutline, thickness);
    }
}

void LyricView::paintBackground (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
        return;
    }

    juce::ColourGradient grad (juce::Colour (0xff0b1240), 0, 0, juce::Colour (0xff3a0d4d), 0, (float) getHeight(), false);
    grad.addColour (0.55, juce::Colour (0xff142a6e));
    g.setGradientFill (grad);
    g.fillAll();
}

void LyricView::paintTitleCard (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (getWidth() * 0.06f, getHeight() * 0.12f);
    area.removeFromBottom (getHeight() * 0.42f);

    const auto h = getHeight() * style.sizeFraction;
    auto titleFont = makeFont (h * 1.25f);
    const auto titleW = juce::GlyphArrangement::getStringWidth (titleFont, song->title);
    if (titleW > area.getWidth())
        titleFont = makeFont (h * 1.25f * area.getWidth() / titleW);

    const auto drawCentred = [&] (const juce::String& text, const juce::Font& font, float baseline, juce::Colour fill)
    {
        const auto w = juce::GlyphArrangement::getStringWidth (font, text);
        drawOutlinedText (g, text, font, getWidth() * 0.5f - w * 0.5f, baseline, fill, style.unsungOutline,
                          juce::jmax (1.5f, font.getHeight() * style.outline));
    };

    drawCentred (song->title, titleFont, area.getY() + titleFont.getHeight(), juce::Colour (0xffffe066));
    drawCentred (song->artist, makeFont (h * 0.6f), area.getY() + titleFont.getHeight() + h * 1.0f, juce::Colours::white);

    if (song->key.isNotEmpty())
        drawCentred ("Key: " + song->key, makeFont (h * 0.45f), area.getY() + titleFont.getHeight() + h * 1.7f,
                     juce::Colour (0xffb8e0ff));
}

void LyricView::paint (juce::Graphics& g)
{
    paintBackground (g);

    if (song == nullptr || song->lines.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (makeFont (getHeight() * 0.045f));
        g.drawFittedText (status.isNotEmpty() ? status : juce::String ("KaraSoft"), getLocalBounds().reduced (40),
                          juce::Justification::centred, 3);
        return;
    }

    const auto& lines = song->lines;
    const int numLines = (int) lines.size();

    // The current line is the last one whose first syllable has started.
    int current = 0;
    while (current + 1 < numLines && lines[(size_t) current + 1].startTick() <= tick)
        ++current;

    const bool introPlaying = tick < lines.front().startTick();
    if (introPlaying)
        paintTitleCard (g);

    // Lines alternate between the upper and lower row; the row whose line has finished
    // already shows the next line, so the singer can read ahead.
    const auto rowHeight = getHeight() * style.sizeFraction * 1.45f;
    const auto bottom = getHeight() * 0.93f;
    juce::Rectangle<float> rows[2] = { { 0.0f, bottom - rowHeight * 2.1f, (float) getWidth(), rowHeight },
                                       { 0.0f, bottom - rowHeight,        (float) getWidth(), rowHeight } };

    for (int row = 0; row < 2; ++row)
    {
        const int line = (current % 2 == row) ? current : current + 1;
        if (line < numLines)
            drawLine (g, line, rows[row], row == 1);
    }

    if (status.isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (makeFont (getHeight() * 0.03f));
        g.drawText (status, getLocalBounds().reduced (16).removeFromTop (40), juce::Justification::topRight);
    }
}

} // namespace kara
