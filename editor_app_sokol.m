// editor_app_sokol.m — Sokol app lifecycle for the map editor (ObjC for Metal backend)
// Mirrors fd_app_sokol.m but calls editor callbacks instead of game callbacks.

#define SOKOL_METAL
#define SOKOL_IMPL

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_gl.h"
#include "sokol_debugtext.h"
#include "sokol_shape.h"

#include "fd_app.h"
#include "fd_gfx.h"
#include "fd_input.h"

// --- Forward declarations for editor code callbacks ---
extern void EditorAppInit(void);
extern void EditorAppFrame(void);
extern void EditorAppCleanup(void);

extern void FdInputProcessEvent(const sapp_event *ev);

// --- Internal state ---
static float frame_time = 0.0f;

// --- App lifecycle callbacks ---

static void init_cb(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    FdGfxInit();
    EditorAppInit();
}

static void frame_cb(void) {
    frame_time = (float)sapp_frame_duration();
    FdInputBeginFrame();
    EditorAppFrame();
}

static void event_cb(const sapp_event *ev) {
    FdInputProcessEvent(ev);
}

static void cleanup_cb(void) {
    EditorAppCleanup();
    FdGfxShutdown();
    sg_shutdown();
}

// --- Public API ---

int FdScreenWidth(void) {
    return sapp_width();
}

int FdScreenHeight(void) {
    return sapp_height();
}

float FdFrameTime(void) {
    return frame_time;
}

void FdToggleFullscreen(void) {
    sapp_toggle_fullscreen();
}

void FdQuitApp(void) {
    sapp_quit();
}

// --- Entry point ---

#define GRID_OFFSET_X 20
#define GRID_OFFSET_Y 40
#define CELL_SIZE 36
#define TOOL_PANEL_W 120

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    // Match the window size calculation from original editor
    int winW = GRID_OFFSET_X + 20 * CELL_SIZE + 20 + TOOL_PANEL_W + 20;
    int winH = GRID_OFFSET_Y + 15 * CELL_SIZE + 80;
    return (sapp_desc){
        .init_cb = init_cb,
        .frame_cb = frame_cb,
        .event_cb = event_cb,
        .cleanup_cb = cleanup_cb,
        .width = winW,
        .height = winH,
        .window_title = "Formal Defense - Map Editor",
        .high_dpi = false,
        .logger.func = slog_func,
        .icon.sokol_default = false,
    };
}
