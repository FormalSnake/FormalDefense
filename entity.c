#include "entity.h"
#include "game.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

// --- Entity ID ---

EntityID EntityIDMake(uint16_t typeBits, uint16_t seq)
{
    return typeBits | (seq & ENTITY_SEQ_MASK);
}

uint16_t EntityIDType(EntityID id)
{
    return id & ~ENTITY_SEQ_MASK;
}

uint16_t EntityIDSeq(EntityID id)
{
    return id & ENTITY_SEQ_MASK;
}

// --- Player Colors ---

const Color PLAYER_COLORS[MAX_PLAYERS] = {
    { 100, 200, 255, 255 },  // Blue
    { 255, 100, 100, 255 },  // Red
    { 100, 255, 100, 255 },  // Green
    { 255, 200, 50, 255 },   // Yellow
};

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

// --- Find by ID ---

Enemy *EnemyFindByID(Enemy enemies[], int maxEnemies, EntityID id)
{
    if (id == ENTITY_ID_NONE) return NULL;
    for (int i = 0; i < maxEnemies; i++) {
        if (enemies[i].active && enemies[i].id == id)
            return &enemies[i];
    }
    return NULL;
}

Tower *TowerFindByID(Tower towers[], int maxTowers, EntityID id)
{
    if (id == ENTITY_ID_NONE) return NULL;
    for (int i = 0; i < maxTowers; i++) {
        if (towers[i].active && towers[i].id == id)
            return &towers[i];
    }
    return NULL;
}

// --- Enemy ---

void EnemySpawn(Enemy enemies[], int maxEnemies, EnemyType type, const Map *map, GameState *gs)
{
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) {
            const EnemyConfig *cfg = &ENEMY_CONFIGS[type];
            float scaledHp = cfg->maxHp * gs->hpMultiplier;
            EntityID eid = EntityIDMake(ENTITY_TYPE_ENEMY, gs->nextEntitySeq++);
            enemies[i] = (Enemy){
                .active = true,
                .id = eid,
                .type = type,
                .hp = scaledHp,
                .maxHp = scaledHp,
                .speed = cfg->speed * gs->speedMultiplier,
                .baseSpeed = cfg->speed * gs->speedMultiplier,
                .waypointIndex = 0,
                .pathProgress = 0.0f,
                .slowTimer = 0.0f,
                .slowFactor = 1.0f,
                .radius = cfg->radius,
                .color = cfg->color,
            };
            enemies[i].worldPos = MapGridToWorldElevated(map, map->waypoints[0]);
            enemies[i].worldPos.y += cfg->radius;
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
            e->active = false;
            gs->lives--;
            if (gs->lives <= 0) {
                gs->lives = 0;
                gs->phase = PHASE_OVER;
            }
            continue;
        }

        Vector3 current = MapGridToWorldElevated(map, map->waypoints[e->waypointIndex]);
        Vector3 next = MapGridToWorldElevated(map, map->waypoints[e->waypointIndex + 1]);
        float segLen = Vector3Distance(current, next);

        if (segLen < 0.01f) {
            e->waypointIndex++;
            e->pathProgress = 0.0f;
            continue;
        }

        // Slope speed modifier
        float slopeFactor = 1.0f;
        if (next.y > current.y + 0.01f) slopeFactor = 0.7f;       // uphill
        else if (next.y < current.y - 0.01f) slopeFactor = 1.3f;  // downhill

        e->pathProgress += (e->speed * slopeFactor * dt) / segLen;

        if (e->pathProgress >= 1.0f) {
            e->waypointIndex++;
            e->pathProgress = 0.0f;
        } else {
            e->worldPos.x = current.x + (next.x - current.x) * e->pathProgress;
            e->worldPos.z = current.z + (next.z - current.z) * e->pathProgress;
            e->worldPos.y = current.y + (next.y - current.y) * e->pathProgress + e->radius;
        }
    }
}

void EnemiesDraw(const Enemy enemies[], int maxEnemies, Model sphereModel)
{
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        const Enemy *e = &enemies[i];
        sphereModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = e->color;
        DrawModel(sphereModel, e->worldPos, e->radius, WHITE);
    }
}

void EnemiesDrawHUD(const Enemy enemies[], int maxEnemies, Camera3D camera)
{
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        const Enemy *e = &enemies[i];

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

void TowerPlace(Tower towers[], int maxTowers, TowerType type, GridPos pos,
                uint8_t ownerPlayer, GameState *gs, const Map *map)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) {
            EntityID tid = EntityIDMake(ENTITY_TYPE_TOWER, gs->nextEntitySeq++);
            towers[i] = (Tower){
                .active = true,
                .id = tid,
                .type = type,
                .level = 0,
                .gridPos = pos,
                .worldPos = MapGridToWorldElevated(map, pos),
                .cooldownTimer = 0.0f,
                .ownerPlayer = ownerPlayer,
            };
            towers[i].worldPos.y += 0.35f;
            return;
        }
    }
}

