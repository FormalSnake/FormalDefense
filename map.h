#ifndef MAP_H
#define MAP_H

#include "raylib.h"
#include <stdbool.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 15
#define TILE_SIZE 1.0f
#define MAX_WAYPOINTS 32
#define MAX_MAP_NAME 64
#define MAX_MAPS 16
#define ELEVATION_HEIGHT 0.5f
#define MAX_ELEVATION 4

typedef enum {
    TILE_EMPTY,
    TILE_PATH,
    TILE_OBSTACLE,
    TILE_SPAWN,
    TILE_BASE,
    TILE_TOWER,
} TileType;

typedef struct {
    int x;
    int z;
} GridPos;

typedef struct {
    char name[MAX_MAP_NAME];
    TileType tiles[MAP_HEIGHT][MAP_WIDTH];
    int elevation[MAP_HEIGHT][MAP_WIDTH];
    GridPos waypoints[MAX_WAYPOINTS];
    int waypointCount;
} Map;

typedef struct {
    char names[MAX_MAPS][MAX_MAP_NAME];
    char paths[MAX_MAPS][256];
    int count;
} MapRegistry;

void MapInit(Map *map);
Vector3 MapGridToWorld(GridPos pos);
Vector3 MapGridToWorldElevated(const Map *map, GridPos pos);
float MapGetElevationY(const Map *map, int x, int z);
GridPos MapWorldToGrid(Vector3 worldPos);
bool MapCanPlaceTower(const Map *map, GridPos pos);
void MapDraw(const Map *map);

bool MapLoad(Map *map, const char *filePath);
void MapSave(const Map *map, const char *filePath);
bool MapLoadFromBuffer(Map *map, const char *data, int len);
int MapSerialize(const Map *map, char *buf, int bufSize);

void MapRegistryScan(MapRegistry *reg, const char *directory);

#endif
