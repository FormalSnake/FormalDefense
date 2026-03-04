#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "map.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

// --- PS1 Graphics Constants ---

#define PS1_DOWNSCALE 3

// --- Editor Tab ---

typedef enum { TAB_2D, TAB_3D } EditorTab;

#define TAB_HEIGHT 30
#define TAB_WIDTH 100

// --- Editor Tool ---

typedef enum {
    TOOL_EMPTY,
    TOOL_OBSTACLE,
    TOOL_WAYPOINT_ADD,
    TOOL_WAYPOINT_DEL,
    TOOL_ELEVATE_UP,
    TOOL_ELEVATE_DOWN,
    TOOL_COUNT,
} EditorTool;

static const char *TOOL_NAMES[] = {
    "Empty", "Obstacle", "Waypt+", "WayptX", "Elev+", "Elev-",
};

// --- Camera Controller (duplicated from main.c for standalone editor) ---

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

// --- Skybox (duplicated from main.c for standalone editor) ---

static Mesh skyboxMesh = {0};
static Material skyboxMaterial = {0};
static bool skyboxReady = false;

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

// --- Editor State ---

typedef struct {
    Map map;
    EditorTool tool;
    bool dirty;
    char savePath[256];
    bool editingName;

    char toastMsg[128];
    float toastTimer;
    Color toastColor;

    bool confirmOverwrite;

    // 3D preview
    EditorTab tab;
    CameraController camCtrl;
    Camera3D camera;
    Shader ps1Shader;
    MapMesh mapMesh;
    bool meshDirty;
    RenderTexture2D renderTarget;
    int rtW, rtH;
} EditorState;

static void EditorUpdateSavePath(EditorState *ed)
{
    if (ed->map.name[0] == '\0') {
        ed->savePath[0] = '\0';
        return;
    }

    char slug[128];
    int j = 0;
    for (int i = 0; ed->map.name[i] && j < (int)sizeof(slug) - 1; i++) {
        char ch = ed->map.name[i];
        if (ch == ' ' || ch == '_') {
            if (j > 0 && slug[j - 1] != '-')
                slug[j++] = '-';
        } else if (isalnum((unsigned char)ch) || ch == '-') {
            slug[j++] = (char)tolower((unsigned char)ch);
        }
    }
    // Strip trailing hyphen
    while (j > 0 && slug[j - 1] == '-') j--;
    slug[j] = '\0';

    if (j == 0) {
        ed->savePath[0] = '\0';
        return;
    }

    snprintf(ed->savePath, sizeof(ed->savePath), "maps/%s.fdmap", slug);
}

static void EditorShowToast(EditorState *ed, const char *msg, Color color)
{
    strncpy(ed->toastMsg, msg, sizeof(ed->toastMsg) - 1);
    ed->toastMsg[sizeof(ed->toastMsg) - 1] = '\0';
    ed->toastTimer = 3.0f;
    ed->toastColor = color;
}

static void EditorInit(EditorState *ed)
{
    memset(ed, 0, sizeof(*ed));
    MapInit(&ed->map);
    ed->tool = TOOL_EMPTY;
    ed->tab = TAB_2D;
    ed->meshDirty = true;
    EditorUpdateSavePath(ed);
}

// --- Text Input ---

static void HandleTextInput(char *buf, int *len, int maxLen)
{
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (*len < maxLen && ch >= 32 && ch < 127) {
            buf[*len] = (char)ch;
            (*len)++;
            buf[*len] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && *len > 0) {
        (*len)--;
        buf[*len] = '\0';
    }
}

// --- Draw ---

#define GRID_OFFSET_X 20
#define GRID_OFFSET_Y (40 + TAB_HEIGHT)
#define CELL_SIZE 36
#define TOOL_PANEL_X (GRID_OFFSET_X + MAP_WIDTH * CELL_SIZE + 20)
#define TOOL_PANEL_W 120

