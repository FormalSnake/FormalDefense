#include "entity.h"
#include "game.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

// --- Enemy Config ---

const EnemyConfig ENEMY_CONFIGS[ENEMY_TYPE_COUNT] = {
    [ENEMY_BASIC] = { 100.0f, 2.0f, 0.3f, GREEN },
    [ENEMY_FAST]  = { 60.0f,  4.0f, 0.25f, LIME },
    [ENEMY_TANK]  = { 300.0f, 1.2f, 0.4f, DARKGREEN },
};

// --- Tower Config: [type][level] ---
// damage, range, fireRate, projSpeed, slowFactor, slowDuration, aoeRadius, cost, color

const TowerConfig TOWER_CONFIGS[TOWER_TYPE_COUNT][TOWER_MAX_LEVEL] = {
    [TOWER_CANNON] = {
        { 40.0f, 3.5f, 1.0f, 8.0f,  1.0f, 0.0f, 0.0f, 50,  (Color){140,140,140,255} },
        { 65.0f, 4.0f, 1.0f, 9.0f,  1.0f, 0.0f, 0.0f, 75,  (Color){170,170,170,255} },
        { 90.0f, 4.5f, 1.2f, 10.0f, 1.0f, 0.0f, 1.5f, 120, (Color){210,210,210,255} },
    },
    [TOWER_MACHINEGUN] = {
        { 10.0f, 3.0f, 4.0f, 12.0f, 1.0f, 0.0f, 0.0f, 40,  (Color){220,140,30,255} },
        { 15.0f, 3.5f, 5.0f, 14.0f, 1.0f, 0.0f, 0.0f, 60,  (Color){230,180,30,255} },
        { 22.0f, 4.0f, 6.5f, 16.0f, 1.0f, 0.0f, 0.0f, 100, (Color){255,220,50,255} },
    },
    [TOWER_SNIPER] = {
        { 80.0f,  7.0f, 0.5f, 20.0f, 1.0f, 0.0f, 0.0f, 70,  (Color){80,120,200,255} },
        { 130.0f, 8.0f, 0.5f, 24.0f, 1.0f, 0.0f, 0.0f, 110, (Color){100,150,230,255} },
        { 200.0f, 9.5f, 0.6f, 28.0f, 1.0f, 0.0f, 0.0f, 160, (Color){140,180,255,255} },
    },
    [TOWER_SLOW] = {
        { 5.0f,  3.5f, 1.5f, 8.0f, 0.5f, 2.0f, 1.0f, 60,  (Color){150,80,200,255} },
        { 8.0f,  4.0f, 1.8f, 9.0f, 0.4f, 2.5f, 1.5f, 90,  (Color){170,100,220,255} },
        { 12.0f, 4.5f, 2.0f, 10.0f,0.3f, 3.0f, 2.0f, 130, (Color){200,130,255,255} },
    },
};

const char *TOWER_NAMES[TOWER_TYPE_COUNT] = { "Cannon", "MG", "Sniper", "Slow" };

// --- Enemy ---

void EnemySpawn(Enemy enemies[], int maxEnemies, EnemyType type, const Map *map)
{
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) {
            const EnemyConfig *cfg = &ENEMY_CONFIGS[type];
            enemies[i] = (Enemy){
                .active = true,
                .type = type,
                .hp = cfg->maxHp,
                .maxHp = cfg->maxHp,
                .speed = cfg->speed,
                .baseSpeed = cfg->speed,
                .waypointIndex = 0,
                .pathProgress = 0.0f,
                .slowTimer = 0.0f,
                .slowFactor = 1.0f,
                .radius = cfg->radius,
                .color = cfg->color,
            };
            // Place at first waypoint
            enemies[i].worldPos = MapGridToWorld(map->waypoints[0]);
            enemies[i].worldPos.y = cfg->radius;
            return;
        }
    }
}

void EnemiesUpdate(Enemy enemies[], int maxEnemies, const Map *map, GameState *gs, float dt)
{
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        Enemy *e = &enemies[i];

        // Slow decay
        if (e->slowTimer > 0.0f) {
            e->slowTimer -= dt;
            e->speed = e->baseSpeed * e->slowFactor;
            if (e->slowTimer <= 0.0f) {
                e->slowTimer = 0.0f;
                e->slowFactor = 1.0f;
                e->speed = e->baseSpeed;
            }
        }

        // Move along path
        if (e->waypointIndex >= map->waypointCount - 1) {
            // Reached base
            e->active = false;
            gs->lives--;
            if (gs->lives <= 0) {
                gs->lives = 0;
                gs->phase = PHASE_OVER;
            }
            continue;
        }

        Vector3 current = MapGridToWorld(map->waypoints[e->waypointIndex]);
        Vector3 next = MapGridToWorld(map->waypoints[e->waypointIndex + 1]);
        float segLen = Vector3Distance(current, next);

        if (segLen < 0.01f) {
            e->waypointIndex++;
            e->pathProgress = 0.0f;
            continue;
        }

        e->pathProgress += (e->speed * dt) / segLen;

        if (e->pathProgress >= 1.0f) {
            e->waypointIndex++;
            e->pathProgress = 0.0f;
        } else {
            e->worldPos.x = current.x + (next.x - current.x) * e->pathProgress;
            e->worldPos.z = current.z + (next.z - current.z) * e->pathProgress;
            e->worldPos.y = e->radius;
        }
    }
}

