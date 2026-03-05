#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "map.h"
#include "entity.h"
#include "game.h"
#include "net.h"
#include "lobby.h"
#include "chat.h"
#include "settings.h"
#include "progress.h"
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

typedef enum {
    SCENE_MENU, SCENE_MAP_SELECT, SCENE_DIFFICULTY_SELECT,
    SCENE_SHOP, SCENE_PERK_SELECT,
    SCENE_LOBBY, SCENE_GAME
} Scene;

// Track whether map select was opened for multiplayer host flow
static bool mapSelectForMultiplayer = false;
static Difficulty selectedDifficulty = DIFFICULTY_NORMAL;

// --- Meta-progression state ---
static PlayerProfile profile;
static RunModifiers runMods;
static EndlessState endlessState;
static int perkOffered[3] = {0};
static unsigned int perkSeed = 0;
static bool crystalsSaved = false;

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

static bool GetMouseGroundPos(Camera3D camera, const Map *map, Vector3 *outPos)
{
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    if (fabsf(ray.direction.y) < 0.001f) return false;

    // Test each tile's elevation plane, find closest valid hit
    float bestT = 1e9f;
    bool found = false;

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            float elevY = map->elevation[z][x] * ELEVATION_HEIGHT;
            float t = (elevY - ray.position.y) / ray.direction.y;
            if (t < 0.0f || t >= bestT) continue;

            float hx = ray.position.x + ray.direction.x * t;
            float hz = ray.position.z + ray.direction.z * t;

            // Check if hit is within this tile's XZ bounds
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

    // Fallback: intersect y=0 plane for tiles at elevation 0
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
        DrawLine3D(p1, p2, color);
    }
}

// --- Skybox (pre-baked mesh) ---

static Mesh skyboxMesh = {0};
static Material skyboxMaterial = {0};
static bool skyboxReady = false;

// --- Water plane (pre-baked mesh) ---

#define WATER_Y -0.3f

static Mesh waterMesh = {0};
static Material waterMaterial = {0};
static Shader waterShader = {0};
static int waterLocTime = -1;
static bool waterReady = false;

static void BuildWaterMesh(void)
{
    float xMin = -500.0f, xMax = 520.0f;
    float zMin = -500.0f, zMax = 515.0f;
    float step = 4.0f;

    int xSteps = (int)((xMax - xMin) / step);
    int zSteps = (int)((zMax - zMin) / step);
    int triCount = xSteps * zSteps * 2;
    int vertCount = triCount * 3;

    float *verts = malloc(vertCount * 3 * sizeof(float));
    unsigned char *cols = malloc(vertCount * 4 * sizeof(unsigned char));
    int vi = 0;

    unsigned char wr = 40, wg = 80, wb = 140;

    for (int zi = 0; zi < zSteps; zi++) {
        for (int xi = 0; xi < xSteps; xi++) {
            float x0 = xMin + xi * step;
            float x1 = x0 + step;
            float z0 = zMin + zi * step;
            float z1 = z0 + step;

            #define WATER_VERT(px,pz) do { \
                verts[vi*3]=px; verts[vi*3+1]=WATER_Y; verts[vi*3+2]=pz; \
                cols[vi*4]=wr; cols[vi*4+1]=wg; cols[vi*4+2]=wb; cols[vi*4+3]=255; \
                vi++; \
            } while(0)

            WATER_VERT(x0, z0);
            WATER_VERT(x1, z1);
            WATER_VERT(x1, z0);

            WATER_VERT(x0, z0);
            WATER_VERT(x0, z1);
            WATER_VERT(x1, z1);

            #undef WATER_VERT
        }
    }

    waterMesh = (Mesh){0};
    waterMesh.vertexCount = vertCount;
    waterMesh.triangleCount = triCount;
    waterMesh.vertices = verts;
    waterMesh.colors = cols;
    UploadMesh(&waterMesh, false);

    waterMaterial = LoadMaterialDefault();
    waterMaterial.shader = waterShader;
    waterReady = true;
}

static void DrawWater(float totalTime)
{
    if (!waterReady) return;
    SetShaderValue(waterShader, waterLocTime, &totalTime, SHADER_UNIFORM_FLOAT);
    DrawMesh(waterMesh, waterMaterial, MatrixIdentity());
}

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

    skyboxMesh = (Mesh){0};
    skyboxMesh.vertexCount = vertCount;
    skyboxMesh.triangleCount = triCount;
    skyboxMesh.vertices = verts;
    skyboxMesh.colors = cols;
    UploadMesh(&skyboxMesh, false);

    skyboxMaterial = LoadMaterialDefault();
    skyboxReady = true;
}

