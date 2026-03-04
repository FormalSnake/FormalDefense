#include "raylib.h"
#include "map.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

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
#define GRID_OFFSET_Y 40
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

int main(void)
{
    int winW = GRID_OFFSET_X + MAP_WIDTH * CELL_SIZE + 20 + TOOL_PANEL_W + 20;
    int winH = GRID_OFFSET_Y + MAP_HEIGHT * CELL_SIZE + 80;
    InitWindow(winW, winH, "Formal Defense - Map Editor");
    SetTargetFPS(60);

    EditorState ed;
    EditorInit(&ed);

    // Try loading default map
    if (MapLoad(&ed.map, "maps/default.fdmap"))
        EditorUpdateSavePath(&ed);

    while (!WindowShouldClose())
    {
        Vector2 mouse = GetMousePosition();

        // Update toast timer
        if (ed.toastTimer > 0.0f)
            ed.toastTimer -= GetFrameTime();

        // --- Text input ---
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
                    }
                    break;
                case TOOL_OBSTACLE:
                    if (ed.map.tiles[gridMouseZ][gridMouseX] == TILE_EMPTY) {
                        ed.map.tiles[gridMouseZ][gridMouseX] = TILE_OBSTACLE;
                        ed.dirty = true;
                    }
                    break;
                case TOOL_ELEVATE_UP:
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (ed.map.elevation[gridMouseZ][gridMouseX] < MAX_ELEVATION) {
                            ed.map.elevation[gridMouseZ][gridMouseX]++;
                            ed.dirty = true;
                        }
                    }
                    break;
                case TOOL_ELEVATE_DOWN:
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (ed.map.elevation[gridMouseZ][gridMouseX] > 0) {
                            ed.map.elevation[gridMouseZ][gridMouseX]--;
                            ed.dirty = true;
                        }
                    }
                    break;
                case TOOL_WAYPOINT_ADD:
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                        ed.map.waypointCount < MAX_WAYPOINTS) {
                        ed.map.waypoints[ed.map.waypointCount].x = gridMouseX;
                        ed.map.waypoints[ed.map.waypointCount].z = gridMouseZ;
                        ed.map.waypointCount++;
                        // Recompute path
                        for (int z = 0; z < MAP_HEIGHT; z++)
                            for (int x = 0; x < MAP_WIDTH; x++)
                                if (ed.map.tiles[z][x] == TILE_PATH ||
                                    ed.map.tiles[z][x] == TILE_SPAWN ||
                                    ed.map.tiles[z][x] == TILE_BASE)
                                    ed.map.tiles[z][x] = TILE_EMPTY;
                        // Re-create a fresh map copy to retrace
                        Map tmp;
                        memset(&tmp, 0, sizeof(tmp));
                        memcpy(tmp.waypoints, ed.map.waypoints, sizeof(tmp.waypoints));
                        tmp.waypointCount = ed.map.waypointCount;
                        // Preserve obstacles
                        for (int z = 0; z < MAP_HEIGHT; z++)
                            for (int x = 0; x < MAP_WIDTH; x++)
                                if (ed.map.tiles[z][x] == TILE_OBSTACLE)
                                    tmp.tiles[z][x] = TILE_OBSTACLE;
                        // MapTracePath is static, so we use MapLoadFromBuffer trick
                        // Actually, let's just serialize and reload
                        strncpy(tmp.name, ed.map.name, MAX_MAP_NAME);
                        char buf[4096];
                        int len = MapSerialize(&tmp, buf, sizeof(buf));
                        if (len > 0) {
                            Map reloaded;
                            if (MapLoadFromBuffer(&reloaded, buf, len)) {
                                // Preserve obstacles from ed.map that were on EMPTY tiles
                                for (int z = 0; z < MAP_HEIGHT; z++)
                                    for (int x = 0; x < MAP_WIDTH; x++)
                                        if (ed.map.tiles[z][x] == TILE_OBSTACLE &&
                                            reloaded.tiles[z][x] == TILE_EMPTY)
                                            reloaded.tiles[z][x] = TILE_OBSTACLE;
                                memcpy(&ed.map, &reloaded, sizeof(Map));
                            }
                        }
                        ed.dirty = true;
                    }
                    break;
                case TOOL_WAYPOINT_DEL:
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ed.map.waypointCount > 0) {
                        // Find nearest waypoint
                        int nearest = -1;
                        float bestDist = 9999.0f;
                        for (int i = 0; i < ed.map.waypointCount; i++) {
                            float dx = (float)(ed.map.waypoints[i].x - gridMouseX);
                            float dz = (float)(ed.map.waypoints[i].z - gridMouseZ);
                            float d = dx * dx + dz * dz;
                            if (d < bestDist) { bestDist = d; nearest = i; }
                        }
                        if (nearest >= 0 && bestDist <= 2.0f) {
                            // Remove waypoint
                            for (int i = nearest; i < ed.map.waypointCount - 1; i++)
                                ed.map.waypoints[i] = ed.map.waypoints[i + 1];
                            ed.map.waypointCount--;
                            // Recompute
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
                        }
                    }
                    break;
                default:
                    break;
                }
            }

            // Right-click erases (sets to empty if obstacle, or removes waypoint if near one)
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                if (ed.map.tiles[gridMouseZ][gridMouseX] == TILE_OBSTACLE) {
                    ed.map.tiles[gridMouseZ][gridMouseX] = TILE_EMPTY;
                    ed.dirty = true;
                }
            }
        }

        // --- Drawing ---
        BeginDrawing();
        ClearBackground((Color){ 30, 30, 35, 255 });

        // Title
        DrawText("Map Editor", 10, 10, 24, WHITE);

        // Grid
        for (int z = 0; z < MAP_HEIGHT; z++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                int px = GRID_OFFSET_X + x * CELL_SIZE;
                int py = GRID_OFFSET_Y + z * CELL_SIZE;
                Color c = TileColor(ed.map.tiles[z][x]);

                // Tint lighter for higher elevation (+15 RGB per level)
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

                // Display elevation number
                if (elev > 0) {
                    const char *elevStr = TextFormat("%d", elev);
                    int tw = MeasureText(elevStr, 12);
                    DrawText(elevStr, px + (CELL_SIZE - tw) / 2, py + (CELL_SIZE - 12) / 2, 12,
                             (Color){ 255, 255, 255, 200 });
                }
            }
        }

        // Draw waypoints as numbered circles with connecting lines
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
            // elevation is zeroed by memset — that's correct for clear
            ed.dirty = true;
        }

        // --- Status bar ---
        int statusY = GRID_OFFSET_Y + MAP_HEIGHT * CELL_SIZE + 10;

        // Map name
        DrawText("Name:", GRID_OFFSET_X, statusY, 16, LIGHTGRAY);
        Rectangle nameBox = { (float)(GRID_OFFSET_X + 50), (float)statusY - 2, 200.0f, 22.0f };
        DrawRectangleRec(nameBox, (Color){ 25, 25, 30, 255 });
        DrawRectangleLinesEx(nameBox, 1,
            ed.editingName ? (Color){ 100, 200, 100, 255 } : (Color){ 60, 60, 60, 255 });
        DrawText(ed.map.name, GRID_OFFSET_X + 54, statusY, 16, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ed.editingName = CheckCollisionPointRec(mouse, nameBox);
        }

        // Derived save path (read-only)
        const char *pathDisplay = ed.savePath[0] ? TextFormat("-> %s", ed.savePath) : "-> (enter a name)";
        DrawText(pathDisplay, GRID_OFFSET_X + 270, statusY, 16, (Color){ 120, 120, 120, 255 });

        // Toast message
        if (ed.toastTimer > 0.0f) {
            float alpha = ed.toastTimer < 1.0f ? ed.toastTimer : 1.0f;
            Color tc = ed.toastColor;
            tc.a = (unsigned char)(alpha * 255);
            DrawText(ed.toastMsg, GRID_OFFSET_X, statusY + 50, 16, tc);
        }

        // Status info
        statusY += 28;
        DrawText(TextFormat("Waypoints: %d   %s",
                ed.map.waypointCount,
                ed.dirty ? "[UNSAVED]" : ""),
                GRID_OFFSET_X, statusY, 16,
                ed.dirty ? YELLOW : LIGHTGRAY);

        if (ed.map.waypointCount < 2)
            DrawText("WARNING: Need at least 2 waypoints!", GRID_OFFSET_X + 250, statusY, 16, RED);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
