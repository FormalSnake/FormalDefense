#include "entity.h"
#include "game.h"
#include "progress.h"
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
// damage, range, fireRate, projSpeed, slowFactor, slowDuration, aoeRadius, cost, color,
// isBeam, chainCount, burnDPS, burnDuration

const TowerConfig TOWER_CONFIGS[TOWER_TYPE_COUNT][TOWER_MAX_LEVEL] = {
    [TOWER_CANNON] = {
        { 40.0f, 3.5f, 1.0f, 8.0f,  1.0f, 0.0f, 0.0f, 50,  (Color){140,140,140,255}, false, 0, 0.0f, 0.0f },
        { 65.0f, 4.0f, 1.0f, 9.0f,  1.0f, 0.0f, 0.0f, 75,  (Color){170,170,170,255}, false, 0, 0.0f, 0.0f },
        { 90.0f, 4.5f, 1.2f, 10.0f, 1.0f, 0.0f, 1.5f, 120, (Color){210,210,210,255}, false, 0, 0.0f, 0.0f },
    },
    [TOWER_MACHINEGUN] = {
        { 10.0f, 3.0f, 4.0f, 12.0f, 1.0f, 0.0f, 0.0f, 40,  (Color){220,140,30,255}, false, 0, 0.0f, 0.0f },
        { 15.0f, 3.5f, 5.0f, 14.0f, 1.0f, 0.0f, 0.0f, 60,  (Color){230,180,30,255}, false, 0, 0.0f, 0.0f },
        { 22.0f, 4.0f, 6.5f, 16.0f, 1.0f, 0.0f, 0.0f, 100, (Color){255,220,50,255}, false, 0, 0.0f, 0.0f },
    },
    [TOWER_SNIPER] = {
        { 80.0f,  7.0f, 0.5f, 20.0f, 1.0f, 0.0f, 0.0f, 70,  (Color){80,120,200,255}, false, 0, 0.0f, 0.0f },
        { 130.0f, 8.0f, 0.5f, 24.0f, 1.0f, 0.0f, 0.0f, 110, (Color){100,150,230,255}, false, 0, 0.0f, 0.0f },
        { 200.0f, 9.5f, 0.6f, 28.0f, 1.0f, 0.0f, 0.0f, 160, (Color){140,180,255,255}, false, 0, 0.0f, 0.0f },
    },
    [TOWER_SLOW] = {
        { 5.0f,  3.5f, 1.5f, 8.0f, 0.5f, 2.0f, 1.0f, 60,  (Color){150,80,200,255}, false, 0, 0.0f, 0.0f },
        { 8.0f,  4.0f, 1.8f, 9.0f, 0.4f, 2.5f, 1.5f, 90,  (Color){170,100,220,255}, false, 0, 0.0f, 0.0f },
        { 12.0f, 4.5f, 2.0f, 10.0f,0.3f, 3.0f, 2.0f, 130, (Color){200,130,255,255}, false, 0, 0.0f, 0.0f },
    },
    [TOWER_LASER] = {
        { 25.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 65,  (Color){0,220,220,255},   true,  0, 0.0f, 0.0f },
        { 35.0f, 3.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 95,  (Color){0,240,240,255},   true,  0, 0.0f, 0.0f },
        { 50.0f, 4.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 140, (Color){100,255,255,255}, true,  0, 0.0f, 0.0f },
    },
    [TOWER_MORTAR] = {
        { 60.0f,  8.0f,  0.4f, 6.0f, 1.0f, 0.0f, 2.0f, 80,  (Color){160,120,60,255},  false, 0, 0.0f, 0.0f },
        { 90.0f,  9.0f,  0.45f,6.0f, 1.0f, 0.0f, 2.5f, 120, (Color){180,140,70,255},  false, 0, 0.0f, 0.0f },
        { 130.0f, 10.0f, 0.5f, 6.0f, 1.0f, 0.0f, 3.0f, 170, (Color){200,170,90,255},  false, 0, 0.0f, 0.0f },
    },
    [TOWER_TESLA] = {
        { 30.0f, 4.0f, 0.8f, 0.0f, 1.0f, 0.0f, 0.0f, 75,  (Color){80,140,255,255},  false, 3, 0.0f, 0.0f },
        { 45.0f, 4.5f, 0.8f, 0.0f, 1.0f, 0.0f, 0.0f, 110, (Color){100,160,255,255}, false, 3, 0.0f, 0.0f },
        { 65.0f, 5.0f, 0.8f, 0.0f, 1.0f, 0.0f, 0.0f, 155, (Color){140,190,255,255}, false, 4, 0.0f, 0.0f },
    },
    [TOWER_FLAME] = {
        { 15.0f, 2.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 55,  (Color){255,100,30,255},  false, 0, 8.0f,  2.0f },
        { 22.0f, 3.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 85,  (Color){255,140,40,255},  false, 0, 12.0f, 2.5f },
        { 30.0f, 3.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 125, (Color){255,180,60,255},  false, 0, 18.0f, 3.0f },
    },
};

