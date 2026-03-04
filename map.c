#include "map.h"
#include "raylib.h"
#include "rlgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

// --- Path Tracing ---

static void MapTracePath(Map *map)
{
    // Mark spawn and base
    if (map->waypointCount >= 2) {
        map->tiles[map->waypoints[0].z][map->waypoints[0].x] = TILE_SPAWN;
        map->tiles[map->waypoints[map->waypointCount - 1].z][map->waypoints[map->waypointCount - 1].x] = TILE_BASE;
    }

    // Trace path between consecutive waypoints
    for (int i = 0; i < map->waypointCount - 1; i++) {
        int x0 = map->waypoints[i].x, z0 = map->waypoints[i].z;
        int x1 = map->waypoints[i + 1].x, z1 = map->waypoints[i + 1].z;

        if (z0 == z1) {
            int minX = x0 < x1 ? x0 : x1;
            int maxX = x0 > x1 ? x0 : x1;
            for (int x = minX; x <= maxX; x++) {
                if (map->tiles[z0][x] == TILE_EMPTY)
                    map->tiles[z0][x] = TILE_PATH;
            }
        } else if (x0 == x1) {
            int minZ = z0 < z1 ? z0 : z1;
            int maxZ = z0 > z1 ? z0 : z1;
            for (int z = minZ; z <= maxZ; z++) {
                if (map->tiles[z][x0] == TILE_EMPTY)
                    map->tiles[z][x0] = TILE_PATH;
            }
        }
    }
}

// --- Init (fallback hardcoded map) ---

void MapInit(Map *map)
{
    memset(map, 0, sizeof(*map));
    strncpy(map->name, "Default", MAX_MAP_NAME - 1);

    GridPos path[] = {
        {0, 7}, {4, 7}, {4, 2}, {8, 2}, {8, 12},
        {12, 12}, {12, 4}, {16, 4}, {16, 10}, {19, 10},
    };
    int pathLen = sizeof(path) / sizeof(path[0]);

    map->waypointCount = pathLen;
    for (int i = 0; i < pathLen; i++)
        map->waypoints[i] = path[i];

    MapTracePath(map);

    GridPos obstacles[] = {
        {2, 4}, {2, 10}, {6, 6}, {6, 9}, {10, 1}, {10, 8},
        {14, 7}, {14, 13}, {17, 2}, {17, 7}, {3, 13}, {11, 6},
    };
    int obsCount = sizeof(obstacles) / sizeof(obstacles[0]);
    for (int i = 0; i < obsCount; i++) {
        int ox = obstacles[i].x, oz = obstacles[i].z;
        if (ox >= 0 && ox < MAP_WIDTH && oz >= 0 && oz < MAP_HEIGHT &&
            map->tiles[oz][ox] == TILE_EMPTY) {
            map->tiles[oz][ox] = TILE_OBSTACLE;
        }
    }
}

// --- Serialize to buffer (.fdmap format) ---

int MapSerialize(const Map *map, char *buf, int bufSize)
{
    int written = 0;
    int n;

    n = snprintf(buf + written, bufSize - written, "name %s\nsize %d %d\n\nwaypoints\n",
                 map->name, MAP_WIDTH, MAP_HEIGHT);
    if (n < 0 || written + n >= bufSize) return -1;
    written += n;

    for (int i = 0; i < map->waypointCount; i++) {
        n = snprintf(buf + written, bufSize - written, "%d %d\n",
                     map->waypoints[i].x, map->waypoints[i].z);
        if (n < 0 || written + n >= bufSize) return -1;
        written += n;
    }

    n = snprintf(buf + written, bufSize - written, "end\n\ntiles\n");
    if (n < 0 || written + n >= bufSize) return -1;
    written += n;

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->tiles[z][x] == TILE_OBSTACLE) {
                n = snprintf(buf + written, bufSize - written, "%d %d obstacle\n", x, z);
                if (n < 0 || written + n >= bufSize) return -1;
                written += n;
            }
        }
    }

    n = snprintf(buf + written, bufSize - written, "end\n");
    if (n < 0 || written + n >= bufSize) return -1;
    written += n;

    return written;
}

// --- Save to file ---

void MapSave(const Map *map, const char *filePath)
{
    char buf[4096];
    int len = MapSerialize(map, buf, sizeof(buf));
    if (len <= 0) return;

    FILE *f = fopen(filePath, "w");
    if (!f) return;
    fwrite(buf, 1, len, f);
    fclose(f);
}

// --- Parse from buffer ---

