#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "map.h"
#include "entity.h"
#include "game.h"
#include "net.h"
#include "lobby.h"
#include "chat.h"
#include <math.h>
#include <string.h>

// --- PS1 Graphics Constants ---

#define PS1_DOWNSCALE 3
#define PS1_JITTER_STRENGTH 1.0f
#define PS1_COLOR_BANDS 24.0f
#define BLOB_SHADOW_SEGMENTS 12

// --- Scene ---

typedef enum { SCENE_MENU, SCENE_LOBBY, SCENE_GAME } Scene;

// --- Camera Controller ---

typedef struct {
    Vector3 target;
    float distance;
    float yaw;
    float pitch;
    float panSpeed;
    float rotSpeed;
    float zoomSpeed;
} CameraController;

static void CameraControllerInit(CameraController *cc)
{
    cc->target = (Vector3){ MAP_WIDTH * TILE_SIZE * 0.5f, 0.0f, MAP_HEIGHT * TILE_SIZE * 0.5f };
    cc->distance = 18.0f;
    cc->yaw = 0.0f;
    cc->pitch = 55.0f;
    cc->panSpeed = 12.0f;
    cc->rotSpeed = 0.3f;
    cc->zoomSpeed = 2.0f;
}

static void CameraControllerUpdate(CameraController *cc, Camera3D *cam, float dt)
{
    float panX = 0.0f, panZ = 0.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    panZ += 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  panZ -= 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  panX -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) panX += 1.0f;

    float yawRad = cc->yaw * DEG2RAD;
    float forwardX = -sinf(yawRad);
    float forwardZ = -cosf(yawRad);
    float rightX = cosf(yawRad);
    float rightZ = -sinf(yawRad);

    cc->target.x += (forwardX * panZ + rightX * panX) * cc->panSpeed * dt;
    cc->target.z += (forwardZ * panZ + rightZ * panX) * cc->panSpeed * dt;

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        cc->yaw   -= delta.x * cc->rotSpeed;
        cc->pitch += delta.y * cc->rotSpeed;
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        float wheel = GetMouseWheelMove();
        cc->distance -= wheel * cc->zoomSpeed;
    }

    if (cc->pitch < 20.0f) cc->pitch = 20.0f;
    if (cc->pitch > 80.0f) cc->pitch = 80.0f;
    if (cc->distance < 5.0f)  cc->distance = 5.0f;
    if (cc->distance > 30.0f) cc->distance = 30.0f;

    float pitchRad = cc->pitch * DEG2RAD;
    cam->position = (Vector3){
        cc->target.x + cc->distance * cosf(pitchRad) * sinf(yawRad),
        cc->target.y + cc->distance * sinf(pitchRad),
        cc->target.z + cc->distance * cosf(pitchRad) * cosf(yawRad),
    };
    cam->target = cc->target;
    cam->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam->fovy = 45.0f;
    cam->projection = CAMERA_PERSPECTIVE;
}

// --- Mouse Ray → Ground Plane ---

static bool GetMouseGroundPos(Camera3D camera, Vector3 *outPos)
{
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    if (fabsf(ray.direction.y) < 0.001f) return false;
    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) return false;
    outPos->x = ray.position.x + ray.direction.x * t;
    outPos->y = 0.0f;
    outPos->z = ray.position.z + ray.direction.z * t;
    return true;
}

// --- Draw Range Circle ---

static void DrawRangeCircle(Vector3 center, float radius, Color color)
{
    int segments = 48;
    for (int i = 0; i < segments; i++) {
        float a1 = (float)i / segments * 2.0f * PI;
        float a2 = (float)(i + 1) / segments * 2.0f * PI;
        Vector3 p1 = { center.x + cosf(a1) * radius, 0.02f, center.z + sinf(a1) * radius };
        Vector3 p2 = { center.x + cosf(a2) * radius, 0.02f, center.z + sinf(a2) * radius };
        DrawLine3D(p1, p2, color);
    }
}

// --- Skybox ---

static void DrawSkybox(Camera3D camera)
{
    int slices = 16;
    int stacks = 8;
    float radius = 500.0f;

    Color topColor = { 25, 50, 120, 255 };
    Color botColor = { 135, 190, 235, 255 };

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    rlPushMatrix();
    rlTranslatef(camera.position.x, camera.position.y, camera.position.z);

    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < stacks; i++) {
        float phi0 = PI * (float)i / stacks;
        float phi1 = PI * (float)(i + 1) / stacks;
        float t0 = 1.0f - (float)i / stacks;       // 1 at top, 0 at bottom
        float t1 = 1.0f - (float)(i + 1) / stacks;

        unsigned char r0 = (unsigned char)(botColor.r + t0 * (topColor.r - botColor.r));
        unsigned char g0 = (unsigned char)(botColor.g + t0 * (topColor.g - botColor.g));
        unsigned char b0 = (unsigned char)(botColor.b + t0 * (topColor.b - botColor.b));
        unsigned char r1 = (unsigned char)(botColor.r + t1 * (topColor.r - botColor.r));
        unsigned char g1 = (unsigned char)(botColor.g + t1 * (topColor.g - botColor.g));
        unsigned char b1 = (unsigned char)(botColor.b + t1 * (topColor.b - botColor.b));

        for (int j = 0; j < slices; j++) {
            float theta0 = 2.0f * PI * (float)j / slices;
            float theta1 = 2.0f * PI * (float)(j + 1) / slices;

            float x00 = radius * sinf(phi0) * cosf(theta0);
            float y00 = radius * cosf(phi0);
            float z00 = radius * sinf(phi0) * sinf(theta0);

            float x10 = radius * sinf(phi1) * cosf(theta0);
            float y10 = radius * cosf(phi1);
            float z10 = radius * sinf(phi1) * sinf(theta0);

            float x01 = radius * sinf(phi0) * cosf(theta1);
            float y01 = radius * cosf(phi0);
            float z01 = radius * sinf(phi0) * sinf(theta1);

            float x11 = radius * sinf(phi1) * cosf(theta1);
            float y11 = radius * cosf(phi1);
            float z11 = radius * sinf(phi1) * sinf(theta1);

            // Triangle 1: (00, 10, 11)
            rlColor4ub(r0, g0, b0, 255);
            rlVertex3f(x00, y00, z00);
            rlColor4ub(r1, g1, b1, 255);
            rlVertex3f(x10, y10, z10);
            rlColor4ub(r1, g1, b1, 255);
            rlVertex3f(x11, y11, z11);

            // Triangle 2: (00, 11, 01)
            rlColor4ub(r0, g0, b0, 255);
            rlVertex3f(x00, y00, z00);
            rlColor4ub(r1, g1, b1, 255);
            rlVertex3f(x11, y11, z11);
            rlColor4ub(r0, g0, b0, 255);
            rlVertex3f(x01, y01, z01);
        }
    }
    rlEnd();

    rlPopMatrix();

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

