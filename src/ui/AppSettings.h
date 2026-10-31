#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>

namespace kara
{

/** Persistent user settings, stored in the user's application data folder. */
class AppSettings
{
public:
    AppSettings();

    juce::PropertiesFile& props() { return *appProps.getUserSettings(); }

    juce::StringArray getSoundFonts();
    void setSoundFonts (const juce::StringArray&);

    juce::String getFontName();
    float getFontSize();          // as a fraction of window height
    bool isFontBold();
    juce::Colour getColour (const juce::String& key, juce::Colour fallback);

    juce::File getLastFolder();
    void setLastFolder (const juce::File&);

private:
    juce::ApplicationProperties appProps;
};

} // namespace kara