const char *TOWER_NAMES[TOWER_TYPE_COUNT] = {
    "Cannon", "MG", "Sniper", "Slow", "Laser", "Mortar", "Tesla", "Flame"
};

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
                .burnTimer = 0.0f,
                .burnDPS = 0.0f,
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

        // Burn DoT tick
        if (e->burnTimer > 0.0f) {
            e->burnTimer -= dt;
            e->hp -= e->burnDPS * dt;
            if (e->hp <= 0.0f) {
                e->active = false;
                gs->playerGold[0] += gs->goldPerKill;
                gs->gold = gs->playerGold[0];
                continue;
            }
            if (e->burnTimer <= 0.0f) {
                e->burnTimer = 0.0f;
                e->burnDPS = 0.0f;
            }
        }

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
            GridPos g = MapWorldToGrid(e->worldPos);
            e->worldPos.y = MapGetElevationY(map, g.x, g.z) + e->radius;
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
                .beamTarget = ENTITY_ID_NONE,
                .cooldownReady = true,
            };
            towers[i].worldPos.y += 0.35f;
            return;
        }
    }
}

static int TowerFindTarget(const Tower *tower, const Enemy enemies[], int maxEnemies,
                           const Map *map, RunModifiers *mods)
{
    const TowerConfig *cfg = &TOWER_CONFIGS[tower->type][tower->level];
    float towerElevY = MapGetElevationY(map, tower->gridPos.x, tower->gridPos.z);
    int bestIdx = -1;
    int bestWaypoint = -1;
    float bestProgress = -1.0f;

    float rangeMult = (mods) ? mods->rangeMultiplier : 1.0f;

    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active) continue;
        float dist = Vector3Distance(tower->worldPos, enemies[i].worldPos);
        // Elevation range bonus: +1 per elevation level above target
        GridPos enemyGrid = MapWorldToGrid(enemies[i].worldPos);
        float enemyElevY = MapGetElevationY(map, enemyGrid.x, enemyGrid.z);
        float elevBonus = 0.0f;
        if (towerElevY > enemyElevY)
            elevBonus = towerElevY - enemyElevY;
        float effectiveRange = cfg->range * rangeMult + elevBonus;
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

static void ApplyDamageAndSlow(Enemy *e, float damage, float slowFactor, float slowDuration,
                               uint8_t ownerPlayer, GameState *gs, RunModifiers *mods)
{
    float actualDamage = damage;
    if (mods) {
        actualDamage *= mods->damageMultiplier;
        // Per-enemy-type damage mods
        if (e->type == ENEMY_TANK) actualDamage *= mods->tankDamageMultiplier;
        else if (e->type == ENEMY_FAST) actualDamage *= mods->fastDamageMultiplier;
    }

    e->hp -= actualDamage;
    if (e->hp <= 0.0f) {
        e->active = false;
        int goldEarned = gs->goldPerKill;
        if (mods) {
            goldEarned = (int)(goldEarned * mods->goldPerKillMultiplier);
            if (mods->goldRushActive) goldEarned *= 2;
        }
        gs->playerGold[ownerPlayer] += goldEarned;
        gs->gold = gs->playerGold[0];
        return;
    }
    if (slowFactor < 1.0f) {
        float actualSlow = slowFactor;
        if (mods && mods->slowEffectMultiplier > 1.0f) {
            // Make slow more effective (lower factor = slower)
            float slowAmount = 1.0f - slowFactor;
            slowAmount *= mods->slowEffectMultiplier;
            if (slowAmount > 0.9f) slowAmount = 0.9f;
            actualSlow = 1.0f - slowAmount;
        }
        e->slowFactor = actualSlow;
        e->slowTimer = slowDuration;
    }
}

