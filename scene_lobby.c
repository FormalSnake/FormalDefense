#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

static void LobbySceneDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    LobbyUpdate(&app->lobbyState, &app->netCtx);

    BeginDrawing();
    ClearBackground((Color){ 20, 22, 28, 255 });
    LobbyDraw(&app->lobbyState, &app->netCtx, screenW, screenH);
    EndDrawing();

    // Host wants to pick a map
    if (app->lobbyState.hostRequested) {
        app->lobbyState.hostRequested = false;
        app->mapSelectForMultiplayer = true;
        MapRegistryScan(&app->mapRegistry, "maps");
        app->selectedMapIdx = 0;
        SceneManagerTransition(app->sceneManager, SCENE_MAP_SELECT);
    }

    // Back button or ESC returns to menu
    if (LobbyBackPressed(&app->lobbyState) ||
        (app->lobbyState.phase == LOBBY_CHOOSE && IsKeyPressed(KEY_ESCAPE))) {
        SceneManagerTransition(app->sceneManager, SCENE_MENU);
    }

    // Check if game should start
    if (LobbyGameStarted(&app->lobbyState, &app->netCtx)) {
        if (app->netCtx.mode == NET_MODE_HOST) {
            if (!MapLoad(&app->map, app->netCtx.selectedMapPath))
                MapInit(&app->map);
            NetSendMapData(&app->netCtx, &app->map);
            GameStateInitMultiplayer(&app->gs, app->netCtx.playerCount,
                                    (Difficulty)app->netCtx.selectedDifficulty, &app->runMods);
        } else {
            char localPath[256];
            snprintf(localPath, sizeof(localPath), "maps/%s.fdmap", app->netCtx.selectedMap);
            if (!MapLoad(&app->map, localPath))
                MapInit(&app->map);
            GameStateInitMultiplayer(&app->gs, app->netCtx.playerCount,
                                    (Difficulty)app->netCtx.selectedDifficulty, &app->runMods);
        }
        MapBuildMesh(&app->gameMapMesh, &app->map, app->ps1Shader);
        memset(app->enemies, 0, sizeof(app->enemies));
        memset(app->towers, 0, sizeof(app->towers));
        memset(app->projectiles, 0, sizeof(app->projectiles));
        CameraControllerInit(&g_camCtrl);
        CameraControllerUpdate(&g_camCtrl, &app->camera, 0.0f);
        app->selectedTowerType = -1;
        app->selectedTowerIdx = -1;
        SceneManagerTransition(app->sceneManager, SCENE_GAME);
    }
}

Scene SceneLobbyCreate(void)
{
    return (Scene){
        .name = "Lobby",
        .draw = LobbySceneDraw,
    };
}