static Color TileColor(TileType t)
{
    switch (t) {
        case TILE_PATH:     return (Color){ 210, 180, 140, 255 };
        case TILE_SPAWN:    return (Color){ 255, 200, 0, 255 };
        case TILE_BASE:     return (Color){ 200, 50, 50, 255 };
        case TILE_OBSTACLE: return (Color){ 130, 130, 130, 255 };
        case TILE_TOWER:    return (Color){ 100, 180, 100, 255 };
        default:            return (Color){ 100, 160, 80, 255 };
    }
}

// --- Tab Bar Drawing & Input ---

static void DrawTabBar(EditorState *ed, Vector2 mouse)
{
    const char *tabNames[] = { "2D Edit", "3D Preview" };
    for (int i = 0; i < 2; i++) {
        Rectangle tabRect = { (float)(10 + i * (TAB_WIDTH + 4)), 5.0f, (float)TAB_WIDTH, (float)TAB_HEIGHT };
        bool hover = CheckCollisionPointRec(mouse, tabRect);
        bool active = ((int)ed->tab == i);

        Color bg = active ? (Color){ 60, 80, 120, 255 } :
                   hover  ? (Color){ 50, 60, 70, 255 }  :
                            (Color){ 35, 40, 50, 255 };
        DrawRectangleRec(tabRect, bg);
        DrawRectangleLinesEx(tabRect, 1, active ? (Color){ 100, 140, 200, 255 } : (Color){ 60, 60, 60, 255 });

        int tw = MeasureText(tabNames[i], 16);
        DrawText(tabNames[i], (int)(tabRect.x + (TAB_WIDTH - tw) / 2), (int)(tabRect.y + 7), 16,
                 active ? WHITE : LIGHTGRAY);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            ed->tab = (EditorTab)i;
    }
}

