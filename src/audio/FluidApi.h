#pragma once

#include <juce_core/juce_core.h>

/** FluidSynth loaded at runtime from its shared library, so the app builds with any
    compiler without an import library and still starts (with a clear error) if the DLL
    is missing. Only the small subset of the API we use is declared here. */
namespace kara::fluid
{

struct Settings;
struct Synth;
struct Preset;

struct Api
{
    Settings* (*new_settings)() = nullptr;
    void (*delete_settings) (Settings*) = nullptr;
    int (*settings_setint) (Settings*, const char*, int) = nullptr;
    int (*settings_setnum) (Settings*, const char*, double) = nullptr;
    int (*settings_setstr) (Settings*, const char*, const char*) = nullptr;

    Synth* (*new_synth) (Settings*) = nullptr;
    void (*delete_synth) (Synth*) = nullptr;
    int (*sfload) (Synth*, const char*, int) = nullptr;
    int (*sfunload) (Synth*, int, int) = nullptr;
    int (*noteon) (Synth*, int, int, int) = nullptr;
    int (*noteoff) (Synth*, int, int) = nullptr;
    int (*cc) (Synth*, int, int, int) = nullptr;
    int (*pitch_bend) (Synth*, int, int) = nullptr;
    int (*channel_pressure) (Synth*, int, int) = nullptr;
    int (*key_pressure) (Synth*, int, int, int) = nullptr;
    int (*program_change) (Synth*, int, int) = nullptr;
    int (*sysex) (Synth*, const char*, int, char*, int*, int*, int) = nullptr;
    int (*system_reset) (Synth*) = nullptr;
    int (*all_notes_off) (Synth*, int) = nullptr;
    int (*all_sounds_off) (Synth*, int) = nullptr;
    int (*process) (Synth*, int, int, float**, int, float**) = nullptr;
    void (*set_gain) (Synth*, float) = nullptr;
    int (*set_interp_method) (Synth*, int, int) = nullptr;
    int (*get_program) (Synth*, int, int*, int*, int*) = nullptr;
    int (*set_channel_type) (Synth*, int, int) = nullptr;
    Preset* (*get_channel_preset) (Synth*, int) = nullptr;
    const char* (*preset_get_name) (Preset*) = nullptr;
    int (*reverb_on) (Synth*, int, int) = nullptr;
    int (*chorus_on) (Synth*, int, int) = nullptr;
    int (*set_reverb_group_roomsize) (Synth*, int, double) = nullptr;
    int (*set_reverb_group_damp) (Synth*, int, double) = nullptr;
    int (*set_reverb_group_width) (Synth*, int, double) = nullptr;
    int (*set_reverb_group_level) (Synth*, int, double) = nullptr;
    int (*set_chorus_group_level) (Synth*, int, double) = nullptr;

    bool load (juce::String& error);
    bool isLoaded() const { return process != nullptr; }

private:
    juce::DynamicLibrary lib;
};

/** Process-wide instance; call load() once at startup. */
Api& api();

constexpr int interpHighest = 7;
constexpr int channelTypeDrum = 1;

} // namespace kara::fluid
