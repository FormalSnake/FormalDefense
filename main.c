#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "map.h"
#include "entity.h"
#include "game.h"
#include <math.h>
#include <string.h>

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
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    panZ -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  panZ += 1.0f;
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
        cc->pitch -= delta.y * cc->rotSpeed;
    }

    float wheel = GetMouseWheelMove();
    cc->distance -= wheel * cc->zoomSpeed;

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

// --- UI Constants ---

#define BOTTOM_BAR_HEIGHT 60
#define BTN_WIDTH 120
#define BTN_HEIGHT 45
#define BTN_MARGIN 10
#define INFO_PANEL_W 200
#define INFO_PANEL_H 160

// --- Main ---

int main(void)
{
    InitWindow(1280, 720, "Formal Defense");
    SetTargetFPS(60);

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

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        Vector2 mouse = GetMousePosition();

        // --- UI hit test (bottom bar takes priority) ---
        bool mouseInUI = (mouse.y >= screenH - BOTTOM_BAR_HEIGHT);

        // Also check info panel
        if (selectedTowerIdx >= 0) {
            Rectangle infoRect = { (float)(screenW - INFO_PANEL_W - 10), 40.0f,
                                   (float)INFO_PANEL_W, (float)INFO_PANEL_H };
            if (CheckCollisionPointRec(mouse, infoRect))
                mouseInUI = true;
        }

        // --- Camera (only when not in UI) ---
        if (!mouseInUI || IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D))
            CameraControllerUpdate(&camCtrl, &camera, dt);

        // --- Bottom bar button clicks ---
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= screenH - BOTTOM_BAR_HEIGHT) {
            for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
                int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
                int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
                Rectangle btnRect = { (float)bx, (float)by, (float)BTN_WIDTH, (float)BTN_HEIGHT };
                if (CheckCollisionPointRec(mouse, btnRect)) {
                    int cost = TOWER_CONFIGS[i][0].cost;
                    if (gs.gold >= cost) {
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
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            selectedTowerType = -1;
            selectedTowerIdx = -1;
        }

        // --- Mouse ground position ---
        Vector3 mouseGround = {0};
        bool mouseOnGround = GetMouseGroundPos(camera, &mouseGround);
        GridPos mouseGrid = {-1, -1};
        bool canPlace = false;

        if (mouseOnGround) {
            mouseGrid = MapWorldToGrid(mouseGround);
            if (selectedTowerType >= 0)
                canPlace = MapCanPlaceTower(&map, mouseGrid);
        }

        // --- Left click in 3D area ---
        if (!mouseInUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gs.phase != PHASE_OVER) {
            if (selectedTowerType >= 0 && canPlace) {
                // Place tower
                int cost = TOWER_CONFIGS[selectedTowerType][0].cost;
                if (gs.gold >= cost) {
                    gs.gold -= cost;
                    TowerPlace(towers, MAX_TOWERS, (TowerType)selectedTowerType, mouseGrid);
                    map.tiles[mouseGrid.z][mouseGrid.x] = TILE_TOWER;
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

        // --- Upgrade selected tower ---
        // (handled in UI draw section via button click check)

        // --- Update game systems ---
        if (gs.phase != PHASE_OVER) {
            GameUpdateWave(&gs, enemies, MAX_ENEMIES, &map, dt);
            EnemiesUpdate(enemies, MAX_ENEMIES, &map, &gs, dt);
            TowersUpdate(towers, MAX_TOWERS, enemies, MAX_ENEMIES, projectiles, MAX_PROJECTILES, dt);
            ProjectilesUpdate(projectiles, MAX_PROJECTILES, enemies, MAX_ENEMIES, &gs, dt);
        }

        // Deselect tower if it became inactive
        if (selectedTowerIdx >= 0 && !towers[selectedTowerIdx].active)
            selectedTowerIdx = -1;

        // =========================
        // DRAW
        // =========================
        BeginDrawing();
        ClearBackground((Color){ 30, 30, 35, 255 });

        BeginMode3D(camera);
            MapDraw(&map);

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

            TowersDraw(towers, MAX_TOWERS);

            // Range indicator for selected tower
            if (selectedTowerIdx >= 0 && towers[selectedTowerIdx].active) {
                const Tower *st = &towers[selectedTowerIdx];
                float range = TOWER_CONFIGS[st->type][st->level].range;
                Vector3 rc = st->worldPos;
                rc.y = 0.0f;
                DrawRangeCircle(rc, range, (Color){ 255, 255, 100, 150 });
            }

            EnemiesDraw(enemies, MAX_ENEMIES, camera);
            ProjectilesDraw(projectiles, MAX_PROJECTILES);
        EndMode3D();

        // =====================
        // 2D UI
        // =====================

        // --- Top bar ---
        DrawRectangle(0, 0, screenW, 32, (Color){ 0, 0, 0, 180 });
        DrawText(TextFormat("Gold: %d", gs.gold), 10, 7, 20, GOLD);
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
            bool affordable = gs.gold >= cost;

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
                bool canUpg = gs.gold >= upgCost;
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
                        gs.gold -= upgCost;
                        towers[selectedTowerIdx].level++;
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

            const char *hint = "Press R to restart or ESC to quit";
            int hintW = MeasureText(hint, 18);
            DrawText(hint, (screenW - hintW) / 2, screenH / 2 + 60, 18, LIGHTGRAY);
        }

        EndDrawing();

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
    }

    CloseWindow();
    return 0;
}
