#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "app.h"
#include "scene.h"
#include "ui.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- PS1 Graphics Constants ---

#define PS1_DOWNSCALE 3
#define PS1_JITTER_STRENGTH 1.0f
#define PS1_COLOR_BANDS 24.0f
#define BLOB_SHADOW_SEGMENTS 12

// --- Global Camera Controllers ---

CameraController g_camCtrl;
CameraController g_menuCamCtrl;

void CameraControllerInit(CameraController *cc)
{
    cc->target = (Vector3){ MAP_WIDTH * TILE_SIZE * 0.5f, 0.0f, MAP_HEIGHT * TILE_SIZE * 0.5f };
    cc->distance = 18.0f;
    cc->yaw = 0.0f;
    cc->pitch = 55.0f;
    cc->panSpeed = 12.0f;
    cc->rotSpeed = 0.3f;
    cc->zoomSpeed = 2.0f;
}

void CameraControllerUpdate(CameraController *cc, Camera3D *cam, float dt)
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

bool GetMouseGroundPos(Camera3D camera, const Map *map, Vector3 *outPos)
{
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
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

void DrawRangeCircle(Vector3 center, float radius, Color color)
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

// --- Skybox ---

static Mesh skyboxMesh = {0};
static Material skyboxMaterial = {0};
static bool skyboxReady = false;

void BuildSkyboxMesh(void)
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

void DrawSkybox(Camera3D camera)
{
    if (!skyboxReady) return;

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    Matrix transform = MatrixTranslate(camera.position.x, camera.position.y, camera.position.z);
    DrawMesh(skyboxMesh, skyboxMaterial, transform);

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

// --- Water ---

#define WATER_Y -0.3f

static Mesh waterMesh = {0};
static Material waterMaterial = {0};
static bool waterReady = false;

void BuildWaterMesh(Shader shader)
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
    waterMaterial.shader = shader;
    waterReady = true;
}

void DrawWater(Shader shader, int locTime, float totalTime)
{
    if (!waterReady) return;
    SetShaderValue(shader, locTime, &totalTime, SHADER_UNIFORM_FLOAT);
    DrawMesh(waterMesh, waterMaterial, MatrixIdentity());
}

// --- Blob Shadow ---

static float blobShadowCos[BLOB_SHADOW_SEGMENTS + 1];
static float blobShadowSin[BLOB_SHADOW_SEGMENTS + 1];

void InitBlobShadowTable(void)
{
    for (int i = 0; i <= BLOB_SHADOW_SEGMENTS; i++) {
        float a = (float)i / BLOB_SHADOW_SEGMENTS * 2.0f * PI;
        blobShadowCos[i] = cosf(a);
        blobShadowSin[i] = sinf(a);
    }
}

void DrawBlobShadowsBatched(const BlobShadowEntry *entries, int count)
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

// --- Chat Callback ---

ChatState *g_chatStatePtr = NULL;

void OnNetChatReceived(uint8_t playerIndex, const char *username, const char *message)
{
    if (g_chatStatePtr)
        ChatAddMessage(g_chatStatePtr, playerIndex, username, message);
}

// --- Game Scene Reset Helper ---

void GameSceneReset(AppContext *app)
{
    app->endlessState = (EndlessState){0};
    app->crystalsSaved = false;
    GameStateInit(&app->gs, app->selectedDifficulty, &app->runMods);
    memset(app->enemies, 0, sizeof(app->enemies));
    memset(app->towers, 0, sizeof(app->towers));
    memset(app->projectiles, 0, sizeof(app->projectiles));
    CameraControllerInit(&g_camCtrl);
    CameraControllerUpdate(&g_camCtrl, &app->camera, 0.0f);
    app->selectedTowerType = -1;
    app->selectedTowerIdx = -1;
}

// --- Main ---

int main(void)
{
    Settings settings;
    SettingsDefault(&settings);
    SettingsLoad(&settings, "settings.cfg");

    int initFlags = FLAG_WINDOW_RESIZABLE;
    if (settings.vsync) initFlags |= FLAG_VSYNC_HINT;
    SetConfigFlags(initFlags);

    int initW = RESOLUTION_PRESETS[settings.resolutionIdx].width;
    int initH = RESOLUTION_PRESETS[settings.resolutionIdx].height;
    InitWindow(initW, initH, "Formal Defense");
    SetExitKey(0);

    if (settings.fullscreen == FULLSCREEN_BORDERLESS) ToggleBorderlessWindowed();
    else if (settings.fullscreen == FULLSCREEN_EXCLUSIVE) ToggleFullscreen();

    // --- PS1 Shader ---
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
    Shader wShader = LoadShader("shaders/water.vs", "shaders/water.fs");
    wShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(wShader, "matModel");

    int wLocResolution = GetShaderLocation(wShader, "resolution");
    int wLocJitter = GetShaderLocation(wShader, "jitterStrength");
    int wLocLightDir = GetShaderLocation(wShader, "lightDir");
    int wLocLightColor = GetShaderLocation(wShader, "lightColor");
    int wLocAmbientColor = GetShaderLocation(wShader, "ambientColor");
    int wLocColorBands = GetShaderLocation(wShader, "colorBands");

    SetShaderValue(wShader, wLocJitter, &jitterStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(wShader, wLocLightDir, lightDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(wShader, wLocLightColor, lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(wShader, wLocAmbientColor, ambientColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(wShader, wLocColorBands, &colorBands, SHADER_UNIFORM_FLOAT);

    int cachedScreenW = GetScreenWidth();
    int cachedScreenH = GetScreenHeight();
    int rtW = cachedScreenW / settings.ps1Downscale;
    int rtH = cachedScreenH / settings.ps1Downscale;
    RenderTexture2D renderTarget = LoadRenderTexture(rtW, rtH);
    SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);

    float resolution[2] = { (float)rtW, (float)rtH };
    SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(wShader, wLocResolution, resolution, SHADER_UNIFORM_VEC2);

    // --- Pre-baked meshes ---
    BuildSkyboxMesh();
    BuildWaterMesh(wShader);
    InitBlobShadowTable();

    Mesh sphereMesh = GenMeshSphere(1.0f, 8, 8);
    Model sphereModel = LoadModelFromMesh(sphereMesh);
    sphereModel.materials[0].shader = ps1Shader;

    // --- Initialize AppContext ---
    AppContext app = {0};
    app.ps1Shader = ps1Shader;
    app.waterShader = wShader;
    app.renderTarget = renderTarget;
    app.rtW = rtW;
    app.rtH = rtH;
    app.locResolution = locResolution;
    app.wLocResolution = wLocResolution;
    app.sphereModel = sphereModel;
    app.settings = settings;
    app.selectedDifficulty = DIFFICULTY_NORMAL;
    app.selectedTowerType = -1;
    app.selectedTowerIdx = -1;

    // Load player profile
    PlayerProfileLoad(&app.profile, "profile.fdsave");

    // Map registry
    MapRegistryScan(&app.mapRegistry, "maps");

    // Menu map
    if (app.mapRegistry.count > 0) {
        int menuMapIdx = GetRandomValue(0, app.mapRegistry.count - 1);
        if (!MapLoad(&app.menuMap, app.mapRegistry.paths[menuMapIdx]))
            MapInit(&app.menuMap);
    } else {
        MapInit(&app.menuMap);
    }
    MapBuildMesh(&app.menuMapMesh, &app.menuMap, ps1Shader);

    CameraControllerInit(&g_menuCamCtrl);
    g_menuCamCtrl.distance = 22.0f;
    CameraControllerUpdate(&g_menuCamCtrl, &app.menuCamera, 0.0f);

    MapInit(&app.map);
    MapBuildMesh(&app.gameMapMesh, &app.map, ps1Shader);

    RunModifiersInit(&app.runMods, &app.profile);
    GameStateInit(&app.gs, DIFFICULTY_NORMAL, &app.runMods);

    CameraControllerInit(&g_camCtrl);
    g_camCtrl.panSpeed = settings.camPanSpeed;
    g_camCtrl.rotSpeed = settings.camRotSpeed;
    g_camCtrl.zoomSpeed = settings.camZoomSpeed;
    CameraControllerUpdate(&g_camCtrl, &app.camera, 0.0f);

    // Multiplayer
    NetContextInit(&app.netCtx);
    LobbyStateInit(&app.lobbyState);
    ChatStateInit(&app.chatState);
    g_chatStatePtr = &app.chatState;
    g_netChatCallback = OnNetChatReceived;

    // --- Scene Manager ---
    SceneManager sm;
    SceneManagerInit(&sm, &app);
    app.sceneManager = &sm;

    SceneManagerRegister(&sm, SCENE_MENU, SceneMenuCreate());
    SceneManagerRegister(&sm, SCENE_MAP_SELECT, SceneMapSelectCreate());
    SceneManagerRegister(&sm, SCENE_DIFFICULTY_SELECT, SceneDifficultyCreate());
    SceneManagerRegister(&sm, SCENE_SHOP, SceneShopCreate());
    SceneManagerRegister(&sm, SCENE_PERK_SELECT, ScenePerkSelectCreate());
    SceneManagerRegister(&sm, SCENE_LOBBY, SceneLobbyCreate());
    SceneManagerRegister(&sm, SCENE_GAME, SceneGameCreate());

    // --- Main Loop ---
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        app.totalTime += dt;
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        if (IsKeyPressed(KEY_F11)) {
            ToggleBorderlessWindowed();
            app.settings.fullscreen = (app.settings.fullscreen == FULLSCREEN_BORDERLESS)
                ? FULLSCREEN_WINDOWED : FULLSCREEN_BORDERLESS;
        }

        // Recreate render target on window resize
        if (screenW != cachedScreenW || screenH != cachedScreenH) {
            cachedScreenW = screenW;
            cachedScreenH = screenH;
            UnloadRenderTexture(app.renderTarget);
            app.rtW = screenW / app.settings.ps1Downscale;
            app.rtH = screenH / app.settings.ps1Downscale;
            app.renderTarget = LoadRenderTexture(app.rtW, app.rtH);
            SetTextureFilter(app.renderTarget.texture, TEXTURE_FILTER_POINT);
            resolution[0] = (float)app.rtW;
            resolution[1] = (float)app.rtH;
            SetShaderValue(ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);
            SetShaderValue(wShader, wLocResolution, resolution, SHADER_UNIFORM_VEC2);
        }

        SceneManagerTick(&sm, dt);
    }

    // --- Cleanup ---
    if (app.netCtx.mode != NET_MODE_NONE) {
        NetContextDestroy(&app.netCtx);
        NetShutdown();
    }
    MapFreeMesh(&app.menuMapMesh);
    MapFreeMesh(&app.gameMapMesh);
    if (skyboxReady) UnloadMesh(skyboxMesh);
    if (waterReady) UnloadMesh(waterMesh);
    UnloadShader(wShader);
    UnloadModel(sphereModel);
    UnloadRenderTexture(app.renderTarget);
    UnloadShader(ps1Shader);
    CloseWindow();
    return 0;
}
