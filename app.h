#ifndef APP_H
#define APP_H

#include "raylib.h"
#include "map.h"
#include "entity.h"
#include "game.h"
#include "net.h"
#include "lobby.h"
#include "chat.h"
#include "settings.h"
#include "progress.h"

// Forward declare SceneManager
typedef struct SceneManager SceneManager;

typedef struct {
    // Rendering
    Shader ps1Shader, waterShader;
    RenderTexture2D renderTarget;
    int rtW, rtH;
    int locResolution, locJitter, locLightDir, locLightColor, locAmbientColor, locColorBands;
    int wLocResolution;
    Model sphereModel, treeModel, zombieModel;
    MapMesh menuMapMesh, gameMapMesh;
    TreeInstance menuTrees[MAX_TREES], gameTrees[MAX_TREES];
    int menuTreeCount, gameTreeCount;
    float totalTime;
    int lastTowerCount;

    // Maps
    Map menuMap, map;
    MapRegistry mapRegistry;
    int selectedMapIdx;
    bool mapSelectForMultiplayer;

    // Entities
    GameState gs;
    Enemy enemies[MAX_ENEMIES];
    Tower towers[MAX_TOWERS];
    Projectile projectiles[MAX_PROJECTILES];

    // Camera
    Camera3D camera, menuCamera;

    // Game UI
    int selectedTowerType, selectedTowerIdx;
    GamePhase phaseBeforePause;
    bool localPaused;

    // Progression
    PlayerProfile profile;
    RunModifiers runMods;
    EndlessState endlessState;
    int perkOffered[3];
    unsigned int perkSeed;
    bool crystalsSaved;
    Difficulty selectedDifficulty;

    // Multiplayer
    NetContext netCtx;
    LobbyState lobbyState;
    ChatState chatState;

    // Settings
    Settings settings;
    SettingsState settingsState;

    // Scene manager
    SceneManager *sceneManager;
} AppContext;

// Camera controller (shared by menu and game)
typedef struct {
    Vector3 target;
    float distance;
    float yaw;
    float pitch;
    float panSpeed;
    float rotSpeed;
    float zoomSpeed;
} CameraController;

// Stored in AppContext-accessible location
extern CameraController g_camCtrl;
extern CameraController g_menuCamCtrl;

void CameraControllerInit(CameraController *cc);
void CameraControllerUpdate(CameraController *cc, Camera3D *cam, float dt);

// Mouse ray ground intersection
bool GetMouseGroundPos(Camera3D camera, const Map *map, Vector3 *outPos);

// Range circle rendering
void DrawRangeCircle(Vector3 center, float radius, Color color);

// Skybox/water (built once, shared)
void BuildSkyboxMesh(void);
void DrawSkybox(Camera3D camera);
void BuildWaterMesh(Shader waterShader);
void DrawWater(Shader waterShader, int waterLocTime, float totalTime);
void InitBlobShadowTable(void);

typedef struct {
    float x, z;
    float radius;
    float elevY;
} BlobShadowEntry;

#define MAX_BLOB_SHADOWS (MAX_TOWERS + MAX_ENEMIES)
void DrawBlobShadowsBatched(const BlobShadowEntry *entries, int count);

// Game scene reset helper
void GameSceneReset(AppContext *app);

// Chat callback setup
extern ChatState *g_chatStatePtr;
void OnNetChatReceived(uint8_t playerIndex, const char *username, const char *message);

#endif