static int TowerFindTarget(const Tower *tower, const Enemy enemies[], int maxEnemies,
                           const Map *map)
{
    const TowerConfig *cfg = &TOWER_CONFIGS[tower->type][tower->level];
    float towerElevY = MapGetElevationY(map, tower->gridPos.x, tower->gridPos.z);
    int bestIdx = -1;
    int bestWaypoint = -1;
    float bestProgress = -1.0f;

    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        float dist = Vector3Distance(tower->worldPos, enemies[i].worldPos);
        // Elevation range bonus: +1 per elevation level above target
        GridPos enemyGrid = MapWorldToGrid(enemies[i].worldPos);
        float enemyElevY = MapGetElevationY(map, enemyGrid.x, enemyGrid.z);
        float elevBonus = 0.0f;
        if (towerElevY > enemyElevY)
            elevBonus = towerElevY - enemyElevY;
        float effectiveRange = cfg->range + elevBonus;
        if (dist <= effectiveRange) {
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
                  Projectile projectiles[], int maxProjectiles, GameState *gs,
                  const Map *map, float dt)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) continue;
        Tower *t = &towers[i];
        const TowerConfig *cfg = &TOWER_CONFIGS[t->type][t->level];

        t->cooldownTimer -= dt;
        if (t->cooldownTimer > 0.0f) continue;

        int target = TowerFindTarget(t, enemies, maxEnemies, map);
        if (target < 0) continue;

        t->cooldownTimer = 1.0f / cfg->fireRate;
        Vector3 origin = t->worldPos;
        origin.y += 0.3f;
        ProjectileSpawn(projectiles, maxProjectiles, origin, enemies[target].id,
                       cfg->damage, cfg->projectileSpeed,
                       cfg->slowFactor, cfg->slowDuration, cfg->aoeRadius, cfg->color,
                       t->ownerPlayer, gs);
    }
}

void TowersDraw(const Tower towers[], int maxTowers, int playerCount)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) continue;
        const Tower *t = &towers[i];
        const TowerConfig *cfg = &TOWER_CONFIGS[t->type][t->level];

        DrawCubeV(t->worldPos, (Vector3){ 0.7f, 0.7f, 0.7f }, cfg->color);

        if (playerCount > 1) {
            // Tint wireframe with player color in multiplayer
            DrawCubeWiresV(t->worldPos, (Vector3){ 0.72f, 0.72f, 0.72f },
                          PLAYER_COLORS[t->ownerPlayer]);
        } else {
            DrawCubeWiresV(t->worldPos, (Vector3){ 0.72f, 0.72f, 0.72f }, BLACK);
        }
    }
}

// --- Projectile ---

void ProjectileSpawn(Projectile projectiles[], int maxProjectiles,
                     Vector3 origin, EntityID targetID, float damage, float speed,
                     float slowFactor, float slowDuration, float aoeRadius, Color color,
                     uint8_t ownerPlayer, GameState *gs)
{
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) {
            EntityID pid = EntityIDMake(ENTITY_TYPE_PROJ, gs->nextEntitySeq++);
            projectiles[i] = (Projectile){
                .active = true,
                .id = pid,
                .position = origin,
                .targetEnemyID = targetID,
                .damage = damage,
                .speed = speed,
                .slowFactor = slowFactor,
                .slowDuration = slowDuration,
                .aoeRadius = aoeRadius,
                .color = color,
                .ownerPlayer = ownerPlayer,
            };
            return;
        }
    }
}

static void ApplyDamageAndSlow(Enemy *e, float damage, float slowFactor, float slowDuration,
                               uint8_t ownerPlayer, GameState *gs)
{
    e->hp -= damage;
    if (e->hp <= 0.0f) {
        e->active = false;
        gs->playerGold[ownerPlayer] += gs->goldPerKill;
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

        Enemy *target = EnemyFindByID(enemies, maxEnemies, p->targetEnemyID);
        if (!target) {
            p->active = false;
            continue;
        }

        Vector3 dir = Vector3Subtract(target->worldPos, p->position);
        float dist = Vector3Length(dir);

        if (dist < 0.2f) {
            if (p->aoeRadius > 0.0f) {
                for (int j = 0; j < maxEnemies; j++) {
                    if (!enemies[j].active) continue;
                    if (Vector3Distance(p->position, enemies[j].worldPos) <= p->aoeRadius) {
                        ApplyDamageAndSlow(&enemies[j], p->damage, p->slowFactor, p->slowDuration,
                                          p->ownerPlayer, gs);
                    }
                }
            } else {
                ApplyDamageAndSlow(target, p->damage, p->slowFactor, p->slowDuration,
                                  p->ownerPlayer, gs);
            }
            p->active = false;
        } else {
            dir = Vector3Scale(Vector3Normalize(dir), p->speed * dt);
            p->position = Vector3Add(p->position, dir);
        }
    }
}

void ProjectilesDraw(const Projectile projectiles[], int maxProjectiles, Model sphereModel)
{
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;
        sphereModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = projectiles[i].color;
        DrawModel(sphereModel, projectiles[i].position, 0.1f, WHITE);
    }
}
