#include "ChannelPanel.h"

namespace kara
{

static const juce::Colour panelBg (0xf0141826), accent (0xff31d2ff);

MixerLookAndFeel::MixerLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a3048));
    setColour (juce::TextButton::buttonOnColourId, accent.darker (0.3f));
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent.withAlpha (0.6f));
    setColour (juce::Slider::backgroundColourId, juce::Colour (0xff0a0c14));
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Label::textColourId, juce::Colours::white);
}

void MixerLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);
        return;
    }

    // Fader: a slot with a cap-shaped thumb.
    const auto cx = x + w * 0.5f;
    g.setColour (juce::Colour (0xff05060a));
    g.fillRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f);
    g.setColour (accent.withAlpha (0.55f));
    g.fillRoundedRectangle (cx - 2.5f, pos, 5.0f, (float) (y + h) - pos, 2.5f);

    juce::Rectangle<float> cap (cx - w * 0.35f, pos - 7.0f, w * 0.7f, 14.0f);
    g.setColour (juce::Colour (0xffd8dce8));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff404858));
    g.drawHorizontalLine ((int) pos, cap.getX() + 2, cap.getRight() - 2);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff05060a));
    g.fillRoundedRectangle (r, 2.0f);

    const auto lit = r.withTop (r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, level));
    juce::ColourGradient grad (juce::Colour (0xffff4040), 0, r.getY(), juce::Colour (0xff40e070), 0, r.getBottom(), false);
    grad.addColour (0.3, juce::Colour (0xffffd040));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (lit, 2.0f);
}

//==============================================================================
ChannelStripView::ChannelStripView (KaraokeEngine& e, int ch) : engine (e), channel (ch)
{
    auto& c = engine.getChannel (channel);

    volume.setRange (0.0, 1.5, 0.01);
    volume.setValue (c.gain.load(), juce::dontSendNotification);
    volume.setDoubleClickReturnValue (true, 1.0);
    volume.onValueChange = [this] { engine.getChannel (channel).gain = (float) volume.getValue(); };

    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.onValueChange = [this]
    {
        // Pan applies to the strip(s) this channel feeds: one strip, or all drum parts.
        for (int s = 0; s < KaraokeEngine::numStrips; ++s)
            if (KaraokeEngine::stripMidiChannel (s) == channel)
                engine.getStrip (s).pan = (float) pan.getValue();
    };

    for (auto* b : { &mute, &solo })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd04040));
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0a020));
    mute.onClick = [this] { engine.getChannel (channel).mute = mute.getToggleState(); };
    solo.onClick = [this] { engine.getChannel (channel).solo = solo.getToggleState(); };

    addAndMakeVisible (volume);
    addAndMakeVisible (pan);
    addAndMakeVisible (meter);
}

void ChannelStripView::refresh()
{
    meter.setLevel (engine.getChannelActivity (channel));

    auto name = engine.getChannelInstrumentName (channel);
    if (name != instrument)
    {
        instrument = name;
        repaint();
    }
}

void ChannelStripView::resized()
{
    auto r = getLocalBounds().reduced (3);
    r.removeFromTop (34);
    auto buttons = r.removeFromBottom (20);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
    solo.setBounds (buttons.reduced (1, 0));
    r.removeFromBottom (2);
    pan.setBounds (r.removeFromTop (28));
    meter.setBounds (r.removeFromRight (6).reduced (0, 4));
    volume.setBounds (r);
}

