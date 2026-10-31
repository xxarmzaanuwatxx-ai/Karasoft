#include "MainComponent.h"

namespace kara
{

static juce::String th (const char* utf8) { return juce::String::fromUTF8 (utf8); }

static constexpr int panelHeight = 270;

//==============================================================================
/** Font, size and colour settings for the lyrics. Changes apply immediately. */
class FontSettingsComponent : public juce::Component
{
public:
    FontSettingsComponent (AppSettings& s, std::function<void()> changed) : settings (s), onChange (std::move (changed))
    {
        fonts.addItemList (juce::Font::findAllTypefaceNames(), 1);
        fonts.setText (settings.getFontName(), juce::dontSendNotification);
        fonts.onChange = [this] { settings.props().setValue ("fontName", fonts.getText()); onChange(); };

        size.setRange (0.05, 0.2, 0.005);
        size.setValue (settings.getFontSize(), juce::dontSendNotification);
        size.onValueChange = [this] { settings.props().setValue ("fontSize", size.getValue()); onChange(); };

        bold.setToggleState (settings.isFontBold(), juce::dontSendNotification);
        bold.onClick = [this] { settings.props().setValue ("fontBold", bold.getToggleState()); onChange(); };

        const LyricStyle defaults;
        addColour ("unsungFill", th ("สีตัวอักษร"), defaults.unsungFill);
        addColour ("unsungOutline", th ("สีขอบตัวอักษร"), defaults.unsungOutline);
        addColour ("sungFill", th ("สีที่ร้องแล้ว"), defaults.sungFill);
        addColour ("sungOutline", th ("สีขอบที่ร้องแล้ว"), defaults.sungOutline);

        for (auto* l : { &fontLabel, &sizeLabel })
            addAndMakeVisible (l);
        addAndMakeVisible (fonts);
        addAndMakeVisible (size);
        addAndMakeVisible (bold);
        setSize (420, 250);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto row = [&] { return r.removeFromTop (30); };

        auto a = row(); fontLabel.setBounds (a.removeFromLeft (110)); fonts.setBounds (a.reduced (0, 2));
        auto b = row(); sizeLabel.setBounds (b.removeFromLeft (110)); size.setBounds (b);
        bold.setBounds (row().withTrimmedLeft (110));
        r.removeFromTop (6);

        for (int i = 0; i < colourButtons.size(); ++i)
            colourButtons[i]->setBounds (r.removeFromTop (30).reduced (0, 2));
    }

private:
    void addColour (const juce::String& key, const juce::String& label, juce::Colour fallback)
    {
        auto* b = colourButtons.add (new juce::TextButton (label));
        auto refresh = [this, b, key, fallback]
        {
            const auto c = settings.getColour (key, fallback);
            b->setColour (juce::TextButton::buttonColourId, c);
            b->setColour (juce::TextButton::textColourOffId, c.contrasting());
        };
        refresh();

        b->onClick = [this, b, key, fallback, refresh]
        {
            auto selector = std::make_unique<ColourPicker> (settings.getColour (key, fallback), [this, key, refresh] (juce::Colour c)
            {
                settings.props().setValue (key, c.toString());
                refresh();
                onChange();
            });
            selector->setSize (300, 300);
            juce::CallOutBox::launchAsynchronously (std::move (selector), b->getScreenBounds(), nullptr);
        };
        addAndMakeVisible (b);
    }

    struct ColourPicker : public juce::ColourSelector, private juce::ChangeListener
    {
        ColourPicker (juce::Colour initial, std::function<void (juce::Colour)> cb)
            : ColourSelector (showColourAtTop | showSliders | showColourspace), callback (std::move (cb))
        {
            setCurrentColour (initial, juce::dontSendNotification);
            addChangeListener (this);
        }
        void changeListenerCallback (juce::ChangeBroadcaster*) override { callback (getCurrentColour()); }
        std::function<void (juce::Colour)> callback;
    };

