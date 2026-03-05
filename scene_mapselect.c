#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include <string.h>

static void MapSelectDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    Vector2 mouse = GetMousePosition();

    BeginDrawing();
    ClearBackground((Color){ 20, 22, 28, 255 });

    const char *msTitle = app->mapSelectForMultiplayer ? "Select Map (Host)" : "Select Map";
    UIDrawCenteredText(msTitle, screenW / 2, 50, 36, WHITE);

    if (app->mapRegistry.count == 0) {
        DrawText("No maps found in maps/ directory", screenW / 2 - 150, 120, 18, LIGHTGRAY);
    }

    for (int i = 0; i < app->mapRegistry.count; i++) {
        int my = 110 + i * 50;
        bool selected = (app->selectedMapIdx == i);
        bool hover = CheckCollisionPointRec(mouse,
            (Rectangle){ (float)(screenW / 2 - 150), (float)my, 300.0f, 42.0f });

        Color bg = selected ? (Color){ 50, 80, 110, 255 } :
                   hover    ? (Color){ 40, 55, 75, 255 }  :
                              (Color){ 30, 35, 45, 200 };
        DrawRectangle(screenW / 2 - 150, my, 300, 42, bg);
        DrawRectangleLines(screenW / 2 - 150, my, 300, 42, (Color){ 80, 100, 120, 200 });
        DrawText(app->mapRegistry.names[i], screenW / 2 - 138, my + 12, 20,
                 selected ? GOLD : WHITE);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            app->selectedMapIdx = i;
    }

    int btnBaseY = 110 + (app->mapRegistry.count > 0 ? app->mapRegistry.count : 1) * 50 + 20;

    bool canStart = app->mapRegistry.count > 0;
    const char *startText = app->mapSelectForMultiplayer ? "Host with Map" : "Play";

    UIButtonResult startBtn = canStart
        ? UIButton((screenW - 200) / 2, btnBaseY, 200, 45, startText, 24, &UI_STYLE_PRIMARY)
        : UIButtonDisabled((screenW - 200) / 2, btnBaseY, 200, 45, startText, 24, &UI_STYLE_PRIMARY);

    int bbY = btnBaseY + 45 + 12;
    UIButtonResult backBtn = UIButton((screenW - 200) / 2, bbY, 200, 40, "Back", 22, &UI_STYLE_DANGER);

    // Handle start click
    if (startBtn.clicked && canStart) {
        if (app->mapSelectForMultiplayer) {
            if (app->lobbyState.usernameLen > 0) {
                NetInit();
                if (NetHostCreate(&app->netCtx, app->lobbyState.username)) {
                    strncpy(app->netCtx.selectedMap, app->mapRegistry.names[app->selectedMapIdx], MAX_MAP_NAME - 1);
                    strncpy(app->netCtx.selectedMapPath, app->mapRegistry.paths[app->selectedMapIdx], 255);
                    SceneManagerTransition(app->sceneManager, SCENE_DIFFICULTY_SELECT);
                }
            }
        } else {
            if (!MapLoad(&app->map, app->mapRegistry.paths[app->selectedMapIdx]))
                MapInit(&app->map);
            MapBuildMesh(&app->gameMapMesh, &app->map, app->ps1Shader);
            SceneManagerTransition(app->sceneManager, SCENE_DIFFICULTY_SELECT);
        }
    }

    // Handle back
    if (backBtn.clicked || IsKeyPressed(KEY_ESCAPE)) {
        if (app->mapSelectForMultiplayer) {
            if (app->netCtx.mode == NET_MODE_HOST) {
                NetContextDestroy(&app->netCtx);
                NetShutdown();
                NetContextInit(&app->netCtx);
            }
            SceneManagerTransition(app->sceneManager, SCENE_LOBBY);
        } else {
            SceneManagerTransition(app->sceneManager, SCENE_MENU);
        }
    }

    EndDrawing();
}

Scene SceneMapSelectCreate(void)
{
    return (Scene){
        .name = "MapSelect",
        .draw = MapSelectDraw,
    };
}
