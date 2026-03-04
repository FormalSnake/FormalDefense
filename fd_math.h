#ifndef FD_MATH_H
#define FD_MATH_H

#include "HandmadeMath.h"
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

// --- Core types using lowercase field names (game code convention) ---
// These are layout-compatible with HMM types via unions.

typedef struct { float x, y; } FdVec2;
typedef struct { float x, y, z; } FdVec3;
typedef struct { float x, y, z, w; } FdVec4;
typedef HMM_Mat4 FdMat4;

typedef struct { uint8_t r, g, b, a; } FdColor;
typedef struct { float x, y, w, h; } FdRect;

// Convenience aliases
typedef FdVec2 Vector2;
typedef FdVec3 Vector3;
typedef FdVec4 Vector4;
typedef FdColor Color;
typedef FdMat4 Matrix;

typedef struct { Vector3 position; Vector3 direction; } FdRay;
typedef FdRay Ray;

// --- HMM conversion helpers (inline, zero-cost) ---

static inline HMM_Vec2 _toHMM2(FdVec2 v) { return (HMM_Vec2){v.x, v.y}; }
static inline HMM_Vec3 _toHMM3(FdVec3 v) { return (HMM_Vec3){v.x, v.y, v.z}; }
static inline HMM_Vec4 _toHMM4(FdVec4 v) { return (HMM_Vec4){v.x, v.y, v.z, v.w}; }
static inline FdVec2 _fromHMM2(HMM_Vec2 v) { return (FdVec2){v.X, v.Y}; }
static inline FdVec3 _fromHMM3(HMM_Vec3 v) { return (FdVec3){v.X, v.Y, v.Z}; }
static inline FdVec4 _fromHMM4(HMM_Vec4 v) { return (FdVec4){v.X, v.Y, v.Z, v.W}; }

// --- Math constants ---

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#ifndef DEG2RAD
#define DEG2RAD (PI / 180.0f)
#endif

#ifndef RAD2DEG
#define RAD2DEG (180.0f / PI)
#endif

// --- Color constants ---

#define WHITE      (Color){255, 255, 255, 255}
#define BLACK      (Color){0, 0, 0, 255}
#define RED        (Color){230, 41, 55, 255}
#define GREEN      (Color){0, 228, 48, 255}
#define BLUE       (Color){0, 121, 241, 255}
#define YELLOW     (Color){253, 249, 0, 255}
#define GOLD       (Color){255, 203, 0, 255}
#define ORANGE     (Color){255, 161, 0, 255}
#define PURPLE     (Color){200, 122, 255, 255}
#define DARKGRAY   (Color){80, 80, 80, 255}
#define LIGHTGRAY  (Color){200, 200, 200, 255}
#define GRAY       (Color){130, 130, 130, 255}
#define MAROON     (Color){190, 33, 55, 255}
#define LIME       (Color){0, 158, 47, 255}
#define DARKGREEN  (Color){0, 117, 44, 255}

// --- Vector2 operations ---

static inline Vector2 Vector2Zero(void) { return (Vector2){0.0f, 0.0f}; }

// --- Vector3 operations ---

static inline Vector3 Vector3Zero(void) { return (Vector3){0.0f, 0.0f, 0.0f}; }

static inline Vector3 Vector3Add(Vector3 a, Vector3 b) {
    return _fromHMM3(HMM_AddV3(_toHMM3(a), _toHMM3(b)));
}
static inline Vector3 Vector3Subtract(Vector3 a, Vector3 b) {
    return _fromHMM3(HMM_SubV3(_toHMM3(a), _toHMM3(b)));
}
static inline Vector3 Vector3Scale(Vector3 v, float s) {
    return _fromHMM3(HMM_MulV3F(_toHMM3(v), s));
}
static inline float Vector3Length(Vector3 v) {
    return HMM_LenV3(_toHMM3(v));
}
static inline Vector3 Vector3Normalize(Vector3 v) {
    return _fromHMM3(HMM_NormV3(_toHMM3(v)));
}
static inline float Vector3Distance(Vector3 a, Vector3 b) {
    return HMM_LenV3(HMM_SubV3(_toHMM3(a), _toHMM3(b)));
}
static inline Vector3 Vector3CrossProduct(Vector3 a, Vector3 b) {
    return _fromHMM3(HMM_Cross(_toHMM3(a), _toHMM3(b)));
}
static inline float Vector3DotProduct(Vector3 a, Vector3 b) {
    return HMM_DotV3(_toHMM3(a), _toHMM3(b));
}

// --- Matrix operations ---

static inline Matrix MatrixIdentity(void) {
    return HMM_M4D(1.0f);
}
static inline Matrix MatrixTranslate(float x, float y, float z) {
    return HMM_Translate((HMM_Vec3){x, y, z});
}
static inline Matrix MatrixScale(float x, float y, float z) {
    return HMM_Scale((HMM_Vec3){x, y, z});
}
static inline Matrix MatrixMultiply(Matrix a, Matrix b) {
    return HMM_MulM4(a, b);
}
static inline Matrix MatrixPerspective(float fovY, float aspect, float nearPlane, float farPlane) {
    return HMM_Perspective_RH_NO(fovY * DEG2RAD, aspect, nearPlane, farPlane);
}
static inline Matrix MatrixLookAt(Vector3 eye, Vector3 target, Vector3 up) {
    return HMM_LookAt_RH(_toHMM3(eye), _toHMM3(target), _toHMM3(up));
}

// --- Utility ---

static inline int FdRandomValue(int min, int max) {
    if (min > max) { int t = min; min = max; max = t; }
    return min + (rand() % (max - min + 1));
}

#define GetRandomValue FdRandomValue

#endif // FD_MATH_H