    AppSettings& settings;
    std::function<void()> onChange;
    juce::Label fontLabel { {}, th ("ฟอนต์") }, sizeLabel { {}, th ("ขนาด") };
    juce::ComboBox fonts;
    juce::Slider size { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton bold { th ("ตัวหนา") };
    juce::OwnedArray<juce::TextButton> colourButtons;
};

//==============================================================================
MainComponent::MainComponent()
{
    addAndMakeVisible (lyrics);
    addChildComponent (panel);
    setWantsKeyboardFocus (true);

    panel.onOpenSong       = [this] { openSongDialog(); };
    panel.onPlayPause      = [this] { engine.isPlaying() ? engine.pause() : engine.play(); };
    panel.onStop           = [this] { engine.stop(); };
    panel.onShowSynthMixer = [this] { showSynthMixer(); };
    panel.onShowMenu       = [this] { showMenu(); };

    engine.setMasterGain ((float) settings.props().getDoubleValue ("masterGain", 0.7));
    engine.setGuideMelody (settings.props().getBoolValue ("guideMelody", true));
    applyStyleFromSettings();

    auto savedDevice = settings.props().getXmlValue ("audioDevice");
    deviceManager.initialise (0, 2, savedDevice.get(), true);
    player.setSource (&engine);
    deviceManager.addAudioCallback (&player);

    juce::String fluidError;
    if (! fluid::api().load (fluidError))
        statusText = fluidError;
    else if (settings.getSoundFonts().isEmpty())
        statusText = th ("คลิกขวา > SoundFont > เพิ่ม SoundFont เพื่อเริ่มใช้งาน");
    else
        applySoundFonts();

    updateStatus();
    setSize (1280, 720);
    startTimerHz (60);
}

MainComponent::~MainComponent()
{
    settings.props().setValue ("masterGain", engine.getMasterGain());
    settings.props().setValue ("guideMelody", engine.isGuideMelodyOn());
    if (auto xml = deviceManager.createStateXml())
        settings.props().setValue ("audioDevice", xml.get());

    mixerWindow.reset();
    deviceManager.removeAudioCallback (&player);
    player.setSource (nullptr);
}

void MainComponent::resized()
{
    lyrics.setBounds (getLocalBounds());
    panel.setBounds (0, panelShown ? 0 : -panelHeight, getWidth(), panelHeight);
}

//==============================================================================
void MainComponent::updateStatus()
{
    lyrics.setStatusText (statusText);
}

void MainComponent::applyStyleFromSettings()
{
    LyricStyle s;
    s.fontName = settings.getFontName();
    s.sizeFraction = settings.getFontSize();
    s.bold = settings.isFontBold();
    s.unsungFill = settings.getColour ("unsungFill", s.unsungFill);
    s.unsungOutline = settings.getColour ("unsungOutline", s.unsungOutline);
    s.sungFill = settings.getColour ("sungFill", s.sungFill);
    s.sungOutline = settings.getColour ("sungOutline", s.sungOutline);
    lyrics.setStyle (s);

    const juce::File bg (settings.props().getValue ("background"));
    lyrics.setBackground (bg.existsAsFile() ? juce::ImageFileFormat::loadFrom (bg) : juce::Image());
}

void MainComponent::applySoundFonts()
{
    auto list = settings.getSoundFonts();
    statusText = th ("กำลังโหลด SoundFont...");
    updateStatus();

    engine.setSoundFonts (list, [this, safe = juce::Component::SafePointer<MainComponent> (this)] (juce::String error)
    {
        if (safe == nullptr)
            return;
        const auto level = settings.props().getIntValue ("reverb", 40);
        engine.setReverb (0.4 + level / 250.0, level / 100.0);
        statusText = error;
        updateStatus();
    });
}

//==============================================================================
void MainComponent::enqueue (const juce::Array<juce::File>& files, bool playNow)
{
    for (auto& f : files)
        if (f.hasFileExtension ("emk;mid;lyr;cur"))
            queue.add (f);

    if (playNow || ! engine.isPlaying())
        playNext();
}

void MainComponent::playNext()
{
    if (! queue.isEmpty())
        loadAndPlay (queue.removeAndReturn (0));
}

void MainComponent::loadAndPlay (const juce::File& file)
{
    auto result = loadSong (file);
    if (result.song == nullptr)
    {
        statusText = result.error;
        updateStatus();
        return;
    }

    settings.setLastFolder (file.getParentDirectory());
    lyrics.setSong (nullptr);
    engine.setSong (std::move (result.song));
    lyrics.setSong (engine.getSong());
    engine.setTranspose (0);
    engine.setSpeed (1.0);
    engine.play();

    statusText = queue.isEmpty() ? juce::String() : th ("เพลงถัดไป: ") + juce::String (queue.size());
    updateStatus();
}

void MainComponent::timerCallback()
{
    lyrics.setPosition (engine.getPositionTicks());

    if (engine.hasFinished() && ! queue.isEmpty())
        playNext();

    // Reveal the music panel when the mouse touches the top edge of the screen.
    const auto mouse = getMouseXYRelative();
    const bool mouseInside = getLocalBounds().contains (mouse) || mouse.y < 0;
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (! panelShown && mouse.y <= 4 && mouse.y > -40 && mouse.x >= 0 && mouse.x < getWidth())
        setPanelVisible (true);
    else if (panelShown)
    {
        const bool overPanel = mouseInside && mouse.y < panelHeight + 20;
        const bool busy = juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()
                          || juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0;

        if (overPanel || busy)
            panelHideTime = now + 700.0;
        else if (now > panelHideTime)
            setPanelVisible (false);
    }

    if (++frame % 3 == 0)
    {
        if (panelShown)
            panel.refresh();
        if (synthMixer != nullptr && mixerWindow->isVisible())
            synthMixer->refresh();
    }
}

void MainComponent::setPanelVisible (bool shouldShow)
{
    if (shouldShow == panelShown)
        return;

    panelShown = shouldShow;
    panelHideTime = juce::Time::getMillisecondCounterHiRes() + 1200.0;
    auto& animator = juce::Desktop::getInstance().getAnimator();

    if (shouldShow)
    {
        panel.setBounds (0, -panelHeight, getWidth(), panelHeight);
        panel.setVisible (true);
        panel.refresh();
        animator.animateComponent (&panel, { 0, 0, getWidth(), panelHeight }, 1.0f, 160, false, 0.0, 0.0);
    }
    else
    {
        animator.animateComponent (&panel, { 0, -panelHeight, getWidth(), panelHeight }, 1.0f, 200, false, 0.0, 0.0);
    }
}

//==============================================================================
void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showMenu();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();

