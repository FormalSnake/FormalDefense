#ifndef FD_INPUT_H
#define FD_INPUT_H

#include "fd_math.h"
#include <stdbool.h>

// --- Frame management ---
void FdInputBeginFrame(void);

// --- Keyboard ---
bool FdKeyDown(int key);
bool FdKeyPressed(int key);

// --- Mouse ---
bool FdMouseDown(int button);
bool FdMousePressed(int button);
Vector2 FdMousePosition(void);
Vector2 FdMouseDelta(void);
float FdMouseWheel(void);

// --- Character input queue ---
int FdCharPressed(void);  // returns 0 when queue empty

// --- Hit testing ---
bool FdPointInRect(Vector2 point, FdRect rect);

// --- Key codes (matching sokol_app key codes) ---
#define FD_KEY_SPACE          32
#define FD_KEY_APOSTROPHE     39
#define FD_KEY_COMMA          44
#define FD_KEY_MINUS          45
#define FD_KEY_PERIOD         46
#define FD_KEY_SLASH          47
#define FD_KEY_0              48
#define FD_KEY_1              49
#define FD_KEY_2              50
#define FD_KEY_3              51
#define FD_KEY_4              52
#define FD_KEY_5              53
#define FD_KEY_6              54
#define FD_KEY_7              55
#define FD_KEY_8              56
#define FD_KEY_9              57
#define FD_KEY_A              65
#define FD_KEY_B              66
#define FD_KEY_C              67
#define FD_KEY_D              68
#define FD_KEY_E              69
#define FD_KEY_F              70
#define FD_KEY_G              71
#define FD_KEY_H              72
#define FD_KEY_I              73
#define FD_KEY_J              74
#define FD_KEY_K              75
#define FD_KEY_L              76
#define FD_KEY_M              77
#define FD_KEY_N              78
#define FD_KEY_O              79
#define FD_KEY_P              80
#define FD_KEY_Q              81
#define FD_KEY_R              82
#define FD_KEY_S              83
#define FD_KEY_T              84
#define FD_KEY_U              85
#define FD_KEY_V              86
#define FD_KEY_W              87
#define FD_KEY_X              88
#define FD_KEY_Y              89
#define FD_KEY_Z              90

#define FD_KEY_ESCAPE         256
#define FD_KEY_ENTER          257
#define FD_KEY_TAB            258
#define FD_KEY_BACKSPACE      259
#define FD_KEY_RIGHT          262
#define FD_KEY_LEFT           263
#define FD_KEY_DOWN           264
#define FD_KEY_UP             265
#define FD_KEY_F1             290
#define FD_KEY_F2             291
#define FD_KEY_F3             292
#define FD_KEY_F4             293
#define FD_KEY_F5             294
#define FD_KEY_F6             295
#define FD_KEY_F7             296
#define FD_KEY_F8             297
#define FD_KEY_F9             298
#define FD_KEY_F10            299
#define FD_KEY_F11            300
#define FD_KEY_F12            301

// Raylib compatibility aliases
#define KEY_SPACE     FD_KEY_SPACE
#define KEY_A         FD_KEY_A
#define KEY_B         FD_KEY_B
#define KEY_C         FD_KEY_C
#define KEY_D         FD_KEY_D
#define KEY_E         FD_KEY_E
#define KEY_F         FD_KEY_F
#define KEY_G         FD_KEY_G
#define KEY_H         FD_KEY_H
#define KEY_I         FD_KEY_I
#define KEY_J         FD_KEY_J
#define KEY_K         FD_KEY_K
#define KEY_L         FD_KEY_L
#define KEY_M         FD_KEY_M
#define KEY_N         FD_KEY_N
#define KEY_O         FD_KEY_O
#define KEY_P         FD_KEY_P
#define KEY_Q         FD_KEY_Q
#define KEY_R         FD_KEY_R
#define KEY_S         FD_KEY_S
#define KEY_T         FD_KEY_T
#define KEY_U         FD_KEY_U
#define KEY_V         FD_KEY_V
#define KEY_W         FD_KEY_W
#define KEY_X         FD_KEY_X
#define KEY_Y         FD_KEY_Y
#define KEY_Z         FD_KEY_Z
#define KEY_ONE       FD_KEY_1
#define KEY_TWO       FD_KEY_2
#define KEY_THREE     FD_KEY_3
#define KEY_FOUR      FD_KEY_4
#define KEY_UP        FD_KEY_UP
#define KEY_DOWN      FD_KEY_DOWN
#define KEY_LEFT      FD_KEY_LEFT
#define KEY_RIGHT     FD_KEY_RIGHT
#define KEY_ESCAPE    FD_KEY_ESCAPE
#define KEY_ENTER     FD_KEY_ENTER
#define KEY_TAB       FD_KEY_TAB
#define KEY_BACKSPACE FD_KEY_BACKSPACE
#define KEY_F11       FD_KEY_F11

// Mouse button codes
#define FD_MOUSE_LEFT    0
#define FD_MOUSE_RIGHT   1
#define FD_MOUSE_MIDDLE  2

#define MOUSE_BUTTON_LEFT   FD_MOUSE_LEFT
#define MOUSE_BUTTON_RIGHT  FD_MOUSE_RIGHT
#define MOUSE_BUTTON_MIDDLE FD_MOUSE_MIDDLE

// --- Raylib compatibility wrappers ---
#define IsKeyDown(k)              FdKeyDown(k)
#define IsKeyPressed(k)           FdKeyPressed(k)
#define IsMouseButtonDown(b)      FdMouseDown(b)
#define IsMouseButtonPressed(b)   FdMousePressed(b)
#define GetMousePosition()        FdMousePosition()
#define GetMouseDelta()           FdMouseDelta()
#define GetMouseWheelMove()       FdMouseWheel()
#define GetCharPressed()          FdCharPressed()

static inline bool CheckCollisionPointRec(Vector2 point, FdRect rect) {
    return FdPointInRect(point, rect);
}

// Rectangle compatibility type for game code
typedef FdRect Rectangle;

#endif // FD_INPUT_H