// Tesla chain helper: find nearest enemy to position within chainRange, excluding already-hit
static int TeslaFindChainTarget(const Enemy enemies[], int maxEnemies,
                                Vector3 pos, float chainRange,
                                const bool hit[], int excludeIdx)
{
    int bestIdx = -1;
    float bestDist = chainRange + 1.0f;
    for (int i = 0; i < maxEnemies; i++) {
        if (!enemies[i].active || hit[i] || i == excludeIdx) continue;
        float d = Vector3Distance(pos, enemies[i].worldPos);
        if (d <= chainRange && d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void TowersUpdate(Tower towers[], int maxTowers, Enemy enemies[], int maxEnemies,
                  Projectile projectiles[], int maxProjectiles, GameState *gs,
                  const Map *map, RunModifiers *mods, float dt)
{
    float fireRateMult = (mods) ? mods->fireRateMultiplier : 1.0f;
    float damageMult = (mods) ? mods->damageMultiplier : 1.0f;
    float aoeBonus = (mods) ? mods->aoeBonus : 0.0f;

    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) continue;
        Tower *t = &towers[i];
        const TowerConfig *cfg = &TOWER_CONFIGS[t->type][t->level];

        // --- Laser: continuous beam, no cooldown ---
        if (cfg->isBeam) {
            int target = TowerFindTarget(t, enemies, maxEnemies, map, mods);
            if (target >= 0) {
                t->beamTarget = enemies[target].id;
                float dps = cfg->damage * damageMult;
                if (mods) {
                    if (enemies[target].type == ENEMY_TANK) dps *= mods->tankDamageMultiplier;
                    else if (enemies[target].type == ENEMY_FAST) dps *= mods->fastDamageMultiplier;
                }
                float goldMult = (mods) ? mods->goldPerKillMultiplier : 1.0f;
                bool goldRush = (mods) ? mods->goldRushActive : false;

                enemies[target].hp -= dps * dt;
                if (enemies[target].hp <= 0.0f) {
                    enemies[target].active = false;
                    int goldEarned = (int)(gs->goldPerKill * goldMult);
                    if (goldRush) goldEarned *= 2;
                    gs->playerGold[t->ownerPlayer] += goldEarned;
                    gs->gold = gs->playerGold[0];
                    t->beamTarget = ENTITY_ID_NONE;
                }
            } else {
                t->beamTarget = ENTITY_ID_NONE;
            }
            continue;
        }

        // --- Tesla: instant chain lightning ---
        if (cfg->chainCount > 0) {
            t->cooldownTimer -= dt;
            if (t->cooldownTimer > 0.0f) continue;

            int target = TowerFindTarget(t, enemies, maxEnemies, map, mods);
            if (target < 0) continue;

            t->cooldownTimer = 1.0f / (cfg->fireRate * fireRateMult);

            float damage = cfg->damage;
            // Overcharge
            if (mods && mods->overcharge && t->cooldownReady) {
                damage *= 1.4f;
                t->cooldownReady = false;
            }

            // Hit primary target
            ApplyDamageAndSlow(&enemies[target], damage, cfg->slowFactor, cfg->slowDuration,
                              t->ownerPlayer, gs, mods);

            // Chain to nearby enemies
            bool hit[MAX_ENEMIES] = {false};
            hit[target] = true;
            Vector3 lastPos = enemies[target].worldPos;
            int chains = cfg->chainCount;
            for (int c = 0; c < chains; c++) {
                int next = TeslaFindChainTarget(enemies, maxEnemies, lastPos, 2.0f, hit, -1);
                if (next < 0) break;
                hit[next] = true;
                float chainDmg = damage * 0.7f; // 70% damage per chain
                ApplyDamageAndSlow(&enemies[next], chainDmg, cfg->slowFactor, cfg->slowDuration,
                                  t->ownerPlayer, gs, mods);
                lastPos = enemies[next].worldPos;
            }
            t->cooldownReady = true;
            continue;
        }

        // --- Flame: cone attack with burn DoT ---
        if (cfg->burnDPS > 0.0f) {
            t->cooldownTimer -= dt;
            if (t->cooldownTimer > 0.0f) continue;

            int target = TowerFindTarget(t, enemies, maxEnemies, map, mods);
            if (target < 0) continue;

            t->cooldownTimer = 1.0f / (cfg->fireRate * fireRateMult);

            float damage = cfg->damage;
            if (mods && mods->overcharge && t->cooldownReady) {
                damage *= 1.4f;
                t->cooldownReady = false;
            }

            // Direction to primary target
            Vector3 dir = Vector3Subtract(enemies[target].worldPos, t->worldPos);
            dir.y = 0.0f;
            float dirLen = Vector3Length(dir);
            if (dirLen < 0.01f) continue;
            dir = Vector3Scale(dir, 1.0f / dirLen);

            float rangeMult = (mods) ? mods->rangeMultiplier : 1.0f;
            float effectiveRange = cfg->range * rangeMult;
            float coneAngle = 30.0f * DEG2RAD; // 60 degree cone = 30 degrees each side

            // Hit all enemies in cone
            for (int j = 0; j < maxEnemies; j++) {
                if (!enemies[j].active) continue;
                float dist = Vector3Distance(t->worldPos, enemies[j].worldPos);
                if (dist > effectiveRange) continue;

                Vector3 toEnemy = Vector3Subtract(enemies[j].worldPos, t->worldPos);
                toEnemy.y = 0.0f;
                float toLen = Vector3Length(toEnemy);
                if (toLen < 0.01f) continue;
                toEnemy = Vector3Scale(toEnemy, 1.0f / toLen);

                float dot = dir.x * toEnemy.x + dir.z * toEnemy.z;
                if (dot >= cosf(coneAngle)) {
                    ApplyDamageAndSlow(&enemies[j], damage, cfg->slowFactor, cfg->slowDuration,
                                      t->ownerPlayer, gs, mods);
                    // Apply burn DoT
                    enemies[j].burnDPS = cfg->burnDPS * damageMult;
                    enemies[j].burnTimer = cfg->burnDuration;
                }
            }
            t->cooldownReady = true;
            continue;
        }

        // --- Standard projectile towers (Cannon, MG, Sniper, Mortar) ---
        t->cooldownTimer -= dt;
        if (t->cooldownTimer > 0.0f) continue;

        int target = TowerFindTarget(t, enemies, maxEnemies, map, mods);
        if (target < 0) continue;

        t->cooldownTimer = 1.0f / (cfg->fireRate * fireRateMult);
        Vector3 origin = t->worldPos;
        origin.y += 0.3f;

        float damage = cfg->damage;
        // Overcharge: first shot after cooldown does +40% damage
        if (mods && mods->overcharge && t->cooldownReady) {
            damage *= 1.4f;
            t->cooldownReady = false;
        }

        float aoe = cfg->aoeRadius + aoeBonus;

        if (t->type == TOWER_MORTAR) {
            // Mortar: spawn arcing projectile to fixed position
            ProjectileSpawn(projectiles, maxProjectiles, origin, enemies[target].id,
                           damage, cfg->projectileSpeed,
                           cfg->slowFactor, cfg->slowDuration, aoe, cfg->color,
                           t->ownerPlayer, gs);
            // Mark latest projectile as arcing
            for (int p = 0; p < maxProjectiles; p++) {
                if (projectiles[p].active && projectiles[p].targetEnemyID == enemies[target].id &&
                    Vector3Distance(projectiles[p].position, origin) < 0.5f) {
                    projectiles[p].isArcing = true;
                    projectiles[p].targetPos = enemies[target].worldPos;
                    projectiles[p].startPos = origin;
                    projectiles[p].arcProgress = 0.0f;
                    break;
                }
            }
        } else {
            // Sniper pierce setup
            int pierce = 0;
            if (t->type == TOWER_SNIPER && mods && mods->sniperPierce)
                pierce = 2;

            ProjectileSpawn(projectiles, maxProjectiles, origin, enemies[target].id,
                           damage, cfg->projectileSpeed,
                           cfg->slowFactor, cfg->slowDuration, aoe, cfg->color,
                           t->ownerPlayer, gs);
            // Set pierce on the spawned projectile
            if (pierce > 0) {
                for (int p = 0; p < maxProjectiles; p++) {
                    if (projectiles[p].active && projectiles[p].targetEnemyID == enemies[target].id &&
                        Vector3Distance(projectiles[p].position, origin) < 0.5f) {
                        projectiles[p].pierceRemaining = pierce;
                        break;
                    }
                }
            }
        }
        t->cooldownReady = true;
    }
}

void TowersDraw(const Tower towers[], int maxTowers, int playerCount,
                const Enemy enemies[], int maxEnemies)
{
    for (int i = 0; i < maxTowers; i++) {
        if (!towers[i].active) continue;
        const Tower *t = &towers[i];
        const TowerConfig *cfg = &TOWER_CONFIGS[t->type][t->level];

        DrawCubeV(t->worldPos, (Vector3){ 0.7f, 0.7f, 0.7f }, cfg->color);

        if (playerCount > 1) {
            DrawCubeWiresV(t->worldPos, (Vector3){ 0.72f, 0.72f, 0.72f },
                          PLAYER_COLORS[t->ownerPlayer]);
        } else {
            DrawCubeWiresV(t->worldPos, (Vector3){ 0.72f, 0.72f, 0.72f }, BLACK);
        }

        // Laser beam rendering
        if (cfg->isBeam && t->beamTarget != ENTITY_ID_NONE) {
            Enemy *target = EnemyFindByID((Enemy *)enemies, maxEnemies, t->beamTarget);
            if (target) {
                Vector3 beamStart = t->worldPos;
                beamStart.y += 0.3f;
                DrawLine3D(beamStart, target->worldPos, cfg->color);
                // Draw a thicker beam effect
                Vector3 offset = {0.02f, 0.02f, 0.0f};
                DrawLine3D(Vector3Add(beamStart, offset), Vector3Add(target->worldPos, offset), cfg->color);
            }
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
                .pierceRemaining = 0,
                .isArcing = false,
                .targetPos = (Vector3){0},
                .arcProgress = 0.0f,
                .startPos = origin,
            };
            return;
        }
    }
}

