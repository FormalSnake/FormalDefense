#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Resolution Presets ---

const ResolutionPreset RESOLUTION_PRESETS[RESOLUTION_PRESET_COUNT] = {
    { 1280,  720, "1280x720"  },
    { 1600,  900, "1600x900"  },
    { 1920, 1080, "1920x1080" },
    { 2560, 1440, "2560x1440" },
    { 3840, 2160, "3840x2160" },
};

// --- Fullscreen mode labels ---

static const char *FULLSCREEN_LABELS[FULLSCREEN_COUNT] = {
    "Windowed", "Borderless", "Exclusive"
};

// --- Keybinds display table ---

typedef struct { const char *action; const char *key; } KeybindEntry;

static const KeybindEntry KEYBINDS[] = {
    { "Pan Camera",      "WASD / Arrows" },
    { "Rotate Camera",   "Q / E" },
    { "Zoom Camera",     "Scroll Wheel" },
    { "Place Tower",     "Left Click" },
    { "Select Tower",    "Left Click" },
    { "Upgrade Tower",   "U" },
    { "Sell Tower",      "X" },
    { "Start Wave",      "Space" },
    { "Pause",           "Escape" },
    { "Fullscreen",      "F11" },
    { "Chat (MP)",       "T" },
    { "Restart (Game Over)", "R" },
};
#define KEYBIND_COUNT ((int)(sizeof(KEYBINDS) / sizeof(KEYBINDS[0])))

// --- Defaults ---

void SettingsDefault(Settings *s)
{
    s->fullscreen = FULLSCREEN_WINDOWED;
    s->resolutionIdx = 0; // 1280x720
    s->vsync = true;
    s->ps1Downscale = 3;

    s->masterVolume = 1.0f;
    s->sfxVolume = 1.0f;
    s->musicVolume = 0.8f;

    s->camPanSpeed = 12.0f;
    s->camRotSpeed = 0.3f;
    s->camZoomSpeed = 2.0f;
}

// --- Load / Save ---

bool SettingsLoad(Settings *s, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[64];
        char val[64];
        if (sscanf(line, "%63s %63s", key, val) != 2) continue;

        if (strcmp(key, "fullscreen") == 0) {
            int v = atoi(val);
            if (v >= 0 && v < FULLSCREEN_COUNT) s->fullscreen = (FullscreenMode)v;
        } else if (strcmp(key, "resolution") == 0) {
            int v = atoi(val);
            if (v >= 0 && v < RESOLUTION_PRESET_COUNT) s->resolutionIdx = v;
        } else if (strcmp(key, "vsync") == 0) {
            s->vsync = atoi(val) != 0;
        } else if (strcmp(key, "ps1_downscale") == 0) {
            int v = atoi(val);
            if (v >= 1 && v <= 5) s->ps1Downscale = v;
        } else if (strcmp(key, "master_volume") == 0) {
            float v = (float)atof(val);
            if (v >= 0.0f && v <= 1.0f) s->masterVolume = v;
        } else if (strcmp(key, "sfx_volume") == 0) {
            float v = (float)atof(val);
            if (v >= 0.0f && v <= 1.0f) s->sfxVolume = v;
        } else if (strcmp(key, "music_volume") == 0) {
            float v = (float)atof(val);
            if (v >= 0.0f && v <= 1.0f) s->musicVolume = v;
        } else if (strcmp(key, "cam_pan_speed") == 0) {
            float v = (float)atof(val);
            if (v >= 1.0f && v <= 50.0f) s->camPanSpeed = v;
        } else if (strcmp(key, "cam_rot_speed") == 0) {
            float v = (float)atof(val);
            if (v >= 0.05f && v <= 2.0f) s->camRotSpeed = v;
        } else if (strcmp(key, "cam_zoom_speed") == 0) {
            float v = (float)atof(val);
            if (v >= 0.5f && v <= 10.0f) s->camZoomSpeed = v;
        }
    }

    fclose(f);
    return true;
}

