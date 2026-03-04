#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "map.h"
#include <stdbool.h>

#define MAX_ENEMIES 128
#define MAX_TOWERS 64
#define MAX_PROJECTILES 256

// Forward declare GameState
typedef struct GameState GameState;

// --- Enemy ---

typedef enum {
    ENEMY_BASIC,
    ENEMY_FAST,
    ENEMY_TANK,
    ENEMY_TYPE_COUNT,
} EnemyType;

typedef struct {
    float maxHp;
    float speed;
    float radius;
    Color color;
} EnemyConfig;

typedef struct {
    bool active;
    EnemyType type;
    Vector3 worldPos;
    float hp;
    float maxHp;
    float speed;
    float baseSpeed;
    int waypointIndex;
    float pathProgress;
    float slowTimer;
    float slowFactor;
    float radius;
    Color color;
} Enemy;

extern const EnemyConfig ENEMY_CONFIGS[ENEMY_TYPE_COUNT];

void EnemySpawn(Enemy enemies[], int maxEnemies, EnemyType type, const Map *map);
void EnemiesUpdate(Enemy enemies[], int maxEnemies, const Map *map, GameState *gs, float dt);
void EnemiesDraw(const Enemy enemies[], int maxEnemies, Camera3D camera);

// --- Projectile ---

typedef struct {
    bool active;
    Vector3 position;
    int targetEnemyIndex;
    float damage;
    float speed;
    float slowFactor;
    float slowDuration;
    float aoeRadius;
    Color color;
} Projectile;

void ProjectileSpawn(Projectile projectiles[], int maxProjectiles,
                     Vector3 origin, int targetIndex, float damage, float speed,
                     float slowFactor, float slowDuration, float aoeRadius, Color color);
void ProjectilesUpdate(Projectile projectiles[], int maxProjectiles,
                       Enemy enemies[], int maxEnemies, GameState *gs, float dt);
void ProjectilesDraw(const Projectile projectiles[], int maxProjectiles);

// --- Tower ---

typedef enum {
    TOWER_CANNON,
    TOWER_MACHINEGUN,
    TOWER_SNIPER,
    TOWER_SLOW,
    TOWER_TYPE_COUNT,
} TowerType;

#define TOWER_MAX_LEVEL 3

typedef struct {
    float damage;
    float range;
    float fireRate;
    float projectileSpeed;
    float slowFactor;
    float slowDuration;
    float aoeRadius;
    int cost;
    Color color;
} TowerConfig;

typedef struct {
    bool active;
    TowerType type;
    int level;
    GridPos gridPos;
    Vector3 worldPos;
    float cooldownTimer;
} Tower;

extern const TowerConfig TOWER_CONFIGS[TOWER_TYPE_COUNT][TOWER_MAX_LEVEL];
extern const char *TOWER_NAMES[TOWER_TYPE_COUNT];

void TowerPlace(Tower towers[], int maxTowers, TowerType type, GridPos pos);
void TowersUpdate(Tower towers[], int maxTowers, Enemy enemies[], int maxEnemies,
                  Projectile projectiles[], int maxProjectiles, float dt);
void TowersDraw(const Tower towers[], int maxTowers);

#endif
