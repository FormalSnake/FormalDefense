#include "fd_gfx.h"
#include "fd_input.h"
#include "fd_app.h"
#include "map.h"
#include "entity.h"
#include "game.h"
#include "net.h"
#include "lobby.h"
#include "chat.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- PS1 Graphics Constants ---

#define PS1_DOWNSCALE 3
#define PS1_JITTER_STRENGTH 1.0f
#define PS1_COLOR_BANDS 24.0f
#define BLOB_SHADOW_SEGMENTS 12

// --- Scene ---

typedef enum { SCENE_MENU, SCENE_MAP_SELECT, SCENE_LOBBY, SCENE_GAME } Scene;

// Track whether map select was opened for multiplayer host flow
static bool mapSelectForMultiplayer = false;

// --- Camera Controller ---

typedef struct {
    Vector3 target;
    float distance;
    float yaw;
    float pitch;
    float panSpeed;
    float rotSpeed;
    float zoomSpeed;
    // Computed camera state
    Vector3 position;
    Vector3 up;
    float fovy;
    FdMat4 view;
    FdMat4 proj;
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
    cc->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cc->fovy = 45.0f;
}

static void CameraControllerUpdate(CameraController *cc, float dt)
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
    cc->position = (Vector3){
        cc->target.x + cc->distance * cosf(pitchRad) * sinf(yawRad),
        cc->target.y + cc->distance * sinf(pitchRad),
        cc->target.z + cc->distance * cosf(pitchRad) * cosf(yawRad),
    };
}

static void CameraControllerComputeMatrices(CameraController *cc, int screenW, int screenH)
{
    cc->view = MatrixLookAt(cc->position, cc->target, cc->up);
    float aspect = (screenH > 0) ? (float)screenW / (float)screenH : 1.0f;
    cc->proj = MatrixPerspective(cc->fovy, aspect, 0.1f, 1000.0f);
}

// --- Mouse Ray → Ground Plane ---

static bool GetMouseGroundPos(CameraController *cc, const Map *map, Vector3 *outPos,
                              int screenW, int screenH)
{
    Ray ray = FdScreenToWorldRay(GetMousePosition(), cc->view, cc->proj, screenW, screenH);
    if (fabsf(ray.direction.y) < 0.001f) return false;

    float bestT = 1e9f;
    bool found = false;

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            float elevY = map->elevation[z][x] * ELEVATION_HEIGHT;
            float t = (elevY - ray.position.y) / ray.direction.y;
            if (t < 0.0f || t >= bestT) continue;

            float hx = ray.position.x + ray.direction.x * t;
            float hz = ray.position.z + ray.direction.z * t;

            float tileX0 = x * TILE_SIZE;
            float tileZ0 = z * TILE_SIZE;
            if (hx >= tileX0 && hx < tileX0 + TILE_SIZE &&
                hz >= tileZ0 && hz < tileZ0 + TILE_SIZE) {
                bestT = t;
                outPos->x = hx;
                outPos->y = elevY;
                outPos->z = hz;
                found = true;
            }
        }
    }

    if (!found) {
        float t = -ray.position.y / ray.direction.y;
        if (t >= 0.0f) {
            outPos->x = ray.position.x + ray.direction.x * t;
            outPos->y = 0.0f;
            outPos->z = ray.position.z + ray.direction.z * t;
            found = true;
        }
    }

    return found;
}

// --- Draw Range Circle ---

static void DrawRangeCircle(Vector3 center, float radius, Color color)
{
    int segments = 48;
    float y = center.y + 0.02f;
    for (int i = 0; i < segments; i++) {
        float a1 = (float)i / segments * 2.0f * PI;
        float a2 = (float)(i + 1) / segments * 2.0f * PI;
        Vector3 p1 = { center.x + cosf(a1) * radius, y, center.z + sinf(a1) * radius };
        Vector3 p2 = { center.x + cosf(a2) * radius, y, center.z + sinf(a2) * radius };
        FdDrawLine3D(p1, p2, color);
    }
}

// --- Skybox (pre-baked mesh) ---

static FdMesh *skyboxMesh = NULL;

static void BuildSkyboxMesh(void)
{
    int slices = 16;
    int stacks = 8;
    float radius = 500.0f;

    Color topColor = { 25, 50, 120, 255 };
    Color botColor = { 135, 190, 235, 255 };

    int triCount = slices * stacks * 2;
    int vertCount = triCount * 3;
    float *verts = malloc(vertCount * 3 * sizeof(float));
    unsigned char *cols = malloc(vertCount * 4 * sizeof(unsigned char));
    int vi = 0;

    for (int i = 0; i < stacks; i++) {
        float phi0 = PI * (float)i / stacks;
        float phi1 = PI * (float)(i + 1) / stacks;
        float t0 = 1.0f - (float)i / stacks;
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

            #define SKY_VERT(px,py,pz,cr,cg,cb) do { \
                verts[vi*3]=px; verts[vi*3+1]=py; verts[vi*3+2]=pz; \
                cols[vi*4]=cr; cols[vi*4+1]=cg; cols[vi*4+2]=cb; cols[vi*4+3]=255; \
                vi++; \
            } while(0)

            SKY_VERT(x00,y00,z00, r0,g0,b0);
            SKY_VERT(x10,y10,z10, r1,g1,b1);
            SKY_VERT(x11,y11,z11, r1,g1,b1);

            SKY_VERT(x00,y00,z00, r0,g0,b0);
            SKY_VERT(x11,y11,z11, r1,g1,b1);
            SKY_VERT(x01,y01,z01, r0,g0,b0);

            #undef SKY_VERT
        }
    }

    skyboxMesh = FdMeshCreate(verts, cols, NULL, vertCount);
    free(verts);
    free(cols);
}

