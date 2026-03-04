#ifndef FD_APP_H
#define FD_APP_H

// --- Window / lifecycle ---
int FdScreenWidth(void);
int FdScreenHeight(void);
float FdFrameTime(void);
void FdToggleFullscreen(void);
void FdQuitApp(void);

// --- Raylib compatibility macros ---
#define GetScreenWidth()            FdScreenWidth()
#define GetScreenHeight()           FdScreenHeight()
#define GetFrameTime()              FdFrameTime()
#define ToggleBorderlessWindowed()  FdToggleFullscreen()

#endif // FD_APP_H
