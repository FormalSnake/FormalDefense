#ifndef SCENE_H
#define SCENE_H

typedef enum {
    SCENE_MENU,
    SCENE_MAP_SELECT,
    SCENE_DIFFICULTY_SELECT,
    SCENE_SHOP,
    SCENE_PERK_SELECT,
    SCENE_LOBBY,
    SCENE_GAME,
    SCENE_COUNT,
} SceneID;

typedef struct Scene {
    SceneID id;
    const char *name;
    void (*init)(struct Scene *scene, void *appCtx);
    void (*cleanup)(struct Scene *scene, void *appCtx);
    void (*update)(struct Scene *scene, void *appCtx, float dt);
    void (*draw)(struct Scene *scene, void *appCtx);
    void *state;
} Scene;

typedef struct SceneManager {
    Scene scenes[SCENE_COUNT];
    SceneID current, requested;
    void *appCtx;
} SceneManager;

void SceneManagerInit(struct SceneManager *sm, void *appCtx);
void SceneManagerRegister(struct SceneManager *sm, SceneID id, Scene scene);
void SceneManagerTransition(struct SceneManager *sm, SceneID next);
void SceneManagerTick(struct SceneManager *sm, float dt);

// Scene registration functions (implemented in scene_*.c files)
Scene SceneMenuCreate(void);
Scene SceneMapSelectCreate(void);
Scene SceneDifficultyCreate(void);
Scene SceneShopCreate(void);
Scene ScenePerkSelectCreate(void);
Scene SceneLobbyCreate(void);
Scene SceneGameCreate(void);

#endif