void EnemiesDraw(const Enemy enemies[], int maxEnemies, Camera3D camera)
{
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        const Enemy *e = &enemies[i];

        DrawSphere(e->worldPos, e->radius, e->color);

        // Health bar (billboard above enemy)
        Vector3 barPos = e->worldPos;
        barPos.y += e->radius + 0.4f;
        Vector2 screen = GetWorldToScreen(barPos, camera);

        float barW = 30.0f;
        float barH = 4.0f;
        float hpRatio = e->hp / e->maxHp;
        Color barCol = hpRatio > 0.5f ? GREEN : (hpRatio > 0.25f ? YELLOW : RED);

        DrawRectangle((int)(screen.x - barW / 2), (int)screen.y, (int)barW, (int)barH, DARKGRAY);
        DrawRectangle((int)(screen.x - barW / 2), (int)screen.y, (int)(barW * hpRatio), (int)barH, barCol);
    }
}

// --- Tower ---

void TowerPlace(Tower towers[], int maxTowers, TowerType type, GridPos pos)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) {
            towers[i] = (Tower){
                .active = true,
                .type = type,
                .level = 0,
                .gridPos = pos,
                .worldPos = MapGridToWorld(pos),
                .cooldownTimer = 0.0f,
            };
            towers[i].worldPos.y = 0.35f;
            return;
        }
    }
}

static int TowerFindTarget(const Tower *tower, const Enemy enemies[], int maxEnemies)
{
    const TowerConfig *cfg = &TOWER_CONFIGS[tower->type][tower->level];
    int bestIdx = -1;
    int bestWaypoint = -1;
    float bestProgress = -1.0f;

    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        float dist = Vector3Distance(tower->worldPos, enemies[i].worldPos);
        if (dist <= cfg->range) {
            // Prioritize enemy furthest along path (closest to base)
            if (enemies[i].waypointIndex > bestWaypoint ||
                (enemies[i].waypointIndex == bestWaypoint && enemies[i].pathProgress > bestProgress)) {
                bestIdx = i;
                bestWaypoint = enemies[i].waypointIndex;
                bestProgress = enemies[i].pathProgress;
            }
        }
    }
    return bestIdx;
}

void TowersUpdate(Tower towers[], int maxTowers, Enemy enemies[], int maxEnemies,
                  Projectile projectiles[], int maxProjectiles, float dt)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) continue;
        Tower *t = &towers[i];
        const TowerConfig *cfg = &TOWER_CONFIGS[t->type][t->level];

        t->cooldownTimer -= dt;
        if (t->cooldownTimer > 0.0f) continue;

        int target = TowerFindTarget(t, enemies, maxEnemies);
        if (target < 0) continue;

        // Fire!
        t->cooldownTimer = 1.0f / cfg->fireRate;
        Vector3 origin = t->worldPos;
        origin.y += 0.3f;
        ProjectileSpawn(projectiles, maxProjectiles, origin, target,
                       cfg->damage, cfg->projectileSpeed,
                       cfg->slowFactor, cfg->slowDuration, cfg->aoeRadius, cfg->color);
    }
}

void TowersDraw(const Tower towers[], int maxTowers)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) continue;
        const Tower *t = &towers[i];
        const TowerConfig *cfg = &TOWER_CONFIGS[t->type][t->level];

        DrawCubeV(t->worldPos, (Vector3){ 0.7f, 0.7f, 0.7f }, cfg->color);
        DrawCubeWiresV(t->worldPos, (Vector3){ 0.72f, 0.72f, 0.72f }, BLACK);
    }
}

// --- Projectile ---

void ProjectileSpawn(Projectile projectiles[], int maxProjectiles,
                     Vector3 origin, int targetIndex, float damage, float speed,
                     float slowFactor, float slowDuration, float aoeRadius, Color color)
{
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) {
            projectiles[i] = (Projectile){
                .active = true,
                .position = origin,
                .targetEnemyIndex = targetIndex,
                .damage = damage,
                .speed = speed,
                .slowFactor = slowFactor,
                .slowDuration = slowDuration,
                .aoeRadius = aoeRadius,
                .color = color,
            };
            return;
        }
    }
}

static void ApplyDamageAndSlow(Enemy *e, float damage, float slowFactor, float slowDuration, GameState *gs)
{
    e->hp -= damage;
    if (e->hp <= 0.0f) {
        e->active = false;
        gs->gold += 10; // gold per kill
        return;
    }
    if (slowFactor < 1.0f) {
        e->slowFactor = slowFactor;
        e->slowTimer = slowDuration;
    }
}

void ProjectilesUpdate(Projectile projectiles[], int maxProjectiles,
                       Enemy enemies[], int maxEnemies, GameState *gs, float dt)
{
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;
        Projectile *p = &projectiles[i];

        // Check if target is still alive
        if (p->targetEnemyIndex < 0 || p->targetEnemyIndex >= maxEnemies ||
            !enemies[p->targetEnemyIndex].active) {
            p->active = false;
            continue;
        }

        Enemy *target = &enemies[p->targetEnemyIndex];
        Vector3 dir = Vector3Subtract(target->worldPos, p->position);
        float dist = Vector3Length(dir);

        if (dist < 0.2f) {
            // Hit!
            if (p->aoeRadius > 0.0f) {
                // AoE damage
                for (int j = 0; j < maxEnemies; j++) {
                    if (!enemies[j].active) continue;
                    if (Vector3Distance(p->position, enemies[j].worldPos) <= p->aoeRadius) {
                        ApplyDamageAndSlow(&enemies[j], p->damage, p->slowFactor, p->slowDuration, gs);
                    }
                }
            } else {
                // Single target
                ApplyDamageAndSlow(target, p->damage, p->slowFactor, p->slowDuration, gs);
            }
            p->active = false;
        } else {
            // Move toward target
            dir = Vector3Scale(Vector3Normalize(dir), p->speed * dt);
            p->position = Vector3Add(p->position, dir);
        }
    }
}

void ProjectilesDraw(const Projectile projectiles[], int maxProjectiles)
{
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;
        DrawSphere(projectiles[i].position, 0.1f, projectiles[i].color);
    }
}