bool SettingsSave(const Settings *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "# Formal Defense Settings\n");
    fprintf(f, "fullscreen %d\n", (int)s->fullscreen);
    fprintf(f, "resolution %d\n", s->resolutionIdx);
    fprintf(f, "vsync %d\n", s->vsync ? 1 : 0);
    fprintf(f, "ps1_downscale %d\n", s->ps1Downscale);
    fprintf(f, "master_volume %.2f\n", s->masterVolume);
    fprintf(f, "sfx_volume %.2f\n", s->sfxVolume);
    fprintf(f, "music_volume %.2f\n", s->musicVolume);
    fprintf(f, "cam_pan_speed %.2f\n", s->camPanSpeed);
    fprintf(f, "cam_rot_speed %.2f\n", s->camRotSpeed);
    fprintf(f, "cam_zoom_speed %.2f\n", s->camZoomSpeed);

    fclose(f);
    return true;
}

// --- Open / Close ---

void SettingsOpen(SettingsState *st, const Settings *current)
{
    st->open = true;
    st->activeTab = SETTINGS_TAB_GRAPHICS;
    st->pending = *current;
}

void SettingsClose(SettingsState *st)
{
    st->open = false;
}

// --- UI Helpers ---

static float Clamp_f(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Draw a cycle widget: [< label >]  returns new index
static int DrawCycleWidget(int x, int y, int w, const char *label, int current, int count,
                           Vector2 mouse, bool allowClick)
{
    int h = 28;
    Rectangle leftBtn  = { (float)x, (float)y, 24.0f, (float)h };
    Rectangle rightBtn = { (float)(x + w - 24), (float)y, 24.0f, (float)h };

    bool leftHover  = CheckCollisionPointRec(mouse, leftBtn);
    bool rightHover = CheckCollisionPointRec(mouse, rightBtn);

    // Background
    DrawRectangle(x, y, w, h, (Color){ 30, 35, 45, 255 });
    DrawRectangleLines(x, y, w, h, (Color){ 60, 70, 90, 200 });

    // Arrows
    Color leftCol  = leftHover  ? WHITE : LIGHTGRAY;
    Color rightCol = rightHover ? WHITE : LIGHTGRAY;
    DrawText("<", x + 8, y + 5, 18, leftCol);
    DrawText(">", x + w - 18, y + 5, 18, rightCol);

    // Label centered
    int labelW = MeasureText(label, 16);
    DrawText(label, x + (w - labelW) / 2, y + 6, 16, WHITE);

    if (allowClick && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (leftHover)  return (current - 1 + count) % count;
        if (rightHover) return (current + 1) % count;
    }
    return current;
}

// Draw a toggle widget: [ON] / [OFF]
static bool DrawToggleWidget(int x, int y, bool value, Vector2 mouse, bool allowClick)
{
    int w = 60, h = 28;
    Rectangle btn = { (float)x, (float)y, (float)w, (float)h };
    bool hover = CheckCollisionPointRec(mouse, btn);

    Color bg = value ? (Color){ 40, 90, 40, 255 } : (Color){ 70, 35, 35, 255 };
    if (hover) { bg.r += 20; bg.g += 20; bg.b += 20; }
    DrawRectangleRec(btn, bg);
    DrawRectangleLinesEx(btn, 1, (Color){ 80, 90, 110, 200 });

    const char *text = value ? "ON" : "OFF";
    int tw = MeasureText(text, 16);
    DrawText(text, x + (w - tw) / 2, y + 6, 16, WHITE);

    if (allowClick && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return !value;
    return value;
}

// Draw a horizontal slider. Returns new value.
static float DrawSliderWidget(int x, int y, int w, float value, float lo, float hi,
                              const char *label, Vector2 mouse, bool allowClick)
{
    int h = 28;
    int labelW = MeasureText(label, 14);
    DrawText(label, x, y + 7, 14, LIGHTGRAY);

    int sliderX = x + labelW + 12;
    int sliderW = w - labelW - 70;
    int sliderY = y + 10;
    int sliderH = 8;

    // Track
    DrawRectangle(sliderX, sliderY, sliderW, sliderH, (Color){ 30, 35, 45, 255 });
    DrawRectangleLines(sliderX, sliderY, sliderW, sliderH, (Color){ 60, 70, 90, 200 });

    // Fill
    float t = (value - lo) / (hi - lo);
    int fillW = (int)(t * sliderW);
    DrawRectangle(sliderX, sliderY, fillW, sliderH, (Color){ 80, 140, 200, 255 });

    // Handle
    int handleX = sliderX + fillW - 4;
    DrawRectangle(handleX, y + 5, 8, 18, WHITE);

    // Value text
    char valText[16];
    snprintf(valText, sizeof(valText), "%.1f", value);
    DrawText(valText, sliderX + sliderW + 8, y + 7, 14, WHITE);

    // Interaction
    Rectangle trackRect = { (float)sliderX, (float)(y), (float)sliderW, (float)h };
    if (allowClick && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, trackRect)) {
        float newT = (mouse.x - sliderX) / sliderW;
        newT = Clamp_f(newT, 0.0f, 1.0f);
        return lo + newT * (hi - lo);
    }

    return value;
}

// --- Update ---

int SettingsUpdate(SettingsState *st)
{
    if (!st->open) {
        int r = st->_result;
        st->_result = 0;
        return r;
    }

    // ESC closes without applying
    if (IsKeyPressed(KEY_ESCAPE)) {
        st->open = false;
        st->_result = -1;
        return 0; // Will be returned next frame
    }

    return 0;
}

// --- Draw ---

void SettingsDraw(const SettingsState *st, int screenW, int screenH)
{
    if (!st->open) return;

    // We need to cast away const to allow UI interaction to modify pending
    SettingsState *mut = (SettingsState *)st;
    Settings *p = &mut->pending;
    Vector2 mouse = GetMousePosition();
    bool click = true; // Always allow clicks in settings overlay

    // Dark backdrop
    DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 160 });

    // Panel
    int panelW = 500, panelH = 420;
    int panelX = (screenW - panelW) / 2;
    int panelY = (screenH - panelH) / 2;
    DrawRectangle(panelX, panelY, panelW, panelH, (Color){ 25, 28, 35, 245 });
    DrawRectangleLinesEx((Rectangle){ (float)panelX, (float)panelY, (float)panelW, (float)panelH },
                         2, (Color){ 70, 80, 100, 200 });

    // Title
    const char *title = "Settings";
    int titleW = MeasureText(title, 28);
    DrawText(title, panelX + (panelW - titleW) / 2, panelY + 10, 28, WHITE);

    // --- Tab bar ---
    static const char *tabNames[SETTINGS_TAB_COUNT] = { "Graphics", "Audio", "Gameplay", "Keys" };
    int tabW = panelW / SETTINGS_TAB_COUNT;
    int tabY = panelY + 45;

    for (int i = 0; i < SETTINGS_TAB_COUNT; i++) {
        int tx = panelX + i * tabW;
        Rectangle tabBtn = { (float)tx, (float)tabY, (float)tabW, 30.0f };
        bool hover = CheckCollisionPointRec(mouse, tabBtn);
        bool active = ((int)mut->activeTab == i);

        Color bg = active ? (Color){ 50, 60, 80, 255 } :
                   hover  ? (Color){ 40, 48, 65, 255 } :
                            (Color){ 30, 35, 48, 255 };
        DrawRectangleRec(tabBtn, bg);
        if (active) DrawRectangle(tx, tabY + 27, tabW, 3, (Color){ 100, 160, 240, 255 });

        int tw = MeasureText(tabNames[i], 16);
        DrawText(tabNames[i], tx + (tabW - tw) / 2, tabY + 7, 16, active ? WHITE : LIGHTGRAY);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            mut->activeTab = (SettingsTab)i;
    }

    // --- Tab content area ---
    int contentX = panelX + 24;
    int contentY = tabY + 42;
    int contentW = panelW - 48;
    int rowH = 36;

    switch (mut->activeTab) {
    case SETTINGS_TAB_GRAPHICS: {
        int y = contentY;

        DrawText("Fullscreen:", contentX, y + 5, 16, LIGHTGRAY);
        p->fullscreen = (FullscreenMode)DrawCycleWidget(
            contentX + 160, y, 200, FULLSCREEN_LABELS[p->fullscreen],
            (int)p->fullscreen, FULLSCREEN_COUNT, mouse, click);
        y += rowH;

        DrawText("Resolution:", contentX, y + 5, 16, LIGHTGRAY);
        p->resolutionIdx = DrawCycleWidget(
            contentX + 160, y, 200, RESOLUTION_PRESETS[p->resolutionIdx].label,
            p->resolutionIdx, RESOLUTION_PRESET_COUNT, mouse, click);
        y += rowH;

        DrawText("VSync:", contentX, y + 5, 16, LIGHTGRAY);
        p->vsync = DrawToggleWidget(contentX + 160, y, p->vsync, mouse, click);
        y += rowH;

        DrawText("PS1 Downscale:", contentX, y + 5, 16, LIGHTGRAY);
        char dsLabel[8];
        snprintf(dsLabel, sizeof(dsLabel), "%d", p->ps1Downscale);
        int newDs = DrawCycleWidget(contentX + 160, y, 200, dsLabel, p->ps1Downscale - 1, 5, mouse, click);
        p->ps1Downscale = newDs + 1;
    } break;

    case SETTINGS_TAB_AUDIO: {
        int y = contentY;

        p->masterVolume = DrawSliderWidget(contentX, y, contentW, p->masterVolume, 0.0f, 1.0f,
                                           "Master", mouse, click);
        y += rowH + 4;

        p->sfxVolume = DrawSliderWidget(contentX, y, contentW, p->sfxVolume, 0.0f, 1.0f,
                                        "SFX", mouse, click);
        y += rowH + 4;

        p->musicVolume = DrawSliderWidget(contentX, y, contentW, p->musicVolume, 0.0f, 1.0f,
                                          "Music", mouse, click);
        y += rowH + 8;

        DrawText("(Audio not yet implemented)", contentX, y, 14, (Color){ 120, 120, 120, 255 });
    } break;

    case SETTINGS_TAB_GAMEPLAY: {
        int y = contentY;

        p->camPanSpeed = DrawSliderWidget(contentX, y, contentW, p->camPanSpeed, 1.0f, 50.0f,
                                          "Pan Speed", mouse, click);
        y += rowH + 4;

        p->camRotSpeed = DrawSliderWidget(contentX, y, contentW, p->camRotSpeed, 0.05f, 2.0f,
                                          "Rot Speed", mouse, click);
        y += rowH + 4;

        p->camZoomSpeed = DrawSliderWidget(contentX, y, contentW, p->camZoomSpeed, 0.5f, 10.0f,
                                           "Zoom Speed", mouse, click);
    } break;

    case SETTINGS_TAB_KEYS: {
        int y = contentY;
        int colAction = contentX;
        int colKey = contentX + 200;

        DrawText("Action", colAction, y, 16, WHITE);
        DrawText("Key", colKey, y, 16, WHITE);
        y += 24;
        DrawLine(contentX, y, contentX + contentW, y, (Color){ 60, 70, 90, 200 });
        y += 6;

        for (int i = 0; i < KEYBIND_COUNT; i++) {
            DrawText(KEYBINDS[i].action, colAction, y, 14, LIGHTGRAY);
            DrawText(KEYBINDS[i].key, colKey, y, 14, WHITE);
            y += 22;
        }
    } break;

    default: break;
    }

    // --- Apply / Back buttons ---
    int btnW = 100, btnH = 36;
    int btnY = panelY + panelH - btnH - 16;

    // Apply button
    int applyX = panelX + panelW / 2 - btnW - 10;
    Rectangle applyBtn = { (float)applyX, (float)btnY, (float)btnW, (float)btnH };
    bool applyHover = CheckCollisionPointRec(mouse, applyBtn);
    Color applyBg = applyHover ? (Color){ 50, 110, 50, 255 } : (Color){ 35, 75, 35, 255 };
    DrawRectangleRec(applyBtn, applyBg);
    DrawRectangleLinesEx(applyBtn, 2, (Color){ 90, 180, 90, 200 });
    const char *applyText = "Apply";
    int applyTextW = MeasureText(applyText, 20);
    DrawText(applyText, applyX + (btnW - applyTextW) / 2, btnY + 8, 20, WHITE);

    if (applyHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        mut->open = false;
        mut->_result = 1; // Applied
    }

    // Back button
    int backX = panelX + panelW / 2 + 10;
    Rectangle backBtn = { (float)backX, (float)btnY, (float)btnW, (float)btnH };
    bool backHover = CheckCollisionPointRec(mouse, backBtn);
    Color backBg = backHover ? (Color){ 90, 55, 55, 255 } : (Color){ 65, 40, 40, 255 };
    DrawRectangleRec(backBtn, backBg);
    DrawRectangleLinesEx(backBtn, 2, (Color){ 180, 90, 90, 200 });
    const char *backText = "Back";
    int backTextW = MeasureText(backText, 20);
    DrawText(backText, backX + (btnW - backTextW) / 2, btnY + 8, 20, WHITE);

    if (backHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        mut->open = false;
        mut->_result = -1; // Cancelled
    }
}