static void DrawSkybox(Vector3 camPos)
{
    if (!skyboxMesh) return;

    FdDisableBackfaceCulling();
    FdDisableDepthWrite();

    FdMat4 transform = MatrixTranslate(camPos.x, camPos.y, camPos.z);
    FdDrawMesh(skyboxMesh, transform, false);

    FdEnableBackfaceCulling();
    FdEnableDepthWrite();
}

// --- Blob Shadow (batched) ---

static float blobShadowCos[BLOB_SHADOW_SEGMENTS + 1];
static float blobShadowSin[BLOB_SHADOW_SEGMENTS + 1];
static bool blobShadowTableReady = false;

static void InitBlobShadowTable(void)
{
    for (int i = 0; i <= BLOB_SHADOW_SEGMENTS; i++) {
        float a = (float)i / BLOB_SHADOW_SEGMENTS * 2.0f * PI;
        blobShadowCos[i] = cosf(a);
        blobShadowSin[i] = sinf(a);
    }
    blobShadowTableReady = true;
}

typedef struct {
    float x, z;
    float radius;
    float elevY;
} BlobShadowEntry;

#define MAX_BLOB_SHADOWS (MAX_TOWERS + MAX_ENEMIES)

static void DrawBlobShadowsBatched(const BlobShadowEntry *entries, int count)
{
    if (count <= 0) return;
    FdBeginTriangles();
    FdTriColor4ub(0, 0, 0, 80);
    for (int e = 0; e < count; e++) {
        float cx = entries[e].x;
        float cz = entries[e].z;
        float r = entries[e].radius;
        float y = entries[e].elevY + 0.01f;
        for (int i = 0; i < BLOB_SHADOW_SEGMENTS; i++) {
            FdTriVertex3f(cx, y, cz);
            FdTriVertex3f(cx + blobShadowCos[i] * r, y, cz + blobShadowSin[i] * r);
            FdTriVertex3f(cx + blobShadowCos[i+1] * r, y, cz + blobShadowSin[i+1] * r);
        }
    }
    FdEndTriangles();
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

// --- Game State (file-level statics for callback-based entry) ---

static Scene currentScene;
static FdRenderTarget *renderTarget;
static int rtW, rtH;
static int cachedScreenW, cachedScreenH;

static MapMesh menuMapMesh;
static MapMesh gameMapMesh;
static int lastTowerCount;

static NetContext netCtx;
static LobbyState lobbyState;
static ChatState chatState;

static MapRegistry mapRegistry;
static int selectedMapIdx;

static Map menuMap;
static CameraController menuCamCtrl;

static Map map;
static GameState gs;

static Enemy enemies[MAX_ENEMIES];
static Tower towers[MAX_TOWERS];
static Projectile projectiles[MAX_PROJECTILES];

static CameraController camCtrl;

static int selectedTowerType;
static int selectedTowerIdx;
static GamePhase phaseBeforePause;
static bool localPaused;

// --- GameInit ---

void GameInit(void)
{
    // PS1 shader params
    FdPS1ShaderSetParams(PS1_JITTER_STRENGTH, PS1_COLOR_BANDS,
        (Vector3){ 0.4f, -0.7f, 0.3f },
        (Vector3){ 0.7f, 0.7f, 0.65f },
        (Vector3){ 0.3f, 0.3f, 0.35f });

    cachedScreenW = FdScreenWidth();
    cachedScreenH = FdScreenHeight();
    rtW = cachedScreenW / PS1_DOWNSCALE;
    rtH = cachedScreenH / PS1_DOWNSCALE;
    renderTarget = FdRenderTargetCreate(rtW, rtH);
    FdPS1ShaderSetResolution((float)rtW, (float)rtH);

    currentScene = SCENE_MENU;

    // Pre-baked meshes
    BuildSkyboxMesh();
    InitBlobShadowTable();

    memset(&menuMapMesh, 0, sizeof(menuMapMesh));
    memset(&gameMapMesh, 0, sizeof(gameMapMesh));
    lastTowerCount = 0;

    // Multiplayer state
    NetContextInit(&netCtx);
    LobbyStateInit(&lobbyState);
    ChatStateInit(&chatState);
    g_chatStatePtr = &chatState;
    g_netChatCallback = OnNetChatReceived;

    // Map registry
    MapRegistryScan(&mapRegistry, "maps");
    selectedMapIdx = 0;

    // Menu state
    if (mapRegistry.count > 0) {
        int menuMapIdx = GetRandomValue(0, mapRegistry.count - 1);
        if (!MapLoad(&menuMap, mapRegistry.paths[menuMapIdx]))
            MapInit(&menuMap);
    } else {
        MapInit(&menuMap);
    }
    MapBuildMesh(&menuMapMesh, &menuMap);

    CameraControllerInit(&menuCamCtrl);
    menuCamCtrl.distance = 22.0f;
    CameraControllerUpdate(&menuCamCtrl, 0.0f);
    CameraControllerComputeMatrices(&menuCamCtrl, cachedScreenW, cachedScreenH);

    MapInit(&map);
    MapBuildMesh(&gameMapMesh, &map);

    GameStateInit(&gs);
    memset(enemies, 0, sizeof(enemies));
    memset(towers, 0, sizeof(towers));
    memset(projectiles, 0, sizeof(projectiles));

    CameraControllerInit(&camCtrl);
    CameraControllerUpdate(&camCtrl, 0.0f);
    CameraControllerComputeMatrices(&camCtrl, cachedScreenW, cachedScreenH);

    selectedTowerType = -1;
    selectedTowerIdx = -1;
    phaseBeforePause = PHASE_PLAYING;
    localPaused = false;
}

// --- GameFrame ---

void GameFrame(void)
{
    float dt = FdFrameTime();
    int screenW = FdScreenWidth();
    int screenH = FdScreenHeight();
    Vector2 mouse = GetMousePosition();

    if (IsKeyPressed(KEY_F11)) FdToggleFullscreen();

    // Recreate render target on window resize
    if (screenW != cachedScreenW || screenH != cachedScreenH) {
        cachedScreenW = screenW;
        cachedScreenH = screenH;
        FdRenderTargetDestroy(renderTarget);
        rtW = screenW / PS1_DOWNSCALE;
        rtH = screenH / PS1_DOWNSCALE;
        renderTarget = FdRenderTargetCreate(rtW, rtH);
        FdPS1ShaderSetResolution((float)rtW, (float)rtH);
    }

    switch (currentScene) {
    case SCENE_MENU: {
        // --- Auto-rotate camera ---
        menuCamCtrl.yaw += 8.0f * dt;
        float yawRad = menuCamCtrl.yaw * DEG2RAD;
        float pitchRad = menuCamCtrl.pitch * DEG2RAD;
        menuCamCtrl.position = (Vector3){
            menuCamCtrl.target.x + menuCamCtrl.distance * cosf(pitchRad) * sinf(yawRad),
            menuCamCtrl.target.y + menuCamCtrl.distance * sinf(pitchRad),
            menuCamCtrl.target.z + menuCamCtrl.distance * cosf(pitchRad) * cosf(yawRad),
        };
        CameraControllerComputeMatrices(&menuCamCtrl, rtW, rtH);

        // --- Draw menu (3D to low-res target) ---
        FdRenderTargetBegin(renderTarget, (Color){ 30, 30, 35, 255 });
        FdBegin3D(menuCamCtrl.view, menuCamCtrl.proj);
            DrawSkybox(menuCamCtrl.position);
            MapDrawMesh(&menuMapMesh);
        FdEnd3D();
        FdRenderTargetEnd();

        // --- Full-res output ---
        FdBeginFrame(BLACK);
        FdRenderTargetBlit(renderTarget, screenW, screenH);

        // Dark overlay
        FdDrawRect(0, 0, screenW, screenH, (Color){ 0, 0, 0, 120 });

        // Title
        const char *title = "Formal Defense";
        int titleSize = 60;
        int titleW = FdMeasureText(title, titleSize);
        FdDrawText(title, (screenW - titleW) / 2, screenH / 2 - 100, titleSize, WHITE);

        // Subtitle
        const char *subtitle = "Tower Defense";
        int subtitleSize = 24;
        int subtitleW = FdMeasureText(subtitle, subtitleSize);
        FdDrawText(subtitle, (screenW - subtitleW) / 2, screenH / 2 - 35, subtitleSize, LIGHTGRAY);

        // Play button
        int pbW = 180, pbH = 50;
        int pbX = (screenW - pbW) / 2;
        int pbY = screenH / 2 + 20;
        FdRect playBtn = { (float)pbX, (float)pbY, (float)pbW, (float)pbH };
        bool playHover = FdPointInRect(mouse, playBtn);
        Color playBg = playHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
        FdDrawRect(pbX, pbY, pbW, pbH, playBg);
        FdDrawRectLinesEx(pbX, pbY, pbW, pbH, 2, (Color){ 100, 200, 100, 200 });
        const char *playText = "Play";
        int playTextW = FdMeasureText(playText, 30);
        FdDrawText(playText, pbX + (pbW - playTextW) / 2, pbY + 10, 30, WHITE);

        // Multiplayer button
        int mpY = pbY + pbH + 15;
        FdRect mpBtn = { (float)pbX, (float)mpY, (float)pbW, (float)pbH };
        bool mpHover = FdPointInRect(mouse, mpBtn);
        Color mpBg = mpHover ? (Color){ 50, 80, 120, 255 } : (Color){ 35, 55, 80, 255 };
        FdDrawRect(pbX, mpY, pbW, pbH, mpBg);
        FdDrawRectLinesEx(pbX, mpY, pbW, pbH, 2, (Color){ 100, 150, 220, 200 });
        const char *mpText = "Multiplayer";
        int mpTextW = FdMeasureText(mpText, 30);
        FdDrawText(mpText, pbX + (pbW - mpTextW) / 2, mpY + 10, 30, WHITE);

        // Quit button
        int qbY = mpY + pbH + 15;
        FdRect quitBtn = { (float)pbX, (float)qbY, (float)pbW, (float)pbH };
        bool quitHover = FdPointInRect(mouse, quitBtn);
        Color quitBg = quitHover ? (Color){ 140, 50, 50, 255 } : (Color){ 100, 35, 35, 255 };
        FdDrawRect(pbX, qbY, pbW, pbH, quitBg);
        FdDrawRectLinesEx(pbX, qbY, pbW, pbH, 2, (Color){ 200, 100, 100, 200 });
        const char *quitText = "Quit";
        int quitTextW = FdMeasureText(quitText, 30);
        FdDrawText(quitText, pbX + (pbW - quitTextW) / 2, qbY + 10, 30, WHITE);

        // Copyright & credit
        FdDrawText("Made by FormalSnake", 10, screenH - 40, 16, LIGHTGRAY);
        FdDrawText("(c) 2026 FormalSnake", 10, screenH - 22, 14, GRAY);

        FdEndFrame();

        // Quit button click
        if (quitHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            FdQuitApp();
            return;
        }

        // Multiplayer button click
        if (mpHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            LobbyStateInit(&lobbyState);
            currentScene = SCENE_LOBBY;
        }

        // Play button click
        if (playHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            mapSelectForMultiplayer = false;
            MapRegistryScan(&mapRegistry, "maps");
            selectedMapIdx = 0;
            currentScene = SCENE_MAP_SELECT;
        }
    } break;

    case SCENE_MAP_SELECT: {
        FdBeginFrame((Color){ 20, 22, 28, 255 });

        const char *msTitle = mapSelectForMultiplayer ? "Select Map (Host)" : "Select Map";
        int msTitleW = FdMeasureText(msTitle, 36);
        FdDrawText(msTitle, (screenW - msTitleW) / 2, 50, 36, WHITE);

        if (mapRegistry.count == 0) {
            FdDrawText("No maps found in maps/ directory", screenW / 2 - 150, 120, 18, LIGHTGRAY);
        }

        for (int i = 0; i < mapRegistry.count; i++) {
            int my = 110 + i * 50;
            bool selected = (selectedMapIdx == i);
            FdRect itemRect = { (float)(screenW / 2 - 150), (float)my, 300.0f, 42.0f };
            bool hover = FdPointInRect(mouse, itemRect);

            Color bg = selected ? (Color){ 50, 80, 110, 255 } :
                       hover    ? (Color){ 40, 55, 75, 255 }  :
                                  (Color){ 30, 35, 45, 200 };
            FdDrawRect(screenW / 2 - 150, my, 300, 42, bg);
            FdDrawRectLines(screenW / 2 - 150, my, 300, 42, (Color){ 80, 100, 120, 200 });
            FdDrawText(mapRegistry.names[i], screenW / 2 - 138, my + 12, 20,
                     selected ? GOLD : WHITE);

            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                selectedMapIdx = i;
        }

        int btnBaseY = 110 + (mapRegistry.count > 0 ? mapRegistry.count : 1) * 50 + 20;

        // Start / Select button
        int sbW = 200, sbH = 45;
        int sbX = (screenW - sbW) / 2;
        FdRect startBtn = { (float)sbX, (float)btnBaseY, (float)sbW, (float)sbH };
        bool startHover = FdPointInRect(mouse, startBtn);
        Color startBg = startHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
        bool canStart = mapRegistry.count > 0;
        if (!canStart) startBg = (Color){ 40, 40, 40, 255 };
        FdDrawRect(sbX, btnBaseY, sbW, sbH, startBg);
        FdDrawRectLinesEx(sbX, btnBaseY, sbW, sbH, 2, (Color){ 100, 200, 100, 200 });
        const char *startText = mapSelectForMultiplayer ? "Host with Map" : "Play";
        int startTextW = FdMeasureText(startText, 24);
        FdDrawText(startText, sbX + (sbW - startTextW) / 2, btnBaseY + 11, 24,
                 canStart ? WHITE : DARKGRAY);

        // Back button
        int bbY = btnBaseY + sbH + 12;
        FdRect backBtn = { (float)sbX, (float)bbY, (float)sbW, 40.0f };
        bool backHover = FdPointInRect(mouse, backBtn);
        Color backBg = backHover ? (Color){ 80, 50, 50, 255 } : (Color){ 55, 35, 35, 255 };
        FdDrawRect(sbX, bbY, sbW, 40, backBg);
        FdDrawRectLinesEx(sbX, bbY, sbW, 40, 2, (Color){ 180, 100, 100, 200 });
        const char *backText = "Back";
        int backTextW = FdMeasureText(backText, 22);
        FdDrawText(backText, sbX + (sbW - backTextW) / 2, bbY + 9, 22, WHITE);

        // Handle start click
        if (canStart && startHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mapSelectForMultiplayer) {
                if (lobbyState.usernameLen > 0) {
                    NetInit();
                    if (NetHostCreate(&netCtx, lobbyState.username)) {
                        strncpy(netCtx.selectedMap, mapRegistry.names[selectedMapIdx], MAX_MAP_NAME - 1);
                        strncpy(netCtx.selectedMapPath, mapRegistry.paths[selectedMapIdx], 255);
                        NetDiscoveryStart(&netCtx);
                        lobbyState.phase = LOBBY_HOST_WAIT;
                        currentScene = SCENE_LOBBY;
                    }
                }
            } else {
                if (!MapLoad(&map, mapRegistry.paths[selectedMapIdx]))
                    MapInit(&map);
                MapBuildMesh(&gameMapMesh, &map);
                GameStateInit(&gs);
                memset(enemies, 0, sizeof(enemies));
                memset(towers, 0, sizeof(towers));
                memset(projectiles, 0, sizeof(projectiles));
                CameraControllerInit(&camCtrl);
                CameraControllerUpdate(&camCtrl, 0.0f);
                CameraControllerComputeMatrices(&camCtrl, screenW, screenH);
                selectedTowerType = -1;
                selectedTowerIdx = -1;
                currentScene = SCENE_GAME;
            }
        }

        // Handle back
        if ((backHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ESCAPE)) {
            currentScene = mapSelectForMultiplayer ? SCENE_LOBBY : SCENE_MENU;
        }

        FdEndFrame();
    } break;

    case SCENE_LOBBY: {
        LobbyUpdate(&lobbyState, &netCtx);

        FdBeginFrame((Color){ 20, 22, 28, 255 });
        LobbyDraw(&lobbyState, &netCtx, screenW, screenH);

        // Host wants to pick a map
        if (lobbyState.hostRequested) {
            lobbyState.hostRequested = false;
            mapSelectForMultiplayer = true;
            MapRegistryScan(&mapRegistry, "maps");
            selectedMapIdx = 0;
            currentScene = SCENE_MAP_SELECT;
        }

        // Back button or ESC returns to menu
        if (LobbyBackPressed(&lobbyState) || (lobbyState.phase == LOBBY_CHOOSE && IsKeyPressed(KEY_ESCAPE))) {
            currentScene = SCENE_MENU;
        }

        FdEndFrame();

        // Check if game should start
        if (LobbyGameStarted(&lobbyState, &netCtx)) {
            if (netCtx.mode == NET_MODE_HOST) {
                if (!MapLoad(&map, netCtx.selectedMapPath))
                    MapInit(&map);
                NetSendMapData(&netCtx, &map);
                GameStateInitMultiplayer(&gs, netCtx.playerCount);
            } else {
                char localPath[256];
                snprintf(localPath, sizeof(localPath), "maps/%s.fdmap", netCtx.selectedMap);
                if (!MapLoad(&map, localPath))
                    MapInit(&map);
                GameStateInitMultiplayer(&gs, netCtx.playerCount);
            }
            MapBuildMesh(&gameMapMesh, &map);
            memset(enemies, 0, sizeof(enemies));
            memset(towers, 0, sizeof(towers));
            memset(projectiles, 0, sizeof(projectiles));
            CameraControllerInit(&camCtrl);
            CameraControllerUpdate(&camCtrl, 0.0f);
            CameraControllerComputeMatrices(&camCtrl, screenW, screenH);
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
        FdRect infoRect = { (float)(screenW - INFO_PANEL_W - 10), 40.0f,
                               (float)INFO_PANEL_W, (float)INFO_PANEL_H };
        if (FdPointInRect(mouse, infoRect))
            mouseInUI = true;
    }

    // --- Chat input (before other input) ---
    bool chatActive = ChatHandleInput(&chatState, &netCtx);
    ChatUpdate(&chatState, dt);

    // --- ESC: deselect, pause, or resume ---
    if (!chatActive && IsKeyPressed(KEY_ESCAPE)) {
        if (netCtx.mode != NET_MODE_NONE) {
            if (localPaused) {
                localPaused = false;
            } else if (selectedTowerType >= 0 || selectedTowerIdx >= 0) {
                selectedTowerType = -1;
                selectedTowerIdx = -1;
            } else {
                localPaused = true;
            }
        } else {
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

    // --- Mouse ground position ---
    Vector3 mouseGround = {0};
    bool mouseOnGround = GetMouseGroundPos(&camCtrl, &map, &mouseGround, rtW, rtH);
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
        CameraControllerUpdate(&camCtrl, dt);

    // --- Bottom bar button clicks ---
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= screenH - BOTTOM_BAR_HEIGHT) {
        for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
            int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
            int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
            FdRect btnRect = { (float)bx, (float)by, (float)BTN_WIDTH, (float)BTN_HEIGHT };
            if (FdPointInRect(mouse, btnRect)) {
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
                              (uint8_t)lpi, &gs, &map);
                    map.tiles[mouseGrid.z][mouseGrid.x] = TILE_TOWER;
                    MapBuildMesh(&gameMapMesh, &map);
                }
            }
        } else if (selectedTowerType < 0 && mouseOnGround) {
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

    // --- Network polling ---
    if (netCtx.mode != NET_MODE_NONE) {
        NetPoll(&netCtx, &gs, enemies, towers, projectiles, &map);

        if (netCtx.mode == NET_MODE_CLIENT && !netCtx.serverPeer) {
            NetContextDestroy(&netCtx);
            NetShutdown();
            NetContextInit(&netCtx);
            localPaused = false;
            MapInit(&menuMap);
            MapBuildMesh(&menuMapMesh, &menuMap);
            menuCamCtrl.yaw = 0.0f;
            currentScene = SCENE_MENU;
            break;
        }
    }

    // --- Update game systems (host and single-player only) ---
    if (netCtx.mode != NET_MODE_CLIENT &&
        gs.phase != PHASE_OVER && gs.phase != PHASE_PAUSED) {
        GameUpdateWave(&gs, enemies, MAX_ENEMIES, &map, dt);
        EnemiesUpdate(enemies, MAX_ENEMIES, &map, &gs, dt);
        TowersUpdate(towers, MAX_TOWERS, enemies, MAX_ENEMIES, projectiles, MAX_PROJECTILES, &gs, &map, dt);
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

    // Rebuild map mesh if towers were placed via network
    {
        int towerCount = 0;
        for (int i = 0; i < MAX_TOWERS; i++)
            if (towers[i].active) towerCount++;
        if (towerCount != lastTowerCount) {
            lastTowerCount = towerCount;
            MapBuildMesh(&gameMapMesh, &map);
        }
    }

    // Compute camera matrices for this frame
    CameraControllerComputeMatrices(&camCtrl, rtW, rtH);

    // =========================
    // DRAW — 3D to low-res render target
    // =========================
    FdRenderTargetBegin(renderTarget, (Color){ 30, 30, 35, 255 });

    FdBegin3D(camCtrl.view, camCtrl.proj);
        DrawSkybox(camCtrl.position);

        MapDrawMesh(&gameMapMesh);

        // Batched blob shadows
        {
            BlobShadowEntry shadowEntries[MAX_BLOB_SHADOWS];
            int shadowCount = 0;
            for (int i = 0; i < MAX_TOWERS; i++) {
                if (!towers[i].active) continue;
                Vector3 tp = towers[i].worldPos;
                shadowEntries[shadowCount++] = (BlobShadowEntry){
                    tp.x, tp.z, 0.45f,
                    MapGetElevationY(&map, towers[i].gridPos.x, towers[i].gridPos.z)
                };
            }
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!enemies[i].active) continue;
                Vector3 ep = enemies[i].worldPos;
                GridPos eg = MapWorldToGrid(ep);
                shadowEntries[shadowCount++] = (BlobShadowEntry){
                    ep.x, ep.z, enemies[i].radius * 1.2f,
                    MapGetElevationY(&map, eg.x, eg.z)
                };
            }
            DrawBlobShadowsBatched(shadowEntries, shadowCount);
        }

        // Grid hover highlight
        if (mouseOnGround && selectedTowerType >= 0 &&
            mouseGrid.x >= 0 && mouseGrid.x < MAP_WIDTH &&
            mouseGrid.z >= 0 && mouseGrid.z < MAP_HEIGHT) {
            Vector3 ghostPos = MapGridToWorldElevated(&map, mouseGrid);
            ghostPos.y += 0.35f;
            Color ghostCol = canPlace ? (Color){ 0, 255, 0, 100 } : (Color){ 255, 0, 0, 100 };
            FdDrawCube(ghostPos, (Vector3){ 0.7f, 0.7f, 0.7f }, ghostCol);

            if (canPlace) {
                float range = TOWER_CONFIGS[selectedTowerType][0].range;
                Vector3 rangeCenter = MapGridToWorldElevated(&map, mouseGrid);
                DrawRangeCircle(rangeCenter, range, (Color){ 255, 255, 255, 80 });
            }
        }

        TowersDraw(towers, MAX_TOWERS, gs.playerCount);

        // Range indicator for selected tower
        if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
            const Tower *st = &towers[selectedTowerIdx];
            float tElevY = MapGetElevationY(&map, st->gridPos.x, st->gridPos.z);
            float range = TOWER_CONFIGS[st->type][st->level].range;
            Vector3 rc = st->worldPos;
            rc.y = tElevY;
            DrawRangeCircle(rc, range, (Color){ 255, 255, 100, 150 });
        }

        EnemiesDraw(enemies, MAX_ENEMIES);
        ProjectilesDraw(projectiles, MAX_PROJECTILES);
    FdEnd3D();
    FdRenderTargetEnd();

    // =========================
    // Full-res output
    // =========================
    FdBeginFrame(BLACK);
    FdRenderTargetBlit(renderTarget, screenW, screenH);

    // Enemy health bars at native resolution
    {
        FdMat4 vp = MatrixMultiply(camCtrl.view, camCtrl.proj);
        EnemiesDrawHUD(enemies, MAX_ENEMIES, vp, screenW, screenH);
    }

    // =====================
    // 2D UI
    // =====================

    // --- Top bar ---
    FdDrawRect(0, 0, screenW, 32, (Color){ 0, 0, 0, 180 });
    int localPI = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
    FdDrawText(FdTextFormat("Gold: %d", gs.playerGold[localPI]), 10, 7, 20, GOLD);
    FdDrawText(FdTextFormat("Lives: %d", gs.lives), 170, 7, 20,
             gs.lives > 5 ? RED : MAROON);
    FdDrawText(FdTextFormat("Wave: %d/%d", gs.currentWave + 1, MAX_WAVES), 330, 7, 20, WHITE);
    FdDrawFPS(screenW - 90, 7);

    // --- Wave countdown ---
    if (gs.phase == PHASE_WAVE_COUNTDOWN) {
        const char *countText = FdTextFormat("Next wave in %.1f", gs.waveCountdown);
        int tw = FdMeasureText(countText, 28);
        FdDrawText(countText, (screenW - tw) / 2, 80, 28, YELLOW);
    }

    // --- Bottom bar (tower buttons) ---
    FdDrawRect(0, screenH - BOTTOM_BAR_HEIGHT, screenW, BOTTOM_BAR_HEIGHT, (Color){ 20, 20, 25, 220 });
    for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
        int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
        int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
        int cost = TOWER_CONFIGS[i][0].cost;
        bool affordable = gs.playerGold[localPI] >= cost;

        Color btnBg = (selectedTowerType == i) ? (Color){ 80, 120, 80, 255 } :
                      affordable ? (Color){ 50, 50, 60, 255 } : (Color){ 40, 40, 40, 255 };
        Color btnFg = affordable ? WHITE : (Color){ 100, 100, 100, 255 };
        Color costCol = affordable ? GOLD : (Color){ 120, 80, 80, 255 };

        FdDrawRect(bx, by, BTN_WIDTH, BTN_HEIGHT, btnBg);
        FdDrawRectLines(bx, by, BTN_WIDTH, BTN_HEIGHT, (Color){ 100, 100, 100, 200 });

        // Tower color swatch
        FdDrawRect(bx + 4, by + 4, 12, 12, TOWER_CONFIGS[i][0].color);

        FdDrawText(TOWER_NAMES[i], bx + 20, by + 4, 16, btnFg);
        FdDrawText(FdTextFormat("$%d", cost), bx + 20, by + 24, 14, costCol);

        // Keybind hint
        FdDrawText(FdTextFormat("[%d]", i + 1), bx + BTN_WIDTH - 28, by + 28, 12, (Color){120,120,120,255});
    }

    // --- Tower info panel ---
    if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
        const Tower *st = &towers[selectedTowerIdx];
        const TowerConfig *cfg = &TOWER_CONFIGS[st->type][st->level];
        int px = screenW - INFO_PANEL_W - 10;
        int py = 40;

        FdDrawRect(px, py, INFO_PANEL_W, INFO_PANEL_H, (Color){ 20, 20, 30, 230 });
        FdDrawRectLines(px, py, INFO_PANEL_W, INFO_PANEL_H, (Color){ 100, 100, 100, 200 });

        FdDrawText(FdTextFormat("%s (Lv %d)", TOWER_NAMES[st->type], st->level + 1),
                 px + 8, py + 6, 18, cfg->color);
        FdDrawText(FdTextFormat("Damage: %.0f", cfg->damage),   px + 8, py + 30, 14, WHITE);
        FdDrawText(FdTextFormat("Range:  %.1f", cfg->range),    px + 8, py + 48, 14, WHITE);
        FdDrawText(FdTextFormat("Rate:   %.1f/s", cfg->fireRate), px + 8, py + 66, 14, WHITE);

        float tElevY = MapGetElevationY(&map, st->gridPos.x, st->gridPos.z);
        if (tElevY > 0.0f)
            FdDrawText(FdTextFormat("Elev:   +%.1f rng", tElevY), px + 8, py + 84, 14, (Color){100,200,255,255});

        int extraY = (tElevY > 0.0f) ? 102 : 84;
        if (cfg->aoeRadius > 0.0f)
            FdDrawText(FdTextFormat("AoE:    %.1f", cfg->aoeRadius), px + 8, py + extraY, 14, ORANGE);
        if (cfg->slowFactor < 1.0f)
            FdDrawText(FdTextFormat("Slow:   %.0f%%", (1.0f - cfg->slowFactor) * 100.0f), px + 8, py + extraY, 14, PURPLE);

        // Upgrade button
        if (st->level < TOWER_MAX_LEVEL - 1) {
            int upgCost = TOWER_CONFIGS[st->type][st->level + 1].cost;
            bool canUpg = gs.playerGold[localPI] >= upgCost;
            int ubx = px + 8, uby = py + INFO_PANEL_H - 30;
            int ubw = INFO_PANEL_W - 16, ubh = 24;

            Color ubCol = canUpg ? (Color){ 60, 100, 60, 255 } : (Color){ 50, 50, 50, 255 };
            FdDrawRect(ubx, uby, ubw, ubh, ubCol);
            FdDrawRectLines(ubx, uby, ubw, ubh, (Color){ 120, 120, 120, 200 });

            const char *upgText = FdTextFormat("Upgrade $%d", upgCost);
            Color upgTxtCol = canUpg ? GOLD : (Color){ 100, 100, 100, 255 };
            FdDrawText(upgText, ubx + 8, uby + 4, 16, upgTxtCol);

            if (canUpg && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                FdRect upgRect = { (float)ubx, (float)uby, (float)ubw, (float)ubh };
                if (FdPointInRect(mouse, upgRect)) {
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
            FdDrawText("MAX LEVEL", px + 8, py + INFO_PANEL_H - 26, 16, (Color){ 180, 180, 50, 255 });
        }
    }

    // --- Placement hint ---
    if (selectedTowerType >= 0) {
        FdDrawText(FdTextFormat("Placing: %s  (Right-click to cancel)",
                 TOWER_NAMES[selectedTowerType]), 10, screenH - BOTTOM_BAR_HEIGHT - 24, 16, YELLOW);
    }

    // --- Multiplayer Player List & Gift UI ---
    if (netCtx.mode != NET_MODE_NONE && gs.playerCount > 1) {
        int plX = 10, plY = 40;
        FdDrawRect(plX, plY, 180, 30 * gs.playerCount + 5, (Color){ 0, 0, 0, 150 });
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            if (!netCtx.playerConnected[i]) continue;
            int py = plY + 3 + i * 30;
            Color pCol = PLAYER_COLORS[i];
            bool isLocal = (i == localPI);
            FdDrawText(FdTextFormat("P%d: %s %s$%d", i + 1,
                    netCtx.playerNames[i],
                    isLocal ? "(You)" : "",
                    gs.playerGold[i]),
                    plX + 5, py, 14, pCol);

            // Gift button (only for other players)
            if (!isLocal && i != localPI) {
                int gbX = plX + 140;
                FdRect giftBtn = { (float)gbX, (float)py, 35.0f, 18.0f };
                bool giftHover = FdPointInRect(mouse, giftBtn);
                FdDrawRect(gbX, py, 35, 18, giftHover ? (Color){60,80,60,255} : (Color){40,50,40,255});
                FdDrawText("$25", gbX + 3, py + 2, 12, GOLD);
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
            FdDrawText(FdTextFormat("Owner: %s", netCtx.playerNames[owner]),
                    screenW - INFO_PANEL_W - 2, 28, 14, PLAYER_COLORS[owner]);
        }
    }

    // --- Pause Menu ---
    if (gs.phase == PHASE_PAUSED || localPaused) {
        FdDrawRect(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });

        const char *pauseTitle = "PAUSED";
        int pauseTitleW = FdMeasureText(pauseTitle, 60);
        FdDrawText(pauseTitle, (screenW - pauseTitleW) / 2, screenH / 2 - 100, 60, WHITE);

        int pBtnW = 200, pBtnH = 45;
        int pBtnX = (screenW - pBtnW) / 2;

        // Resume button
        int resumeY = screenH / 2 - 20;
        FdRect resumeBtn = { (float)pBtnX, (float)resumeY, (float)pBtnW, (float)pBtnH };
        bool resumeHover = FdPointInRect(mouse, resumeBtn);
        Color resumeBg = resumeHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
        FdDrawRect(pBtnX, resumeY, pBtnW, pBtnH, resumeBg);
        FdDrawRectLinesEx(pBtnX, resumeY, pBtnW, pBtnH, 2, (Color){ 100, 200, 100, 200 });
        const char *resumeText = "Resume";
        int resumeTextW = FdMeasureText(resumeText, 24);
        FdDrawText(resumeText, pBtnX + (pBtnW - resumeTextW) / 2, resumeY + 11, 24, WHITE);

        // Main Menu button
        int pmMenuY = resumeY + pBtnH + 12;
        FdRect pmMenuBtn = { (float)pBtnX, (float)pmMenuY, (float)pBtnW, (float)pBtnH };
        bool pmMenuHover = FdPointInRect(mouse, pmMenuBtn);
        Color pmMenuBg = pmMenuHover ? (Color){ 80, 80, 100, 255 } : (Color){ 50, 50, 65, 255 };
        FdDrawRect(pBtnX, pmMenuY, pBtnW, pBtnH, pmMenuBg);
        FdDrawRectLinesEx(pBtnX, pmMenuY, pBtnW, pBtnH, 2, (Color){ 120, 120, 160, 200 });
        const char *pmMenuText = "Main Menu";
        int pmMenuTextW = FdMeasureText(pmMenuText, 24);
        FdDrawText(pmMenuText, pBtnX + (pBtnW - pmMenuTextW) / 2, pmMenuY + 11, 24, WHITE);

        // Quit button
        int pQuitY = pmMenuY + pBtnH + 12;
        FdRect pQuitBtn = { (float)pBtnX, (float)pQuitY, (float)pBtnW, (float)pBtnH };
        bool pQuitHover = FdPointInRect(mouse, pQuitBtn);
        Color pQuitBg = pQuitHover ? (Color){ 140, 50, 50, 255 } : (Color){ 100, 35, 35, 255 };
        FdDrawRect(pBtnX, pQuitY, pBtnW, pBtnH, pQuitBg);
        FdDrawRectLinesEx(pBtnX, pQuitY, pBtnW, pBtnH, 2, (Color){ 200, 100, 100, 200 });
        const char *pQuitText = "Quit";
        int pQuitTextW = FdMeasureText(pQuitText, 24);
        FdDrawText(pQuitText, pBtnX + (pBtnW - pQuitTextW) / 2, pQuitY + 11, 24, WHITE);
    }

    // --- Chat overlay ---
    if (netCtx.mode != NET_MODE_NONE) {
        ChatDraw(&chatState, screenW, screenH);
    }

    // --- Game Over Screen ---
    if (gs.phase == PHASE_OVER) {
        FdDrawRect(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });
        bool won = gs.currentWave >= MAX_WAVES;
        const char *msg = won ? "VICTORY!" : "GAME OVER";
        Color msgCol = won ? GOLD : RED;
        int msgW = FdMeasureText(msg, 60);
        FdDrawText(msg, (screenW - msgW) / 2, screenH / 2 - 50, 60, msgCol);

        const char *sub = won ?
            FdTextFormat("All %d waves cleared!", MAX_WAVES) :
            FdTextFormat("Survived to wave %d/%d", gs.currentWave + 1, MAX_WAVES);
        int subW = FdMeasureText(sub, 24);
        FdDrawText(sub, (screenW - subW) / 2, screenH / 2 + 20, 24, WHITE);

        const char *hint = "Press R to restart | ESC for Main Menu";
        int hintW = FdMeasureText(hint, 18);
        FdDrawText(hint, (screenW - hintW) / 2, screenH / 2 + 60, 18, LIGHTGRAY);

        // Main Menu button
        int mbW = 180, mbH = 40;
        int mbX = (screenW - mbW) / 2;
        int mbY = screenH / 2 + 95;
        FdRect menuBtn = { (float)mbX, (float)mbY, (float)mbW, (float)mbH };
        bool menuHover = FdPointInRect(mouse, menuBtn);
        Color menuBg = menuHover ? (Color){ 80, 80, 100, 255 } : (Color){ 50, 50, 65, 255 };
        FdDrawRect(mbX, mbY, mbW, mbH, menuBg);
        FdDrawRectLinesEx(mbX, mbY, mbW, mbH, 2, (Color){ 120, 120, 160, 200 });
        const char *menuText = "Main Menu";
        int menuTextW = FdMeasureText(menuText, 22);
        FdDrawText(menuText, mbX + (mbW - menuTextW) / 2, mbY + 9, 22, WHITE);
    }

    FdEndFrame();

    // --- Pause menu button clicks ---
    if (gs.phase == PHASE_PAUSED || localPaused) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int pBtnW = 200, pBtnH = 45;
            int pBtnX = (screenW - pBtnW) / 2;
            int resumeY = screenH / 2 - 20;
            int pmMenuY = resumeY + pBtnH + 12;
            int pQuitY = pmMenuY + pBtnH + 12;

            FdRect resumeBtn = { (float)pBtnX, (float)resumeY, (float)pBtnW, (float)pBtnH };
            FdRect pmMenuBtn = { (float)pBtnX, (float)pmMenuY, (float)pBtnW, (float)pBtnH };
            FdRect pQuitBtn = { (float)pBtnX, (float)pQuitY, (float)pBtnW, (float)pBtnH };

            if (FdPointInRect(mouse, resumeBtn)) {
                if (localPaused) localPaused = false;
                else gs.phase = phaseBeforePause;
            } else if (FdPointInRect(mouse, pmMenuBtn)) {
                localPaused = false;
                if (netCtx.mode != NET_MODE_NONE) {
                    NetContextDestroy(&netCtx);
                    NetShutdown();
                    NetContextInit(&netCtx);
                }
                MapInit(&menuMap);
                MapBuildMesh(&menuMapMesh, &menuMap);
                menuCamCtrl.yaw = 0.0f;
                currentScene = SCENE_MENU;
            } else if (FdPointInRect(mouse, pQuitBtn)) {
                if (netCtx.mode != NET_MODE_NONE) {
                    NetContextDestroy(&netCtx);
                    NetShutdown();
                }
                FdQuitApp();
                return;
            }
        }
    }

    // --- Restart ---
    if (gs.phase == PHASE_OVER && IsKeyPressed(KEY_R)) {
        bool reloaded = false;
        for (int i = 0; i < mapRegistry.count; i++) {
            if (strcmp(mapRegistry.names[i], map.name) == 0) {
                reloaded = MapLoad(&map, mapRegistry.paths[i]);
                break;
            }
        }
        if (!reloaded) MapInit(&map);
        MapBuildMesh(&gameMapMesh, &map);
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
        FdRect menuBtn = { (float)((screenW - 180) / 2), (float)(screenH / 2 + 95), 180.0f, 40.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && FdPointInRect(mouse, menuBtn))
            goMenu = true;
        if (goMenu) {
            if (netCtx.mode != NET_MODE_NONE) {
                NetContextDestroy(&netCtx);
                NetShutdown();
                NetContextInit(&netCtx);
            }
            MapInit(&menuMap);
            MapBuildMesh(&menuMapMesh, &menuMap);
            menuCamCtrl.yaw = 0.0f;
            currentScene = SCENE_MENU;
        }
    }

    } break; // end SCENE_GAME
    } // end switch
}

// --- GameCleanup ---

void GameCleanup(void)
{
    if (netCtx.mode != NET_MODE_NONE) {
        NetContextDestroy(&netCtx);
        NetShutdown();
    }
    MapFreeMesh(&menuMapMesh);
    MapFreeMesh(&gameMapMesh);
    if (skyboxMesh) FdMeshDestroy(skyboxMesh);
    FdRenderTargetDestroy(renderTarget);
}
