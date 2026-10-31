#include "AppSettings.h"

namespace kara
{

AppSettings::AppSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "KaraSoft";
    o.filenameSuffix = ".settings";
    o.folderName = "KaraSoft";
    o.osxLibrarySubFolder = "Application Support";
    appProps.setStorageParameters (o);
}

juce::StringArray AppSettings::getSoundFonts()
{
    juce::StringArray list;
    list.addLines (props().getValue ("soundFonts"));
    list.removeEmptyStrings();
    return list;
}

void AppSettings::setSoundFonts (const juce::StringArray& list)
{
    props().setValue ("soundFonts", list.joinIntoString ("\n"));
}

juce::String AppSettings::getFontName()
{
   #if JUCE_WINDOWS
    return props().getValue ("fontName", "Tahoma");
   #else
    return props().getValue ("fontName", juce::Font::getDefaultSansSerifFontName());
   #endif
}

float AppSettings::getFontSize() { return (float) props().getDoubleValue ("fontSize", 0.11); }
bool AppSettings::isFontBold()   { return props().getBoolValue ("fontBold", true); }

juce::Colour AppSettings::getColour (const juce::String& key, juce::Colour fallback)
{
    const auto v = props().getValue (key);
    return v.isEmpty() ? fallback : juce::Colour::fromString (v);
}

juce::File AppSettings::getLastFolder()
{
    return juce::File (props().getValue ("lastFolder",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName()));
}

void AppSettings::setLastFolder (const juce::File& f) { props().setValue ("lastFolder", f.getFullPathName()); }

} // namespace kara