    if (code == juce::KeyPress::spaceKey)          { engine.isPlaying() ? engine.pause() : engine.play(); return true; }
    if (code == juce::KeyPress::escapeKey)         { engine.stop(); return true; }
    if (code == juce::KeyPress::F11Key)            { toggleFullScreen(); return true; }
    if (code == juce::KeyPress::upKey)             { engine.setTranspose (engine.getTranspose() + 1); return true; }
    if (code == juce::KeyPress::downKey)           { engine.setTranspose (engine.getTranspose() - 1); return true; }
    if (code == juce::KeyPress::rightKey)          { engine.seek (engine.getPositionSeconds() + 5.0); return true; }
    if (code == juce::KeyPress::leftKey)           { engine.seek (engine.getPositionSeconds() - 5.0); return true; }
    if (code == juce::KeyPress::pageDownKey)       { playNext(); return true; }
    if (key == juce::KeyPress ('o', juce::ModifierKeys::ctrlModifier, 0)) { openSongDialog(); return true; }
    if (key.getTextCharacter() == 'm')             { engine.setGuideMelody (! engine.isGuideMelodyOn()); return true; }
    return false;
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("emk;mid;sf2;sf3"))
            return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> songs;
    auto fonts = settings.getSoundFonts();
    bool fontsChanged = false;

    for (auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension ("sf2;sf3")) { fonts.addIfNotAlreadyThere (path); fontsChanged = true; }
        else                                songs.add (f);
    }

    if (fontsChanged)
    {
        settings.setSoundFonts (fonts);
        applySoundFonts();
    }

    if (! songs.isEmpty())
        enqueue (songs, true);
}

//==============================================================================
void MainComponent::openSongDialog()
{
    chooser = std::make_unique<juce::FileChooser> (th ("เปิดเพลง NCN / EMK"), settings.getLastFolder(), "*.emk;*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
    {
        if (! fc.getResults().isEmpty())
            enqueue (fc.getResults(), true);
    });
}

void MainComponent::addSoundFontDialog()
{
    chooser = std::make_unique<juce::FileChooser> (th ("เพิ่ม SoundFont"), settings.getLastFolder(), "*.sf2;*.sf3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResults().isEmpty())
            return;

        auto list = settings.getSoundFonts();
        for (auto& f : fc.getResults())
            list.addIfNotAlreadyThere (f.getFullPathName());
        settings.setSoundFonts (list);
        applySoundFonts();
    });
}