int main(void)
{
    int winW = GRID_OFFSET_X + MAP_WIDTH * CELL_SIZE + 20 + TOOL_PANEL_W + 20;
    int winH = GRID_OFFSET_Y + MAP_HEIGHT * CELL_SIZE + 80;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(winW, winH, "Formal Defense - Map Editor");
    SetTargetFPS(60);

    EditorState ed;
    EditorInit(&ed);

    // Try loading default map
    if (MapLoad(&ed.map, "maps/default.fdmap"))
        EditorUpdateSavePath(&ed);

    // --- 3D Preview Resources ---
    ed.ps1Shader = LoadShader("shaders/ps1.vs", "shaders/ps1.fs");
    ed.ps1Shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(ed.ps1Shader, "matModel");

    int locResolution = GetShaderLocation(ed.ps1Shader, "resolution");
    int locJitter = GetShaderLocation(ed.ps1Shader, "jitterStrength");
    int locLightDir = GetShaderLocation(ed.ps1Shader, "lightDir");
    int locLightColor = GetShaderLocation(ed.ps1Shader, "lightColor");
    int locAmbientColor = GetShaderLocation(ed.ps1Shader, "ambientColor");
    int locColorBands = GetShaderLocation(ed.ps1Shader, "colorBands");

    float jitterStrength = 1.0f;
    SetShaderValue(ed.ps1Shader, locJitter, &jitterStrength, SHADER_UNIFORM_FLOAT);

    float lightDir[3] = { 0.4f, -0.7f, 0.3f };
    SetShaderValue(ed.ps1Shader, locLightDir, lightDir, SHADER_UNIFORM_VEC3);

    float lightColor[3] = { 0.7f, 0.7f, 0.65f };
    SetShaderValue(ed.ps1Shader, locLightColor, lightColor, SHADER_UNIFORM_VEC3);

    float ambientColor[3] = { 0.3f, 0.3f, 0.35f };
    SetShaderValue(ed.ps1Shader, locAmbientColor, ambientColor, SHADER_UNIFORM_VEC3);

    float colorBands = 24.0f;
    SetShaderValue(ed.ps1Shader, locColorBands, &colorBands, SHADER_UNIFORM_FLOAT);

    ed.rtW = GetScreenWidth() / PS1_DOWNSCALE;
    ed.rtH = GetScreenHeight() / PS1_DOWNSCALE;
    ed.renderTarget = LoadRenderTexture(ed.rtW, ed.rtH);
    SetTextureFilter(ed.renderTarget.texture, TEXTURE_FILTER_POINT);

    float resolution[2] = { (float)ed.rtW, (float)ed.rtH };
    SetShaderValue(ed.ps1Shader, locResolution, resolution, SHADER_UNIFORM_VEC2);

    CameraControllerInit(&ed.camCtrl);
    BuildSkyboxMesh();

    while (!WindowShouldClose())
    {
        Vector2 mouse = GetMousePosition();
        float dt = GetFrameTime();

        // Update toast timer
        if (ed.toastTimer > 0.0f)
            ed.toastTimer -= dt;

        // --- Tab bar input (both tabs) ---
        // (Tab switching handled in DrawTabBar during draw phase)

        if (ed.tab == TAB_2D) {
            // --- 2D Tab: Text input ---
            if (ed.editingName) {
                int nameLen = (int)strlen(ed.map.name);
                HandleTextInput(ed.map.name, &nameLen, MAX_MAP_NAME - 1);
                EditorUpdateSavePath(&ed);
                ed.confirmOverwrite = false;
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB))
                    ed.editingName = false;
            }

            // --- Grid interaction ---
            int gridMouseX = ((int)mouse.x - GRID_OFFSET_X) / CELL_SIZE;
            int gridMouseZ = ((int)mouse.y - GRID_OFFSET_Y) / CELL_SIZE;
            bool inGrid = gridMouseX >= 0 && gridMouseX < MAP_WIDTH &&
                          gridMouseZ >= 0 && gridMouseZ < MAP_HEIGHT &&
                          mouse.x >= GRID_OFFSET_X && mouse.y >= GRID_OFFSET_Y;

            if (inGrid && !ed.editingName) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    switch (ed.tool) {
                    case TOOL_EMPTY:
                        if (ed.map.tiles[gridMouseZ][gridMouseX] == TILE_OBSTACLE) {
                            ed.map.tiles[gridMouseZ][gridMouseX] = TILE_EMPTY;
                            ed.dirty = true;
                            ed.meshDirty = true;
                        }
                        break;
                    case TOOL_OBSTACLE:
                        if (ed.map.tiles[gridMouseZ][gridMouseX] == TILE_EMPTY) {
                            ed.map.tiles[gridMouseZ][gridMouseX] = TILE_OBSTACLE;
                            ed.dirty = true;
                            ed.meshDirty = true;
                        }
                        break;
                    case TOOL_ELEVATE_UP:
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            if (ed.map.elevation[gridMouseZ][gridMouseX] < MAX_ELEVATION) {
                                ed.map.elevation[gridMouseZ][gridMouseX]++;
                                ed.dirty = true;
                                ed.meshDirty = true;
                            }
                        }
                        break;
                    case TOOL_ELEVATE_DOWN:
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            if (ed.map.elevation[gridMouseZ][gridMouseX] > 0) {
                                ed.map.elevation[gridMouseZ][gridMouseX]--;
                                ed.dirty = true;
                                ed.meshDirty = true;
                            }
                        }
                        break;
                    case TOOL_WAYPOINT_ADD:
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                            ed.map.waypointCount < MAX_WAYPOINTS) {
                            ed.map.waypoints[ed.map.waypointCount].x = gridMouseX;
                            ed.map.waypoints[ed.map.waypointCount].z = gridMouseZ;
                            ed.map.waypointCount++;
                            for (int z = 0; z < MAP_HEIGHT; z++)
                                for (int x = 0; x < MAP_WIDTH; x++)
                                    if (ed.map.tiles[z][x] == TILE_PATH ||
                                        ed.map.tiles[z][x] == TILE_SPAWN ||
                                        ed.map.tiles[z][x] == TILE_BASE)
                                        ed.map.tiles[z][x] = TILE_EMPTY;
                            Map tmp;
                            memset(&tmp, 0, sizeof(tmp));
                            memcpy(tmp.waypoints, ed.map.waypoints, sizeof(tmp.waypoints));
                            tmp.waypointCount = ed.map.waypointCount;
                            for (int z = 0; z < MAP_HEIGHT; z++)
                                for (int x = 0; x < MAP_WIDTH; x++)
                                    if (ed.map.tiles[z][x] == TILE_OBSTACLE)
                                        tmp.tiles[z][x] = TILE_OBSTACLE;
                            strncpy(tmp.name, ed.map.name, MAX_MAP_NAME);
                            char buf[4096];
                            int len = MapSerialize(&tmp, buf, sizeof(buf));
                            if (len > 0) {
                                Map reloaded;
                                if (MapLoadFromBuffer(&reloaded, buf, len)) {
                                    for (int z = 0; z < MAP_HEIGHT; z++)
                                        for (int x = 0; x < MAP_WIDTH; x++)
                                            if (ed.map.tiles[z][x] == TILE_OBSTACLE &&
                                                reloaded.tiles[z][x] == TILE_EMPTY)
                                                reloaded.tiles[z][x] = TILE_OBSTACLE;
                                    memcpy(&ed.map, &reloaded, sizeof(Map));
                                }
                            }
                            ed.dirty = true;
                            ed.meshDirty = true;
                        }
                        break;
                    case TOOL_WAYPOINT_DEL:
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ed.map.waypointCount > 0) {
                            int nearest = -1;
                            float bestDist = 9999.0f;
                            for (int i = 0; i < ed.map.waypointCount; i++) {
                                float dx = (float)(ed.map.waypoints[i].x - gridMouseX);
                                float dz = (float)(ed.map.waypoints[i].z - gridMouseZ);
                                float d = dx * dx + dz * dz;
                                if (d < bestDist) { bestDist = d; nearest = i; }
                            }
                            if (nearest >= 0 && bestDist <= 2.0f) {
                                for (int i = nearest; i < ed.map.waypointCount - 1; i++)
                                    ed.map.waypoints[i] = ed.map.waypoints[i + 1];
                                ed.map.waypointCount--;
                                for (int z = 0; z < MAP_HEIGHT; z++)
                                    for (int x = 0; x < MAP_WIDTH; x++)
                                        if (ed.map.tiles[z][x] == TILE_PATH ||
                                            ed.map.tiles[z][x] == TILE_SPAWN ||
                                            ed.map.tiles[z][x] == TILE_BASE)
                                            ed.map.tiles[z][x] = TILE_EMPTY;
                                Map tmp2;
                                memset(&tmp2, 0, sizeof(tmp2));
                                memcpy(tmp2.waypoints, ed.map.waypoints, sizeof(tmp2.waypoints));
                                tmp2.waypointCount = ed.map.waypointCount;
                                strncpy(tmp2.name, ed.map.name, MAX_MAP_NAME);
                                for (int z = 0; z < MAP_HEIGHT; z++)
                                    for (int x = 0; x < MAP_WIDTH; x++)
                                        if (ed.map.tiles[z][x] == TILE_OBSTACLE)
                                            tmp2.tiles[z][x] = TILE_OBSTACLE;
                                char buf2[4096];
                                int len2 = MapSerialize(&tmp2, buf2, sizeof(buf2));
                                if (len2 > 0) {
                                    Map reloaded2;
                                    if (MapLoadFromBuffer(&reloaded2, buf2, len2)) {
                                        for (int z = 0; z < MAP_HEIGHT; z++)
                                            for (int x = 0; x < MAP_WIDTH; x++)
                                                if (ed.map.tiles[z][x] == TILE_OBSTACLE &&
                                                    reloaded2.tiles[z][x] == TILE_EMPTY)
                                                    reloaded2.tiles[z][x] = TILE_OBSTACLE;
                                        memcpy(&ed.map, &reloaded2, sizeof(Map));
                                    }
                                }
                                ed.dirty = true;
                                ed.meshDirty = true;
                            }
                        }
                        break;
                    default:
                        break;
                    }
                }

                // Right-click erases
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    if (ed.map.tiles[gridMouseZ][gridMouseX] == TILE_OBSTACLE) {
                        ed.map.tiles[gridMouseZ][gridMouseX] = TILE_EMPTY;
                        ed.dirty = true;
                        ed.meshDirty = true;
                    }
                }
            }

            // --- 2D Drawing ---
            BeginDrawing();
            ClearBackground((Color){ 30, 30, 35, 255 });

            // Tab bar
            DrawTabBar(&ed, mouse);

            // Title
            DrawText("Map Editor", 10, TAB_HEIGHT + 10, 24, WHITE);

            // Grid
            for (int z = 0; z < MAP_HEIGHT; z++) {
                for (int x = 0; x < MAP_WIDTH; x++) {
                    int px = GRID_OFFSET_X + x * CELL_SIZE;
                    int py = GRID_OFFSET_Y + z * CELL_SIZE;
                    Color c = TileColor(ed.map.tiles[z][x]);

                    int elev = ed.map.elevation[z][x];
                    if (elev > 0) {
                        int boost = elev * 15;
                        int r = c.r + boost; if (r > 255) r = 255;
                        int g = c.g + boost; if (g > 255) g = 255;
                        int b = c.b + boost; if (b > 255) b = 255;
                        c = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
                    }

                    DrawRectangle(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, c);
                    DrawRectangleLines(px, py, CELL_SIZE, CELL_SIZE, (Color){ 50, 50, 50, 150 });

                    if (elev > 0) {
                        const char *elevStr = TextFormat("%d", elev);
                        int tw = MeasureText(elevStr, 12);
                        DrawText(elevStr, px + (CELL_SIZE - tw) / 2, py + (CELL_SIZE - 12) / 2, 12,
                                 (Color){ 255, 255, 255, 200 });
                    }
                }
            }

            // Draw waypoints
            for (int i = 0; i < ed.map.waypointCount; i++) {
                int cx = GRID_OFFSET_X + ed.map.waypoints[i].x * CELL_SIZE + CELL_SIZE / 2;
                int cy = GRID_OFFSET_Y + ed.map.waypoints[i].z * CELL_SIZE + CELL_SIZE / 2;

                if (i > 0) {
                    int px = GRID_OFFSET_X + ed.map.waypoints[i - 1].x * CELL_SIZE + CELL_SIZE / 2;
                    int py = GRID_OFFSET_Y + ed.map.waypoints[i - 1].z * CELL_SIZE + CELL_SIZE / 2;
                    DrawLine(px, py, cx, cy, (Color){ 255, 200, 50, 200 });
                }

                Color wpCol = (i == 0) ? GOLD :
                              (i == ed.map.waypointCount - 1) ? RED :
                              (Color){ 255, 200, 50, 255 };
                DrawCircle(cx, cy, 8, wpCol);
                DrawText(TextFormat("%d", i), cx - 4, cy - 5, 10, BLACK);
            }

            // Grid hover highlight
            if (inGrid) {
                int px = GRID_OFFSET_X + gridMouseX * CELL_SIZE;
                int py = GRID_OFFSET_Y + gridMouseZ * CELL_SIZE;
                DrawRectangleLines(px, py, CELL_SIZE, CELL_SIZE, WHITE);
            }

            // --- Tool panel ---
            int tpX = TOOL_PANEL_X;
            int tpY = GRID_OFFSET_Y;

            DrawText("Tools", tpX, tpY, 20, WHITE);
            tpY += 28;

            for (int i = 0; i < TOOL_COUNT; i++) {
                Rectangle btn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
                bool hover = CheckCollisionPointRec(mouse, btn);
                bool selected = ((int)ed.tool == i);
                Color bg = selected ? (Color){ 60, 100, 60, 255 } :
                           hover    ? (Color){ 50, 60, 70, 255 }  :
                                      (Color){ 35, 40, 50, 255 };
                DrawRectangleRec(btn, bg);
                DrawRectangleLinesEx(btn, 1, (Color){ 80, 80, 80, 200 });
                DrawText(TOOL_NAMES[i], tpX + 8, tpY + 7, 16, selected ? GOLD : WHITE);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    ed.tool = i;
                tpY += 34;
            }

            tpY += 20;

            // Save button
            Rectangle saveBtn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
            bool saveHover = CheckCollisionPointRec(mouse, saveBtn);
            DrawRectangleRec(saveBtn, saveHover ? (Color){ 50, 80, 50, 255 } : (Color){ 35, 55, 35, 255 });
            DrawRectangleLinesEx(saveBtn, 1, (Color){ 80, 120, 80, 200 });
            DrawText("Save", tpX + 8, tpY + 7, 16, WHITE);
            if (saveHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (ed.savePath[0] == '\0') {
                    EditorShowToast(&ed, "Enter a map name first", RED);
                } else if (!ed.confirmOverwrite && access(ed.savePath, F_OK) == 0) {
                    ed.confirmOverwrite = true;
                    EditorShowToast(&ed, "File exists -- click Save again to overwrite", YELLOW);
                } else {
                    ed.confirmOverwrite = false;
                    if (MapSave(&ed.map, ed.savePath)) {
                        EditorShowToast(&ed, TextFormat("Saved to %s", ed.savePath), GREEN);
                        ed.dirty = false;
                    } else {
                        EditorShowToast(&ed, "Save failed!", RED);
                    }
                }
            }
            tpY += 34;

            // Load button
            Rectangle loadBtn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
            bool loadHover = CheckCollisionPointRec(mouse, loadBtn);
            DrawRectangleRec(loadBtn, loadHover ? (Color){ 50, 50, 80, 255 } : (Color){ 35, 35, 55, 255 });
            DrawRectangleLinesEx(loadBtn, 1, (Color){ 80, 80, 120, 200 });
            DrawText("Load", tpX + 8, tpY + 7, 16, WHITE);
            if (loadHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (ed.savePath[0] == '\0') {
                    EditorShowToast(&ed, "Enter a map name first", RED);
                } else if (MapLoad(&ed.map, ed.savePath)) {
                    EditorUpdateSavePath(&ed);
                    ed.dirty = false;
                    ed.meshDirty = true;
                    EditorShowToast(&ed, TextFormat("Loaded %s", ed.savePath), GREEN);
                } else {
                    EditorShowToast(&ed, TextFormat("Failed to load %s", ed.savePath), RED);
                }
            }
            tpY += 34;

            // Clear button
            Rectangle clearBtn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
            bool clearHover = CheckCollisionPointRec(mouse, clearBtn);
            DrawRectangleRec(clearBtn, clearHover ? (Color){ 80, 40, 40, 255 } : (Color){ 55, 30, 30, 255 });
            DrawRectangleLinesEx(clearBtn, 1, (Color){ 120, 80, 80, 200 });
            DrawText("Clear", tpX + 8, tpY + 7, 16, WHITE);
            if (clearHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                char savedName[MAX_MAP_NAME];
                strncpy(savedName, ed.map.name, MAX_MAP_NAME);
                memset(&ed.map, 0, sizeof(Map));
                strncpy(ed.map.name, savedName, MAX_MAP_NAME);
                ed.dirty = true;
                ed.meshDirty = true;
            }

            // --- Status bar ---
            int statusY = GRID_OFFSET_Y + MAP_HEIGHT * CELL_SIZE + 10;

            DrawText("Name:", GRID_OFFSET_X, statusY, 16, LIGHTGRAY);
            Rectangle nameBox = { (float)(GRID_OFFSET_X + 50), (float)statusY - 2, 200.0f, 22.0f };
            DrawRectangleRec(nameBox, (Color){ 25, 25, 30, 255 });
            DrawRectangleLinesEx(nameBox, 1,
                ed.editingName ? (Color){ 100, 200, 100, 255 } : (Color){ 60, 60, 60, 255 });
            DrawText(ed.map.name, GRID_OFFSET_X + 54, statusY, 16, WHITE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ed.editingName = CheckCollisionPointRec(mouse, nameBox);
            }

            const char *pathDisplay = ed.savePath[0] ? TextFormat("-> %s", ed.savePath) : "-> (enter a name)";
            DrawText(pathDisplay, GRID_OFFSET_X + 270, statusY, 16, (Color){ 120, 120, 120, 255 });

            if (ed.toastTimer > 0.0f) {
                float alpha = ed.toastTimer < 1.0f ? ed.toastTimer : 1.0f;
                Color tc = ed.toastColor;
                tc.a = (unsigned char)(alpha * 255);
                DrawText(ed.toastMsg, GRID_OFFSET_X, statusY + 50, 16, tc);
            }

            statusY += 28;
            DrawText(TextFormat("Waypoints: %d   %s",
                    ed.map.waypointCount,
                    ed.dirty ? "[UNSAVED]" : ""),
                    GRID_OFFSET_X, statusY, 16,
                    ed.dirty ? YELLOW : LIGHTGRAY);

            if (ed.map.waypointCount < 2)
                DrawText("WARNING: Need at least 2 waypoints!", GRID_OFFSET_X + 250, statusY, 16, RED);

            EndDrawing();

        } else {
            // --- 3D Tab ---

            // Rebuild mesh if dirty
            if (ed.meshDirty) {
                if (ed.mapMesh.ready)
                    MapFreeMesh(&ed.mapMesh);
                MapBuildMesh(&ed.mapMesh, &ed.map, ed.ps1Shader);
                ed.meshDirty = false;
            }

            // Handle render target resize
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            int newRtW = screenW / PS1_DOWNSCALE;
            int newRtH = screenH / PS1_DOWNSCALE;
            if (newRtW < 1) newRtW = 1;
            if (newRtH < 1) newRtH = 1;
            if (newRtW != ed.rtW || newRtH != ed.rtH) {
                UnloadRenderTexture(ed.renderTarget);
                ed.rtW = newRtW;
                ed.rtH = newRtH;
                ed.renderTarget = LoadRenderTexture(ed.rtW, ed.rtH);
                SetTextureFilter(ed.renderTarget.texture, TEXTURE_FILTER_POINT);
                float res[2] = { (float)ed.rtW, (float)ed.rtH };
                SetShaderValue(ed.ps1Shader, locResolution, res, SHADER_UNIFORM_VEC2);
            }

            // Camera update
            CameraControllerUpdate(&ed.camCtrl, &ed.camera, dt);

            // Render to low-res target
            BeginTextureMode(ed.renderTarget);
            ClearBackground((Color){ 30, 30, 35, 255 });

            BeginMode3D(ed.camera);
            DrawSkybox(ed.camera);
            BeginShaderMode(ed.ps1Shader);
            MapDrawMesh(&ed.mapMesh);
            EndShaderMode();
            EndMode3D();

            EndTextureMode();

            // Draw upscaled to screen
            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                ed.renderTarget.texture,
                (Rectangle){ 0, 0, (float)ed.rtW, -(float)ed.rtH },
                (Rectangle){ 0, 0, (float)screenW, (float)screenH },
                (Vector2){ 0, 0 }, 0.0f, WHITE
            );

            // Tab bar on top
            DrawTabBar(&ed, mouse);

            // Controls hint
            DrawText("WASD: Pan | Scroll: Zoom | Middle Mouse: Rotate", 10, screenH - 24, 16, (Color){ 180, 180, 180, 200 });

            // Toast in 3D view too
            if (ed.toastTimer > 0.0f) {
                float alpha = ed.toastTimer < 1.0f ? ed.toastTimer : 1.0f;
                Color tc = ed.toastColor;
                tc.a = (unsigned char)(alpha * 255);
                DrawText(ed.toastMsg, 10, screenH - 48, 16, tc);
            }

            EndDrawing();
        }
    }

    // Cleanup
    if (ed.mapMesh.ready)
        MapFreeMesh(&ed.mapMesh);
    UnloadShader(ed.ps1Shader);
    UnloadRenderTexture(ed.renderTarget);
    if (skyboxReady) {
        UnloadMesh(skyboxMesh);
        UnloadMaterial(skyboxMaterial);
    }

    CloseWindow();
    return 0;
}
