#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include <math.h>

static void MenuUpdate(Scene *scene, void *ctx, float dt)
{
    (void)scene; (void)dt;
    AppContext *app = (AppContext *)ctx;

    // Auto-rotate camera
    g_menuCamCtrl.yaw += 8.0f * dt;
    float yawRad = g_menuCamCtrl.yaw * DEG2RAD;
    float pitchRad = g_menuCamCtrl.pitch * DEG2RAD;
    app->menuCamera.position = (Vector3){
        g_menuCamCtrl.target.x + g_menuCamCtrl.distance * cosf(pitchRad) * sinf(yawRad),
        g_menuCamCtrl.target.y + g_menuCamCtrl.distance * sinf(pitchRad),
        g_menuCamCtrl.target.z + g_menuCamCtrl.distance * cosf(pitchRad) * cosf(yawRad),
    };
    app->menuCamera.target = g_menuCamCtrl.target;
    app->menuCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    app->menuCamera.fovy = 45.0f;
    app->menuCamera.projection = CAMERA_PERSPECTIVE;
}

static void MenuDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    // 3D to low-res target
    BeginTextureMode(app->renderTarget);
    ClearBackground((Color){ 30, 30, 35, 255 });
    BeginMode3D(app->menuCamera);
        DrawSkybox(app->menuCamera);
        DrawWater(app->waterShader, GetShaderLocation(app->waterShader, "time"), app->totalTime);
        MapDrawMesh(&app->menuMapMesh);
        BeginShaderMode(app->ps1Shader);
            for (int i = 0; i < app->menuTreeCount; i++) {
                float s = app->menuTrees[i].scale * 0.05f;
                DrawModelEx(app->treeModel, app->menuTrees[i].position,
                            (Vector3){0,1,0}, app->menuTrees[i].rotation,
                            (Vector3){s,s,s}, WHITE);
            }
        EndShaderMode();
    EndMode3D();
    EndTextureMode();

    // Full-res output
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(app->renderTarget.texture,
        (Rectangle){ 0, 0, (float)app->rtW, -(float)app->rtH },
        (Rectangle){ 0, 0, (float)screenW, (float)screenH },
        (Vector2){ 0, 0 }, 0.0f, WHITE);

    // Dark overlay
    DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 120 });

    // Title
    UIDrawCenteredText("Formal Defense", screenW / 2, screenH / 2 - 100, 60, WHITE);
    UIDrawCenteredText("Tower Defense", screenW / 2, screenH / 2 - 35, 24, LIGHTGRAY);

    // Buttons
    int pbW = 180, pbH = 50;
    int pbX = (screenW - pbW) / 2;
    int pbY = screenH / 2 + 20;

    UIButtonResult playBtn = UIButton(pbX, pbY, pbW, pbH, "Play", 30, &UI_STYLE_PRIMARY);
    UIButtonResult mpBtn = UIButton(pbX, pbY + pbH + 15, pbW, pbH, "Multiplayer", 30, &UI_STYLE_SECONDARY);

    // Settings button (purple style)
    static const UIStyle settingsStyle = {
        .bgNormal = { 48, 38, 78, 255 }, .bgHover = { 70, 55, 110, 255 },
        .bgPressed = { 38, 28, 60, 255 }, .bgDisabled = { 40, 40, 40, 255 },
        .bgSelected = { 60, 48, 90, 255 },
        .border = { 140, 110, 200, 200 }, .borderHover = { 160, 130, 220, 255 },
        .text = WHITE, .textDisabled = { 100, 100, 100, 255 }, .borderWidth = 2.0f,
    };
    UIButtonResult setBtn = UIButton(pbX, pbY + (pbH + 15) * 2, pbW, pbH, "Settings", 30, &settingsStyle);
    UIButtonResult quitBtn = UIButton(pbX, pbY + (pbH + 15) * 3, pbW, pbH, "Quit", 30, &UI_STYLE_DANGER);

    // Settings overlay
    SettingsDraw(&app->settingsState, screenW, screenH);

    // Copyright
    DrawText("Made by FormalSnake", 10, screenH - 40, 16, LIGHTGRAY);
    DrawText("(c) 2026 FormalSnake", 10, screenH - 22, 14, GRAY);

    EndDrawing();

    // Settings update
    int settingsResult = SettingsUpdate(&app->settingsState);
    if (settingsResult == 1) {
        app->settings = app->settingsState.pending;
        SettingsSave(&app->settings, "settings.cfg");

        UnloadRenderTexture(app->renderTarget);
        app->rtW = screenW / app->settings.ps1Downscale;
        app->rtH = screenH / app->settings.ps1Downscale;
        app->renderTarget = LoadRenderTexture(app->rtW, app->rtH);
        SetTextureFilter(app->renderTarget.texture, TEXTURE_FILTER_POINT);
        float resolution[2] = { (float)app->rtW, (float)app->rtH };
        SetShaderValue(app->ps1Shader, app->locResolution, resolution, SHADER_UNIFORM_VEC2);
        SetShaderValue(app->waterShader, app->wLocResolution, resolution, SHADER_UNIFORM_VEC2);

        if (app->settings.vsync) SetWindowState(FLAG_VSYNC_HINT);
        else ClearWindowState(FLAG_VSYNC_HINT);

        if (app->settings.fullscreen == FULLSCREEN_WINDOWED) {
            if (IsWindowFullscreen()) ToggleFullscreen();
            else if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
            SetWindowSize(RESOLUTION_PRESETS[app->settings.resolutionIdx].width,
                          RESOLUTION_PRESETS[app->settings.resolutionIdx].height);
        } else if (app->settings.fullscreen == FULLSCREEN_BORDERLESS) {
            if (IsWindowFullscreen()) ToggleFullscreen();
            if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
        } else if (app->settings.fullscreen == FULLSCREEN_EXCLUSIVE) {
            if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
            if (!IsWindowFullscreen()) ToggleFullscreen();
        }

        g_camCtrl.panSpeed = app->settings.camPanSpeed;
        g_camCtrl.rotSpeed = app->settings.camRotSpeed;
        g_camCtrl.zoomSpeed = app->settings.camZoomSpeed;
    }

    if (app->settingsState.open) return;

    if (setBtn.clicked) {
        SettingsOpen(&app->settingsState, &app->settings);
        return;
    }

    if (quitBtn.clicked) {
        CloseWindow();
        return;
    }

    if (mpBtn.clicked) {
        LobbyStateInit(&app->lobbyState);
        SceneManagerTransition(app->sceneManager, SCENE_LOBBY);
    }

    if (playBtn.clicked) {
        app->mapSelectForMultiplayer = false;
        MapRegistryScan(&app->mapRegistry, "maps");
        app->selectedMapIdx = 0;
        SceneManagerTransition(app->sceneManager, SCENE_MAP_SELECT);
    }
}

Scene SceneMenuCreate(void)
{
    return (Scene){
        .name = "Menu",
        .update = MenuUpdate,
        .draw = MenuDraw,
    };
}