void MainComponent::chooseBackground()
{
    chooser = std::make_unique<juce::FileChooser> (th ("เลือกภาพพื้นหลัง"), juce::File(), "*.jpg;*.jpeg;*.png;*.bmp");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
        {
            settings.props().setValue ("background", fc.getResult().getFullPathName());
            applyStyleFromSettings();
        }
    });
}

void MainComponent::showFontDialog()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new FontSettingsComponent (settings, [this] { applyStyleFromSettings(); }));
    o.dialogTitle = th ("ตัวอักษรเนื้อร้อง");
    o.dialogBackgroundColour = juce::Colour (0xff1e2336);
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, false, false, true, false);
    selector->setSize (520, 380);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = th ("อุปกรณ์เสียง");
    o.dialogBackgroundColour = juce::Colour (0xff1e2336);
    o.useNativeTitleBar = true;
    o.launchAsync();
}

void MainComponent::showSynthMixer()
{
    if (mixerWindow == nullptr)
    {
        struct MixerWindow : public juce::DocumentWindow
        {
            MixerWindow() : DocumentWindow ("Synth Mixer", juce::Colour (0xff141826), allButtons) {}
            void closeButtonPressed() override { setVisible (false); }
        };

        mixerWindow = std::make_unique<MixerWindow>();
        synthMixer = new SynthMixer (engine, settings);
        mixerWindow->setContentOwned (synthMixer, true);
        mixerWindow->setUsingNativeTitleBar (true);
        mixerWindow->setResizable (true, false);
        mixerWindow->centreWithSize (juce::jmin (1400, getWidth()), 470);
    }

    mixerWindow->setVisible (true);
    mixerWindow->toFront (true);
}

void MainComponent::toggleFullScreen()
{
    if (auto* window = findParentComponentOfClass<juce::ResizableWindow>())
        window->setFullScreen (! window->isFullScreen());
}

