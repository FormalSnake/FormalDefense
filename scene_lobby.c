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
        // Load the map now (persists through shop/perk screens)
        if (app->netCtx.mode == NET_MODE_HOST) {
            if (!MapLoad(&app->map, app->netCtx.selectedMapPath))
                MapInit(&app->map);
            NetSendMapData(&app->netCtx, &app->map);
        } else {
            char localPath[256];
            snprintf(localPath, sizeof(localPath), "maps/%s.fdmap", app->netCtx.selectedMap);
            if (!MapLoad(&app->map, localPath))
                MapInit(&app->map);
        }
        MapBuildMesh(&app->gameMapMesh, &app->map, app->ps1Shader);

        // Each player inits RunModifiers from their own local profile
        RunModifiersInit(&app->runMods, &app->profile);

        // Send per-player unlocks to host
        NetSendPlayerUnlocks(&app->netCtx, &app->runMods);

        // Go to shop (each player sees their own crystal shop)
        SceneManagerTransition(app->sceneManager, SCENE_SHOP);
    }
}

Scene SceneLobbyCreate(void)
{
    return (Scene){
        .name = "Lobby",
        .draw = LobbySceneDraw,
    };
}
