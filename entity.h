#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "map.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_ENEMIES 256
#define MAX_TOWERS 64
#define MAX_PROJECTILES 256
#define MAX_PLAYERS 4

// Forward declare GameState
typedef struct GameState GameState;

// --- Entity ID ---

typedef uint16_t EntityID;

#define ENTITY_ID_NONE 0
#define ENTITY_TYPE_BITS 2
#define ENTITY_SEQ_BITS 14
#define ENTITY_SEQ_MASK ((1 << ENTITY_SEQ_BITS) - 1)

#define ENTITY_TYPE_ENEMY  (0 << ENTITY_SEQ_BITS)
#define ENTITY_TYPE_TOWER  (1 << ENTITY_SEQ_BITS)
#define ENTITY_TYPE_PROJ   (2 << ENTITY_SEQ_BITS)

EntityID EntityIDMake(uint16_t typeBits, uint16_t seq);
uint16_t EntityIDType(EntityID id);
uint16_t EntityIDSeq(EntityID id);

// Player colors for multiplayer tower tinting
extern const Color PLAYER_COLORS[MAX_PLAYERS];

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
    EntityID id;
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

void EnemySpawn(Enemy enemies[], int maxEnemies, EnemyType type, const Map *map, GameState *gs);
void EnemiesUpdate(Enemy enemies[], int maxEnemies, const Map *map, GameState *gs, float dt);
void EnemiesDraw(const Enemy enemies[], int maxEnemies, Model sphereModel);
void EnemiesDrawHUD(const Enemy enemies[], int maxEnemies, Camera3D camera);
Enemy *EnemyFindByID(Enemy enemies[], int maxEnemies, EntityID id);

// --- Projectile ---

typedef struct {
    bool active;
    EntityID id;
    Vector3 position;
    EntityID targetEnemyID;
    float damage;
    float speed;
    float slowFactor;
    float slowDuration;
    float aoeRadius;
    Color color;
    uint8_t ownerPlayer;
} Projectile;

void ProjectileSpawn(Projectile projectiles[], int maxProjectiles,
                     Vector3 origin, EntityID targetID, float damage, float speed,
                     float slowFactor, float slowDuration, float aoeRadius, Color color,
                     uint8_t ownerPlayer, GameState *gs);
void ProjectilesUpdate(Projectile projectiles[], int maxProjectiles,
                       Enemy enemies[], int maxEnemies, GameState *gs, float dt);
void ProjectilesDraw(const Projectile projectiles[], int maxProjectiles, Model sphereModel);

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
    EntityID id;
    TowerType type;
    int level;
    GridPos gridPos;
    Vector3 worldPos;
    float cooldownTimer;
    uint8_t ownerPlayer;
} Tower;

extern const TowerConfig TOWER_CONFIGS[TOWER_TYPE_COUNT][TOWER_MAX_LEVEL];
extern const char *TOWER_NAMES[TOWER_TYPE_COUNT];

void TowerPlace(Tower towers[], int maxTowers, TowerType type, GridPos pos,
                uint8_t ownerPlayer, GameState *gs, const Map *map);
void TowersUpdate(Tower towers[], int maxTowers, Enemy enemies[], int maxEnemies,
                  Projectile projectiles[], int maxProjectiles, GameState *gs,
                  const Map *map, float dt);
void TowersDraw(const Tower towers[], int maxTowers, int playerCount);
Tower *TowerFindByID(Tower towers[], int maxTowers, EntityID id);

#endif
