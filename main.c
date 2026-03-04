#include "raylib.h"
#include "map.h"
#include "entity.h"
#include "game.h"

int main(void)
{
    InitWindow(1280, 720, "Formal Defense");
    SetTargetFPS(60);

    Map map;
    MapInit(&map);

    GameState gs;
    GameStateInit(&gs);

    Camera3D camera = {
        .position   = (Vector3){ 10.0f, 12.0f, 18.0f },
        .target     = (Vector3){ 10.0f, 0.0f, 7.5f },
        .up         = (Vector3){ 0.0f, 1.0f, 0.0f },
        .fovy       = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                MapDraw(&map);
            EndMode3D();
            DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
