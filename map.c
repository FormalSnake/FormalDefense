#include "map.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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

    // --- Elevation layout ---

    // Top-left rolling hills (z:0-4, x:0-6)
    for (int ez = 0; ez <= 3; ez++)
        for (int ex = 0; ex <= 3; ex++)
            map->elevation[ez][ex] = 1;
    for (int ez = 1; ez <= 2; ez++)
        for (int ex = 1; ex <= 2; ex++)
            map->elevation[ez][ex] = 2;
    map->elevation[0][5] = 1;
    map->elevation[0][6] = 1;
    map->elevation[1][5] = 1;

    // Central ridge / plateau (z:5-7, x:9-12) — cliffs alongside path
    for (int ez = 5; ez <= 7; ez++)
        for (int ex = 9; ex <= 12; ex++)
            map->elevation[ez][ex] = 2;
    map->elevation[5][10] = 3;
    map->elevation[5][11] = 3;
    map->elevation[6][10] = 3;
    map->elevation[6][11] = 3;

    // Slope leading up to central ridge
    map->elevation[4][9] = 1;
    map->elevation[4][10] = 1;
    map->elevation[4][11] = 1;
    map->elevation[4][12] = 1;
    map->elevation[8][9] = 1;
    map->elevation[8][10] = 1;

    // Bottom-right mesa (z:10-13, x:14-18)
    for (int ez = 10; ez <= 13; ez++)
        for (int ex = 14; ex <= 18; ex++)
            map->elevation[ez][ex] = 1;
    for (int ez = 11; ez <= 12; ez++)
        for (int ex = 15; ex <= 17; ex++)
            map->elevation[ez][ex] = 2;

    // Scattered peaks for visual interest
    map->elevation[1][10] = 3;
    map->elevation[2][15] = 2;
    map->elevation[3][16] = 1;
    map->elevation[9][1] = 1;
    map->elevation[9][2] = 2;
    map->elevation[13][5] = 1;
    map->elevation[13][6] = 1;
    map->elevation[0][14] = 1;
    map->elevation[0][15] = 2;
    map->elevation[0][16] = 1;

    // Ridge near bottom-left
    map->elevation[12][1] = 1;
    map->elevation[12][2] = 2;
    map->elevation[12][3] = 1;
    map->elevation[11][2] = 1;

    GridPos obstacles[] = {
        {2, 4}, {2, 10}, {6, 6}, {6, 9}, {10, 1}, {10, 8},
        {14, 7}, {14, 13}, {17, 2}, {17, 7}, {3, 13}, {11, 6},
        // Obstacles on elevated terrain
        {1, 1}, {2, 2}, {10, 5}, {11, 7}, {15, 11}, {16, 12},
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

bool MapSave(const Map *map, const char *filePath)
{
    char buf[4096];
    int len = MapSerialize(map, buf, sizeof(buf));
    if (len <= 0) return false;

    FILE *f = fopen(filePath, "w");
    if (!f) return false;
    fwrite(buf, 1, len, f);
    fclose(f);
    return true;
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

// 2D gradient noise for smooth terrain undulation (fixes seams between tiles)
static const unsigned char perm[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,
    69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,
    94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,
    171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,
    1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,
    164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,
    255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,
    19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,
    238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,
    181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static float NoiseGrad(int hash, float x, float z)
{
    int h = hash & 3;
    float u = h < 2 ? x : z;
    float v = h < 2 ? z : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

static float TerrainNoise(float x, float z)
{
    int xi = (int)(x >= 0 ? x : x - 1) & 255;
    int zi = (int)(z >= 0 ? z : z - 1) & 255;
    float xf = x - (int)(x >= 0 ? x : x - 1);
    float zf = z - (int)(z >= 0 ? z : z - 1);

    float u = SmoothStep(xf);
    float v = SmoothStep(zf);

    int aa = perm[(perm[xi] + zi) & 255];
    int ab = perm[(perm[xi] + zi + 1) & 255];
    int ba = perm[(perm[(xi + 1) & 255] + zi) & 255];
    int bb = perm[(perm[(xi + 1) & 255] + zi + 1) & 255];

    float x1 = NoiseGrad(aa, xf, zf) * (1 - u) + NoiseGrad(ba, xf - 1, zf) * u;
    float x2 = NoiseGrad(ab, xf, zf - 1) * (1 - u) + NoiseGrad(bb, xf - 1, zf - 1) * u;

    return (x1 * (1 - v) + x2 * v) * 0.08f;  // ~±0.08 amplitude
}

// --- Per-vertex baked terrain shadows (PS1-style) ---

// Returns shadow factor for grid corner (cx,cz): 1.0 = fully lit, 0.55 = shadowed
static float CornerShadow(const Map *map, int cx, int cz)
{
    // Light dir (0.4, -0.7, 0.3) — trace toward light on XZ plane
    float stepX = -0.4f;
    float stepZ = -0.3f;
    float len = sqrtf(stepX * stepX + stepZ * stepZ);
    stepX /= len;
    stepZ /= len;
    // Y rise per horizontal step — low rate for long, dramatic shadows
    float verticalRate = 0.3f;

    // Corner elevation = max of neighboring tiles (prevents self-shadowing
    // at plateau edges where averaging would lower the base)
    float baseElev = 0.0f;
    for (int dz = -1; dz <= 0; dz++) {
        for (int dx = -1; dx <= 0; dx++) {
            int tx = cx + dx, tz = cz + dz;
            if (tx >= 0 && tx < MAP_WIDTH && tz >= 0 && tz < MAP_HEIGHT) {
                float e = map->elevation[tz][tx] * ELEVATION_HEIGHT;
                if (e > baseElev) baseElev = e;
            }
        }
    }

    float fx = (float)cx;
    float fz = (float)cz;

    for (int i = 1; i <= 25; i++) {
        float sx = fx + stepX * i;
        float sz = fz + stepZ * i;
        int gx = (int)sx;
        int gz = (int)sz;
        if (gx < 0 || gx >= MAP_WIDTH || gz < 0 || gz >= MAP_HEIGHT) break;

        float sampleElev = map->elevation[gz][gx] * ELEVATION_HEIGHT;
        float rayHeight = baseElev + verticalRate * i;

        if (sampleElev > rayHeight) return 0.55f;  // In shadow
    }
    return 1.0f;  // Fully lit
}

// --- Pre-baked Map Mesh ---

// Max triangles: tile tops (300*2) + cliff faces (~3000) + island skirt (~1600) = ~5500
#define MAP_MESH_MAX_TRIS 8192

// --- Island Skirt Constants ---
#define ISLAND_SKIRT_WIDTH 12
#define ISLAND_WATER_Y -0.3f

// Returns 0.0-1.0 based on noise-perturbed elliptical distance from map center
static float IslandAlpha(float wx, float wz)
{
    float cx = MAP_WIDTH * TILE_SIZE * 0.5f;
    float cz = MAP_HEIGHT * TILE_SIZE * 0.5f;
    float rx = cx + ISLAND_SKIRT_WIDTH * TILE_SIZE * 0.4f;
    float rz = cz + ISLAND_SKIRT_WIDTH * TILE_SIZE * 0.4f;

    float dx = (wx - cx) / rx;
    float dz = (wz - cz) / rz;
    float dist = sqrtf(dx * dx + dz * dz);

    // Perturb with low-frequency noise for organic coastline
    float noise = TerrainNoise(wx * 0.15f, wz * 0.15f) * 4.0f;
    dist += noise;

    // Map: dist=0 → alpha=1 (center), dist=1 → alpha=0 (coast edge)
    return 1.0f - dist;
}

// Returns vertex Y: terrain noise inland, beach slope near coast, cliff drop below water
static float SkirtY(float alpha, float wx, float wz)
{
    if (alpha >= 0.25f) {
        // Inland — use terrain noise
        return TerrainNoise(wx, wz);
    } else if (alpha > 0.0f) {
        // Beach transition — slope down from terrain to near water
        float t = alpha / 0.25f;
        float noiseY = TerrainNoise(wx, wz);
        return noiseY * t + 0.05f * (1.0f - t);
    } else {
        // Below coastline — cliff drop to below water
        float depth = -alpha;
        float cliffY = ISLAND_WATER_Y - depth * 2.0f;
        if (cliffY < ISLAND_WATER_Y - 1.0f) cliffY = ISLAND_WATER_Y - 1.0f;
        return cliffY;
    }
}

// Returns skirt vertex color: smooth grass→sand blend, or dark cliff
static Color SkirtColor(float alpha, int xi, int zi, int corner)
{
    Color base;
    if (alpha > 0.65f) {
        base = (Color){ 100, 160, 80, 255 };  // pure grass
    } else if (alpha > 0.15f) {
        // Smooth blend from sand to grass across alpha 0.15–0.65
        float t = (alpha - 0.15f) / (0.65f - 0.15f);
        base = (Color){
            (unsigned char)(210 + (100 - 210) * t),
            (unsigned char)(190 + (160 - 190) * t),
            (unsigned char)(140 + (80 - 140) * t),
            255
        };
    } else if (alpha > 0.0f) {
        base = (Color){ 210, 190, 140, 255 };  // sand/beach
    } else {
        base = (Color){ 80, 70, 55, 255 };     // dark cliff below water
    }
    return PerturbColor(base, xi, zi, corner);
}

void MapBuildMesh(MapMesh *mm, const Map *map, Shader ps1Shader)
{
    if (mm->ready) MapFreeMesh(mm);

    // Temporary buffers for vertex data
    int maxVerts = MAP_MESH_MAX_TRIS * 3;
    float *vertices = malloc(maxVerts * 3 * sizeof(float));
    unsigned char *colors = malloc(maxVerts * 4 * sizeof(unsigned char));
    int vertCount = 0;

    #define EMIT_VERT(px, py, pz, cr, cg, cb, ca) do { \
        int _i = vertCount * 3; \
        vertices[_i+0] = (px); vertices[_i+1] = (py); vertices[_i+2] = (pz); \
        int _ci = vertCount * 4; \
        colors[_ci+0] = (cr); colors[_ci+1] = (cg); colors[_ci+2] = (cb); colors[_ci+3] = (ca); \
        vertCount++; \
    } while(0)

    // Precompute per-corner shadow grid: (MAP_WIDTH+1) x (MAP_HEIGHT+1) corners
    float shadow[MAP_HEIGHT + 1][MAP_WIDTH + 1];
    for (int cz = 0; cz <= MAP_HEIGHT; cz++)
        for (int cx = 0; cx <= MAP_WIDTH; cx++)
            shadow[cz][cx] = CornerShadow(map, cx, cz);

    // Pass 1: Tile top faces
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            Color base = TileBaseColor(map->tiles[z][x]);

            // Blend tiles near map edge toward sand for smooth transition
            int edgeDist = x;
            if (MAP_WIDTH - 1 - x < edgeDist) edgeDist = MAP_WIDTH - 1 - x;
            if (z < edgeDist) edgeDist = z;
            if (MAP_HEIGHT - 1 - z < edgeDist) edgeDist = MAP_HEIGHT - 1 - z;
            if (edgeDist < 3) {
                float t = edgeDist / 3.0f;  // 0 at edge, 1 at 3 tiles in
                base.r = (unsigned char)(210 + (base.r - 210) * t);
                base.g = (unsigned char)(190 + (base.g - 190) * t);
                base.b = (unsigned char)(140 + (base.b - 140) * t);
            }

            float elev = map->elevation[z][x] * ELEVATION_HEIGHT;

            float x0 = x * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z0 = z * TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            float y00 = elev + TerrainNoise(x, z);
            float y10 = elev + TerrainNoise(x + 1, z);
            float y01 = elev + TerrainNoise(x, z + 1);
            float y11 = elev + TerrainNoise(x + 1, z + 1);

            Color c0 = PerturbColor(base, x, z, 0);
            Color c1 = PerturbColor(base, x, z, 1);
            Color c2 = PerturbColor(base, x, z, 2);
            Color c3 = PerturbColor(base, x, z, 3);

            // Apply per-vertex shadow (smooth interpolation across triangles)
            float s00 = shadow[z][x];
            float s10 = shadow[z][x + 1];
            float s01 = shadow[z + 1][x];
            float s11 = shadow[z + 1][x + 1];

            c0.r *= s00; c0.g *= s00; c0.b *= s00;
            c1.r *= s10; c1.g *= s10; c1.b *= s10;
            c2.r *= s01; c2.g *= s01; c2.b *= s01;
            c3.r *= s11; c3.g *= s11; c3.b *= s11;

            // Triangle 1: (x0,z0), (x1,z1), (x1,z0)
            EMIT_VERT(x0, y00, z0, c0.r, c0.g, c0.b, 255);
            EMIT_VERT(x1, y11, z1, c3.r, c3.g, c3.b, 255);
            EMIT_VERT(x1, y10, z0, c1.r, c1.g, c1.b, 255);

            // Triangle 2: (x0,z0), (x0,z1), (x1,z1)
            EMIT_VERT(x0, y00, z0, c0.r, c0.g, c0.b, 255);
            EMIT_VERT(x0, y01, z1, c2.r, c2.g, c2.b, 255);
            EMIT_VERT(x1, y11, z1, c3.r, c3.g, c3.b, 255);
        }
    }

    // Pass 2: Cliff faces
    // Per-face directional brightness — light comes from -X, -Z (upper-left)
    // Lit faces (facing light): left (-X normal), north (-Z normal) → 1.0
    // Shadow faces (facing away): right (+X normal), south (+Z normal) → dark
    float rightBright = 0.35f;   // +X normal, facing away from light
    float leftBright  = 1.0f;    // -X normal, facing light
    float southBright = 0.45f;   // +Z normal, facing away from light
    float northBright = 1.0f;    // -Z normal, facing light

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            float elev = map->elevation[z][x] * ELEVATION_HEIGHT;
            Color base = TileBaseColor(map->tiles[z][x]);

            // Average shadow of tile's corners for cliff darkening
            float tileShadow = (shadow[z][x] + shadow[z][x+1] +
                                shadow[z+1][x] + shadow[z+1][x+1]) * 0.25f;

            float x0 = x * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z0 = z * TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            // Right neighbor (x+1)
            float neighborElev;
            if (x + 1 < MAP_WIDTH)
                neighborElev = map->elevation[z][x + 1] * ELEVATION_HEIGHT;
            else
                neighborElev = 0.0f;

            if (elev > neighborElev) {
                Color rc = { (unsigned char)(base.r * rightBright * tileShadow),
                             (unsigned char)(base.g * rightBright * tileShadow),
                             (unsigned char)(base.b * rightBright * tileShadow), 255 };
                EMIT_VERT(x1, elev, z0, rc.r, rc.g, rc.b, 255);
                EMIT_VERT(x1, neighborElev, z1, rc.r, rc.g, rc.b, 255);
                EMIT_VERT(x1, neighborElev, z0, rc.r, rc.g, rc.b, 255);
                EMIT_VERT(x1, elev, z0, rc.r, rc.g, rc.b, 255);
                EMIT_VERT(x1, elev, z1, rc.r, rc.g, rc.b, 255);
                EMIT_VERT(x1, neighborElev, z1, rc.r, rc.g, rc.b, 255);
            } else if (elev < neighborElev) {
                // Reverse cliff: face normal is -X (left-facing), use neighbor's shadow
                Color nBase = (x + 1 < MAP_WIDTH) ? TileBaseColor(map->tiles[z][x + 1]) : base;
                float nShadow = (x + 1 < MAP_WIDTH) ?
                    (shadow[z][x+1] + shadow[z][x+2] + shadow[z+1][x+1] + shadow[z+1][x+2]) * 0.25f : 1.0f;
                Color nCliff = { (unsigned char)(nBase.r * leftBright * nShadow),
                                 (unsigned char)(nBase.g * leftBright * nShadow),
                                 (unsigned char)(nBase.b * leftBright * nShadow), 255 };
                EMIT_VERT(x1, neighborElev, z0, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, elev, z0, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, elev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, neighborElev, z0, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, elev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, neighborElev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
            }

            // Bottom neighbor (z+1)
            if (z + 1 < MAP_HEIGHT)
                neighborElev = map->elevation[z + 1][x] * ELEVATION_HEIGHT;
            else
                neighborElev = 0.0f;

            if (elev > neighborElev) {
                Color sc = { (unsigned char)(base.r * southBright * tileShadow),
                             (unsigned char)(base.g * southBright * tileShadow),
                             (unsigned char)(base.b * southBright * tileShadow), 255 };
                EMIT_VERT(x0, elev, z1, sc.r, sc.g, sc.b, 255);
                EMIT_VERT(x0, neighborElev, z1, sc.r, sc.g, sc.b, 255);
                EMIT_VERT(x1, neighborElev, z1, sc.r, sc.g, sc.b, 255);
                EMIT_VERT(x0, elev, z1, sc.r, sc.g, sc.b, 255);
                EMIT_VERT(x1, neighborElev, z1, sc.r, sc.g, sc.b, 255);
                EMIT_VERT(x1, elev, z1, sc.r, sc.g, sc.b, 255);
            } else if (elev < neighborElev) {
                // Reverse cliff: face normal is -Z (north-facing), use neighbor's shadow
                Color nBase = (z + 1 < MAP_HEIGHT) ? TileBaseColor(map->tiles[z + 1][x]) : base;
                float nShadow = (z + 1 < MAP_HEIGHT) ?
                    (shadow[z+1][x] + shadow[z+1][x+1] + shadow[z+2][x] + shadow[z+2][x+1]) * 0.25f : 1.0f;
                Color nCliff = { (unsigned char)(nBase.r * northBright * nShadow),
                                 (unsigned char)(nBase.g * northBright * nShadow),
                                 (unsigned char)(nBase.b * northBright * nShadow), 255 };
                EMIT_VERT(x0, neighborElev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, elev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x0, elev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x0, neighborElev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, neighborElev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
                EMIT_VERT(x1, elev, z1, nCliff.r, nCliff.g, nCliff.b, 255);
            }

            // Left edge (x=0): face normal is -X
            if (x == 0 && elev > 0.0f) {
                Color lc = { (unsigned char)(base.r * leftBright * tileShadow),
                             (unsigned char)(base.g * leftBright * tileShadow),
                             (unsigned char)(base.b * leftBright * tileShadow), 255 };
                EMIT_VERT(x0, elev, z1, lc.r, lc.g, lc.b, 255);
                EMIT_VERT(x0, 0.0f, z0, lc.r, lc.g, lc.b, 255);
                EMIT_VERT(x0, 0.0f, z1, lc.r, lc.g, lc.b, 255);
                EMIT_VERT(x0, elev, z1, lc.r, lc.g, lc.b, 255);
                EMIT_VERT(x0, elev, z0, lc.r, lc.g, lc.b, 255);
                EMIT_VERT(x0, 0.0f, z0, lc.r, lc.g, lc.b, 255);
            }

            // Top edge (z=0): face normal is -Z
            if (z == 0 && elev > 0.0f) {
                Color nc = { (unsigned char)(base.r * northBright * tileShadow),
                             (unsigned char)(base.g * northBright * tileShadow),
                             (unsigned char)(base.b * northBright * tileShadow), 255 };
                EMIT_VERT(x0, elev, z0, nc.r, nc.g, nc.b, 255);
                EMIT_VERT(x1, 0.0f, z0, nc.r, nc.g, nc.b, 255);
                EMIT_VERT(x0, 0.0f, z0, nc.r, nc.g, nc.b, 255);
                EMIT_VERT(x0, elev, z0, nc.r, nc.g, nc.b, 255);
                EMIT_VERT(x1, elev, z0, nc.r, nc.g, nc.b, 255);
                EMIT_VERT(x1, 0.0f, z0, nc.r, nc.g, nc.b, 255);
            }
        }
    }

    // Pass 3: Island skirt — extends terrain with beach and cliff around map edges
    for (int zi = -ISLAND_SKIRT_WIDTH; zi < MAP_HEIGHT + ISLAND_SKIRT_WIDTH; zi++) {
        for (int xi = -ISLAND_SKIRT_WIDTH; xi < MAP_WIDTH + ISLAND_SKIRT_WIDTH; xi++) {
            // Skip cells inside the existing map grid
            if (xi >= 0 && xi < MAP_WIDTH && zi >= 0 && zi < MAP_HEIGHT) continue;

            float x0 = xi * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z0 = zi * TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            // Corner alphas and Y values
            float a00 = IslandAlpha(x0, z0);
            float a10 = IslandAlpha(x1, z0);
            float a01 = IslandAlpha(x0, z1);
            float a11 = IslandAlpha(x1, z1);

            // Skip cells fully underwater (all corners well below coast)
            if (a00 < -0.3f && a10 < -0.3f && a01 < -0.3f && a11 < -0.3f) continue;

            // For border cells adjacent to the map, stitch Y to match map terrain noise
            float y00, y10, y01, y11;
            if (xi >= -1 && xi <= MAP_WIDTH && zi >= -1 && zi <= MAP_HEIGHT) {
                // Use terrain noise at integer coords to match map grid corners
                int cx0 = xi, cz0 = zi;
                int cx1 = xi + 1, cz1 = zi + 1;
                float e00 = (cx0 >= 0 && cx0 < MAP_WIDTH && cz0 >= 0 && cz0 < MAP_HEIGHT)
                            ? map->elevation[cz0][cx0] * ELEVATION_HEIGHT : 0.0f;
                float e10 = (cx1 >= 0 && cx1 < MAP_WIDTH && cz0 >= 0 && cz0 < MAP_HEIGHT)
                            ? map->elevation[cz0][cx1] * ELEVATION_HEIGHT : 0.0f;
                float e01 = (cx0 >= 0 && cx0 < MAP_WIDTH && cz1 >= 0 && cz1 < MAP_HEIGHT)
                            ? map->elevation[cz1][cx0] * ELEVATION_HEIGHT : 0.0f;
                float e11 = (cx1 >= 0 && cx1 < MAP_WIDTH && cz1 >= 0 && cz1 < MAP_HEIGHT)
                            ? map->elevation[cz1][cx1] * ELEVATION_HEIGHT : 0.0f;

                y00 = (a00 >= 0.15f) ? e00 + TerrainNoise(cx0, cz0) : SkirtY(a00, x0, z0);
                y10 = (a10 >= 0.15f) ? e10 + TerrainNoise(cx1, cz0) : SkirtY(a10, x1, z0);
                y01 = (a01 >= 0.15f) ? e01 + TerrainNoise(cx0, cz1) : SkirtY(a01, x0, z1);
                y11 = (a11 >= 0.15f) ? e11 + TerrainNoise(cx1, cz1) : SkirtY(a11, x1, z1);
            } else {
                y00 = SkirtY(a00, x0, z0);
                y10 = SkirtY(a10, x1, z0);
                y01 = SkirtY(a01, x0, z1);
                y11 = SkirtY(a11, x1, z1);
            }

            Color c0 = SkirtColor(a00, xi, zi, 0);
            Color c1 = SkirtColor(a10, xi, zi, 1);
            Color c2 = SkirtColor(a01, xi, zi, 2);
            Color c3 = SkirtColor(a11, xi, zi, 3);

            EMIT_VERT(x0, y00, z0, c0.r, c0.g, c0.b, 255);
            EMIT_VERT(x1, y11, z1, c3.r, c3.g, c3.b, 255);
            EMIT_VERT(x1, y10, z0, c1.r, c1.g, c1.b, 255);

            EMIT_VERT(x0, y00, z0, c0.r, c0.g, c0.b, 255);
            EMIT_VERT(x0, y01, z1, c2.r, c2.g, c2.b, 255);
            EMIT_VERT(x1, y11, z1, c3.r, c3.g, c3.b, 255);
        }
    }

    #undef EMIT_VERT

    // Build Raylib Mesh
    mm->mesh = (Mesh){0};
    mm->mesh.vertexCount = vertCount;
    mm->mesh.triangleCount = vertCount / 3;
    mm->mesh.vertices = vertices;
    mm->mesh.colors = colors;

    // Generate normals for lighting
    mm->mesh.normals = malloc(vertCount * 3 * sizeof(float));
    for (int t = 0; t < mm->mesh.triangleCount; t++) {
        int i0 = t * 3;
        Vector3 v0 = { vertices[i0*3], vertices[i0*3+1], vertices[i0*3+2] };
        Vector3 v1 = { vertices[(i0+1)*3], vertices[(i0+1)*3+1], vertices[(i0+1)*3+2] };
        Vector3 v2 = { vertices[(i0+2)*3], vertices[(i0+2)*3+1], vertices[(i0+2)*3+2] };
        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 n = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
        for (int k = 0; k < 3; k++) {
            mm->mesh.normals[(i0+k)*3+0] = n.x;
            mm->mesh.normals[(i0+k)*3+1] = n.y;
            mm->mesh.normals[(i0+k)*3+2] = n.z;
        }
    }

    UploadMesh(&mm->mesh, false);

    // Setup material with PS1 shader
    mm->material = LoadMaterialDefault();
    mm->material.shader = ps1Shader;
    mm->ready = true;
}

void MapDrawMesh(const MapMesh *mm)
{
    if (!mm->ready) return;
    DrawMesh(mm->mesh, mm->material, MatrixIdentity());
}

void MapFreeMesh(MapMesh *mm)
{
    if (!mm->ready) return;
    UnloadMesh(mm->mesh);
    mm->ready = false;
}

void MapDraw(const Map *map)
{
    // Pass 1: Tile top faces (quads as triangle pairs via rlgl)
    rlBegin(RL_TRIANGLES);
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            Color base = TileBaseColor(map->tiles[z][x]);
            float elev = map->elevation[z][x] * ELEVATION_HEIGHT;

            float x0 = x * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z0 = z * TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            // Corner Y values with smooth noise (absolute coords → shared vertices match)
            float y00 = elev + TerrainNoise(x, z);
            float y10 = elev + TerrainNoise(x + 1, z);
            float y01 = elev + TerrainNoise(x, z + 1);
            float y11 = elev + TerrainNoise(x + 1, z + 1);

            Color c0 = PerturbColor(base, x, z, 0);
            Color c1 = PerturbColor(base, x, z, 1);
            Color c2 = PerturbColor(base, x, z, 2);
            Color c3 = PerturbColor(base, x, z, 3);

            // Triangle 1: (x0,z0), (x1,z1), (x1,z0) — CCW from above
            rlColor4ub(c0.r, c0.g, c0.b, 255);
            rlVertex3f(x0, y00, z0);
            rlColor4ub(c3.r, c3.g, c3.b, 255);
            rlVertex3f(x1, y11, z1);
            rlColor4ub(c1.r, c1.g, c1.b, 255);
            rlVertex3f(x1, y10, z0);

            // Triangle 2: (x0,z0), (x0,z1), (x1,z1) — CCW from above
            rlColor4ub(c0.r, c0.g, c0.b, 255);
            rlVertex3f(x0, y00, z0);
            rlColor4ub(c2.r, c2.g, c2.b, 255);
            rlVertex3f(x0, y01, z1);
            rlColor4ub(c3.r, c3.g, c3.b, 255);
            rlVertex3f(x1, y11, z1);
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
                // Vertical wall on right edge — CCW from +X
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x1, elev, z0);
                rlVertex3f(x1, neighborElev, z1);
                rlVertex3f(x1, neighborElev, z0);

                rlVertex3f(x1, elev, z0);
                rlVertex3f(x1, elev, z1);
                rlVertex3f(x1, neighborElev, z1);
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
                rlVertex3f(x1, elev, z1);
                rlVertex3f(x0, elev, z1);

                rlVertex3f(x0, neighborElev, z1);
                rlVertex3f(x1, neighborElev, z1);
                rlVertex3f(x1, elev, z1);
            }

            // Check left neighbor (x-1) — only needed for edge at x=0
            if (x == 0 && elev > 0.0f) {
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x0, elev, z1);
                rlVertex3f(x0, 0.0f, z0);
                rlVertex3f(x0, 0.0f, z1);

                rlVertex3f(x0, elev, z1);
                rlVertex3f(x0, elev, z0);
                rlVertex3f(x0, 0.0f, z0);
            }

            // Check top neighbor (z=0) — only needed for edge at z=0
            if (z == 0 && elev > 0.0f) {
                rlColor4ub(cliff.r, cliff.g, cliff.b, 255);
                rlVertex3f(x0, elev, z0);
                rlVertex3f(x1, 0.0f, z0);
                rlVertex3f(x0, 0.0f, z0);

                rlVertex3f(x0, elev, z0);
                rlVertex3f(x1, elev, z0);
                rlVertex3f(x1, 0.0f, z0);
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

// --- Tree Placement ---

// Simple hash for deterministic per-tile RNG
static unsigned int TreeHash(unsigned int x, unsigned int z, unsigned int seed)
{
    unsigned int h = seed;
    h ^= x * 2654435761u;
    h ^= z * 2246822519u;
    h ^= (h >> 16);
    h *= 0x45d9f3b;
    h ^= (h >> 16);
    return h;
}

void MapPlaceTrees(TreeInstance *trees, int *count, const Map *map, unsigned int seed)
{
    *count = 0;
    for (int z = 0; z < MAP_HEIGHT && *count < MAX_TREES; z++) {
        for (int x = 0; x < MAP_WIDTH && *count < MAX_TREES; x++) {
            if (map->tiles[z][x] != TILE_EMPTY) continue;

            unsigned int h = TreeHash((unsigned int)x, (unsigned int)z, seed);
            // ~25% chance of a tree
            if ((h % 100) >= 25) continue;

            float elevY = map->elevation[z][x] * ELEVATION_HEIGHT;
            // Random offset within tile (avoid edges)
            float offX = ((float)((h >> 8) % 100) / 100.0f - 0.5f) * 0.5f;
            float offZ = ((float)((h >> 16) % 100) / 100.0f - 0.5f) * 0.5f;

            TreeInstance *t = &trees[*count];
            t->position = (Vector3){
                x * TILE_SIZE + TILE_SIZE * 0.5f + offX,
                elevY,
                z * TILE_SIZE + TILE_SIZE * 0.5f + offZ,
            };
            t->rotation = (float)(h % 360);
            t->scale = 0.8f + (float)((h >> 4) % 40) / 100.0f; // 0.8 - 1.2
            (*count)++;
        }
    }
}
