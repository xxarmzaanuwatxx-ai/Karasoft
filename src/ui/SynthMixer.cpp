#include "SynthMixer.h"

namespace kara
{

static constexpr int stripWidth = 64;

//==============================================================================
SynthStripView::SynthStripView (SynthMixer& o, KaraokeEngine& e, int s) : owner (o), engine (e), strip (s)
{
    auto& st = engine.getStrip (strip);

    fader.setRange (0.0, 1.5, 0.01);
    fader.setValue (st.gain.load(), juce::dontSendNotification);
    fader.setDoubleClickReturnValue (true, 1.0);
    fader.onValueChange = [this] { engine.getStrip (strip).gain = (float) fader.getValue(); };

    pan.setRange (-1.0, 1.0, 0.01);
    pan.setValue (st.pan.load(), juce::dontSendNotification);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.onValueChange = [this] { engine.getStrip (strip).pan = (float) pan.getValue(); };

    for (auto* b : { &mute, &solo })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd04040));
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0a020));
    mute.setToggleState (st.mute.load(), juce::dontSendNotification);
    solo.setToggleState (st.solo.load(), juce::dontSendNotification);
    mute.onClick = [this] { engine.getStrip (strip).mute = mute.getToggleState(); };
    solo.onClick = [this] { engine.getStrip (strip).solo = solo.getToggleState(); };

    for (int i = 0; i < MixerStrip::numInsertSlots; ++i)
    {
        auto* b = slots.add (new juce::TextButton());
        b->onClick = [this, i] { showSlotMenu (i); };
        addAndMakeVisible (b);
        updateSlotButton (i);
    }

    addAndMakeVisible (fader);
    addAndMakeVisible (pan);
    addAndMakeVisible (meterL);
    addAndMakeVisible (meterR);
}

void SynthStripView::updateSlotButton (int slot)
{
    auto& st = engine.getStrip (strip);
    auto* b = slots[slot];

    if (auto& p = st.inserts[(size_t) slot])
    {
        b->setButtonText (p->getName());
        b->setColour (juce::TextButton::buttonColourId,
                      st.bypass[(size_t) slot] ? juce::Colour (0xff4a4a4a) : juce::Colour (0xff2d6a4f));
    }
    else
    {
        b->setButtonText ("VST");
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23283a));
    }
}

void SynthStripView::showSlotMenu (int slot)
{
    auto& st = engine.getStrip (strip);
    const bool loaded = st.inserts[(size_t) slot] != nullptr;
    auto types = owner.getKnownPlugins().getTypes();

    juce::PopupMenu menu, pluginMenu;
    for (int i = 0; i < types.size(); ++i)
        if (types[i].isInstrument == false)
            pluginMenu.addItem (1000 + i, types[i].name + " (" + types[i].manufacturerName + ")");

    if (loaded)
    {
        menu.addItem (1, juce::String::fromUTF8 ("เปิดหน้าปลั๊กอิน"));
        menu.addItem (2, "Bypass", true, st.bypass[(size_t) slot].load());
        menu.addItem (3, juce::String::fromUTF8 ("ถอดปลั๊กอิน"));
        menu.addSeparator();
    }

    menu.addSubMenu (juce::String::fromUTF8 ("ใส่ปลั๊กอิน"), pluginMenu, types.size() > 0);
    menu.addItem (4, juce::String::fromUTF8 ("ค้นหาปลั๊กอิน VST..."));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slots[slot]),
                        [this, slot, types] (int result)
    {
        auto& s = engine.getStrip (strip);

        if (result == 1)       owner.openPluginEditor (strip, slot);
        else if (result == 2)  s.bypass[(size_t) slot] = ! s.bypass[(size_t) slot].load();
        else if (result == 3)  { owner.closePluginEditor (strip, slot); engine.clearInsert (strip, slot); }
        else if (result == 4)  owner.showPluginScanner();
        else if (result >= 1000)
        {
            owner.closePluginEditor (strip, slot);
            juce::String error;
            if (! engine.loadInsert (strip, slot, types[result - 1000], error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST", error);
            else
                owner.openPluginEditor (strip, slot);
        }

        updateSlotButton (slot);
    });
}

void SynthStripView::refresh()
{
    auto& st = engine.getStrip (strip);
    meterL.setLevel (st.peakL.load());
    meterR.setLevel (st.peakR.load());

    juce::String newName;
    if (strip >= KaraokeEngine::numMidiChannels)
        newName = KaraokeEngine::getDrumPartName (strip - KaraokeEngine::numMidiChannels);
    else if (strip == KaraokeEngine::drumChannel)
        newName = "(Drums)";
    else
        newName = engine.getChannelInstrumentName (strip);

    if (newName != name)
    {
        name = newName;
        repaint();
    }
}

