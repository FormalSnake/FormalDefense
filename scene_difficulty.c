#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"

static void DifficultyDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    Vector2 mouse = GetMousePosition();

    BeginDrawing();
    ClearBackground((Color){ 20, 22, 28, 255 });

    UIDrawCenteredText("Select Difficulty", screenW / 2, 50, 36, WHITE);

    const char *descriptions[DIFFICULTY_COUNT] = {
        "Fewer, weaker enemies. More gold and lives.",
        "The standard experience.",
        "Tougher enemies, less gold, faster spawns.",
        "Extreme challenge. Only for the brave.",
    };

    for (int i = 0; i < DIFFICULTY_COUNT; i++) {
        int dy = 120 + i * 65;
        bool selected = ((int)app->selectedDifficulty == i);
        bool hover = CheckCollisionPointRec(mouse,
            (Rectangle){ (float)(screenW / 2 - 180), (float)dy, 360.0f, 55.0f });

        Color bg = selected ? (Color){ 50, 80, 110, 255 } :
                   hover    ? (Color){ 40, 55, 75, 255 }  :
                              (Color){ 30, 35, 45, 200 };
        DrawRectangle(screenW / 2 - 180, dy, 360, 55, bg);
        DrawRectangleLines(screenW / 2 - 180, dy, 360, 55, (Color){ 80, 100, 120, 200 });
        DrawText(DIFFICULTY_CONFIGS[i].name, screenW / 2 - 168, dy + 6, 24,
                 selected ? DIFFICULTY_CONFIGS[i].color : WHITE);
        DrawText(descriptions[i], screenW / 2 - 168, dy + 32, 14, LIGHTGRAY);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            app->selectedDifficulty = (Difficulty)i;
    }

    int dsBtnY = 120 + DIFFICULTY_COUNT * 65 + 20;
    int dsBtnW = 200;
    int dsBtnX = (screenW - dsBtnW) / 2;

    UIButtonResult startBtn = UIButton(dsBtnX, dsBtnY, dsBtnW, 45, "Start", 24, &UI_STYLE_PRIMARY);
    UIButtonResult backBtn = UIButton(dsBtnX, dsBtnY + 57, dsBtnW, 40, "Back", 22, &UI_STYLE_DANGER);

    EndDrawing();

    if (startBtn.clicked) {
        if (app->mapSelectForMultiplayer) {
            app->netCtx.selectedDifficulty = (uint8_t)app->selectedDifficulty;
            NetDiscoveryStart(&app->netCtx);
            app->lobbyState.phase = LOBBY_HOST_WAIT;
            SceneManagerTransition(app->sceneManager, SCENE_LOBBY);
        } else {
            SceneManagerTransition(app->sceneManager, SCENE_SHOP);
        }
    }

    if (backBtn.clicked || IsKeyPressed(KEY_ESCAPE)) {
        if (app->mapSelectForMultiplayer) {
            if (app->netCtx.mode == NET_MODE_HOST) {
                NetContextDestroy(&app->netCtx);
                NetShutdown();
                NetContextInit(&app->netCtx);
            }
        }
        SceneManagerTransition(app->sceneManager, SCENE_MAP_SELECT);
    }
}

Scene SceneDifficultyCreate(void)
{
    return (Scene){
        .name = "DifficultySelect",
        .draw = DifficultyDraw,
    };
}
