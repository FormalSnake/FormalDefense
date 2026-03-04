#ifndef MAP_H
#define MAP_H

#include "raylib.h"
#include <stdbool.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 15
#define TILE_SIZE 1.0f
#define MAX_WAYPOINTS 32

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
    TileType tiles[MAP_HEIGHT][MAP_WIDTH];
    GridPos waypoints[MAX_WAYPOINTS];
    int waypointCount;
} Map;

void MapInit(Map *map);
Vector3 MapGridToWorld(GridPos pos);
GridPos MapWorldToGrid(Vector3 worldPos);
bool MapCanPlaceTower(const Map *map, GridPos pos);
void MapDraw(const Map *map);

#endif