void ProjectilesUpdate(Projectile projectiles[], int maxProjectiles,
                       Enemy enemies[], int maxEnemies, GameState *gs,
                       RunModifiers *mods, float dt)
{
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;
        Projectile *p = &projectiles[i];

        // --- Arcing projectile (Mortar) ---
        if (p->isArcing) {
            float totalDist = Vector3Distance(p->startPos, p->targetPos);
            if (totalDist < 0.1f) { totalDist = 0.1f; }
            float travelSpeed = p->speed / totalDist;
            p->arcProgress += travelSpeed * dt;

            if (p->arcProgress >= 1.0f) {
                // Hit ground at target position — AoE damage
                if (p->aoeRadius > 0.0f) {
                    for (int j = 0; j < maxEnemies; j++) {
                        if (!enemies[j].active) continue;
                        if (Vector3Distance(p->targetPos, enemies[j].worldPos) <= p->aoeRadius) {
                            ApplyDamageAndSlow(&enemies[j], p->damage, p->slowFactor, p->slowDuration,
                                              p->ownerPlayer, gs, mods);
                        }
                    }
                }
                p->active = false;
            } else {
                // Parabolic arc: lerp XZ, arc Y
                float t = p->arcProgress;
                p->position.x = p->startPos.x + (p->targetPos.x - p->startPos.x) * t;
                p->position.z = p->startPos.z + (p->targetPos.z - p->startPos.z) * t;
                float baseY = p->startPos.y + (p->targetPos.y - p->startPos.y) * t;
                float arcHeight = totalDist * 0.4f; // arc height proportional to distance
                p->position.y = baseY + arcHeight * 4.0f * t * (1.0f - t);
            }
            continue;
        }

        // --- Standard tracking projectile ---
        Enemy *target = EnemyFindByID(enemies, maxEnemies, p->targetEnemyID);
        if (!target) {
            // If piercing, try to find next target
            if (p->pierceRemaining > 0) {
                // Find closest enemy to continue toward
                float bestDist = 999.0f;
                int bestIdx = -1;
                for (int j = 0; j < maxEnemies; j++) {
                    if (!enemies[j].active) continue;
                    float d = Vector3Distance(p->position, enemies[j].worldPos);
                    if (d < bestDist) {
                        bestDist = d;
                        bestIdx = j;
                    }
                }
                if (bestIdx >= 0) {
                    p->targetEnemyID = enemies[bestIdx].id;
                    continue;
                }
            }
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
                                          p->ownerPlayer, gs, mods);
                    }
                }
            } else {
                ApplyDamageAndSlow(target, p->damage, p->slowFactor, p->slowDuration,
                                  p->ownerPlayer, gs, mods);
            }

            // Pierce: continue through
            if (p->pierceRemaining > 0) {
                p->pierceRemaining--;
                // Find next target
                float bestDist = 999.0f;
                int bestIdx = -1;
                for (int j = 0; j < maxEnemies; j++) {
                    if (!enemies[j].active || enemies[j].id == target->id) continue;
                    float d = Vector3Distance(p->position, enemies[j].worldPos);
                    if (d < bestDist) {
                        bestDist = d;
                        bestIdx = j;
                    }
                }
                if (bestIdx >= 0) {
                    p->targetEnemyID = enemies[bestIdx].id;
                } else {
                    p->active = false;
                }
            } else {
                p->active = false;
            }
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
        float size = projectiles[i].isArcing ? 0.15f : 0.1f;
        DrawModel(sphereModel, projectiles[i].position, size, WHITE);
    }
}
