#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include <string.h>

static void PerkSelectDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    Vector2 mouse = GetMousePosition();

    BeginDrawing();
    ClearBackground((Color){ 20, 22, 28, 255 });

    UIDrawCenteredText("Choose a Perk", screenW / 2, 40, 36, WHITE);
    UIDrawCenteredText("Or skip for no perk", screenW / 2, 82, 16, LIGHTGRAY);

    int cardW = 220, cardH = 140;
    int gap = 30;
    int totalW = 3 * cardW + 2 * gap;
    int startX = (screenW - totalW) / 2;
    int cardY = 120;

    for (int p = 0; p < 3; p++) {
        int pid = app->perkOffered[p];
        const PerkConfig *pc = &PERK_CONFIGS[pid];
        int cx = startX + p * (cardW + gap);
        Rectangle cardRect = { (float)cx, (float)cardY, (float)cardW, (float)cardH };
        bool hover = CheckCollisionPointRec(mouse, cardRect);

        Color bg = hover ? (Color){50,60,80,255} : (Color){35,40,50,200};
        DrawRectangleRec(cardRect, bg);
        DrawRectangleLinesEx(cardRect, 2, hover ? (Color){100,150,220,255} : (Color){60,70,90,200});

        DrawText(pc->name, cx + 10, cardY + 10, 20, WHITE);
        DrawText(pc->description, cx + 10, cardY + 40, 14, (Color){200,200,200,255});

        if (pc->hasTradeoff)
            DrawText("Trade-off", cx + 10, cardY + cardH - 24, 12, (Color){255,180,80,255});
        else
            DrawText("Pure buff", cx + 10, cardY + cardH - 24, 12, (Color){100,200,100,255});

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            RunModifiersApplyPerk(&app->runMods, pid);
            GameSceneReset(app);
            SceneManagerTransition(app->sceneManager, SCENE_GAME);
        }
    }

    // Skip button
    static const UIStyle skipStyle = {
        .bgNormal = { 50, 45, 40, 255 }, .bgHover = { 70, 60, 50, 255 },
        .bgPressed = { 40, 35, 30, 255 }, .bgDisabled = { 40, 40, 40, 255 },
        .bgSelected = { 60, 55, 45, 255 },
        .border = { 150, 130, 100, 200 }, .borderHover = { 170, 150, 120, 255 },
        .text = WHITE, .textDisabled = { 100, 100, 100, 255 }, .borderWidth = 2.0f,
    };
    int skipW = 140, skipH = 40;
    int skipX = (screenW - skipW) / 2;
    int skipY = cardY + cardH + 30;
    UIButtonResult skipBtn = UIButton(skipX, skipY, skipW, skipH, "Skip", 22, &skipStyle);

    EndDrawing();

    if (skipBtn.clicked) {
        GameSceneReset(app);
        SceneManagerTransition(app->sceneManager, SCENE_GAME);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        SceneManagerTransition(app->sceneManager, SCENE_SHOP);
    }
}

Scene ScenePerkSelectCreate(void)
{
    return (Scene){
        .name = "PerkSelect",
        .draw = PerkSelectDraw,
    };
}