void SynthStripView::resized()
{
    auto r = getLocalBounds().reduced (3);
    r.removeFromTop (32);

    for (auto* b : slots)
    {
        b->setBounds (r.removeFromTop (20).reduced (0, 1));
    }
    r.removeFromTop (4);
    pan.setBounds (r.removeFromTop (30));

    auto buttons = r.removeFromBottom (20);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
    solo.setBounds (buttons.reduced (1, 0));
    r.removeFromBottom (4);

    meterR.setBounds (r.removeFromRight (5).reduced (0, 4));
    r.removeFromRight (1);
    meterL.setBounds (r.removeFromRight (5).reduced (0, 4));
    fader.setBounds (r);
}

void SynthStripView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool drumPart = strip >= KaraokeEngine::numMidiChannels;
    g.setColour (drumPart ? juce::Colour (0xff2a2236) : juce::Colour (0xff1e2336));
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (drumPart ? juce::Colour (0xffe08cff) : juce::Colour (0xff31d2ff));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (juce::String (strip + 1), r.removeFromTop (15), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawFittedText (name, r.removeFromTop (16).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
SynthMixer::SynthMixer (KaraokeEngine& e, AppSettings& s) : engine (e), settings (s)
{
    setLookAndFeel (&lnf);

    if (auto xml = settings.props().getXmlValue ("knownPlugins"))
        knownPlugins.recreateFromXml (*xml);

    for (int i = 0; i < KaraokeEngine::numStrips; ++i)
        content.addAndMakeVisible (strips.add (new SynthStripView (*this, engine, i)));

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    setSize (1280, 460);
}

SynthMixer::~SynthMixer()
{
    editors.clear();
    setLookAndFeel (nullptr);
}

void SynthMixer::saveKnownPlugins()
{
    if (auto xml = knownPlugins.createXml())
        settings.props().setValue ("knownPlugins", xml.get());
}

void SynthMixer::showPluginScanner()
{
    struct ScannerWindow : public juce::DocumentWindow
    {
        ScannerWindow (SynthMixer& m)
            : DocumentWindow (juce::String::fromUTF8 ("ปลั๊กอิน VST"), juce::Colour (0xff1e2336), closeButton), mixer (m)
        {
            auto deadMansPedal = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                     .getChildFile ("KaraSoft").getChildFile ("plugin-scan.tmp");
            setContentOwned (new juce::PluginListComponent (m.engine.getPluginFormats(), m.knownPlugins,
                                                            deadMansPedal, &m.settings.props()), true);
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            centreWithSize (700, 500);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            mixer.saveKnownPlugins();
            delete this;
        }

        SynthMixer& mixer;
    };

    new ScannerWindow (*this);
}

void SynthMixer::openPluginEditor (int strip, int slot)
{
    auto& p = engine.getStrip (strip).inserts[(size_t) slot];
    if (p == nullptr)
        return;

    auto key = std::make_pair (strip, slot);
    if (auto it = editors.find (key); it != editors.end())
    {
        it->second->toFront (true);
        return;
    }

    struct EditorWindow : public juce::DocumentWindow
    {
        EditorWindow (SynthMixer& m, std::pair<int, int> k, juce::AudioProcessorEditor* editor, const juce::String& title)
            : DocumentWindow (title, juce::Colours::black, closeButton), mixer (m), key (k)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (editor, true);
            setVisible (true);
            centreWithSize (getWidth(), getHeight());
        }

        void closeButtonPressed() override
        {
            juce::MessageManager::callAsync ([&m = mixer, k = key] { m.closePluginEditor (k.first, k.second); });
        }

        SynthMixer& mixer;
        std::pair<int, int> key;
    };

    if (auto* editor = p->createEditorIfNeeded())
        editors[key] = std::make_unique<EditorWindow> (*this, key, editor,
                                                       p->getName() + " - Strip " + juce::String (strip + 1));
}

void SynthMixer::closePluginEditor (int strip, int slot)
{
    editors.erase (std::make_pair (strip, slot));
}

void SynthMixer::refresh()
{
    for (auto* s : strips)
        s->refresh();
}

void SynthMixer::resized()
{
    viewport.setBounds (getLocalBounds());
    const auto h = getHeight() - (viewport.canScrollHorizontally() ? viewport.getScrollBarThickness() : 0);
    const auto w = juce::jmax (stripWidth, getWidth() / KaraokeEngine::numStrips);
    content.setSize (w * KaraokeEngine::numStrips, h);

    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (i * w, 0, w, h);
}

void SynthMixer::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141826));
}

} // namespace kara
