// fd_app_sokol.m — Sokol app lifecycle (ObjC for Metal backend)
// This file contains ALL sokol implementation includes and the sokol_main entry point.

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

// --- Forward declarations for game code callbacks ---
// These are implemented in main.c
extern void GameInit(void);
extern void GameFrame(void);
extern void GameCleanup(void);

// Forward declaration of input event processor
extern void FdInputProcessEvent(const sapp_event *ev);

// --- Internal state ---
static float frame_time = 0.0f;
static bool fullscreen_requested = false;

// --- App lifecycle callbacks ---

static void init_cb(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    FdGfxInit();
    FdSphereMeshInit();
    GameInit();
}

static void frame_cb(void) {
    frame_time = (float)sapp_frame_duration();
    FdInputBeginFrame();
    GameFrame();

    if (fullscreen_requested) {
        sapp_toggle_fullscreen();
        fullscreen_requested = false;
    }
}

static void event_cb(const sapp_event *ev) {
    FdInputProcessEvent(ev);
}

static void cleanup_cb(void) {
    GameCleanup();
    FdSphereMeshShutdown();
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
    fullscreen_requested = true;
}

void FdQuitApp(void) {
    sapp_quit();
}

// --- Entry point ---

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init_cb,
        .frame_cb = frame_cb,
        .event_cb = event_cb,
        .cleanup_cb = cleanup_cb,
        .width = 1280,
        .height = 720,
        .window_title = "Formal Defense",
        .high_dpi = false,
        .logger.func = slog_func,
        .icon.sokol_default = false,
    };
}
