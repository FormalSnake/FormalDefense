#include "raylib.h"
#include "rlgl.h"

int main(void)
{
    InitWindow(800, 600, "Hello 3D World");
    SetTargetFPS(60);

    Camera3D camera = {
        .position   = (Vector3){ 4.0f, 4.0f, 4.0f },
        .target     = (Vector3){ 0.0f, 0.5f, 0.0f },
        .up         = (Vector3){ 0.0f, 1.0f, 0.0f },
        .fovy       = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    float rotationAngle = 0.0f;

    while (!WindowShouldClose())
    {
        rotationAngle += 90.0f * GetFrameTime();

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                rlPushMatrix();
                    rlRotatef(rotationAngle, 0.0f, 1.0f, 0.0f);
                    DrawCubeV((Vector3){ 0.0f, 0.5f, 0.0f }, (Vector3){ 1.0f, 1.0f, 1.0f }, RED);
                    DrawCubeWiresV((Vector3){ 0.0f, 0.5f, 0.0f }, (Vector3){ 1.0f, 1.0f, 1.0f }, MAROON);
                rlPopMatrix();
                DrawGrid(10, 1.0f);
            EndMode3D();

            DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
