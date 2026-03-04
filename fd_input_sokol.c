// fd_input_sokol.c — Input state accumulation from sokol_app events
#include "fd_input.h"
#include "sokol_app.h"
#include <string.h>

// --- Internal state ---

#define MAX_KEYS 512
#define MAX_MOUSE_BUTTONS 3
#define CHAR_QUEUE_SIZE 32

static struct {
    bool keyDown[MAX_KEYS];
    bool keyPressed[MAX_KEYS];
    bool keyPressedPrev[MAX_KEYS];  // for edge detection

    bool mouseDown[MAX_MOUSE_BUTTONS];
    bool mousePressed[MAX_MOUSE_BUTTONS];
    bool mousePressedPrev[MAX_MOUSE_BUTTONS];

    float mouseX, mouseY;
    float mouseDX, mouseDY;
    float wheelDelta;

    int charQueue[CHAR_QUEUE_SIZE];
    int charHead;
    int charTail;
} input;

// --- sokol keycode to our key code mapping ---

static int SokolKeyToFd(sapp_keycode key) {
    // sokol_app keycodes match GLFW which our FD_KEY codes are based on
    return (int)key;
}

static int SokolMouseButtonToFd(sapp_mousebutton btn) {
    switch (btn) {
        case SAPP_MOUSEBUTTON_LEFT:   return FD_MOUSE_LEFT;
        case SAPP_MOUSEBUTTON_RIGHT:  return FD_MOUSE_RIGHT;
        case SAPP_MOUSEBUTTON_MIDDLE: return FD_MOUSE_MIDDLE;
        default: return -1;
    }
}

// --- Public API ---

void FdInputBeginFrame(void) {
    // Edge-detect: keyPressed = keyDown && !keyDownPrev
    for (int i = 0; i < MAX_KEYS; i++) {
        input.keyPressed[i] = input.keyDown[i] && !input.keyPressedPrev[i];
        input.keyPressedPrev[i] = input.keyDown[i];
    }
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) {
        input.mousePressed[i] = input.mouseDown[i] && !input.mousePressedPrev[i];
        input.mousePressedPrev[i] = input.mouseDown[i];
    }
    // Wheel resets each frame — accumulated in events
    input.wheelDelta = 0.0f;
    // Delta resets — accumulated in events
    input.mouseDX = 0.0f;
    input.mouseDY = 0.0f;
}

bool FdKeyDown(int key) {
    if (key < 0 || key >= MAX_KEYS) return false;
    return input.keyDown[key];
}

bool FdKeyPressed(int key) {
    if (key < 0 || key >= MAX_KEYS) return false;
    return input.keyPressed[key];
}

bool FdMouseDown(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return input.mouseDown[button];
}

bool FdMousePressed(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return input.mousePressed[button];
}

Vector2 FdMousePosition(void) {
    return (Vector2){ input.mouseX, input.mouseY };
}

Vector2 FdMouseDelta(void) {
    return (Vector2){ input.mouseDX, input.mouseDY };
}

float FdMouseWheel(void) {
    return input.wheelDelta;
}

int FdCharPressed(void) {
    if (input.charHead == input.charTail) return 0;
    int ch = input.charQueue[input.charTail];
    input.charTail = (input.charTail + 1) % CHAR_QUEUE_SIZE;
    return ch;
}

bool FdPointInRect(Vector2 point, FdRect rect) {
    return point.x >= rect.x && point.x <= rect.x + rect.w &&
           point.y >= rect.y && point.y <= rect.y + rect.h;
}

// --- Event processing (called from fd_app_sokol.m) ---

void FdInputProcessEvent(const sapp_event *ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN: {
            int k = SokolKeyToFd(ev->key_code);
            if (k >= 0 && k < MAX_KEYS) {
                input.keyDown[k] = true;
            }
        } break;

        case SAPP_EVENTTYPE_KEY_UP: {
            int k = SokolKeyToFd(ev->key_code);
            if (k >= 0 && k < MAX_KEYS) {
                input.keyDown[k] = false;
            }
        } break;

        case SAPP_EVENTTYPE_CHAR: {
            uint32_t ch = ev->char_code;
            if (ch >= 32 && ch < 127) {
                int next = (input.charHead + 1) % CHAR_QUEUE_SIZE;
                if (next != input.charTail) {
                    input.charQueue[input.charHead] = (int)ch;
                    input.charHead = next;
                }
            }
        } break;

        case SAPP_EVENTTYPE_MOUSE_DOWN: {
            int b = SokolMouseButtonToFd(ev->mouse_button);
            if (b >= 0 && b < MAX_MOUSE_BUTTONS) {
                input.mouseDown[b] = true;
            }
        } break;

        case SAPP_EVENTTYPE_MOUSE_UP: {
            int b = SokolMouseButtonToFd(ev->mouse_button);
            if (b >= 0 && b < MAX_MOUSE_BUTTONS) {
                input.mouseDown[b] = false;
            }
        } break;

        case SAPP_EVENTTYPE_MOUSE_MOVE: {
            input.mouseX = ev->mouse_x;
            input.mouseY = ev->mouse_y;
            input.mouseDX += ev->mouse_dx;
            input.mouseDY += ev->mouse_dy;
        } break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL: {
            input.wheelDelta += ev->scroll_y;
        } break;

        default:
            break;
    }
}
