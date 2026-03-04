#include "map.h"
#include "raylib.h"
#include "rlgl.h"

void MapInit(Map *map)
{
    // Clear all tiles to empty
    for (int z = 0; z < MAP_HEIGHT; z++)
        for (int x = 0; x < MAP_WIDTH; x++)
            map->tiles[z][x] = TILE_EMPTY;

    // Define S-shaped path as waypoints
    // Path: spawn on left edge, zigzag across, base on right edge
    GridPos path[] = {
        {0, 7},   // spawn
        {4, 7},
        {4, 2},
        {8, 2},
        {8, 12},
        {12, 12},
        {12, 4},
        {16, 4},
        {16, 10},
        {19, 10}, // base
    };
    int pathLen = sizeof(path) / sizeof(path[0]);

    // Copy waypoints
    map->waypointCount = pathLen;
    for (int i = 0; i < pathLen; i++)
        map->waypoints[i] = path[i];

    // Mark spawn and base
    map->tiles[path[0].z][path[0].x] = TILE_SPAWN;
    map->tiles[path[pathLen - 1].z][path[pathLen - 1].x] = TILE_BASE;

    // Trace path between consecutive waypoints
    for (int i = 0; i < pathLen - 1; i++) {
        int x0 = path[i].x, z0 = path[i].z;
        int x1 = path[i + 1].x, z1 = path[i + 1].z;

        // Horizontal segment
        if (z0 == z1) {
            int minX = x0 < x1 ? x0 : x1;
            int maxX = x0 > x1 ? x0 : x1;
            for (int x = minX; x <= maxX; x++) {
                if (map->tiles[z0][x] == TILE_EMPTY)
                    map->tiles[z0][x] = TILE_PATH;
            }
        }
        // Vertical segment
        else if (x0 == x1) {
            int minZ = z0 < z1 ? z0 : z1;
            int maxZ = z0 > z1 ? z0 : z1;
            for (int z = minZ; z <= maxZ; z++) {
                if (map->tiles[z][x0] == TILE_EMPTY)
                    map->tiles[z][x0] = TILE_PATH;
            }
        }
    }

    // Scatter some obstacles on empty tiles
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
            // Draw thin border
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