void ChannelStripView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff1e2336));
    g.fillRoundedRectangle (r, 4.0f);

    const bool isMelody = engine.getSong() != nullptr && engine.getSong()->vocalChannel == channel;
    g.setColour (isMelody ? juce::Colour (0xffffd040) : accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String (channel + 1), r.removeFromTop (16), juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawFittedText (channel == KaraokeEngine::drumChannel ? juce::String ("Drums") : instrument,
                      r.removeFromTop (18).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
ChannelPanel::ChannelPanel (KaraokeEngine& e) : engine (e)
{
    setLookAndFeel (&lnf);

    for (int ch = 0; ch < KaraokeEngine::numMidiChannels; ++ch)
        addAndMakeVisible (strips.add (new ChannelStripView (engine, ch)));

    openButton.onClick  = [this] { if (onOpenSong) onOpenSong(); };
    playButton.onClick  = [this] { if (onPlayPause) onPlayPause(); };
    stopButton.onClick  = [this] { if (onStop) onStop(); };
    mixerButton.onClick = [this] { if (onShowSynthMixer) onShowSynthMixer(); };
    menuButton.onClick  = [this] { if (onShowMenu) onShowMenu(); };

    keyDown.onClick   = [this] { engine.setTranspose (engine.getTranspose() - 1); updateLabels(); };
    keyUp.onClick     = [this] { engine.setTranspose (engine.getTranspose() + 1); updateLabels(); };
    tempoDown.onClick = [this] { engine.setSpeed (engine.getSpeed() - 0.02); updateLabels(); };
    tempoUp.onClick   = [this] { engine.setSpeed (engine.getSpeed() + 0.02); updateLabels(); };

    melodyButton.setClickingTogglesState (true);
    melodyButton.setToggleState (engine.isGuideMelodyOn(), juce::dontSendNotification);
    melodyButton.onClick = [this] { engine.setGuideMelody (melodyButton.getToggleState()); };

    positionSlider.setRange (0.0, 1.0);
    positionSlider.onDragEnd = [this] { engine.seek (positionSlider.getValue() * engine.getLengthSeconds()); };

    masterSlider.setRange (0.0, 1.0, 0.01);
    masterSlider.setValue (engine.getMasterGain(), juce::dontSendNotification);
    masterSlider.onValueChange = [this] { engine.setMasterGain ((float) masterSlider.getValue()); };

    for (auto* l : { &keyLabel, &tempoLabel, &timeLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (l);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &openButton, &playButton, &stopButton, &keyDown, &keyUp,
                                                              &tempoDown, &tempoUp, &melodyButton, &mixerButton,
                                                              &menuButton, &positionSlider, &masterSlider })
        addAndMakeVisible (c);

    updateLabels();
}

ChannelPanel::~ChannelPanel()
{
    setLookAndFeel (nullptr);
}

void ChannelPanel::updateLabels()
{
    const auto t = engine.getTranspose();
    keyLabel.setText (juce::String::fromUTF8 ("คีย์ ") + (t > 0 ? "+" : "") + juce::String (t), juce::dontSendNotification);
    tempoLabel.setText (juce::String::fromUTF8 ("จังหวะ ") + juce::String (juce::roundToInt (engine.getSpeed() * 100)) + "%",
                        juce::dontSendNotification);
}

void ChannelPanel::refresh()
{
    for (auto* s : strips)
        s->refresh();

    const auto pos = engine.getPositionSeconds(), len = engine.getLengthSeconds();
    if (! positionSlider.isMouseButtonDown() && len > 0)
        positionSlider.setValue (pos / len, juce::dontSendNotification);

    auto fmt = [] (double s) { return juce::String::formatted ("%d:%02d", (int) s / 60, (int) s % 60); };
    timeLabel.setText (fmt (pos) + " / " + fmt (juce::jmax (0.0, len - 1.0)), juce::dontSendNotification);
    playButton.setButtonText (engine.isPlaying() ? "Pause" : "Play");
    melodyButton.setToggleState (engine.isGuideMelodyOn(), juce::dontSendNotification);
    updateLabels();
}

void ChannelPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto bar = r.removeFromBottom (28);
    r.removeFromBottom (6);

    openButton.setBounds (bar.removeFromLeft (80).reduced (2));
    playButton.setBounds (bar.removeFromLeft (64).reduced (2));
    stopButton.setBounds (bar.removeFromLeft (56).reduced (2));
    menuButton.setBounds (bar.removeFromRight (70).reduced (2));
    mixerButton.setBounds (bar.removeFromRight (100).reduced (2));
    masterSlider.setBounds (bar.removeFromRight (110).reduced (2));
    melodyButton.setBounds (bar.removeFromRight (64).reduced (2));
    tempoUp.setBounds (bar.removeFromRight (26).reduced (2));
    tempoLabel.setBounds (bar.removeFromRight (90));
    tempoDown.setBounds (bar.removeFromRight (26).reduced (2));
    keyUp.setBounds (bar.removeFromRight (26).reduced (2));
    keyLabel.setBounds (bar.removeFromRight (60));
    keyDown.setBounds (bar.removeFromRight (26).reduced (2));
    timeLabel.setBounds (bar.removeFromRight (100));
    positionSlider.setBounds (bar.reduced (4, 2));

    const auto w = r.getWidth() / KaraokeEngine::numMidiChannels;
    for (auto* s : strips)
        s->setBounds (r.removeFromLeft (w));
}

void ChannelPanel::paint (juce::Graphics& g)
{
    g.setColour (panelBg);
    g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedTop (-8.0f), 8.0f);
    g.setColour (accent.withAlpha (0.5f));
    g.drawHorizontalLine (getHeight() - 1, 8.0f, getWidth() - 8.0f);
}

} // namespace kara