bool MapLoadFromBuffer(Map *map, const char *data, int len)
{
    memset(map, 0, sizeof(*map));

    // Copy data to a mutable null-terminated buffer
    char *buf = malloc(len + 1);
    if (!buf) return false;
    memcpy(buf, data, len);
    buf[len] = '\0';

    enum { SEC_NONE, SEC_WAYPOINTS, SEC_TILES } section = SEC_NONE;

    char *line = buf;
    while (line && *line) {
        // Find end of line
        char *eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            line = eol ? eol + 1 : NULL;
            continue;
        }

        // Trim trailing whitespace
        int lineLen = (int)strlen(line);
        while (lineLen > 0 && (line[lineLen - 1] == '\r' || line[lineLen - 1] == ' '))
            line[--lineLen] = '\0';

        if (strcmp(line, "waypoints") == 0) {
            section = SEC_WAYPOINTS;
        } else if (strcmp(line, "tiles") == 0) {
            section = SEC_TILES;
        } else if (strcmp(line, "end") == 0) {
            section = SEC_NONE;
        } else if (strncmp(line, "name ", 5) == 0) {
            strncpy(map->name, line + 5, MAX_MAP_NAME - 1);
            map->name[MAX_MAP_NAME - 1] = '\0';
        } else if (strncmp(line, "size ", 5) == 0) {
            // Parse but we use fixed MAP_WIDTH/MAP_HEIGHT
        } else if (section == SEC_WAYPOINTS) {
            int x, z;
            if (sscanf(line, "%d %d", &x, &z) == 2 && map->waypointCount < MAX_WAYPOINTS) {
                map->waypoints[map->waypointCount].x = x;
                map->waypoints[map->waypointCount].z = z;
                map->waypointCount++;
            }
        } else if (section == SEC_TILES) {
            int x, z;
            char typeStr[32];
            if (sscanf(line, "%d %d %31s", &x, &z, typeStr) == 3) {
                if (x >= 0 && x < MAP_WIDTH && z >= 0 && z < MAP_HEIGHT) {
                    if (strcmp(typeStr, "obstacle") == 0)
                        map->tiles[z][x] = TILE_OBSTACLE;
                }
            }
        }

        line = eol ? eol + 1 : NULL;
    }

    free(buf);

    if (map->waypointCount < 2) return false;

    MapTracePath(map);
    return true;
}

// --- Load from file ---

bool MapLoad(Map *map, const char *filePath)
{
    FILE *f = fopen(filePath, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 32768) {
        fclose(f);
        return false;
    }

    char *data = malloc(size);
    if (!data) {
        fclose(f);
        return false;
    }

    size_t read = fread(data, 1, size, f);
    fclose(f);

    bool ok = MapLoadFromBuffer(map, data, (int)read);
    free(data);
    return ok;
}

// --- Map Registry ---

void MapRegistryScan(MapRegistry *reg, const char *directory)
{
    memset(reg, 0, sizeof(*reg));

    DIR *dir = opendir(directory);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && reg->count < MAX_MAPS) {
        int nameLen = (int)strlen(ent->d_name);
        if (nameLen < 7 || strcmp(ent->d_name + nameLen - 6, ".fdmap") != 0)
            continue;

        char fullPath[256];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, ent->d_name);

        // Quick-parse just the name
        Map tmp;
        if (MapLoad(&tmp, fullPath)) {
            strncpy(reg->names[reg->count], tmp.name, MAX_MAP_NAME - 1);
            strncpy(reg->paths[reg->count], fullPath, 255);
            reg->count++;
        }
    }

    closedir(dir);
}

// --- Utility ---

Vector3 MapGridToWorld(GridPos pos)
{
    return (Vector3){
        pos.x * TILE_SIZE + TILE_SIZE * 0.5f,
        0.0f,
        pos.z * TILE_SIZE + TILE_SIZE * 0.5f,
    };
}

GridPos MapWorldToGrid(Vector3 worldPos)
{
    return (GridPos){
        (int)(worldPos.x / TILE_SIZE),
        (int)(worldPos.z / TILE_SIZE),
    };
}

bool MapCanPlaceTower(const Map *map, GridPos pos)
{
    if (pos.x < 0 || pos.x >= MAP_WIDTH || pos.z < 0 || pos.z >= MAP_HEIGHT)
        return false;
    return map->tiles[pos.z][pos.x] == TILE_EMPTY;
}

void MapDraw(const Map *map)
{
    float cubeHeight = 0.05f;

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            Vector3 pos = {
                x * TILE_SIZE + TILE_SIZE * 0.5f,
                -cubeHeight * 0.5f,
                z * TILE_SIZE + TILE_SIZE * 0.5f,
            };
            Vector3 size = { TILE_SIZE * 0.95f, cubeHeight, TILE_SIZE * 0.95f };

            Color col;
            switch (map->tiles[z][x]) {
                case TILE_PATH:     col = (Color){ 210, 180, 140, 255 }; break; // tan
                case TILE_SPAWN:    col = (Color){ 255, 200, 0, 255 };   break; // gold
                case TILE_BASE:     col = (Color){ 200, 50, 50, 255 };   break; // red
                case TILE_OBSTACLE: col = (Color){ 130, 130, 130, 255 }; break; // gray
                case TILE_TOWER:    col = (Color){ 100, 180, 100, 255 }; break; // muted green
                default:            col = (Color){ 100, 160, 80, 255 };  break; // green (empty)
            }

            DrawCubeV(pos, size, col);
            DrawCubeWiresV(pos, size, (Color){ 60, 60, 60, 80 });
        }
    }

    // Draw obstacle cubes as raised blocks
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->tiles[z][x] == TILE_OBSTACLE) {
                Vector3 pos = {
                    x * TILE_SIZE + TILE_SIZE * 0.5f,
                    0.25f,
                    z * TILE_SIZE + TILE_SIZE * 0.5f,
                };
                DrawCubeV(pos, (Vector3){ 0.6f, 0.5f, 0.6f }, DARKGRAY);
            }
        }
    }
}
