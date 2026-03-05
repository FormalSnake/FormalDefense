#ifndef UI_H
#define UI_H

#include "raylib.h"
#include <stdbool.h>

// --- Theme ---

typedef struct {
    Color bgNormal, bgHover, bgPressed, bgDisabled, bgSelected;
    Color border, borderHover, text, textDisabled;
    float borderWidth;
} UIStyle;

extern const UIStyle UI_STYLE_PRIMARY;
extern const UIStyle UI_STYLE_SECONDARY;
extern const UIStyle UI_STYLE_NEUTRAL;
extern const UIStyle UI_STYLE_DANGER;

// --- Button ---

typedef struct {
    bool clicked;
    bool hovered;
} UIButtonResult;

UIButtonResult UIButton(int x, int y, int w, int h, const char *text, int fontSize, const UIStyle *style);
UIButtonResult UIButtonDisabled(int x, int y, int w, int h, const char *text, int fontSize, const UIStyle *style);

// --- Text Input ---

typedef struct {
    char *buf;
    int *len;
    int maxLen;
    bool active;
} UITextInput;

bool UITextInputUpdate(UITextInput *input);
void UITextInputDraw(const UITextInput *input, int x, int y, int w, int h);

// --- Centered Text ---

void UIDrawCenteredText(const char *text, int centerX, int y, int fontSize, Color color);

// --- List ---

typedef void (*UIListItemRenderer)(int index, int x, int y, int w, int h, bool selected, bool hovered, void *userdata);

typedef struct {
    int selectedIndex;
    bool itemClicked;
    int clickedIndex;
} UIListResult;

UIListResult UIListCustom(int x, int y, int w, int itemH, int itemCount,
                          UIListItemRenderer renderer, int selectedIdx, void *userdata);

// --- Player List ---

void UIDrawPlayerList(int x, int y, int w, int playerCount,
                      const char names[][16], const bool connected[],
                      const Color colors[], int localPlayerIdx);

#endif
