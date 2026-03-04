#ifndef FD_GFX_H
#define FD_GFX_H

#include "fd_math.h"
#include <stdbool.h>

// --- Lifecycle ---
void FdGfxInit(void);
void FdGfxShutdown(void);

// --- Render targets ---
typedef struct FdRenderTarget FdRenderTarget;
FdRenderTarget *FdRenderTargetCreate(int w, int h);
void FdRenderTargetDestroy(FdRenderTarget *rt);
void FdRenderTargetBegin(FdRenderTarget *rt, Color clear);
void FdRenderTargetEnd(void);
void FdRenderTargetBlit(FdRenderTarget *rt, int dstW, int dstH);  // point-filtered fullscreen

// --- Frame ---
void FdBeginFrame(Color clear);
void FdEndFrame(void);

// --- 3D mode ---
void FdBegin3D(FdMat4 view, FdMat4 proj);
void FdEnd3D(void);

// --- Meshes (opaque, backend-owned) ---
typedef struct FdMesh FdMesh;

// Create mesh from raw vertex data (positions, vertex colors, normals)
// positions: float[vertCount*3], colors: uint8_t[vertCount*4], normals: float[vertCount*3] (nullable)
FdMesh *FdMeshCreate(const float *positions, const unsigned char *colors, const float *normals, int vertCount);
void FdMeshDestroy(FdMesh *mesh);

// --- Sphere mesh (shared, ref-counted internally) ---
void FdSphereMeshInit(void);     // call once at startup
void FdSphereMeshShutdown(void); // call once at shutdown

// --- 3D draw calls ---
void FdDrawMesh(const FdMesh *mesh, FdMat4 model, bool usePS1Shader);
void FdDrawCube(Vector3 pos, Vector3 size, Color color);
void FdDrawCubeWires(Vector3 pos, Vector3 size, Color color);
void FdDrawSphere(Vector3 pos, float radius, Color color);
void FdDrawLine3D(Vector3 a, Vector3 b, Color color);

// --- Immediate-mode triangles (for blob shadows, etc.) ---
void FdBeginTriangles(void);
void FdTriVertex3f(float x, float y, float z);
void FdTriColor4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void FdEndTriangles(void);

// --- GPU state ---
void FdDisableBackfaceCulling(void);
void FdEnableBackfaceCulling(void);
void FdDisableDepthWrite(void);
void FdEnableDepthWrite(void);

// --- PS1 shader control ---
void FdPS1ShaderSetParams(float jitter, float bands, Vector3 lightDir, Vector3 lightColor, Vector3 ambient);
void FdPS1ShaderSetResolution(float w, float h);

// --- 2D drawing (screen-space) ---
void FdDrawRect(int x, int y, int w, int h, Color color);
void FdDrawRectLines(int x, int y, int w, int h, Color color);
void FdDrawRectLinesEx(int x, int y, int w, int h, int thick, Color color);
void FdDrawText(const char *text, int x, int y, int size, Color color);
int  FdMeasureText(const char *text, int size);
void FdDrawFPS(int x, int y);

// 2D lines and circles (editor)
void FdDrawLine2D(int x1, int y1, int x2, int y2, Color color);
void FdDrawCircle2D(int cx, int cy, int radius, Color color);

// --- Projection helpers ---
Ray FdScreenToWorldRay(Vector2 mousePos, FdMat4 view, FdMat4 proj, int screenW, int screenH);
Vector2 FdWorldToScreen(Vector3 worldPos, FdMat4 vp, int screenW, int screenH);

// --- Text formatting helper ---
const char *FdTextFormat(const char *fmt, ...);

// --- Raylib compatibility macros ---
#define DrawRectangle(x,y,w,h,c)       FdDrawRect(x,y,w,h,c)
#define DrawRectangleLines(x,y,w,h,c)  FdDrawRectLines(x,y,w,h,c)
#define DrawRectangleRec(r,c)          FdDrawRect((int)(r).x,(int)(r).y,(int)(r).w,(int)(r).h,c)
#define DrawRectangleLinesEx(r,t,c)    FdDrawRectLinesEx((int)(r).x,(int)(r).y,(int)(r).w,(int)(r).h,t,c)
#define DrawText(s,x,y,sz,c)           FdDrawText(s,x,y,sz,c)
#define MeasureText(s,sz)              FdMeasureText(s,sz)
#define DrawFPS(x,y)                   FdDrawFPS(x,y)
#define TextFormat(...)                FdTextFormat(__VA_ARGS__)
#define DrawLine3D(a,b,c)             FdDrawLine3D(a,b,c)
#define DrawCubeV(p,s,c)              FdDrawCube(p,s,c)
#define DrawCubeWiresV(p,s,c)         FdDrawCubeWires(p,s,c)
#define DrawLine(x1,y1,x2,y2,c)       FdDrawLine2D(x1,y1,x2,y2,c)
#define DrawCircle(cx,cy,r,c)          FdDrawCircle2D(cx,cy,(int)(r),c)

#endif // FD_GFX_H
