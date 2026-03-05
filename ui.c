#include "ui.h"
#include <string.h>
#include <stdio.h>

// --- Predefined Styles ---

const UIStyle UI_STYLE_PRIMARY = {
    .bgNormal  = { 40, 80, 40, 255 },
    .bgHover   = { 60, 120, 60, 255 },
    .bgPressed = { 30, 60, 30, 255 },
    .bgDisabled = { 40, 40, 40, 255 },
    .bgSelected = { 80, 120, 80, 255 },
    .border     = { 100, 200, 100, 200 },
    .borderHover = { 120, 220, 120, 255 },
    .text       = WHITE,
    .textDisabled = { 100, 100, 100, 255 },
    .borderWidth = 2.0f,
};

const UIStyle UI_STYLE_SECONDARY = {
    .bgNormal  = { 35, 55, 80, 255 },
    .bgHover   = { 50, 80, 120, 255 },
    .bgPressed = { 25, 40, 60, 255 },
    .bgDisabled = { 40, 40, 40, 255 },
    .bgSelected = { 60, 90, 130, 255 },
    .border     = { 100, 150, 220, 200 },
    .borderHover = { 120, 170, 240, 255 },
    .text       = WHITE,
    .textDisabled = { 100, 100, 100, 255 },
    .borderWidth = 2.0f,
};

const UIStyle UI_STYLE_NEUTRAL = {
    .bgNormal  = { 40, 50, 65, 255 },
    .bgHover   = { 60, 80, 100, 255 },
    .bgPressed = { 30, 40, 50, 255 },
    .bgDisabled = { 30, 30, 35, 255 },
    .bgSelected = { 50, 80, 110, 255 },
    .border     = { 100, 140, 180, 200 },
    .borderHover = { 120, 160, 200, 255 },
    .text       = WHITE,
    .textDisabled = { 100, 100, 100, 255 },
    .borderWidth = 2.0f,
};

const UIStyle UI_STYLE_DANGER = {
    .bgNormal  = { 100, 35, 35, 255 },
    .bgHover   = { 140, 50, 50, 255 },
    .bgPressed = { 80, 25, 25, 255 },
    .bgDisabled = { 40, 40, 40, 255 },
    .bgSelected = { 120, 45, 45, 255 },
    .border     = { 200, 100, 100, 200 },
    .borderHover = { 220, 120, 120, 255 },
    .text       = WHITE,
    .textDisabled = { 100, 100, 100, 255 },
    .borderWidth = 2.0f,
};

// --- Button ---

UIButtonResult UIButton(int x, int y, int w, int h, const char *text, int fontSize, const UIStyle *style)
{
    Vector2 mouse = GetMousePosition();
    Rectangle rect = { (float)x, (float)y, (float)w, (float)h };
    bool hovered = CheckCollisionPointRec(mouse, rect);
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = hovered ? style->bgHover : style->bgNormal;
    Color border = hovered ? style->borderHover : style->border;

    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, style->borderWidth, border);

    int tw = MeasureText(text, fontSize);
    DrawText(text, x + (w - tw) / 2, y + (h - fontSize) / 2, fontSize, style->text);

    return (UIButtonResult){ .clicked = pressed, .hovered = hovered };
}

UIButtonResult UIButtonDisabled(int x, int y, int w, int h, const char *text, int fontSize, const UIStyle *style)
{
    Rectangle rect = { (float)x, (float)y, (float)w, (float)h };

    DrawRectangleRec(rect, style->bgDisabled);
    DrawRectangleLinesEx(rect, style->borderWidth, style->border);

    int tw = MeasureText(text, fontSize);
    DrawText(text, x + (w - tw) / 2, y + (h - fontSize) / 2, fontSize, style->textDisabled);

    return (UIButtonResult){ .clicked = false, .hovered = false };
}

// --- Text Input ---

bool UITextInputUpdate(UITextInput *input)
{
    if (!input->active) return false;

    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (*input->len < input->maxLen && ch >= 32 && ch < 127) {
            input->buf[*input->len] = (char)ch;
            (*input->len)++;
            input->buf[*input->len] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && *input->len > 0) {
        (*input->len)--;
        input->buf[*input->len] = '\0';
    }
    return true;
}

void UITextInputDraw(const UITextInput *input, int x, int y, int w, int h)
{
    Rectangle rect = { (float)x, (float)y, (float)w, (float)h };
    Color border = input->active ? (Color){ 100, 200, 100, 255 } : (Color){ 80, 80, 80, 255 };
    DrawRectangleRec(rect, (Color){ 25, 25, 30, 255 });
    DrawRectangleLinesEx(rect, 2, border);
    DrawText(input->buf, x + 6, y + (h - 16) / 2, 16, WHITE);
    if (input->active) {
        int tw = MeasureText(input->buf, 16);
        DrawText("_", x + 6 + tw, y + (h - 16) / 2, 16, (Color){ 100, 200, 100, 255 });
    }
}

// --- Centered Text ---

void UIDrawCenteredText(const char *text, int centerX, int y, int fontSize, Color color)
{
    int tw = MeasureText(text, fontSize);
    DrawText(text, centerX - tw / 2, y, fontSize, color);
}

// --- List ---

UIListResult UIListCustom(int x, int y, int w, int itemH, int itemCount,
                          UIListItemRenderer renderer, int selectedIdx, void *userdata)
{
    Vector2 mouse = GetMousePosition();
    UIListResult result = { .selectedIndex = selectedIdx, .itemClicked = false, .clickedIndex = -1 };

    for (int i = 0; i < itemCount; i++) {
        int iy = y + i * itemH;
        Rectangle itemRect = { (float)x, (float)iy, (float)w, (float)(itemH - 2) };
        bool hovered = CheckCollisionPointRec(mouse, itemRect);
        bool selected = (i == selectedIdx);

        renderer(i, x, iy, w, itemH - 2, selected, hovered, userdata);

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            result.selectedIndex = i;
            result.itemClicked = true;
            result.clickedIndex = i;
        }
    }

    return result;
}

// --- Player List ---

void UIDrawPlayerList(int x, int y, int w, int playerCount,
                      const char names[][16], const bool connected[],
                      const Color colors[], int localPlayerIdx)
{
    for (int i = 0; i < playerCount; i++) {
        int py = y + i * 35;
        if (connected[i]) {
            DrawRectangle(x, py, w, 30, (Color){ 30, 40, 50, 200 });
            const char *suffix = (i == localPlayerIdx) ? " (You)" : "";
            DrawText(TextFormat("P%d: %s%s", i + 1, names[i], suffix),
                     x + 10, py + 6, 18, colors[i]);
        } else {
            DrawRectangle(x, py, w, 30, (Color){ 20, 20, 25, 150 });
            DrawText(TextFormat("P%d: ---", i + 1), x + 10, py + 6, 18, DARKGRAY);
        }
    }
}