// --- Blob Shadow ---

static void DrawBlobShadow(Vector3 center, float radius)
{
    float y = 0.01f;
    Color col = (Color){ 0, 0, 0, 80 };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < BLOB_SHADOW_SEGMENTS; i++) {
        float a1 = (float)i / BLOB_SHADOW_SEGMENTS * 2.0f * PI;
        float a2 = (float)(i + 1) / BLOB_SHADOW_SEGMENTS * 2.0f * PI;
        rlColor4ub(col.r, col.g, col.b, col.a);
        rlVertex3f(center.x, y, center.z);
        rlVertex3f(center.x + cosf(a1) * radius, y, center.z + sinf(a1) * radius);
        rlVertex3f(center.x + cosf(a2) * radius, y, center.z + sinf(a2) * radius);
    }
    rlEnd();
}

// --- UI Constants ---

#define BOTTOM_BAR_HEIGHT 60
#define BTN_WIDTH 120
#define BTN_HEIGHT 45
#define BTN_MARGIN 10
#define INFO_PANEL_W 200
#define INFO_PANEL_H 160

// --- Chat Callback ---

static ChatState *g_chatStatePtr = NULL;

static void OnNetChatReceived(uint8_t playerIndex, const char *username, const char *message)
{
    if (g_chatStatePtr)
        ChatAddMessage(g_chatStatePtr, playerIndex, username, message);
}

// --- Main ---

