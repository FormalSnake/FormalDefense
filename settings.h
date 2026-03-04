#ifndef SETTINGS_H
#define SETTINGS_H

#include "raylib.h"
#include <stdbool.h>

// --- Fullscreen Modes ---

typedef enum {
    FULLSCREEN_WINDOWED = 0,
    FULLSCREEN_BORDERLESS,
    FULLSCREEN_EXCLUSIVE,
    FULLSCREEN_COUNT
} FullscreenMode;

// --- Resolution Presets ---

typedef struct {
    int width;
    int height;
    const char *label;
} ResolutionPreset;

#define RESOLUTION_PRESET_COUNT 5
extern const ResolutionPreset RESOLUTION_PRESETS[RESOLUTION_PRESET_COUNT];

// --- Settings ---

typedef struct {
    // Graphics
    FullscreenMode fullscreen;
    int resolutionIdx;
    bool vsync;
    int ps1Downscale; // 1-5

    // Audio (stubs for future)
    float masterVolume;
    float sfxVolume;
    float musicVolume;

    // Gameplay
    float camPanSpeed;
    float camRotSpeed;
    float camZoomSpeed;
} Settings;

// --- Settings UI State ---

typedef enum {
    SETTINGS_TAB_GRAPHICS = 0,
    SETTINGS_TAB_AUDIO,
    SETTINGS_TAB_GAMEPLAY,
    SETTINGS_TAB_KEYS,
    SETTINGS_TAB_COUNT
} SettingsTab;

typedef struct {
    bool open;
    SettingsTab activeTab;
    Settings pending; // Working copy edited by UI
    int _result;      // Internal: 0=none, 1=apply clicked, -1=back clicked
} SettingsState;

// --- Functions ---

void SettingsDefault(Settings *s);
bool SettingsLoad(Settings *s, const char *path);
bool SettingsSave(const Settings *s, const char *path);

void SettingsOpen(SettingsState *st, const Settings *current);
void SettingsClose(SettingsState *st);

// Returns: 0 = still open, 1 = applied (copy pending back), -1 = cancelled
int  SettingsUpdate(SettingsState *st);
void SettingsDraw(const SettingsState *st, int screenW, int screenH);

#endif // SETTINGS_H
