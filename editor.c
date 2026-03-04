#include "fd_gfx.h"
#include "fd_input.h"
#include "fd_app.h"
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

static EditorState ed;

static void EditorUpdateSavePath(EditorState *e)
{
    if (e->map.name[0] == '\0') {
        e->savePath[0] = '\0';
        return;
    }

    char slug[128];
    int j = 0;
    for (int i = 0; e->map.name[i] && j < (int)sizeof(slug) - 1; i++) {
        char ch = e->map.name[i];
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
        e->savePath[0] = '\0';
        return;
    }

    snprintf(e->savePath, sizeof(e->savePath), "maps/%s.fdmap", slug);
}

static void EditorShowToast(EditorState *e, const char *msg, Color color)
{
    strncpy(e->toastMsg, msg, sizeof(e->toastMsg) - 1);
    e->toastMsg[sizeof(e->toastMsg) - 1] = '\0';
    e->toastTimer = 3.0f;
    e->toastColor = color;
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

// --- Callbacks ---

void EditorAppInit(void)
{
    memset(&ed, 0, sizeof(ed));
    MapInit(&ed.map);
    ed.tool = TOOL_EMPTY;
    EditorUpdateSavePath(&ed);

    // Try loading default map
    if (MapLoad(&ed.map, "maps/default.fdmap"))
        EditorUpdateSavePath(&ed);
}

void EditorAppFrame(void)
{
    Vector2 mouse = GetMousePosition();

    // Update toast timer
    if (ed.toastTimer > 0.0f)
        ed.toastTimer -= FdFrameTime();

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
            }
        }
    }

    // --- Drawing ---
    Color clearCol = { 30, 30, 35, 255 };
    FdBeginFrame(clearCol);

    // Title
    FdDrawText("Map Editor", 10, 10, 24, WHITE);

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
                int rv = c.r + boost; if (rv > 255) rv = 255;
                int gv = c.g + boost; if (gv > 255) gv = 255;
                int bv = c.b + boost; if (bv > 255) bv = 255;
                c = (Color){ (unsigned char)rv, (unsigned char)gv, (unsigned char)bv, 255 };
            }

            FdDrawRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, c);
            Color gridLineCol = { 50, 50, 50, 150 };
            FdDrawRectLines(px, py, CELL_SIZE, CELL_SIZE, gridLineCol);

            // Display elevation number
            if (elev > 0) {
                const char *elevStr = FdTextFormat("%d", elev);
                int tw = FdMeasureText(elevStr, 12);
                Color elevTextCol = { 255, 255, 255, 200 };
                FdDrawText(elevStr, px + (CELL_SIZE - tw) / 2, py + (CELL_SIZE - 12) / 2, 12, elevTextCol);
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
            Color lineCol = { 255, 200, 50, 200 };
            FdDrawLine2D(px, py, cx, cy, lineCol);
        }

        Color wpCol;
        if (i == 0)
            wpCol = GOLD;
        else if (i == ed.map.waypointCount - 1)
            wpCol = RED;
        else
            wpCol = (Color){ 255, 200, 50, 255 };
        FdDrawCircle2D(cx, cy, 8, wpCol);
        FdDrawText(FdTextFormat("%d", i), cx - 4, cy - 5, 10, BLACK);
    }

    // Grid hover highlight
    if (inGrid) {
        int px = GRID_OFFSET_X + gridMouseX * CELL_SIZE;
        int py = GRID_OFFSET_Y + gridMouseZ * CELL_SIZE;
        FdDrawRectLines(px, py, CELL_SIZE, CELL_SIZE, WHITE);
    }

    // --- Tool panel ---
    int tpX = TOOL_PANEL_X;
    int tpY = GRID_OFFSET_Y;

    FdDrawText("Tools", tpX, tpY, 20, WHITE);
    tpY += 28;

    for (int i = 0; i < TOOL_COUNT; i++) {
        FdRect btn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
        bool hover = FdPointInRect(mouse, btn);
        bool selected = ((int)ed.tool == i);
        Color bg;
        if (selected)     bg = (Color){ 60, 100, 60, 255 };
        else if (hover)   bg = (Color){ 50, 60, 70, 255 };
        else              bg = (Color){ 35, 40, 50, 255 };
        FdDrawRect(tpX, tpY, TOOL_PANEL_W, 30, bg);
        Color btnBorder = { 80, 80, 80, 200 };
        FdDrawRectLinesEx(tpX, tpY, TOOL_PANEL_W, 30, 1, btnBorder);
        FdDrawText(TOOL_NAMES[i], tpX + 8, tpY + 7, 16, selected ? GOLD : WHITE);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            ed.tool = i;
        tpY += 34;
    }

    tpY += 20;

    // Save button
    {
        FdRect saveBtn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
        bool saveHover = FdPointInRect(mouse, saveBtn);
        Color saveBg = saveHover ? (Color){ 50, 80, 50, 255 } : (Color){ 35, 55, 35, 255 };
        FdDrawRect(tpX, tpY, TOOL_PANEL_W, 30, saveBg);
        Color saveBorder = { 80, 120, 80, 200 };
        FdDrawRectLinesEx(tpX, tpY, TOOL_PANEL_W, 30, 1, saveBorder);
        FdDrawText("Save", tpX + 8, tpY + 7, 16, WHITE);
        if (saveHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (ed.savePath[0] == '\0') {
                EditorShowToast(&ed, "Enter a map name first", RED);
            } else if (!ed.confirmOverwrite && access(ed.savePath, F_OK) == 0) {
                ed.confirmOverwrite = true;
                EditorShowToast(&ed, "File exists -- click Save again to overwrite", YELLOW);
            } else {
                ed.confirmOverwrite = false;
                if (MapSave(&ed.map, ed.savePath)) {
                    EditorShowToast(&ed, FdTextFormat("Saved to %s", ed.savePath), GREEN);
                    ed.dirty = false;
                } else {
                    EditorShowToast(&ed, "Save failed!", RED);
                }
            }
        }
        tpY += 34;
    }

    // Load button
    {
        FdRect loadBtn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
        bool loadHover = FdPointInRect(mouse, loadBtn);
        Color loadBg = loadHover ? (Color){ 50, 50, 80, 255 } : (Color){ 35, 35, 55, 255 };
        FdDrawRect(tpX, tpY, TOOL_PANEL_W, 30, loadBg);
        Color loadBorder = { 80, 80, 120, 200 };
        FdDrawRectLinesEx(tpX, tpY, TOOL_PANEL_W, 30, 1, loadBorder);
        FdDrawText("Load", tpX + 8, tpY + 7, 16, WHITE);
        if (loadHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (ed.savePath[0] == '\0') {
                EditorShowToast(&ed, "Enter a map name first", RED);
            } else if (MapLoad(&ed.map, ed.savePath)) {
                EditorUpdateSavePath(&ed);
                ed.dirty = false;
                EditorShowToast(&ed, FdTextFormat("Loaded %s", ed.savePath), GREEN);
            } else {
                EditorShowToast(&ed, FdTextFormat("Failed to load %s", ed.savePath), RED);
            }
        }
        tpY += 34;
    }

    // Clear button
    {
        FdRect clearBtn = { (float)tpX, (float)tpY, (float)TOOL_PANEL_W, 30.0f };
        bool clearHover = FdPointInRect(mouse, clearBtn);
        Color clearBg = clearHover ? (Color){ 80, 40, 40, 255 } : (Color){ 55, 30, 30, 255 };
        FdDrawRect(tpX, tpY, TOOL_PANEL_W, 30, clearBg);
        Color clearBorder = { 120, 80, 80, 200 };
        FdDrawRectLinesEx(tpX, tpY, TOOL_PANEL_W, 30, 1, clearBorder);
        FdDrawText("Clear", tpX + 8, tpY + 7, 16, WHITE);
        if (clearHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            char savedName[MAX_MAP_NAME];
            strncpy(savedName, ed.map.name, MAX_MAP_NAME);
            memset(&ed.map, 0, sizeof(Map));
            strncpy(ed.map.name, savedName, MAX_MAP_NAME);
            ed.dirty = true;
        }
    }

    // --- Status bar ---
    int statusY = GRID_OFFSET_Y + MAP_HEIGHT * CELL_SIZE + 10;

    // Map name
    FdDrawText("Name:", GRID_OFFSET_X, statusY, 16, LIGHTGRAY);
    FdRect nameBox = { (float)(GRID_OFFSET_X + 50), (float)statusY - 2, 200.0f, 22.0f };
    Color nameBg = { 25, 25, 30, 255 };
    FdDrawRect((int)nameBox.x, (int)nameBox.y, (int)nameBox.w, (int)nameBox.h, nameBg);
    Color nameBorder = ed.editingName ? (Color){ 100, 200, 100, 255 } : (Color){ 60, 60, 60, 255 };
    FdDrawRectLinesEx((int)nameBox.x, (int)nameBox.y, (int)nameBox.w, (int)nameBox.h, 1, nameBorder);
    FdDrawText(ed.map.name, GRID_OFFSET_X + 54, statusY, 16, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ed.editingName = FdPointInRect(mouse, nameBox);
    }

    // Derived save path (read-only)
    const char *pathDisplay = ed.savePath[0] ? FdTextFormat("-> %s", ed.savePath) : "-> (enter a name)";
    Color pathCol = { 120, 120, 120, 255 };
    FdDrawText(pathDisplay, GRID_OFFSET_X + 270, statusY, 16, pathCol);

    // Toast message
    if (ed.toastTimer > 0.0f) {
        float alpha = ed.toastTimer < 1.0f ? ed.toastTimer : 1.0f;
        Color tc = ed.toastColor;
        tc.a = (unsigned char)(alpha * 255);
        FdDrawText(ed.toastMsg, GRID_OFFSET_X, statusY + 50, 16, tc);
    }

    // Status info
    statusY += 28;
    FdDrawText(FdTextFormat("Waypoints: %d   %s",
            ed.map.waypointCount,
            ed.dirty ? "[UNSAVED]" : ""),
            GRID_OFFSET_X, statusY, 16,
            ed.dirty ? YELLOW : LIGHTGRAY);

    if (ed.map.waypointCount < 2)
        FdDrawText("WARNING: Need at least 2 waypoints!", GRID_OFFSET_X + 250, statusY, 16, RED);

    FdEndFrame();
}

void EditorAppCleanup(void)
{
    // Nothing to clean up
}
