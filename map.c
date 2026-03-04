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

    // Add some elevation for visual interest
    // Raised plateau near center
    for (int ez = 3; ez <= 5; ez++)
        for (int ex = 5; ex <= 7; ex++)
            map->elevation[ez][ex] = 2;
    // Hill near end of path
    for (int ez = 9; ez <= 11; ez++)
        for (int ex = 15; ex <= 17; ex++)
            map->elevation[ez][ex] = 1;
    // Single high tile
    map->elevation[1][10] = 3;

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

    n = snprintf(buf + written, bufSize - written, "end\n\nelevation\n");
    if (n < 0 || written + n >= bufSize) return -1;
    written += n;

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->elevation[z][x] > 0) {
                n = snprintf(buf + written, bufSize - written, "%d %d %d\n", x, z, map->elevation[z][x]);
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

    enum { SEC_NONE, SEC_WAYPOINTS, SEC_TILES, SEC_ELEVATION } section = SEC_NONE;

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
        } else if (strcmp(line, "elevation") == 0) {
            section = SEC_ELEVATION;
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
        } else if (section == SEC_ELEVATION) {
            int x, z, level;
            if (sscanf(line, "%d %d %d", &x, &z, &level) == 3) {
                if (x >= 0 && x < MAP_WIDTH && z >= 0 && z < MAP_HEIGHT &&
                    level >= 0 && level <= MAX_ELEVATION) {
                    map->elevation[z][x] = level;
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

Vector3 MapGridToWorldElevated(const Map *map, GridPos pos)
{
    return (Vector3){
        pos.x * TILE_SIZE + TILE_SIZE * 0.5f,
        map->elevation[pos.z][pos.x] * ELEVATION_HEIGHT,
        pos.z * TILE_SIZE + TILE_SIZE * 0.5f,
    };
}

float MapGetElevationY(const Map *map, int x, int z)
{
    if (x < 0 || x >= MAP_WIDTH || z < 0 || z >= MAP_HEIGHT) return 0.0f;
    return map->elevation[z][x] * ELEVATION_HEIGHT;
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

// --- Per-vertex color variation for PS1 feel ---

static Color PerturbColor(Color base, int x, int z, int corner)
{
    unsigned int h = (unsigned int)(x * 7919 + z * 6271 + corner * 3571);
    h = (h ^ (h >> 13)) * 0x45d9f3b;
    int offset = (int)((h & 0x1F)) - 16;  // -16 to +15

    int r = base.r + offset;
    int g = base.g + offset;
    int b = base.b + offset;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

static Color TileBaseColor(TileType t)
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

// Tiny deterministic Y jitter per corner for organic feel
static float CornerJitter(int x, int z, int corner)
{
    unsigned int h = (unsigned int)(x * 4919 + z * 3271 + corner * 7571);
    h = (h ^ (h >> 11)) * 0x27d4eb2d;
    return ((float)(h & 0xFF) / 255.0f - 0.5f) * 0.04f;  // +/- 0.02
}

void MapDraw(const Map *map)
{
    // Pass 1: Tile top faces (quads as triangle pairs via rlgl)
    rlBegin(RL_TRIANGLES);
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->tiles[z][x] == TILE_OBSTACLE) continue;

            Color base = TileBaseColor(map->tiles[z][x]);
            float elev = map->elevation[z][x] * ELEVATION_HEIGHT;

            float x0 = x * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z0 = z * TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            // Corner Y values with jitter
            float y00 = elev + CornerJitter(x, z, 0);
            float y10 = elev + CornerJitter(x, z, 1);
            float y01 = elev + CornerJitter(x, z, 2);
            float y11 = elev + CornerJitter(x, z, 3);

            Color c0 = PerturbColor(base, x, z, 0);
            Color c1 = PerturbColor(base, x, z, 1);
            Color c2 = PerturbColor(base, x, z, 2);
            Color c3 = PerturbColor(base, x, z, 3);

            // Triangle 1: (x0,z0), (x1,z0), (x1,z1)
            rlColor4ub(c0.r, c0.g, c0.b, 255);
            rlVertex3f(x0, y00, z0);
            rlColor4ub(c1.r, c1.g, c1.b, 255);
            rlVertex3f(x1, y10, z0);
            rlColor4ub(c3.r, c3.g, c3.b, 255);
            rlVertex3f(x1, y11, z1);

            // Triangle 2: (x0,z0), (x1,z1), (x0,z1)
            rlColor4ub(c0.r, c0.g, c0.b, 255);
            rlVertex3f(x0, y00, z0);
            rlColor4ub(c3.r, c3.g, c3.b, 255);
            rlVertex3f(x1, y11, z1);
            rlColor4ub(c2.r, c2.g, c2.b, 255);
            rlVertex3f(x0, y01, z1);
        }
    }
    rlEnd();

    // Pass 2: Cliff faces between tiles with different elevation
    rlBegin(RL_TRIANGLES);
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            float elev = map->elevation[z][x] * ELEVATION_HEIGHT;
            Color base = TileBaseColor(map->tiles[z][x]);
            // Darken cliff color
            Color cliff = { (unsigned char)(base.r * 0.5f), (unsigned char)(base.g * 0.5f),
                            (unsigned char)(base.b * 0.5f), 255 };

            float x0 = x * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z0 = z * TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            // Check right neighbor (x+1)
            float neighborElev;
            if (x + 1 < MAP_WIDTH)
                neighborElev = map->elevation[z][x + 1] * ELEVATION_HEIGHT;
            else
                neighborElev = 0.0f;

            if (elev > neighborElev) {
                // Vertical wall on right edge
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x1, elev, z0);
                rlVertex3f(x1, neighborElev, z0);
                rlVertex3f(x1, neighborElev, z1);

                rlVertex3f(x1, elev, z0);
                rlVertex3f(x1, neighborElev, z1);
                rlVertex3f(x1, elev, z1);
            } else if (elev < neighborElev) {
                // Neighbor is higher — draw their cliff facing us
                Color nBase = (x + 1 < MAP_WIDTH) ? TileBaseColor(map->tiles[z][x + 1]) : base;
                Color nCliff = { (unsigned char)(nBase.r * 0.5f), (unsigned char)(nBase.g * 0.5f),
                                 (unsigned char)(nBase.b * 0.5f), 255 };
                rlColor4ub(nCliff.r, nCliff.g, nCliff.b, 255);
                rlVertex3f(x1, neighborElev, z0);
                rlVertex3f(x1, elev, z0);
                rlVertex3f(x1, elev, z1);

                rlVertex3f(x1, neighborElev, z0);
                rlVertex3f(x1, elev, z1);
                rlVertex3f(x1, neighborElev, z1);
            }

            // Check bottom neighbor (z+1)
            if (z + 1 < MAP_HEIGHT)
                neighborElev = map->elevation[z + 1][x] * ELEVATION_HEIGHT;
            else
                neighborElev = 0.0f;

            if (elev > neighborElev) {
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x0, elev, z1);
                rlVertex3f(x0, neighborElev, z1);
                rlVertex3f(x1, neighborElev, z1);

                rlVertex3f(x0, elev, z1);
                rlVertex3f(x1, neighborElev, z1);
                rlVertex3f(x1, elev, z1);
            } else if (elev < neighborElev) {
                Color nBase = (z + 1 < MAP_HEIGHT) ? TileBaseColor(map->tiles[z + 1][x]) : base;
                Color nCliff = { (unsigned char)(nBase.r * 0.5f), (unsigned char)(nBase.g * 0.5f),
                                 (unsigned char)(nBase.b * 0.5f), 255 };
                rlColor4ub(nCliff.r, nCliff.g, nCliff.b, 255);
                rlVertex3f(x0, neighborElev, z1);
                rlVertex3f(x0, elev, z1);
                rlVertex3f(x1, elev, z1);

                rlVertex3f(x0, neighborElev, z1);
                rlVertex3f(x1, elev, z1);
                rlVertex3f(x1, neighborElev, z1);
            }

            // Check left neighbor (x-1) — only needed for edge at x=0
            if (x == 0 && elev > 0.0f) {
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x0, elev, z1);
                rlVertex3f(x0, 0.0f, z1);
                rlVertex3f(x0, 0.0f, z0);

                rlVertex3f(x0, elev, z1);
                rlVertex3f(x0, 0.0f, z0);
                rlVertex3f(x0, elev, z0);
            }

            // Check top neighbor (z=0) — only needed for edge at z=0
            if (z == 0 && elev > 0.0f) {
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x0, elev, z0);
                rlVertex3f(x0, 0.0f, z0);
                rlVertex3f(x1, 0.0f, z0);

                rlVertex3f(x0, elev, z0);
                rlVertex3f(x1, 0.0f, z0);
                rlVertex3f(x1, elev, z0);
            }
        }
    }
    rlEnd();

    // Pass 3: Obstacle cubes positioned on top of elevation
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->tiles[z][x] == TILE_OBSTACLE) {
                float elev = map->elevation[z][x] * ELEVATION_HEIGHT;
                Vector3 pos = {
                    x * TILE_SIZE + TILE_SIZE * 0.5f,
                    elev + 0.25f,
                    z * TILE_SIZE + TILE_SIZE * 0.5f,
                };
                DrawCubeV(pos, (Vector3){ 0.6f, 0.5f, 0.6f }, DARKGRAY);
            }
        }
    }
}