int main(void)
{
    InitWindow(1280, 720, "Formal Defense");
    SetExitKey(0);
    SetTargetFPS(60);

    // --- PS1 Shader & Render Target ---
    Shader ps1Shader = LoadShader("shaders/ps1.vs", "shaders/ps1.fs");
    ps1Shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(ps1Shader, "matModel");

    int locResolution = GetShaderLocation(ps1Shader, "resolution");
    int locJitter = GetShaderLocation(ps1Shader, "jitterStrength");
    int locLightDir = GetShaderLocation(ps1Shader, "lightDir");
    int locLightColor = GetShaderLocation(ps1Shader, "lightColor");
    int locAmbientColor = GetShaderLocation(ps1Shader, "ambientColor");
    int locColorBands = GetShaderLocation(ps1Shader, "colorBands");

    float jitterStrength = PS1_JITTER_STRENGTH;
    SetShaderValue(ps1Shader, locJitter, &jitterStrength, SHADER_UNIFORM_FLOAT);

    float lightDir[3] = { 0.4f, -0.7f, 0.3f };
    SetShaderValue(ps1Shader, locLightDir, lightDir, SHADER_UNIFORM_VEC3);

    float lightColor[3] = { 0.7f, 0.7f, 0.65f };
    SetShaderValue(ps1Shader, locLightColor, lightColor, SHADER_UNIFORM_VEC3);

    float ambientColor[3] = { 0.3f, 0.3f, 0.35f };
    SetShaderValue(ps1Shader, locAmbientColor, ambientColor, SHADER_UNIFORM_VEC3);

    float colorBands = PS1_COLOR_BANDS;
    SetShaderValue(ps1Shader, locColorBands, &colorBands, SHADER_UNIFORM_FLOAT);

    int cachedScreenW = GetScreenWidth();
    int cachedScreenH = GetScreenHeight();
    int rtW = cachedScreenW / PS1_DOWNSCALE;
    int rtH = cachedScreenH / PS1_DOWNSCALE;
    RenderTexture2D renderTarget = LoadRenderTexture(rtW, rtH);
    SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);

    float resolution[2] = { (float)rtW, (float)rtH };
    SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);

    Scene currentScene = SCENE_MENU;

    // --- Multiplayer state ---
    NetContext netCtx;
    NetContextInit(&netCtx);
    LobbyState lobbyState;
    LobbyStateInit(&lobbyState);
    ChatState chatState;
    ChatStateInit(&chatState);
    g_chatStatePtr = &chatState;
    g_netChatCallback = OnNetChatReceived;

    // --- Menu state ---
    Map menuMap;
    MapInit(&menuMap);
    CameraController menuCamCtrl;
    CameraControllerInit(&menuCamCtrl);
    menuCamCtrl.distance = 22.0f;
    Camera3D menuCamera = {0};
    CameraControllerUpdate(&menuCamCtrl, &menuCamera, 0.0f);

    Map map;
    MapInit(&map);

    GameState gs;
    GameStateInit(&gs);

    Enemy enemies[MAX_ENEMIES];
    memset(enemies, 0, sizeof(enemies));

    Tower towers[MAX_TOWERS];
    memset(towers, 0, sizeof(towers));

    Projectile projectiles[MAX_PROJECTILES];
    memset(projectiles, 0, sizeof(projectiles));

    CameraController camCtrl;
    CameraControllerInit(&camCtrl);

    Camera3D camera = {0};
    CameraControllerUpdate(&camCtrl, &camera, 0.0f);

    int selectedTowerType = -1;  // -1 = no selection for placement
    int selectedTowerIdx = -1;   // -1 = no tower selected for info
    GamePhase phaseBeforePause = PHASE_PLAYING;
    bool localPaused = false;    // For multiplayer: local UI pause without stopping sim

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        Vector2 mouse = GetMousePosition();

        // Recreate render target on window resize
        if (screenW != cachedScreenW || screenH != cachedScreenH) {
            cachedScreenW = screenW;
            cachedScreenH = screenH;
            UnloadRenderTexture(renderTarget);
            rtW = screenW / PS1_DOWNSCALE;
            rtH = screenH / PS1_DOWNSCALE;
            renderTarget = LoadRenderTexture(rtW, rtH);
            SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);
            resolution[0] = (float)rtW;
            resolution[1] = (float)rtH;
            SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);
        }

        switch (currentScene) {
        case SCENE_MENU: {
            // --- Auto-rotate camera ---
            menuCamCtrl.yaw += 8.0f * dt;
            float yawRad = menuCamCtrl.yaw * DEG2RAD;
            float pitchRad = menuCamCtrl.pitch * DEG2RAD;
            menuCamera.position = (Vector3){
                menuCamCtrl.target.x + menuCamCtrl.distance * cosf(pitchRad) * sinf(yawRad),
                menuCamCtrl.target.y + menuCamCtrl.distance * sinf(pitchRad),
                menuCamCtrl.target.z + menuCamCtrl.distance * cosf(pitchRad) * cosf(yawRad),
            };
            menuCamera.target = menuCamCtrl.target;
            menuCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
            menuCamera.fovy = 45.0f;
            menuCamera.projection = CAMERA_PERSPECTIVE;

            // --- Draw menu (3D to low-res target) ---
            BeginTextureMode(renderTarget);
            ClearBackground((Color){ 30, 30, 35, 255 });
            BeginMode3D(menuCamera);
                DrawSkybox(menuCamera);
                BeginShaderMode(ps1Shader);
                    MapDraw(&menuMap);
                EndShaderMode();
            EndMode3D();
            EndTextureMode();

            // --- Full-res output ---
            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(renderTarget.texture,
                (Rectangle){ 0, 0, (float)rtW, -(float)rtH },
                (Rectangle){ 0, 0, (float)screenW, (float)screenH },
                (Vector2){ 0, 0 }, 0.0f, WHITE);

            // Dark overlay
            DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 120 });

            // Title
            const char *title = "Formal Defense";
            int titleSize = 60;
            int titleW = MeasureText(title, titleSize);
            DrawText(title, (screenW - titleW) / 2, screenH / 2 - 100, titleSize, WHITE);

            // Subtitle
            const char *subtitle = "Tower Defense";
            int subtitleSize = 24;
            int subtitleW = MeasureText(subtitle, subtitleSize);
            DrawText(subtitle, (screenW - subtitleW) / 2, screenH / 2 - 35, subtitleSize, LIGHTGRAY);

            // Play button
            int pbW = 180, pbH = 50;
            int pbX = (screenW - pbW) / 2;
            int pbY = screenH / 2 + 20;
            Rectangle playBtn = { (float)pbX, (float)pbY, (float)pbW, (float)pbH };
            bool playHover = CheckCollisionPointRec(mouse, playBtn);
            Color playBg = playHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
            DrawRectangleRec(playBtn, playBg);
            DrawRectangleLinesEx(playBtn, 2, (Color){ 100, 200, 100, 200 });
            const char *playText = "Play";
            int playTextW = MeasureText(playText, 30);
            DrawText(playText, pbX + (pbW - playTextW) / 2, pbY + 10, 30, WHITE);

            // Multiplayer button
            int mpY = pbY + pbH + 15;
            Rectangle mpBtn = { (float)pbX, (float)mpY, (float)pbW, (float)pbH };
            bool mpHover = CheckCollisionPointRec(mouse, mpBtn);
            Color mpBg = mpHover ? (Color){ 50, 80, 120, 255 } : (Color){ 35, 55, 80, 255 };
            DrawRectangleRec(mpBtn, mpBg);
            DrawRectangleLinesEx(mpBtn, 2, (Color){ 100, 150, 220, 200 });
            const char *mpText = "Multiplayer";
            int mpTextW = MeasureText(mpText, 30);
            DrawText(mpText, pbX + (pbW - mpTextW) / 2, mpY + 10, 30, WHITE);

            // Quit button
            int qbY = mpY + pbH + 15;
            Rectangle quitBtn = { (float)pbX, (float)qbY, (float)pbW, (float)pbH };
            bool quitHover = CheckCollisionPointRec(mouse, quitBtn);
            Color quitBg = quitHover ? (Color){ 140, 50, 50, 255 } : (Color){ 100, 35, 35, 255 };
            DrawRectangleRec(quitBtn, quitBg);
            DrawRectangleLinesEx(quitBtn, 2, (Color){ 200, 100, 100, 200 });
            const char *quitText = "Quit";
            int quitTextW = MeasureText(quitText, 30);
            DrawText(quitText, pbX + (pbW - quitTextW) / 2, qbY + 10, 30, WHITE);

            // Copyright & credit
            DrawText("Made by FormalSnake", 10, screenH - 40, 16, LIGHTGRAY);
            DrawText("(c) 2026 FormalSnake", 10, screenH - 22, 14, GRAY);

            EndDrawing();

            // Quit button click
            if (quitHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                CloseWindow();
                return 0;
            }

            // Multiplayer button click
            if (mpHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                LobbyStateInit(&lobbyState);
                currentScene = SCENE_LOBBY;
            }

            // Play button click
            if (playHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                MapInit(&map);
                GameStateInit(&gs);
                memset(enemies, 0, sizeof(enemies));
                memset(towers, 0, sizeof(towers));
                memset(projectiles, 0, sizeof(projectiles));
                CameraControllerInit(&camCtrl);
                CameraControllerUpdate(&camCtrl, &camera, 0.0f);
                selectedTowerType = -1;
                selectedTowerIdx = -1;
                currentScene = SCENE_GAME;
            }
        } break;

        case SCENE_LOBBY: {
            LobbyUpdate(&lobbyState, &netCtx);

            BeginDrawing();
            ClearBackground((Color){ 20, 22, 28, 255 });
            LobbyDraw(&lobbyState, &netCtx, screenW, screenH);

            // Back button in choose phase returns to menu
            if (lobbyState.phase == LOBBY_CHOOSE && IsKeyPressed(KEY_ESCAPE)) {
                currentScene = SCENE_MENU;
            }

            EndDrawing();

            // Check if game should start
            if (LobbyGameStarted(&lobbyState, &netCtx)) {
                MapInit(&map);
                if (netCtx.mode == NET_MODE_HOST) {
                    GameStateInitMultiplayer(&gs, netCtx.playerCount);
                } else {
                    GameStateInitMultiplayer(&gs, netCtx.playerCount);
                }
                memset(enemies, 0, sizeof(enemies));
                memset(towers, 0, sizeof(towers));
                memset(projectiles, 0, sizeof(projectiles));
                CameraControllerInit(&camCtrl);
                CameraControllerUpdate(&camCtrl, &camera, 0.0f);
                selectedTowerType = -1;
                selectedTowerIdx = -1;
                currentScene = SCENE_GAME;
            }
        } break;

        case SCENE_GAME: {

        // --- UI hit test (bottom bar takes priority) ---
        bool mouseInUI = (mouse.y >= screenH - BOTTOM_BAR_HEIGHT);

        // Also check info panel
        if (selectedTowerIdx >= 0) {
            Rectangle infoRect = { (float)(screenW - INFO_PANEL_W - 10), 40.0f,
                                   (float)INFO_PANEL_W, (float)INFO_PANEL_H };
            if (CheckCollisionPointRec(mouse, infoRect))
                mouseInUI = true;
        }

        // --- Chat input (before other input) ---
        bool chatActive = ChatHandleInput(&chatState, &netCtx);
        ChatUpdate(&chatState, dt);

        // --- ESC: deselect, pause, or resume ---
        if (!chatActive && IsKeyPressed(KEY_ESCAPE)) {
            if (netCtx.mode != NET_MODE_NONE) {
                // Multiplayer: local pause overlay only
                if (localPaused) {
                    localPaused = false;
                } else if (selectedTowerType >= 0 || selectedTowerIdx >= 0) {
                    selectedTowerType = -1;
                    selectedTowerIdx = -1;
                } else {
                    localPaused = true;
                }
            } else {
                // Single-player
                if (gs.phase == PHASE_PAUSED) {
                    gs.phase = phaseBeforePause;
                } else if (selectedTowerType >= 0 || selectedTowerIdx >= 0) {
                    selectedTowerType = -1;
                    selectedTowerIdx = -1;
                } else if (gs.phase != PHASE_OVER) {
                    phaseBeforePause = gs.phase;
                    gs.phase = PHASE_PAUSED;
                }
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            selectedTowerType = -1;
            selectedTowerIdx = -1;
        }

        // --- Mouse ground position (needed for drawing even when paused) ---
        Vector3 mouseGround = {0};
        bool mouseOnGround = GetMouseGroundPos(camera, &mouseGround);
        GridPos mouseGrid = {-1, -1};
        bool canPlace = false;

        if (mouseOnGround) {
            mouseGrid = MapWorldToGrid(mouseGround);
            if (selectedTowerType >= 0)
                canPlace = MapCanPlaceTower(&map, mouseGrid);
        }

        if (gs.phase != PHASE_PAUSED) {

        // --- Camera (only when not in UI or chat) ---
        if (!chatActive && (!mouseInUI || IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D)))
            CameraControllerUpdate(&camCtrl, &camera, dt);

        // --- Bottom bar button clicks ---
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= screenH - BOTTOM_BAR_HEIGHT) {
            for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
                int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
                int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
                Rectangle btnRect = { (float)bx, (float)by, (float)BTN_WIDTH, (float)BTN_HEIGHT };
                if (CheckCollisionPointRec(mouse, btnRect)) {
                    int cost = TOWER_CONFIGS[i][0].cost;
                    int lpi = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
                    if (gs.playerGold[lpi] >= cost) {
                        selectedTowerType = i;
                        selectedTowerIdx = -1;
                    }
                }
            }
        }

        // --- Tower selection keys ---
        if (IsKeyPressed(KEY_ONE))   { selectedTowerType = TOWER_CANNON;     selectedTowerIdx = -1; }
        if (IsKeyPressed(KEY_TWO))   { selectedTowerType = TOWER_MACHINEGUN; selectedTowerIdx = -1; }
        if (IsKeyPressed(KEY_THREE)) { selectedTowerType = TOWER_SNIPER;     selectedTowerIdx = -1; }
        if (IsKeyPressed(KEY_FOUR))  { selectedTowerType = TOWER_SLOW;       selectedTowerIdx = -1; }

        // --- Left click in 3D area ---
        if (!mouseInUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gs.phase != PHASE_OVER) {
            int lpi = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
            if (selectedTowerType >= 0 && canPlace) {
                int cost = TOWER_CONFIGS[selectedTowerType][0].cost;
                if (gs.playerGold[lpi] >= cost) {
                    if (netCtx.mode == NET_MODE_CLIENT) {
                        NetSendPlaceTower(&netCtx, (TowerType)selectedTowerType, mouseGrid);
                    } else {
                        gs.playerGold[lpi] -= cost;
                        gs.gold = gs.playerGold[0];
                        TowerPlace(towers, MAX_TOWERS, (TowerType)selectedTowerType, mouseGrid,
                                  (uint8_t)lpi, &gs);
                        map.tiles[mouseGrid.z][mouseGrid.x] = TILE_TOWER;
                    }
                }
            } else if (selectedTowerType < 0 && mouseOnGround) {
                // Try to select an existing tower
                selectedTowerIdx = -1;
                for (int i = 0; i < MAX_TOWERS; i++) {
                    if (!towers[i].active) continue;
                    if (towers[i].gridPos.x == mouseGrid.x && towers[i].gridPos.z == mouseGrid.z) {
                        selectedTowerIdx = i;
                        break;
                    }
                }
            }
        }

        } // end PHASE_PAUSED guard

        // --- Upgrade selected tower ---
        // (handled in UI draw section via button click check)

        // --- Network polling ---
        if (netCtx.mode != NET_MODE_NONE) {
            NetPoll(&netCtx, &gs, enemies, towers, projectiles, &map);

            // Client: check if host disconnected
            if (netCtx.mode == NET_MODE_CLIENT && !netCtx.serverPeer) {
                NetContextDestroy(&netCtx);
                NetShutdown();
                NetContextInit(&netCtx);
                localPaused = false;
                MapInit(&menuMap);
                menuCamCtrl.yaw = 0.0f;
                currentScene = SCENE_MENU;
                break; // exit SCENE_GAME case
            }
        }

        // --- Update game systems (host and single-player only) ---
        if (netCtx.mode != NET_MODE_CLIENT &&
            gs.phase != PHASE_OVER && gs.phase != PHASE_PAUSED) {
            GameUpdateWave(&gs, enemies, MAX_ENEMIES, &map, dt);
            EnemiesUpdate(enemies, MAX_ENEMIES, &map, &gs, dt);
            TowersUpdate(towers, MAX_TOWERS, enemies, MAX_ENEMIES, projectiles, MAX_PROJECTILES, &gs, dt);
            ProjectilesUpdate(projectiles, MAX_PROJECTILES, enemies, MAX_ENEMIES, &gs, dt);
        }

        // --- Broadcast snapshots (host only) ---
        if (netCtx.mode == NET_MODE_HOST) {
            netCtx.snapshotTimer += dt;
            if (netCtx.snapshotTimer >= NET_SNAPSHOT_RATE) {
                netCtx.snapshotTimer -= NET_SNAPSHOT_RATE;
                NetBroadcastSnapshot(&netCtx, &gs, enemies, towers, projectiles);
            }
        }

        // Deselect tower if it became inactive
        if (selectedTowerIdx >= 0 && !towers[selectedTowerIdx].active)
            selectedTowerIdx = -1;

        // =========================
        // DRAW — 3D to low-res render target
        // =========================
        BeginTextureMode(renderTarget);
        ClearBackground((Color){ 30, 30, 35, 255 });

        BeginMode3D(camera);
            DrawSkybox(camera);

            BeginShaderMode(ps1Shader);
                MapDraw(&map);

                // Blob shadows under towers
                for (int i = 0; i < MAX_TOWERS; i++) {
                    if (!towers[i].active) continue;
                    Vector3 tp = towers[i].worldPos;
                    DrawBlobShadow((Vector3){ tp.x, 0.0f, tp.z }, 0.45f);
                }

                // Blob shadows under enemies
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!enemies[i].active) continue;
                    Vector3 ep = enemies[i].worldPos;
                    DrawBlobShadow((Vector3){ ep.x, 0.0f, ep.z }, enemies[i].radius * 1.2f);
                }

                // Grid hover highlight
                if (mouseOnGround && selectedTowerType >= 0 &&
                    mouseGrid.x >= 0 && mouseGrid.x < MAP_WIDTH &&
                    mouseGrid.z >= 0 && mouseGrid.z < MAP_HEIGHT) {
                    Vector3 ghostPos = MapGridToWorld(mouseGrid);
                    ghostPos.y = 0.35f;
                    Color ghostCol = canPlace ? (Color){ 0, 255, 0, 100 } : (Color){ 255, 0, 0, 100 };
                    DrawCubeV(ghostPos, (Vector3){ 0.7f, 0.7f, 0.7f }, ghostCol);

                    // Range preview
                    if (canPlace) {
                        float range = TOWER_CONFIGS[selectedTowerType][0].range;
                        Vector3 rangeCenter = MapGridToWorld(mouseGrid);
                        DrawRangeCircle(rangeCenter, range, (Color){ 255, 255, 255, 80 });
                    }
                }

                TowersDraw(towers, MAX_TOWERS, gs.playerCount);

                // Range indicator for selected tower
                if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
                    const Tower *st = &towers[selectedTowerIdx];
                    float range = TOWER_CONFIGS[st->type][st->level].range;
                    Vector3 rc = st->worldPos;
                    rc.y = 0.0f;
                    DrawRangeCircle(rc, range, (Color){ 255, 255, 100, 150 });
                }

                EnemiesDraw(enemies, MAX_ENEMIES);
                ProjectilesDraw(projectiles, MAX_PROJECTILES);
            EndShaderMode();
        EndMode3D();
        EndTextureMode();

        // =========================
        // Full-res output
        // =========================
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(renderTarget.texture,
            (Rectangle){ 0, 0, (float)rtW, -(float)rtH },
            (Rectangle){ 0, 0, (float)screenW, (float)screenH },
            (Vector2){ 0, 0 }, 0.0f, WHITE);

        // Enemy health bars at native resolution
        EnemiesDrawHUD(enemies, MAX_ENEMIES, camera);

        // =====================
        // 2D UI
        // =====================

        // --- Top bar ---
        DrawRectangle(0, 0, screenW, 32, (Color){ 0, 0, 0, 180 });
        int localPI = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
        DrawText(TextFormat("Gold: %d", gs.playerGold[localPI]), 10, 7, 20, GOLD);
        DrawText(TextFormat("Lives: %d", gs.lives), 170, 7, 20,
                 gs.lives > 5 ? RED : MAROON);
        DrawText(TextFormat("Wave: %d/%d", gs.currentWave + 1, MAX_WAVES), 330, 7, 20, WHITE);
        DrawFPS(screenW - 90, 7);

        // --- Wave countdown ---
        if (gs.phase == PHASE_WAVE_COUNTDOWN) {
            const char *countText = TextFormat("Next wave in %.1f", gs.waveCountdown);
            int tw = MeasureText(countText, 28);
            DrawText(countText, (screenW - tw) / 2, 80, 28, YELLOW);
        }

        // --- Bottom bar (tower buttons) ---
        DrawRectangle(0, screenH - BOTTOM_BAR_HEIGHT, screenW, BOTTOM_BAR_HEIGHT, (Color){ 20, 20, 25, 220 });
        for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
            int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
            int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
            int cost = TOWER_CONFIGS[i][0].cost;
            bool affordable = gs.playerGold[localPI] >= cost;

            Color btnBg = (selectedTowerType == i) ? (Color){ 80, 120, 80, 255 } :
                          affordable ? (Color){ 50, 50, 60, 255 } : (Color){ 40, 40, 40, 255 };
            Color btnFg = affordable ? WHITE : (Color){ 100, 100, 100, 255 };
            Color costCol = affordable ? GOLD : (Color){ 120, 80, 80, 255 };

            DrawRectangle(bx, by, BTN_WIDTH, BTN_HEIGHT, btnBg);
            DrawRectangleLines(bx, by, BTN_WIDTH, BTN_HEIGHT, (Color){ 100, 100, 100, 200 });

            // Tower color swatch
            DrawRectangle(bx + 4, by + 4, 12, 12, TOWER_CONFIGS[i][0].color);

            DrawText(TOWER_NAMES[i], bx + 20, by + 4, 16, btnFg);
            DrawText(TextFormat("$%d", cost), bx + 20, by + 24, 14, costCol);

            // Keybind hint
            DrawText(TextFormat("[%d]", i + 1), bx + BTN_WIDTH - 28, by + 28, 12, (Color){120,120,120,255});
        }

        // --- Tower info panel ---
        if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
            const Tower *st = &towers[selectedTowerIdx];
            const TowerConfig *cfg = &TOWER_CONFIGS[st->type][st->level];
            int px = screenW - INFO_PANEL_W - 10;
            int py = 40;

            DrawRectangle(px, py, INFO_PANEL_W, INFO_PANEL_H, (Color){ 20, 20, 30, 230 });
            DrawRectangleLines(px, py, INFO_PANEL_W, INFO_PANEL_H, (Color){ 100, 100, 100, 200 });

            DrawText(TextFormat("%s (Lv %d)", TOWER_NAMES[st->type], st->level + 1),
                     px + 8, py + 6, 18, cfg->color);
            DrawText(TextFormat("Damage: %.0f", cfg->damage),   px + 8, py + 30, 14, WHITE);
            DrawText(TextFormat("Range:  %.1f", cfg->range),    px + 8, py + 48, 14, WHITE);
            DrawText(TextFormat("Rate:   %.1f/s", cfg->fireRate), px + 8, py + 66, 14, WHITE);

            if (cfg->aoeRadius > 0.0f)
                DrawText(TextFormat("AoE:    %.1f", cfg->aoeRadius), px + 8, py + 84, 14, ORANGE);
            if (cfg->slowFactor < 1.0f)
                DrawText(TextFormat("Slow:   %.0f%%", (1.0f - cfg->slowFactor) * 100.0f), px + 8, py + 84, 14, PURPLE);

            // Upgrade button
            if (st->level < TOWER_MAX_LEVEL - 1) {
                int upgCost = TOWER_CONFIGS[st->type][st->level + 1].cost;
                bool canUpg = gs.playerGold[localPI] >= upgCost;
                int ubx = px + 8, uby = py + INFO_PANEL_H - 30;
                int ubw = INFO_PANEL_W - 16, ubh = 24;

                Color ubCol = canUpg ? (Color){ 60, 100, 60, 255 } : (Color){ 50, 50, 50, 255 };
                DrawRectangle(ubx, uby, ubw, ubh, ubCol);
                DrawRectangleLines(ubx, uby, ubw, ubh, (Color){ 120, 120, 120, 200 });

                const char *upgText = TextFormat("Upgrade $%d", upgCost);
                Color upgTxtCol = canUpg ? GOLD : (Color){ 100, 100, 100, 255 };
                DrawText(upgText, ubx + 8, uby + 4, 16, upgTxtCol);

                // Check upgrade click
                if (canUpg && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Rectangle upgRect = { (float)ubx, (float)uby, (float)ubw, (float)ubh };
                    if (CheckCollisionPointRec(mouse, upgRect)) {
                        if (netCtx.mode == NET_MODE_CLIENT) {
                            NetSendUpgradeTower(&netCtx, towers[selectedTowerIdx].id);
                        } else {
                            gs.playerGold[localPI] -= upgCost;
                            gs.gold = gs.playerGold[0];
                            towers[selectedTowerIdx].level++;
                        }
                    }
                }
            } else {
                DrawText("MAX LEVEL", px + 8, py + INFO_PANEL_H - 26, 16, (Color){ 180, 180, 50, 255 });
            }
        }

        // --- Placement hint ---
        if (selectedTowerType >= 0) {
            DrawText(TextFormat("Placing: %s  (Right-click to cancel)",
                     TOWER_NAMES[selectedTowerType]), 10, screenH - BOTTOM_BAR_HEIGHT - 24, 16, YELLOW);
        }

        // --- Multiplayer Player List & Gift UI ---
        if (netCtx.mode != NET_MODE_NONE && gs.playerCount > 1) {
            int plX = 10, plY = 40;
            DrawRectangle(plX, plY, 180, 30 * gs.playerCount + 5, (Color){ 0, 0, 0, 150 });
            for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                if (!netCtx.playerConnected[i]) continue;
                int py = plY + 3 + i * 30;
                Color pCol = PLAYER_COLORS[i];
                bool isLocal = (i == localPI);
                DrawText(TextFormat("P%d: %s %s$%d", i + 1,
                        netCtx.playerNames[i],
                        isLocal ? "(You)" : "",
                        gs.playerGold[i]),
                        plX + 5, py, 14, pCol);

                // Gift button (only for other players)
                if (!isLocal && i != localPI) {
                    int gbX = plX + 140;
                    Rectangle giftBtn = { (float)gbX, (float)py, 35.0f, 18.0f };
                    bool giftHover = CheckCollisionPointRec(mouse, giftBtn);
                    DrawRectangleRec(giftBtn, giftHover ? (Color){60,80,60,255} : (Color){40,50,40,255});
                    DrawText("$25", gbX + 3, py + 2, 12, GOLD);
                    if (giftHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gs.playerGold[localPI] >= 25) {
                        if (netCtx.mode == NET_MODE_CLIENT) {
                            NetSendGiftGold(&netCtx, (uint8_t)i, 25);
                        } else {
                            gs.playerGold[localPI] -= 25;
                            gs.playerGold[i] += 25;
                        }
                    }
                }
            }

            // Tower owner name
            if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
                uint8_t owner = towers[selectedTowerIdx].ownerPlayer;
                DrawText(TextFormat("Owner: %s", netCtx.playerNames[owner]),
                        screenW - INFO_PANEL_W - 2, 28, 14, PLAYER_COLORS[owner]);
            }
        }

        // --- Pause Menu ---
        if (gs.phase == PHASE_PAUSED || localPaused) {
            DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });

            const char *pauseTitle = "PAUSED";
            int pauseTitleW = MeasureText(pauseTitle, 60);
            DrawText(pauseTitle, (screenW - pauseTitleW) / 2, screenH / 2 - 100, 60, WHITE);

            int pBtnW = 200, pBtnH = 45;
            int pBtnX = (screenW - pBtnW) / 2;

            // Resume button
            int resumeY = screenH / 2 - 20;
            Rectangle resumeBtn = { (float)pBtnX, (float)resumeY, (float)pBtnW, (float)pBtnH };
            bool resumeHover = CheckCollisionPointRec(mouse, resumeBtn);
            Color resumeBg = resumeHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
            DrawRectangleRec(resumeBtn, resumeBg);
            DrawRectangleLinesEx(resumeBtn, 2, (Color){ 100, 200, 100, 200 });
            const char *resumeText = "Resume";
            int resumeTextW = MeasureText(resumeText, 24);
            DrawText(resumeText, pBtnX + (pBtnW - resumeTextW) / 2, resumeY + 11, 24, WHITE);

            // Main Menu button
            int pmMenuY = resumeY + pBtnH + 12;
            Rectangle pmMenuBtn = { (float)pBtnX, (float)pmMenuY, (float)pBtnW, (float)pBtnH };
            bool pmMenuHover = CheckCollisionPointRec(mouse, pmMenuBtn);
            Color pmMenuBg = pmMenuHover ? (Color){ 80, 80, 100, 255 } : (Color){ 50, 50, 65, 255 };
            DrawRectangleRec(pmMenuBtn, pmMenuBg);
            DrawRectangleLinesEx(pmMenuBtn, 2, (Color){ 120, 120, 160, 200 });
            const char *pmMenuText = "Main Menu";
            int pmMenuTextW = MeasureText(pmMenuText, 24);
            DrawText(pmMenuText, pBtnX + (pBtnW - pmMenuTextW) / 2, pmMenuY + 11, 24, WHITE);

            // Quit button
            int pQuitY = pmMenuY + pBtnH + 12;
            Rectangle pQuitBtn = { (float)pBtnX, (float)pQuitY, (float)pBtnW, (float)pBtnH };
            bool pQuitHover = CheckCollisionPointRec(mouse, pQuitBtn);
            Color pQuitBg = pQuitHover ? (Color){ 140, 50, 50, 255 } : (Color){ 100, 35, 35, 255 };
            DrawRectangleRec(pQuitBtn, pQuitBg);
            DrawRectangleLinesEx(pQuitBtn, 2, (Color){ 200, 100, 100, 200 });
            const char *pQuitText = "Quit";
            int pQuitTextW = MeasureText(pQuitText, 24);
            DrawText(pQuitText, pBtnX + (pBtnW - pQuitTextW) / 2, pQuitY + 11, 24, WHITE);
        }

        // --- Chat overlay ---
        if (netCtx.mode != NET_MODE_NONE) {
            ChatDraw(&chatState, screenW, screenH);
        }

        // --- Game Over Screen ---
        if (gs.phase == PHASE_OVER) {
            DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });
            bool won = gs.currentWave >= MAX_WAVES;
            const char *msg = won ? "VICTORY!" : "GAME OVER";
            Color msgCol = won ? GOLD : RED;
            int msgW = MeasureText(msg, 60);
            DrawText(msg, (screenW - msgW) / 2, screenH / 2 - 50, 60, msgCol);

            const char *sub = won ?
                TextFormat("All %d waves cleared!", MAX_WAVES) :
                TextFormat("Survived to wave %d/%d", gs.currentWave + 1, MAX_WAVES);
            int subW = MeasureText(sub, 24);
            DrawText(sub, (screenW - subW) / 2, screenH / 2 + 20, 24, WHITE);

            const char *hint = "Press R to restart | ESC for Main Menu";
            int hintW = MeasureText(hint, 18);
            DrawText(hint, (screenW - hintW) / 2, screenH / 2 + 60, 18, LIGHTGRAY);

            // Main Menu button
            int mbW = 180, mbH = 40;
            int mbX = (screenW - mbW) / 2;
            int mbY = screenH / 2 + 95;
            Rectangle menuBtn = { (float)mbX, (float)mbY, (float)mbW, (float)mbH };
            bool menuHover = CheckCollisionPointRec(mouse, menuBtn);
            Color menuBg = menuHover ? (Color){ 80, 80, 100, 255 } : (Color){ 50, 50, 65, 255 };
            DrawRectangleRec(menuBtn, menuBg);
            DrawRectangleLinesEx(menuBtn, 2, (Color){ 120, 120, 160, 200 });
            const char *menuText = "Main Menu";
            int menuTextW = MeasureText(menuText, 22);
            DrawText(menuText, mbX + (mbW - menuTextW) / 2, mbY + 9, 22, WHITE);
        }

        EndDrawing();

        // --- Pause menu button clicks ---
        if (gs.phase == PHASE_PAUSED || localPaused) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int pBtnW = 200, pBtnH = 45;
                int pBtnX = (screenW - pBtnW) / 2;
                int resumeY = screenH / 2 - 20;
                int pmMenuY = resumeY + pBtnH + 12;
                int pQuitY = pmMenuY + pBtnH + 12;

                Rectangle resumeBtn = { (float)pBtnX, (float)resumeY, (float)pBtnW, (float)pBtnH };
                Rectangle pmMenuBtn = { (float)pBtnX, (float)pmMenuY, (float)pBtnW, (float)pBtnH };
                Rectangle pQuitBtn = { (float)pBtnX, (float)pQuitY, (float)pBtnW, (float)pBtnH };

                if (CheckCollisionPointRec(mouse, resumeBtn)) {
                    if (localPaused) localPaused = false;
                    else gs.phase = phaseBeforePause;
                } else if (CheckCollisionPointRec(mouse, pmMenuBtn)) {
                    localPaused = false;
                    if (netCtx.mode != NET_MODE_NONE) {
                        NetContextDestroy(&netCtx);
                        NetShutdown();
                        NetContextInit(&netCtx);
                    }
                    MapInit(&menuMap);
                    menuCamCtrl.yaw = 0.0f;
                    currentScene = SCENE_MENU;
                } else if (CheckCollisionPointRec(mouse, pQuitBtn)) {
                    if (netCtx.mode != NET_MODE_NONE) {
                        NetContextDestroy(&netCtx);
                        NetShutdown();
                    }
                    CloseWindow();
                    return 0;
                }
            }
        }

        // --- Restart ---
        if (gs.phase == PHASE_OVER && IsKeyPressed(KEY_R)) {
            MapInit(&map);
            GameStateInit(&gs);
            memset(enemies, 0, sizeof(enemies));
            memset(towers, 0, sizeof(towers));
            memset(projectiles, 0, sizeof(projectiles));
            selectedTowerType = -1;
            selectedTowerIdx = -1;
        }

        // --- Back to menu ---
        if (gs.phase == PHASE_OVER) {
            bool goMenu = IsKeyPressed(KEY_ESCAPE);
            Rectangle menuBtn = { (float)((screenW - 180) / 2), (float)(screenH / 2 + 95), 180.0f, 40.0f };
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, menuBtn))
                goMenu = true;
            if (goMenu) {
                if (netCtx.mode != NET_MODE_NONE) {
                    NetContextDestroy(&netCtx);
                    NetShutdown();
                    NetContextInit(&netCtx);
                }
                MapInit(&menuMap);
                menuCamCtrl.yaw = 0.0f;
                currentScene = SCENE_MENU;
            }
        }

        } break; // end SCENE_GAME
        } // end switch
    }

    if (netCtx.mode != NET_MODE_NONE) {
        NetContextDestroy(&netCtx);
        NetShutdown();
    }
    UnloadRenderTexture(renderTarget);
    UnloadShader(ps1Shader);
    CloseWindow();
    return 0;
}
