#include "scene.h"
#include <string.h>

void SceneManagerInit(SceneManager *sm, void *appCtx)
{
    memset(sm, 0, sizeof(*sm));
    sm->appCtx = appCtx;
    sm->current = SCENE_MENU;
    sm->requested = SCENE_MENU;
}

void SceneManagerRegister(SceneManager *sm, SceneID id, Scene scene)
{
    scene.id = id;
    sm->scenes[id] = scene;
}

void SceneManagerTransition(SceneManager *sm, SceneID next)
{
    sm->requested = next;
}

void SceneManagerTick(SceneManager *sm, float dt)
{
    if (sm->requested != sm->current) {
        Scene *old = &sm->scenes[sm->current];
        if (old->cleanup) old->cleanup(old, sm->appCtx);

        sm->current = sm->requested;

        Scene *cur = &sm->scenes[sm->current];
        if (cur->init) cur->init(cur, sm->appCtx);
    }

    Scene *cur = &sm->scenes[sm->current];
    if (cur->update) cur->update(cur, sm->appCtx, dt);
    if (cur->draw) cur->draw(cur, sm->appCtx);
}
