#include "FluidApi.h"

namespace kara::fluid
{

Api& api()
{
    static Api instance;
    return instance;
}

bool Api::load (juce::String& error)
{
    if (isLoaded())
        return true;

   #if JUCE_WINDOWS
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    const juce::StringArray candidates { exeDir.getChildFile ("libfluidsynth-3.dll").getFullPathName(), "libfluidsynth-3.dll" };
   #elif JUCE_MAC
    const juce::StringArray candidates { "libfluidsynth.3.dylib", "/opt/homebrew/lib/libfluidsynth.3.dylib", "/usr/local/lib/libfluidsynth.3.dylib" };
   #else
    const juce::StringArray candidates { "libfluidsynth.so.3", "libfluidsynth.so" };
   #endif

    bool opened = false;
    for (auto& c : candidates)
        if ((opened = lib.open (c)))
            break;

    if (! opened)
    {
        error = "Cannot load FluidSynth library (" + candidates.joinIntoString (", ") + ")";
        return false;
    }

    juce::StringArray missing;

    auto bind = [&] (auto& fn, const char* name)
    {
        fn = reinterpret_cast<std::remove_reference_t<decltype (fn)>> (lib.getFunction (name));
        if (fn == nullptr)
            missing.add (name);
    };

    bind (new_settings, "new_fluid_settings");
    bind (delete_settings, "delete_fluid_settings");
    bind (settings_setint, "fluid_settings_setint");
    bind (settings_setnum, "fluid_settings_setnum");
    bind (settings_setstr, "fluid_settings_setstr");
    bind (new_synth, "new_fluid_synth");
    bind (delete_synth, "delete_fluid_synth");
    bind (sfload, "fluid_synth_sfload");
    bind (sfunload, "fluid_synth_sfunload");
    bind (noteon, "fluid_synth_noteon");
    bind (noteoff, "fluid_synth_noteoff");
    bind (cc, "fluid_synth_cc");
    bind (pitch_bend, "fluid_synth_pitch_bend");
    bind (channel_pressure, "fluid_synth_channel_pressure");
    bind (key_pressure, "fluid_synth_key_pressure");
    bind (program_change, "fluid_synth_program_change");
    bind (sysex, "fluid_synth_sysex");
    bind (system_reset, "fluid_synth_system_reset");
    bind (all_notes_off, "fluid_synth_all_notes_off");
    bind (all_sounds_off, "fluid_synth_all_sounds_off");
    bind (process, "fluid_synth_process");
    bind (set_gain, "fluid_synth_set_gain");
    bind (set_interp_method, "fluid_synth_set_interp_method");
    bind (get_program, "fluid_synth_get_program");
    bind (set_channel_type, "fluid_synth_set_channel_type");
    bind (get_channel_preset, "fluid_synth_get_channel_preset");
    bind (preset_get_name, "fluid_preset_get_name");
    bind (reverb_on, "fluid_synth_reverb_on");
    bind (chorus_on, "fluid_synth_chorus_on");
    bind (set_reverb_group_roomsize, "fluid_synth_set_reverb_group_roomsize");
    bind (set_reverb_group_damp, "fluid_synth_set_reverb_group_damp");
    bind (set_reverb_group_width, "fluid_synth_set_reverb_group_width");
    bind (set_reverb_group_level, "fluid_synth_set_reverb_group_level");
    bind (set_chorus_group_level, "fluid_synth_set_chorus_group_level");

    if (! missing.isEmpty())
    {
        error = "FluidSynth 2.2 or newer is required (missing: " + missing.joinIntoString (", ") + ")";
        process = nullptr;
        return false;
    }

    return true;
}

} // namespace kara::fluid