//==============================================================================
void MainComponent::showMenu()
{
    enum
    {
        openSong = 1, playPause, stopSong, nextSong, clearQueue,
        keyUp, keyDown, keyReset, tempoUp, tempoDown, tempoReset, melody,
        addFont, clearFonts, reloadFonts,
        fontDialog, fontBigger, fontSmaller, background, noBackground,
        mixer, audioSettings, fullScreen, quit,
        reverbBase = 200, removeFontBase = 300
    };

    juce::PopupMenu menu, keyMenu, tempoMenu, fontMenu, sfMenu, removeMenu, reverbMenu, screenMenu;

    menu.addItem (openSong, th ("เปิดเพลง (NCN / EMK)...\tCtrl+O"));
    menu.addItem (playPause, engine.isPlaying() ? th ("หยุดชั่วคราว\tSpace") : th ("เล่น\tSpace"), engine.getSong() != nullptr);
    menu.addItem (stopSong, th ("หยุด\tEsc"), engine.getSong() != nullptr);
    menu.addItem (nextSong, th ("เพลงถัดไป (") + juce::String (queue.size()) + ")\tPgDn", ! queue.isEmpty());
    menu.addItem (clearQueue, th ("ล้างคิวเพลง"), ! queue.isEmpty());
    menu.addSeparator();

    const auto t = engine.getTranspose();
    keyMenu.addItem (keyUp, th ("เพิ่มคีย์ +1\tUp"));
    keyMenu.addItem (keyDown, th ("ลดคีย์ -1\tDown"));
    keyMenu.addItem (keyReset, th ("คีย์เดิม"), t != 0);
    menu.addSubMenu (th ("คีย์ (") + (t > 0 ? "+" : "") + juce::String (t) + ")", keyMenu);

    tempoMenu.addItem (tempoUp, th ("เร็วขึ้น +2%"));
    tempoMenu.addItem (tempoDown, th ("ช้าลง -2%"));
    tempoMenu.addItem (tempoReset, th ("จังหวะเดิม"), ! juce::exactlyEqual (engine.getSpeed(), 1.0));
    menu.addSubMenu (th ("จังหวะ (") + juce::String (juce::roundToInt (engine.getSpeed() * 100)) + "%)", tempoMenu);
    menu.addItem (melody, th ("เสียงทำนองนำ\tM"), true, engine.isGuideMelodyOn());
    menu.addSeparator();

    const auto fonts = settings.getSoundFonts();
    for (int i = 0; i < fonts.size(); ++i)
    {
        sfMenu.addItem (removeFontBase + i, juce::File (fonts[i]).getFileName(), false, true);
        removeMenu.addItem (removeFontBase + i, juce::File (fonts[i]).getFileName());
    }
    if (! fonts.isEmpty())
        sfMenu.addSeparator();
    sfMenu.addItem (addFont, th ("เพิ่ม SoundFont..."));
    sfMenu.addSubMenu (th ("เอาออก"), removeMenu, ! fonts.isEmpty());
    sfMenu.addItem (clearFonts, th ("เอาออกทั้งหมด"), ! fonts.isEmpty());
    sfMenu.addItem (reloadFonts, th ("โหลดใหม่"), ! fonts.isEmpty());
    menu.addSubMenu ("SoundFont", sfMenu);

    const int reverbLevels[] = { 0, 20, 40, 60, 80, 100 };
    const auto currentReverb = settings.props().getIntValue ("reverb", 40);
    for (auto r : reverbLevels)
        reverbMenu.addItem (reverbBase + r, juce::String (r) + "%", true, r == currentReverb);
    menu.addSubMenu (th ("เสียงก้อง"), reverbMenu);
    menu.addItem (mixer, "Synth Mixer (32 ch + VST)...");
    menu.addItem (audioSettings, th ("อุปกรณ์เสียง..."));
    menu.addSeparator();

    fontMenu.addItem (fontDialog, th ("ฟอนต์และสี..."));
    fontMenu.addItem (fontBigger, th ("ตัวอักษรใหญ่ขึ้น"));
    fontMenu.addItem (fontSmaller, th ("ตัวอักษรเล็กลง"));
    menu.addSubMenu (th ("ตัวอักษร"), fontMenu);

    screenMenu.addItem (background, th ("เลือกภาพพื้นหลัง..."));
    screenMenu.addItem (noBackground, th ("พื้นหลังปกติ"), settings.props().containsKey ("background"));
    screenMenu.addItem (fullScreen, th ("เต็มจอ\tF11"));
    menu.addSubMenu (th ("หน้าจอ"), screenMenu);
    menu.addSeparator();
    menu.addItem (quit, th ("ออกจากโปรแกรม"));

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, fonts] (int result)
    {
        auto bumpFont = [this] (double delta)
        {
            settings.props().setValue ("fontSize", juce::jlimit (0.05, 0.2, settings.getFontSize() + delta));
            applyStyleFromSettings();
        };

        switch (result)
        {
            case 0:            return;
            case openSong:     openSongDialog(); break;
            case playPause:    engine.isPlaying() ? engine.pause() : engine.play(); break;
            case stopSong:     engine.stop(); break;
            case nextSong:     playNext(); break;
            case clearQueue:   queue.clear(); break;
            case keyUp:        engine.setTranspose (engine.getTranspose() + 1); break;
            case keyDown:      engine.setTranspose (engine.getTranspose() - 1); break;
            case keyReset:     engine.setTranspose (0); break;
            case tempoUp:      engine.setSpeed (engine.getSpeed() + 0.02); break;
            case tempoDown:    engine.setSpeed (engine.getSpeed() - 0.02); break;
            case tempoReset:   engine.setSpeed (1.0); break;
            case melody:       engine.setGuideMelody (! engine.isGuideMelodyOn()); break;
            case addFont:      addSoundFontDialog(); break;
            case clearFonts:   settings.setSoundFonts ({}); applySoundFonts(); break;
            case reloadFonts:  applySoundFonts(); break;
            case fontDialog:   showFontDialog(); break;
            case fontBigger:   bumpFont (0.01); break;
            case fontSmaller:  bumpFont (-0.01); break;
            case background:   chooseBackground(); break;
            case noBackground: settings.props().removeValue ("background"); applyStyleFromSettings(); break;
            case mixer:        showSynthMixer(); break;
            case audioSettings: showAudioSettings(); break;
            case fullScreen:   toggleFullScreen(); break;
            case quit:         juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            default:
                if (result >= removeFontBase)
                {
                    auto list = fonts;
                    list.remove (result - removeFontBase);
                    settings.setSoundFonts (list);
                    applySoundFonts();
                }
                else if (result >= reverbBase)
                {
                    const auto level = result - reverbBase;
                    settings.props().setValue ("reverb", level);
                    engine.setReverb (0.4 + level / 250.0, level / 100.0);
                }
                break;
        }
    });
}

} // namespace kara