static void DrawSkybox(Camera3D camera)
{
    if (!skyboxReady) return;

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    Matrix transform = MatrixTranslate(camera.position.x, camera.position.y, camera.position.z);
    DrawMesh(skyboxMesh, skyboxMaterial, transform);

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

// --- Blob Shadow (batched) ---

// Pre-compute sin/cos table for blob shadow segments
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
    rlBegin(RL_TRIANGLES);
    rlColor4ub(0, 0, 0, 80);
    for (int e = 0; e < count; e++) {
        float cx = entries[e].x;
        float cz = entries[e].z;
        float r = entries[e].radius;
        float y = entries[e].elevY + 0.01f;
        for (int i = 0; i < BLOB_SHADOW_SEGMENTS; i++) {
            rlVertex3f(cx, y, cz);
            rlVertex3f(cx + blobShadowCos[i] * r, y, cz + blobShadowSin[i] * r);
            rlVertex3f(cx + blobShadowCos[i+1] * r, y, cz + blobShadowSin[i+1] * r);
        }
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

// --- Settings globals ---

static Settings settings;
static SettingsState settingsState = {0};

// --- Main ---

int main(void)
{
    SettingsDefault(&settings);
    SettingsLoad(&settings, "settings.cfg");

    int initFlags = FLAG_WINDOW_RESIZABLE;
    if (settings.vsync) initFlags |= FLAG_VSYNC_HINT;
    SetConfigFlags(initFlags);

    int initW = RESOLUTION_PRESETS[settings.resolutionIdx].width;
    int initH = RESOLUTION_PRESETS[settings.resolutionIdx].height;
    InitWindow(initW, initH, "Formal Defense");
    SetExitKey(0);

    // Apply fullscreen mode after window creation
    if (settings.fullscreen == FULLSCREEN_BORDERLESS) ToggleBorderlessWindowed();
    else if (settings.fullscreen == FULLSCREEN_EXCLUSIVE) ToggleFullscreen();

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

    // --- Water Shader ---
    waterShader = LoadShader("shaders/water.vs", "shaders/water.fs");
    waterShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(waterShader, "matModel");
    waterLocTime = GetShaderLocation(waterShader, "time");

    int wLocResolution = GetShaderLocation(waterShader, "resolution");
    int wLocJitter = GetShaderLocation(waterShader, "jitterStrength");
    int wLocLightDir = GetShaderLocation(waterShader, "lightDir");
    int wLocLightColor = GetShaderLocation(waterShader, "lightColor");
    int wLocAmbientColor = GetShaderLocation(waterShader, "ambientColor");
    int wLocColorBands = GetShaderLocation(waterShader, "colorBands");

    SetShaderValue(waterShader, wLocJitter, &jitterStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(waterShader, wLocLightDir, lightDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(waterShader, wLocLightColor, lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(waterShader, wLocAmbientColor, ambientColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(waterShader, wLocColorBands, &colorBands, SHADER_UNIFORM_FLOAT);

    int cachedScreenW = GetScreenWidth();
    int cachedScreenH = GetScreenHeight();
    int rtW = cachedScreenW / settings.ps1Downscale;
    int rtH = cachedScreenH / settings.ps1Downscale;
    RenderTexture2D renderTarget = LoadRenderTexture(rtW, rtH);
    SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);

    float resolution[2] = { (float)rtW, (float)rtH };
    SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(waterShader, wLocResolution, resolution, SHADER_UNIFORM_VEC2);

    Scene currentScene = SCENE_MENU;
    float totalTime = 0.0f;

    // --- Pre-baked meshes ---
    BuildSkyboxMesh();
    BuildWaterMesh();
    InitBlobShadowTable();

    Mesh sphereMesh = GenMeshSphere(1.0f, 8, 8);
    Model sphereModel = LoadModelFromMesh(sphereMesh);
    sphereModel.materials[0].shader = ps1Shader;

    MapMesh menuMapMesh = {0};
    MapMesh gameMapMesh = {0};
    int lastTowerCount = 0;  // Track tower count to detect network-placed towers

    // --- Multiplayer state ---
    NetContext netCtx;
    NetContextInit(&netCtx);
    LobbyState lobbyState;
    LobbyStateInit(&lobbyState);
    ChatState chatState;
    ChatStateInit(&chatState);
    g_chatStatePtr = &chatState;
    g_netChatCallback = OnNetChatReceived;

    // --- Map registry ---
    MapRegistry mapRegistry;
    MapRegistryScan(&mapRegistry, "maps");
    int selectedMapIdx = 0;

    // --- Load player profile ---
    PlayerProfileLoad(&profile, "profile.fdsave");

    // --- Menu state ---
    Map menuMap;
    if (mapRegistry.count > 0) {
        int menuMapIdx = GetRandomValue(0, mapRegistry.count - 1);
        if (!MapLoad(&menuMap, mapRegistry.paths[menuMapIdx]))
            MapInit(&menuMap);
    } else {
        MapInit(&menuMap);
    }
    MapBuildMesh(&menuMapMesh, &menuMap, ps1Shader);
    CameraController menuCamCtrl;
    CameraControllerInit(&menuCamCtrl);
    menuCamCtrl.distance = 22.0f;
    Camera3D menuCamera = {0};
    CameraControllerUpdate(&menuCamCtrl, &menuCamera, 0.0f);

    Map map;
    MapInit(&map);
    MapBuildMesh(&gameMapMesh, &map, ps1Shader);

    GameState gs;
    RunModifiersInit(&runMods, &profile);
    GameStateInit(&gs, DIFFICULTY_NORMAL, &runMods);

    Enemy enemies[MAX_ENEMIES];
    memset(enemies, 0, sizeof(enemies));

    Tower towers[MAX_TOWERS];
    memset(towers, 0, sizeof(towers));

    Projectile projectiles[MAX_PROJECTILES];
    memset(projectiles, 0, sizeof(projectiles));

    CameraController camCtrl;
    CameraControllerInit(&camCtrl);
    camCtrl.panSpeed = settings.camPanSpeed;
    camCtrl.rotSpeed = settings.camRotSpeed;
    camCtrl.zoomSpeed = settings.camZoomSpeed;

    Camera3D camera = {0};
    CameraControllerUpdate(&camCtrl, &camera, 0.0f);

    int selectedTowerType = -1;  // -1 = no selection for placement
    int selectedTowerIdx = -1;   // -1 = no tower selected for info
    GamePhase phaseBeforePause = PHASE_PLAYING;
    bool localPaused = false;    // For multiplayer: local UI pause without stopping sim

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        totalTime += dt;
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        Vector2 mouse = GetMousePosition();

        if (IsKeyPressed(KEY_F11)) {
            ToggleBorderlessWindowed();
            settings.fullscreen = (settings.fullscreen == FULLSCREEN_BORDERLESS)
                ? FULLSCREEN_WINDOWED : FULLSCREEN_BORDERLESS;
        }

        // Recreate render target on window resize
        if (screenW != cachedScreenW || screenH != cachedScreenH) {
            cachedScreenW = screenW;
            cachedScreenH = screenH;
            UnloadRenderTexture(renderTarget);
            rtW = screenW / settings.ps1Downscale;
            rtH = screenH / settings.ps1Downscale;
            renderTarget = LoadRenderTexture(rtW, rtH);
            SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);
            resolution[0] = (float)rtW;
            resolution[1] = (float)rtH;
            SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);
            SetShaderValue(waterShader, wLocResolution, resolution, SHADER_UNIFORM_VEC2);
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
                DrawWater(totalTime);
                MapDrawMesh(&menuMapMesh);
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

            // Settings button
            int stY = mpY + pbH + 15;
            Rectangle setBtn = { (float)pbX, (float)stY, (float)pbW, (float)pbH };
            bool setHover = CheckCollisionPointRec(mouse, setBtn);
            Color setBg = setHover ? (Color){ 70, 55, 110, 255 } : (Color){ 48, 38, 78, 255 };
            DrawRectangleRec(setBtn, setBg);
            DrawRectangleLinesEx(setBtn, 2, (Color){ 140, 110, 200, 200 });
            const char *setText = "Settings";
            int setTextW = MeasureText(setText, 30);
            DrawText(setText, pbX + (pbW - setTextW) / 2, stY + 10, 30, WHITE);

            // Quit button
            int qbY = stY + pbH + 15;
            Rectangle quitBtn = { (float)pbX, (float)qbY, (float)pbW, (float)pbH };
            bool quitHover = CheckCollisionPointRec(mouse, quitBtn);
            Color quitBg = quitHover ? (Color){ 140, 50, 50, 255 } : (Color){ 100, 35, 35, 255 };
            DrawRectangleRec(quitBtn, quitBg);
            DrawRectangleLinesEx(quitBtn, 2, (Color){ 200, 100, 100, 200 });
            const char *quitText = "Quit";
            int quitTextW = MeasureText(quitText, 30);
            DrawText(quitText, pbX + (pbW - quitTextW) / 2, qbY + 10, 30, WHITE);

            // Settings overlay (drawn on top)
            SettingsDraw(&settingsState, screenW, screenH);

            // Copyright & credit
            DrawText("Made by FormalSnake", 10, screenH - 40, 16, LIGHTGRAY);
            DrawText("(c) 2026 FormalSnake", 10, screenH - 22, 14, GRAY);

            EndDrawing();

            // Settings update
            int settingsResult = SettingsUpdate(&settingsState);
            if (settingsResult == 1) {
                // Apply settings
                settings = settingsState.pending;
                SettingsSave(&settings, "settings.cfg");

                // Recreate render target with new downscale
                UnloadRenderTexture(renderTarget);
                rtW = screenW / settings.ps1Downscale;
                rtH = screenH / settings.ps1Downscale;
                renderTarget = LoadRenderTexture(rtW, rtH);
                SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);
                resolution[0] = (float)rtW;
                resolution[1] = (float)rtH;
                SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);

                // VSync
                if (settings.vsync) SetWindowState(FLAG_VSYNC_HINT);
                else ClearWindowState(FLAG_VSYNC_HINT);

                // Fullscreen - compare with current state
                // (simplified: just set window size for windowed)
                if (settings.fullscreen == FULLSCREEN_WINDOWED) {
                    if (IsWindowFullscreen()) ToggleFullscreen();
                    else if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                    SetWindowSize(RESOLUTION_PRESETS[settings.resolutionIdx].width,
                                  RESOLUTION_PRESETS[settings.resolutionIdx].height);
                } else if (settings.fullscreen == FULLSCREEN_BORDERLESS) {
                    if (IsWindowFullscreen()) ToggleFullscreen();
                    if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                } else if (settings.fullscreen == FULLSCREEN_EXCLUSIVE) {
                    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                    if (!IsWindowFullscreen()) ToggleFullscreen();
                }

                // Camera speeds
                camCtrl.panSpeed = settings.camPanSpeed;
                camCtrl.rotSpeed = settings.camRotSpeed;
                camCtrl.zoomSpeed = settings.camZoomSpeed;
            }

            // Skip button clicks when settings overlay is open
            if (settingsState.open) break;

            // Settings button click
            if (setHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                SettingsOpen(&settingsState, &settings);
                break;
            }

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
                mapSelectForMultiplayer = false;
                MapRegistryScan(&mapRegistry, "maps");
                selectedMapIdx = 0;
                currentScene = SCENE_MAP_SELECT;
            }
        } break;

        case SCENE_MAP_SELECT: {
            BeginDrawing();
            ClearBackground((Color){ 20, 22, 28, 255 });

            const char *msTitle = mapSelectForMultiplayer ? "Select Map (Host)" : "Select Map";
            int msTitleW = MeasureText(msTitle, 36);
            DrawText(msTitle, (screenW - msTitleW) / 2, 50, 36, WHITE);

            if (mapRegistry.count == 0) {
                DrawText("No maps found in maps/ directory", screenW / 2 - 150, 120, 18, LIGHTGRAY);
            }

            for (int i = 0; i < mapRegistry.count; i++) {
                int my = 110 + i * 50;
                bool selected = (selectedMapIdx == i);
                bool hover = CheckCollisionPointRec(mouse,
                    (Rectangle){ (float)(screenW / 2 - 150), (float)my, 300.0f, 42.0f });

                Color bg = selected ? (Color){ 50, 80, 110, 255 } :
                           hover    ? (Color){ 40, 55, 75, 255 }  :
                                      (Color){ 30, 35, 45, 200 };
                DrawRectangle(screenW / 2 - 150, my, 300, 42, bg);
                DrawRectangleLines(screenW / 2 - 150, my, 300, 42, (Color){ 80, 100, 120, 200 });
                DrawText(mapRegistry.names[i], screenW / 2 - 138, my + 12, 20,
                         selected ? GOLD : WHITE);

                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    selectedMapIdx = i;
            }

            int btnBaseY = 110 + (mapRegistry.count > 0 ? mapRegistry.count : 1) * 50 + 20;

            // Start / Select button
            int sbW = 200, sbH = 45;
            int sbX = (screenW - sbW) / 2;
            Rectangle startBtn = { (float)sbX, (float)btnBaseY, (float)sbW, (float)sbH };
            bool startHover = CheckCollisionPointRec(mouse, startBtn);
            Color startBg = startHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
            bool canStart = mapRegistry.count > 0;
            if (!canStart) startBg = (Color){ 40, 40, 40, 255 };
            DrawRectangleRec(startBtn, startBg);
            DrawRectangleLinesEx(startBtn, 2, (Color){ 100, 200, 100, 200 });
            const char *startText = mapSelectForMultiplayer ? "Host with Map" : "Play";
            int startTextW = MeasureText(startText, 24);
            DrawText(startText, sbX + (sbW - startTextW) / 2, btnBaseY + 11, 24,
                     canStart ? WHITE : DARKGRAY);

            // Back button
            int bbY = btnBaseY + sbH + 12;
            Rectangle backBtn = { (float)sbX, (float)bbY, (float)sbW, 40.0f };
            bool backHover = CheckCollisionPointRec(mouse, backBtn);
            Color backBg = backHover ? (Color){ 80, 50, 50, 255 } : (Color){ 55, 35, 35, 255 };
            DrawRectangleRec(backBtn, backBg);
            DrawRectangleLinesEx(backBtn, 2, (Color){ 180, 100, 100, 200 });
            const char *backText = "Back";
            int backTextW = MeasureText(backText, 22);
            DrawText(backText, sbX + (sbW - backTextW) / 2, bbY + 9, 22, WHITE);

            // Handle start click
            if (canStart && startHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (mapSelectForMultiplayer) {
                    // Host flow: load map, set up net context, go to difficulty select
                    if (lobbyState.usernameLen > 0) {
                        NetInit();
                        if (NetHostCreate(&netCtx, lobbyState.username)) {
                            strncpy(netCtx.selectedMap, mapRegistry.names[selectedMapIdx], MAX_MAP_NAME - 1);
                            strncpy(netCtx.selectedMapPath, mapRegistry.paths[selectedMapIdx], 255);
                            currentScene = SCENE_DIFFICULTY_SELECT;
                        }
                    }
                } else {
                    // Single-player: load map, go to difficulty select
                    if (!MapLoad(&map, mapRegistry.paths[selectedMapIdx]))
                        MapInit(&map);
                    MapBuildMesh(&gameMapMesh, &map, ps1Shader);
                    currentScene = SCENE_DIFFICULTY_SELECT;
                }
            }

            // Handle back
            if ((backHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ESCAPE)) {
                if (mapSelectForMultiplayer) {
                    // If we already created the host, tear it down
                    if (netCtx.mode == NET_MODE_HOST) {
                        NetContextDestroy(&netCtx);
                        NetShutdown();
                        NetContextInit(&netCtx);
                    }
                    currentScene = SCENE_LOBBY;
                } else {
                    currentScene = SCENE_MENU;
                }
            }

            EndDrawing();
        } break;

        case SCENE_DIFFICULTY_SELECT: {
            BeginDrawing();
            ClearBackground((Color){ 20, 22, 28, 255 });

            const char *dsTitle = "Select Difficulty";
            int dsTitleW = MeasureText(dsTitle, 36);
            DrawText(dsTitle, (screenW - dsTitleW) / 2, 50, 36, WHITE);

            const char *descriptions[DIFFICULTY_COUNT] = {
                "Fewer, weaker enemies. More gold and lives.",
                "The standard experience.",
                "Tougher enemies, less gold, faster spawns.",
                "Extreme challenge. Only for the brave.",
            };

            for (int i = 0; i < DIFFICULTY_COUNT; i++) {
                int dy = 120 + i * 65;
                bool selected = ((int)selectedDifficulty == i);
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
                    selectedDifficulty = (Difficulty)i;
            }

            int dsBtnY = 120 + DIFFICULTY_COUNT * 65 + 20;
            int dsBtnW = 200, dsBtnH = 45;
            int dsBtnX = (screenW - dsBtnW) / 2;

            // Start button
            Rectangle dsStartBtn = { (float)dsBtnX, (float)dsBtnY, (float)dsBtnW, (float)dsBtnH };
            bool dsStartHover = CheckCollisionPointRec(mouse, dsStartBtn);
            Color dsStartBg = dsStartHover ? (Color){ 60, 120, 60, 255 } : (Color){ 40, 80, 40, 255 };
            DrawRectangleRec(dsStartBtn, dsStartBg);
            DrawRectangleLinesEx(dsStartBtn, 2, (Color){ 100, 200, 100, 200 });
            const char *dsStartText = "Start";
            int dsStartTextW = MeasureText(dsStartText, 24);
            DrawText(dsStartText, dsBtnX + (dsBtnW - dsStartTextW) / 2, dsBtnY + 11, 24, WHITE);

            // Back button
            int dsBbY = dsBtnY + dsBtnH + 12;
            Rectangle dsBackBtn = { (float)dsBtnX, (float)dsBbY, (float)dsBtnW, 40.0f };
            bool dsBackHover = CheckCollisionPointRec(mouse, dsBackBtn);
            Color dsBackBg = dsBackHover ? (Color){ 80, 50, 50, 255 } : (Color){ 55, 35, 35, 255 };
            DrawRectangleRec(dsBackBtn, dsBackBg);
            DrawRectangleLinesEx(dsBackBtn, 2, (Color){ 180, 100, 100, 200 });
            const char *dsBackText = "Back";
            int dsBackTextW = MeasureText(dsBackText, 22);
            DrawText(dsBackText, dsBtnX + (dsBtnW - dsBackTextW) / 2, dsBbY + 9, 22, WHITE);

            EndDrawing();

            // Start click
            if (dsStartHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (mapSelectForMultiplayer) {
                    // Host: go to lobby with discovery
                    netCtx.selectedDifficulty = (uint8_t)selectedDifficulty;
                    NetDiscoveryStart(&netCtx);
                    lobbyState.phase = LOBBY_HOST_WAIT;
                    currentScene = SCENE_LOBBY;
                } else {
                    // Single-player: go to shop
                    currentScene = SCENE_SHOP;
                }
            }

            // Back click
            if ((dsBackHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ESCAPE)) {
                if (mapSelectForMultiplayer) {
                    // Tear down host and go back to map select
                    if (netCtx.mode == NET_MODE_HOST) {
                        NetContextDestroy(&netCtx);
                        NetShutdown();
                        NetContextInit(&netCtx);
                    }
                }
                currentScene = SCENE_MAP_SELECT;
            }
        } break;

        case SCENE_SHOP: {
            BeginDrawing();
            ClearBackground((Color){ 20, 22, 28, 255 });

            const char *shopTitle = "Crystal Shop";
            int shopTitleW = MeasureText(shopTitle, 36);
            DrawText(shopTitle, (screenW - shopTitleW) / 2, 20, 36, WHITE);

            // Crystal balance
            DrawText(TextFormat("Crystals: %d", profile.crystals), screenW - 220, 25, 24, (Color){180,100,255,255});

            // Shop categories
            const char *catNames[] = { "Stat Boosts", "Tower Unlocks", "Abilities" };
            int catItems[][6] = {
                { SHOP_DAMAGE_1, SHOP_DAMAGE_2, SHOP_STARTING_GOLD, SHOP_STARTING_LIVES, SHOP_TOWER_COST, SHOP_GOLD_PER_KILL },
                { SHOP_TOWER_RANGE, SHOP_UNLOCK_SNIPER, SHOP_UNLOCK_SLOW, SHOP_UNLOCK_LASER, SHOP_UNLOCK_MORTAR, SHOP_UNLOCK_TESLA },
                { SHOP_UNLOCK_FLAME, SHOP_ABILITY_AIRSTRIKE, SHOP_ABILITY_GOLD_RUSH, SHOP_ABILITY_FORTIFY, -1, -1 },
            };
            int catCounts[] = { 6, 6, 4 };

            int shopY = 65;
            for (int cat = 0; cat < 3; cat++) {
                DrawText(catNames[cat], 30, shopY, 20, GOLD);
                shopY += 26;

                for (int ci = 0; ci < catCounts[cat]; ci++) {
                    int itemID = catItems[cat][ci];
                    if (itemID < 0) continue;
                    const ShopItemConfig *si = &SHOP_ITEMS[itemID];
                    bool purchased = profile.shopPurchased[itemID];
                    bool canBuy = ShopCanPurchase(&profile, (ShopItemID)itemID);

                    int itemX = 40;
                    int itemW = screenW - 80;
                    int itemH = 32;
                    Rectangle itemRect = { (float)itemX, (float)shopY, (float)itemW, (float)itemH };
                    bool hover = CheckCollisionPointRec(mouse, itemRect);

                    Color bg = purchased ? (Color){30,50,30,200} :
                               canBuy && hover ? (Color){50,60,80,255} :
                               canBuy ? (Color){35,40,50,200} :
                               (Color){30,30,35,200};
                    DrawRectangleRec(itemRect, bg);
                    DrawRectangleLinesEx(itemRect, 1, (Color){60,60,70,200});

                    Color nameCol = purchased ? (Color){100,180,100,255} : canBuy ? WHITE : (Color){100,100,100,255};
                    DrawText(si->name, itemX + 8, shopY + 4, 16, nameCol);
                    DrawText(si->description, itemX + 200, shopY + 4, 14, (Color){180,180,180,255});

                    if (purchased) {
                        DrawText("OWNED", itemX + itemW - 70, shopY + 8, 14, (Color){100,180,100,255});
                    } else {
                        Color costCol = canBuy ? (Color){180,100,255,255} : (Color){100,60,60,255};
                        DrawText(TextFormat("%d", si->cost), itemX + itemW - 60, shopY + 8, 14, costCol);
                    }

                    // Buy on click
                    if (!purchased && canBuy && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        ShopPurchase(&profile, (ShopItemID)itemID);
                        PlayerProfileSave(&profile, "profile.fdsave");
                    }

                    shopY += itemH + 2;
                }
                shopY += 8;
            }

            // Continue button
            int cBtnW = 200, cBtnH = 45;
            int cBtnX = (screenW - cBtnW) / 2;
            int cBtnY = screenH - 110;
            Rectangle contBtn = { (float)cBtnX, (float)cBtnY, (float)cBtnW, (float)cBtnH };
            bool contHover = CheckCollisionPointRec(mouse, contBtn);
            DrawRectangleRec(contBtn, contHover ? (Color){60,120,60,255} : (Color){40,80,40,255});
            DrawRectangleLinesEx(contBtn, 2, (Color){100,200,100,200});
            const char *contText = "Continue";
            int contTextW = MeasureText(contText, 24);
            DrawText(contText, cBtnX + (cBtnW - contTextW) / 2, cBtnY + 11, 24, WHITE);

            // Back button
            int sBkY = cBtnY + cBtnH + 10;
            Rectangle shopBackBtn = { (float)cBtnX, (float)sBkY, (float)cBtnW, 40.0f };
            bool shopBackHover = CheckCollisionPointRec(mouse, shopBackBtn);
            DrawRectangleRec(shopBackBtn, shopBackHover ? (Color){80,50,50,255} : (Color){55,35,35,255});
            DrawRectangleLinesEx(shopBackBtn, 2, (Color){180,100,100,200});
            const char *shopBackText = "Back";
            int shopBackTextW = MeasureText(shopBackText, 22);
            DrawText(shopBackText, cBtnX + (cBtnW - shopBackTextW) / 2, sBkY + 9, 22, WHITE);

            EndDrawing();

            if (contHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                RunModifiersInit(&runMods, &profile);
                perkSeed = (unsigned int)(GetTime() * 1000.0);
                PerkSelectRandom(perkOffered, perkSeed);
                currentScene = SCENE_PERK_SELECT;
            }
            if ((shopBackHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ESCAPE)) {
                currentScene = SCENE_DIFFICULTY_SELECT;
            }
        } break;

        case SCENE_PERK_SELECT: {
            BeginDrawing();
            ClearBackground((Color){ 20, 22, 28, 255 });

            const char *perkTitle = "Choose a Perk";
            int perkTitleW = MeasureText(perkTitle, 36);
            DrawText(perkTitle, (screenW - perkTitleW) / 2, 40, 36, WHITE);
            DrawText("Or skip for no perk", (screenW - MeasureText("Or skip for no perk", 16)) / 2, 82, 16, LIGHTGRAY);

            int cardW = 220, cardH = 140;
            int gap = 30;
            int totalW = 3 * cardW + 2 * gap;
            int startX = (screenW - totalW) / 2;
            int cardY = 120;

            for (int p = 0; p < 3; p++) {
                int pid = perkOffered[p];
                const PerkConfig *pc = &PERK_CONFIGS[pid];
                int cx = startX + p * (cardW + gap);
                Rectangle cardRect = { (float)cx, (float)cardY, (float)cardW, (float)cardH };
                bool hover = CheckCollisionPointRec(mouse, cardRect);

                Color bg = hover ? (Color){50,60,80,255} : (Color){35,40,50,200};
                DrawRectangleRec(cardRect, bg);
                DrawRectangleLinesEx(cardRect, 2, hover ? (Color){100,150,220,255} : (Color){60,70,90,200});

                DrawText(pc->name, cx + 10, cardY + 10, 20, WHITE);
                // Word-wrap description manually (simple approach)
                DrawText(pc->description, cx + 10, cardY + 40, 14, (Color){200,200,200,255});

                if (pc->hasTradeoff)
                    DrawText("Trade-off", cx + 10, cardY + cardH - 24, 12, (Color){255,180,80,255});
                else
                    DrawText("Pure buff", cx + 10, cardY + cardH - 24, 12, (Color){100,200,100,255});

                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    RunModifiersApplyPerk(&runMods, pid);
                    // Init game and go
                    endlessState = (EndlessState){0};
                    crystalsSaved = false;
                    GameStateInit(&gs, selectedDifficulty, &runMods);
                    memset(enemies, 0, sizeof(enemies));
                    memset(towers, 0, sizeof(towers));
                    memset(projectiles, 0, sizeof(projectiles));
                    CameraControllerInit(&camCtrl);
                    CameraControllerUpdate(&camCtrl, &camera, 0.0f);
                    selectedTowerType = -1;
                    selectedTowerIdx = -1;
                    currentScene = SCENE_GAME;
                }
            }

            // Skip button
            int skipW = 140, skipH = 40;
            int skipX = (screenW - skipW) / 2;
            int skipY = cardY + cardH + 30;
            Rectangle skipBtn = { (float)skipX, (float)skipY, (float)skipW, (float)skipH };
            bool skipHover = CheckCollisionPointRec(mouse, skipBtn);
            DrawRectangleRec(skipBtn, skipHover ? (Color){70,60,50,255} : (Color){50,45,40,255});
            DrawRectangleLinesEx(skipBtn, 2, (Color){150,130,100,200});
            const char *skipText = "Skip";
            int skipTextW = MeasureText(skipText, 22);
            DrawText(skipText, skipX + (skipW - skipTextW) / 2, skipY + 9, 22, WHITE);

            EndDrawing();

            if (skipHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // No perk, start game
                endlessState = (EndlessState){0};
                crystalsSaved = false;
                GameStateInit(&gs, selectedDifficulty, &runMods);
                memset(enemies, 0, sizeof(enemies));
                memset(towers, 0, sizeof(towers));
                memset(projectiles, 0, sizeof(projectiles));
                CameraControllerInit(&camCtrl);
                CameraControllerUpdate(&camCtrl, &camera, 0.0f);
                selectedTowerType = -1;
                selectedTowerIdx = -1;
                currentScene = SCENE_GAME;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                currentScene = SCENE_SHOP;
            }
        } break;

        case SCENE_LOBBY: {
            LobbyUpdate(&lobbyState, &netCtx);

            BeginDrawing();
            ClearBackground((Color){ 20, 22, 28, 255 });
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

            EndDrawing();

            // Check if game should start
            if (LobbyGameStarted(&lobbyState, &netCtx)) {
                if (netCtx.mode == NET_MODE_HOST) {
                    // Host: load the selected map
                    if (!MapLoad(&map, netCtx.selectedMapPath))
                        MapInit(&map);
                    // Send map data to clients
                    NetSendMapData(&netCtx, &map);
                    GameStateInitMultiplayer(&gs, netCtx.playerCount, (Difficulty)netCtx.selectedDifficulty, &runMods);
                } else {
                    // Client: load map by name (may have been saved by MSG_MAP_DATA handler)
                    char localPath[256];
                    snprintf(localPath, sizeof(localPath), "maps/%s.fdmap", netCtx.selectedMap);
                    if (!MapLoad(&map, localPath))
                        MapInit(&map);
                    GameStateInitMultiplayer(&gs, netCtx.playerCount, (Difficulty)netCtx.selectedDifficulty, &runMods);
                }
                MapBuildMesh(&gameMapMesh, &map, ps1Shader);
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
        if (!chatActive && !settingsState.open && IsKeyPressed(KEY_ESCAPE)) {
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
        bool mouseOnGround = GetMouseGroundPos(camera, &map, &mouseGround);
        GridPos mouseGrid = {-1, -1};
        bool canPlace = false;

        if (mouseOnGround) {
            mouseGrid = MapWorldToGrid(mouseGround);
            if (selectedTowerType >= 0)
                canPlace = MapCanPlaceTower(&map, mouseGrid);
        }

        // Build list of unlocked tower types (needed for both input and draw)
        int unlockedTowers[TOWER_TYPE_COUNT];
        int unlockedCount = 0;
        for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
            if (runMods.towerUnlocked[i])
                unlockedTowers[unlockedCount++] = i;
        }

        if (gs.phase != PHASE_PAUSED) {

        // --- Camera (only when not in UI or chat) ---
        if (!chatActive && (!mouseInUI || IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D)))
            CameraControllerUpdate(&camCtrl, &camera, dt);

        // --- Ability cooldown ticking ---
        for (int a = 0; a < ABILITY_COUNT; a++) {
            if (runMods.abilityTimer[a] > 0.0f)
                runMods.abilityTimer[a] -= dt;
        }

        // --- Ability input ---
        if (!chatActive && gs.phase == PHASE_PLAYING) {
            // Q = Airstrike: deal 200 damage to all enemies in 3.0 radius at mouse pos
            if (runMods.abilityUnlocked[ABILITY_AIRSTRIKE] && runMods.abilityTimer[ABILITY_AIRSTRIKE] <= 0.0f
                && IsKeyPressed(KEY_Q) && mouseOnGround) {
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (!enemies[j].active) continue;
                    if (Vector3Distance(mouseGround, enemies[j].worldPos) <= 3.0f) {
                        enemies[j].hp -= 200.0f;
                        if (enemies[j].hp <= 0.0f) {
                            enemies[j].active = false;
                            int lpi = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
                            gs.playerGold[lpi] += gs.goldPerKill;
                            gs.gold = gs.playerGold[0];
                        }
                    }
                }
                runMods.abilityTimer[ABILITY_AIRSTRIKE] = ABILITY_AIRSTRIKE_COOLDOWN;
            }
            // E = Gold Rush: toggle 2x gold for duration
            if (runMods.abilityUnlocked[ABILITY_GOLD_RUSH] && runMods.abilityTimer[ABILITY_GOLD_RUSH] <= 0.0f
                && IsKeyPressed(KEY_E) && !runMods.goldRushActive) {
                runMods.goldRushActive = true;
                runMods.abilityCooldown[ABILITY_GOLD_RUSH] = ABILITY_GOLD_RUSH_DURATION;
            }
            // R = Fortify: +5 lives instantly
            if (runMods.abilityUnlocked[ABILITY_FORTIFY] && runMods.abilityTimer[ABILITY_FORTIFY] <= 0.0f
                && IsKeyPressed(KEY_R)) {
                gs.lives += 5;
                runMods.abilityTimer[ABILITY_FORTIFY] = ABILITY_FORTIFY_COOLDOWN;
            }
        }

        // Gold Rush duration tracking
        if (runMods.goldRushActive) {
            runMods.abilityCooldown[ABILITY_GOLD_RUSH] -= dt;
            if (runMods.abilityCooldown[ABILITY_GOLD_RUSH] <= 0.0f) {
                runMods.goldRushActive = false;
                runMods.abilityTimer[ABILITY_GOLD_RUSH] = ABILITY_GOLD_RUSH_COOLDOWN;
            }
        }

        // --- Bottom bar button clicks ---
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= screenH - BOTTOM_BAR_HEIGHT) {
            for (int i = 0; i < unlockedCount; i++) {
                int ttype = unlockedTowers[i];
                int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
                int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
                Rectangle btnRect = { (float)bx, (float)by, (float)BTN_WIDTH, (float)BTN_HEIGHT };
                if (CheckCollisionPointRec(mouse, btnRect)) {
                    int cost = (int)(TOWER_CONFIGS[ttype][0].cost * runMods.towerCostMultiplier);
                    int lpi = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
                    if (gs.playerGold[lpi] >= cost) {
                        selectedTowerType = ttype;
                        selectedTowerIdx = -1;
                    }
                }
            }
        }

        // --- Tower selection keys (1-8 map to unlocked slots) ---
        {
            int keys[] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT };
            for (int i = 0; i < unlockedCount && i < 8; i++) {
                if (IsKeyPressed(keys[i])) {
                    selectedTowerType = unlockedTowers[i];
                    selectedTowerIdx = -1;
                }
            }
        }

        // --- Left click in 3D area ---
        if (!mouseInUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gs.phase != PHASE_OVER) {
            int lpi = netCtx.mode != NET_MODE_NONE ? netCtx.localPlayerIndex : 0;
            if (selectedTowerType >= 0 && canPlace) {
                int cost = (int)(TOWER_CONFIGS[selectedTowerType][0].cost * runMods.towerCostMultiplier);
                if (gs.playerGold[lpi] >= cost) {
                    if (netCtx.mode == NET_MODE_CLIENT) {
                        NetSendPlaceTower(&netCtx, (TowerType)selectedTowerType, mouseGrid);
                    } else {
                        gs.playerGold[lpi] -= cost;
                        gs.gold = gs.playerGold[0];
                        TowerPlace(towers, MAX_TOWERS, (TowerType)selectedTowerType, mouseGrid,
                                  (uint8_t)lpi, &gs, &map);
                        map.tiles[mouseGrid.z][mouseGrid.x] = TILE_TOWER;
                        MapBuildMesh(&gameMapMesh, &map, ps1Shader);
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
                MapBuildMesh(&menuMapMesh, &menuMap, ps1Shader);
                menuCamCtrl.yaw = 0.0f;
                currentScene = SCENE_MENU;
                break; // exit SCENE_GAME case
            }
        }

        // --- Update game systems (host and single-player only) ---
        if (netCtx.mode != NET_MODE_CLIENT &&
            gs.phase != PHASE_OVER && gs.phase != PHASE_PAUSED) {
            GameUpdateWave(&gs, enemies, MAX_ENEMIES, &map, &runMods, dt);
            EnemiesUpdate(enemies, MAX_ENEMIES, &map, &gs, dt);
            TowersUpdate(towers, MAX_TOWERS, enemies, MAX_ENEMIES, projectiles, MAX_PROJECTILES, &gs, &map, &runMods, dt);
            ProjectilesUpdate(projectiles, MAX_PROJECTILES, enemies, MAX_ENEMIES, &gs, &runMods, dt);
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
                MapBuildMesh(&gameMapMesh, &map, ps1Shader);
            }
        }

        // =========================
        // DRAW — 3D to low-res render target
        // =========================
        BeginTextureMode(renderTarget);
        ClearBackground((Color){ 30, 30, 35, 255 });

        BeginMode3D(camera);
            DrawSkybox(camera);
            DrawWater(totalTime);

            MapDrawMesh(&gameMapMesh);

            BeginShaderMode(ps1Shader);
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
                    DrawCubeV(ghostPos, (Vector3){ 0.7f, 0.7f, 0.7f }, ghostCol);

                    // Range preview
                    if (canPlace) {
                        float range = TOWER_CONFIGS[selectedTowerType][0].range;
                        Vector3 rangeCenter = MapGridToWorldElevated(&map, mouseGrid);
                        DrawRangeCircle(rangeCenter, range, (Color){ 255, 255, 255, 80 });
                    }
                }

                TowersDraw(towers, MAX_TOWERS, gs.playerCount, enemies, MAX_ENEMIES);

                // Range indicator for selected tower
                if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
                    const Tower *st = &towers[selectedTowerIdx];
                    float tElevY = MapGetElevationY(&map, st->gridPos.x, st->gridPos.z);
                    float range = TOWER_CONFIGS[st->type][st->level].range;
                    Vector3 rc = st->worldPos;
                    rc.y = tElevY;
                    DrawRangeCircle(rc, range, (Color){ 255, 255, 100, 150 });
                }

                EnemiesDraw(enemies, MAX_ENEMIES, sphereModel);
                ProjectilesDraw(projectiles, MAX_PROJECTILES, sphereModel);
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
        if (gs.endlessActive)
            DrawText(TextFormat("Wave: %d (Endless +%d)", gs.currentWave + 1, gs.endlessWave), 330, 7, 20, (Color){180,100,255,255});
        else
            DrawText(TextFormat("Wave: %d/%d", gs.currentWave + 1, MAX_WAVES), 330, 7, 20, WHITE);
        {
            const DifficultyConfig *hdc = &DIFFICULTY_CONFIGS[gs.difficulty];
            DrawText(hdc->name, 510, 7, 20, hdc->color);
        }
        // Active perk indicator
        if (runMods.activePerk >= 0 && runMods.activePerk < PERK_COUNT) {
            DrawText(PERK_CONFIGS[runMods.activePerk].name, 640, 7, 16, (Color){200,180,100,255});
        }
        DrawFPS(screenW - 90, 7);

        // --- Wave countdown ---
        if (gs.phase == PHASE_WAVE_COUNTDOWN) {
            const char *countText = TextFormat("Next wave in %.1f", gs.waveCountdown);
            int tw = MeasureText(countText, 28);
            DrawText(countText, (screenW - tw) / 2, 80, 28, YELLOW);
        }

        // --- Ability HUD (top-right) ---
        {
            const char *abilityNames[] = { "Airstrike [Q]", "Gold Rush [E]", "Fortify [R]" };
            int abX = screenW - 160;
            int abY = 40;
            for (int a = 0; a < ABILITY_COUNT; a++) {
                if (!runMods.abilityUnlocked[a]) continue;
                bool onCooldown = runMods.abilityTimer[a] > 0.0f;
                Color abBg = onCooldown ? (Color){40,30,30,200} : (Color){30,40,50,200};
                DrawRectangle(abX, abY, 150, 28, abBg);
                DrawRectangleLines(abX, abY, 150, 28, (Color){80,80,100,200});
                Color abTxt = onCooldown ? (Color){100,80,80,255} : WHITE;
                DrawText(abilityNames[a], abX + 5, abY + 6, 14, abTxt);
                if (onCooldown) {
                    DrawText(TextFormat("%.0fs", runMods.abilityTimer[a]),
                             abX + 120, abY + 6, 14, (Color){180,80,80,255});
                } else if (a == ABILITY_GOLD_RUSH && runMods.goldRushActive) {
                    DrawText("ACTIVE", abX + 100, abY + 6, 12, GOLD);
                }
                abY += 32;
            }
        }

        // --- Bottom bar (tower buttons — only unlocked) ---
        DrawRectangle(0, screenH - BOTTOM_BAR_HEIGHT, screenW, BOTTOM_BAR_HEIGHT, (Color){ 20, 20, 25, 220 });
        for (int ui = 0; ui < unlockedCount; ui++) {
            int i = unlockedTowers[ui];
            int bx = BTN_MARGIN + ui * (BTN_WIDTH + BTN_MARGIN);
            int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
            int cost = (int)(TOWER_CONFIGS[i][0].cost * runMods.towerCostMultiplier);
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
            DrawText(TextFormat("[%d]", ui + 1), bx + BTN_WIDTH - 28, by + 28, 12, (Color){120,120,120,255});
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

            // Elevation range bonus
            float tElevY = MapGetElevationY(&map, st->gridPos.x, st->gridPos.z);
            if (tElevY > 0.0f)
                DrawText(TextFormat("Elev:   +%.1f rng", tElevY), px + 8, py + 84, 14, (Color){100,200,255,255});

            int extraY = (tElevY > 0.0f) ? 102 : 84;
            if (cfg->aoeRadius > 0.0f)
                DrawText(TextFormat("AoE:    %.1f", cfg->aoeRadius), px + 8, py + extraY, 14, ORANGE);
            if (cfg->slowFactor < 1.0f)
                DrawText(TextFormat("Slow:   %.0f%%", (1.0f - cfg->slowFactor) * 100.0f), px + 8, py + extraY, 14, PURPLE);

            // Upgrade button
            if (st->level < TOWER_MAX_LEVEL - 1) {
                int upgCost = (int)(TOWER_CONFIGS[st->type][st->level + 1].cost * runMods.upgradeCostMultiplier);
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

            // Settings button
            int pSetY = resumeY + pBtnH + 12;
            Rectangle pSetBtn = { (float)pBtnX, (float)pSetY, (float)pBtnW, (float)pBtnH };
            bool pSetHover = CheckCollisionPointRec(mouse, pSetBtn);
            Color pSetBg = pSetHover ? (Color){ 70, 55, 110, 255 } : (Color){ 48, 38, 78, 255 };
            DrawRectangleRec(pSetBtn, pSetBg);
            DrawRectangleLinesEx(pSetBtn, 2, (Color){ 140, 110, 200, 200 });
            const char *pSetText = "Settings";
            int pSetTextW = MeasureText(pSetText, 24);
            DrawText(pSetText, pBtnX + (pBtnW - pSetTextW) / 2, pSetY + 11, 24, WHITE);

            // Main Menu button
            int pmMenuY = pSetY + pBtnH + 12;
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

            // Settings overlay on top of pause menu
            SettingsDraw(&settingsState, screenW, screenH);
        }

        // --- Chat overlay ---
        if (netCtx.mode != NET_MODE_NONE) {
            ChatDraw(&chatState, screenW, screenH);
        }

        // --- Game Over / Victory Screen ---
        if (gs.phase == PHASE_OVER || gs.phase == PHASE_VICTORY) {
            DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });
            bool won = (gs.phase == PHASE_VICTORY);
            const char *msg = won ? "VICTORY!" : "GAME OVER";
            Color msgCol = won ? GOLD : RED;
            int msgW = MeasureText(msg, 60);
            DrawText(msg, (screenW - msgW) / 2, screenH / 2 - 80, 60, msgCol);

            int totalWaves = gs.endlessActive ? gs.currentWave : gs.currentWave;
            const char *sub = won ?
                TextFormat("All %d waves cleared!", gs.endlessActive ? totalWaves : MAX_WAVES) :
                TextFormat("Survived to wave %d/%d", gs.currentWave + 1,
                          gs.endlessActive ? gs.currentWave + 1 : MAX_WAVES);
            int subW = MeasureText(sub, 24);
            DrawText(sub, (screenW - subW) / 2, screenH / 2 - 10, 24, WHITE);

            // Crystal earnings
            int crystalsEarned = CrystalsCalculate(gs.currentWave, gs.lives, gs.maxLives,
                                                    (int)gs.difficulty, won);
            DrawText(TextFormat("Crystals earned: +%d", crystalsEarned),
                     (screenW - MeasureText(TextFormat("Crystals earned: +%d", crystalsEarned), 22)) / 2,
                     screenH / 2 + 20, 22, (Color){180,100,255,255});

            const char *hint = "Press R to restart | ESC for Main Menu";
            int hintW = MeasureText(hint, 18);
            DrawText(hint, (screenW - hintW) / 2, screenH / 2 + 55, 18, LIGHTGRAY);

            // Endless mode button (only on first victory, not already endless)
            int nextBtnY = screenH / 2 + 80;
            if (won && !gs.endlessActive) {
                int eBtnW = 220, eBtnH = 40;
                int eBtnX = (screenW - eBtnW) / 2;
                Rectangle endlessBtn = { (float)eBtnX, (float)nextBtnY, (float)eBtnW, (float)eBtnH };
                bool endlessHover = CheckCollisionPointRec(mouse, endlessBtn);
                DrawRectangleRec(endlessBtn, endlessHover ? (Color){60,60,100,255} : (Color){40,40,70,255});
                DrawRectangleLinesEx(endlessBtn, 2, (Color){100,100,200,200});
                const char *endlessText = "Continue (Endless)";
                int endlessTextW = MeasureText(endlessText, 20);
                DrawText(endlessText, eBtnX + (eBtnW - endlessTextW) / 2, nextBtnY + 10, 20, WHITE);
                nextBtnY += eBtnH + 10;

                if (endlessHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    gs.endlessActive = true;
                    gs.endlessWave = 0;
                    gs.phase = PHASE_WAVE_COUNTDOWN;
                    gs.waveCountdown = 5.0f;
                }
            }

            // Main Menu button
            int mbW = 180, mbH = 40;
            int mbX = (screenW - mbW) / 2;
            int mbY = nextBtnY;
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

        // --- Pause menu: settings update ---
        if (gs.phase == PHASE_PAUSED || localPaused) {
            int pauseSettingsResult = SettingsUpdate(&settingsState);
            if (pauseSettingsResult == 1) {
                settings = settingsState.pending;
                SettingsSave(&settings, "settings.cfg");

                UnloadRenderTexture(renderTarget);
                rtW = screenW / settings.ps1Downscale;
                rtH = screenH / settings.ps1Downscale;
                renderTarget = LoadRenderTexture(rtW, rtH);
                SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);
                resolution[0] = (float)rtW;
                resolution[1] = (float)rtH;
                SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);

                if (settings.vsync) SetWindowState(FLAG_VSYNC_HINT);
                else ClearWindowState(FLAG_VSYNC_HINT);

                if (settings.fullscreen == FULLSCREEN_WINDOWED) {
                    if (IsWindowFullscreen()) ToggleFullscreen();
                    else if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                    SetWindowSize(RESOLUTION_PRESETS[settings.resolutionIdx].width,
                                  RESOLUTION_PRESETS[settings.resolutionIdx].height);
                } else if (settings.fullscreen == FULLSCREEN_BORDERLESS) {
                    if (IsWindowFullscreen()) ToggleFullscreen();
                    if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                } else if (settings.fullscreen == FULLSCREEN_EXCLUSIVE) {
                    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                    if (!IsWindowFullscreen()) ToggleFullscreen();
                }

                camCtrl.panSpeed = settings.camPanSpeed;
                camCtrl.rotSpeed = settings.camRotSpeed;
                camCtrl.zoomSpeed = settings.camZoomSpeed;
            }
        }

        // --- Pause menu button clicks ---
        if ((gs.phase == PHASE_PAUSED || localPaused) && !settingsState.open) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int pBtnW = 200, pBtnH = 45;
                int pBtnX = (screenW - pBtnW) / 2;
                int resumeY = screenH / 2 - 20;
                int pSetY = resumeY + pBtnH + 12;
                int pmMenuY = pSetY + pBtnH + 12;
                int pQuitY = pmMenuY + pBtnH + 12;

                Rectangle resumeBtn = { (float)pBtnX, (float)resumeY, (float)pBtnW, (float)pBtnH };
                Rectangle pSetBtn = { (float)pBtnX, (float)pSetY, (float)pBtnW, (float)pBtnH };
                Rectangle pmMenuBtn = { (float)pBtnX, (float)pmMenuY, (float)pBtnW, (float)pBtnH };
                Rectangle pQuitBtn = { (float)pBtnX, (float)pQuitY, (float)pBtnW, (float)pBtnH };

                if (CheckCollisionPointRec(mouse, resumeBtn)) {
                    if (localPaused) localPaused = false;
                    else gs.phase = phaseBeforePause;
                } else if (CheckCollisionPointRec(mouse, pSetBtn)) {
                    SettingsOpen(&settingsState, &settings);
                } else if (CheckCollisionPointRec(mouse, pmMenuBtn)) {
                    localPaused = false;
                    if (netCtx.mode != NET_MODE_NONE) {
                        NetContextDestroy(&netCtx);
                        NetShutdown();
                        NetContextInit(&netCtx);
                    }
                    MapInit(&menuMap);
                    MapBuildMesh(&menuMapMesh, &menuMap, ps1Shader);
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

        // --- Save crystals on game end (once) ---
        if ((gs.phase == PHASE_OVER || gs.phase == PHASE_VICTORY) && !crystalsSaved) {
            bool won = (gs.phase == PHASE_VICTORY);
            int earned = CrystalsCalculate(gs.currentWave, gs.lives, gs.maxLives,
                                           (int)gs.difficulty, won);
            profile.crystals += earned;
            profile.totalRuns++;
            if (won) profile.totalWins++;
            PlayerProfileSave(&profile, "profile.fdsave");
            crystalsSaved = true;
        }

        // --- Restart ---
        if ((gs.phase == PHASE_OVER || gs.phase == PHASE_VICTORY) && IsKeyPressed(KEY_R)) {
            crystalsSaved = false;
            // Reload current map (use name to find path)
            bool reloaded = false;
            for (int i = 0; i < mapRegistry.count; i++) {
                if (strcmp(mapRegistry.names[i], map.name) == 0) {
                    reloaded = MapLoad(&map, mapRegistry.paths[i]);
                    break;
                }
            }
            if (!reloaded) MapInit(&map);
            MapBuildMesh(&gameMapMesh, &map, ps1Shader);
            RunModifiersInit(&runMods, &profile);
            endlessState = (EndlessState){0};
            GameStateInit(&gs, selectedDifficulty, &runMods);
            memset(enemies, 0, sizeof(enemies));
            memset(towers, 0, sizeof(towers));
            memset(projectiles, 0, sizeof(projectiles));
            selectedTowerType = -1;
            selectedTowerIdx = -1;
        }

        // --- Back to menu ---
        if (gs.phase == PHASE_OVER || gs.phase == PHASE_VICTORY) {
            bool goMenu = IsKeyPressed(KEY_ESCAPE);
            // Approximate menu button position (dynamic based on endless)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int mbW = 180, mbH = 40;
                int mbX = (screenW - mbW) / 2;
                int approxY = screenH / 2 + 80;
                if (gs.phase == PHASE_VICTORY && !gs.endlessActive) approxY += 50;
                Rectangle menuBtn = { (float)mbX, (float)approxY, (float)mbW, (float)mbH };
                if (CheckCollisionPointRec(mouse, menuBtn))
                    goMenu = true;
            }
            if (goMenu) {
                crystalsSaved = false;
                if (netCtx.mode != NET_MODE_NONE) {
                    NetContextDestroy(&netCtx);
                    NetShutdown();
                    NetContextInit(&netCtx);
                }
                MapInit(&menuMap);
                MapBuildMesh(&menuMapMesh, &menuMap, ps1Shader);
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
    MapFreeMesh(&menuMapMesh);
    MapFreeMesh(&gameMapMesh);
    if (skyboxReady) UnloadMesh(skyboxMesh);
    if (waterReady) UnloadMesh(waterMesh);
    UnloadShader(waterShader);
    UnloadModel(sphereModel);
    UnloadRenderTexture(renderTarget);
    UnloadShader(ps1Shader);
    CloseWindow();
    return 0;
}
