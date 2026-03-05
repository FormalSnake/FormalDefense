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

// Forward declare RunModifiers
typedef struct RunModifiers RunModifiers;

// --- Entity ID ---

typedef uint16_t EntityID;

#define ENTITY_ID_NONE 0

EntityID EntityIDMake(uint16_t seq);

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
    // Flame burn DoT
    float burnTimer;
    float burnDPS;
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
    int pierceRemaining;
    // Mortar arcing
    bool isArcing;
    Vector3 targetPos;
    float arcProgress;
    Vector3 startPos;
} Projectile;

void ProjectileSpawn(Projectile projectiles[], int maxProjectiles,
                     Vector3 origin, EntityID targetID, float damage, float speed,
                     float slowFactor, float slowDuration, float aoeRadius, Color color,
                     uint8_t ownerPlayer, GameState *gs);
void ProjectilesUpdate(Projectile projectiles[], int maxProjectiles,
                       Enemy enemies[], int maxEnemies, GameState *gs,
                       RunModifiers *mods, float dt);
void ProjectilesDraw(const Projectile projectiles[], int maxProjectiles, Model sphereModel);

// --- Tower ---

typedef enum {
    TOWER_CANNON,
    TOWER_MACHINEGUN,
    TOWER_SNIPER,
    TOWER_SLOW,
    TOWER_LASER,
    TOWER_MORTAR,
    TOWER_TESLA,
    TOWER_FLAME,
    TOWER_TYPE_COUNT,
} TowerType;

#define TOWER_MAX_LEVEL 3

typedef enum {
    TOWER_ATTACK_PROJECTILE,  // Cannon, MG, Sniper
    TOWER_ATTACK_BEAM,        // Laser
    TOWER_ATTACK_CHAIN,       // Tesla
    TOWER_ATTACK_CONE,        // Flame
    TOWER_ATTACK_MORTAR,      // Mortar
} TowerAttackMode;

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
    TowerAttackMode attackMode;
    int chainCount;       // Tesla: chain lightning count
    float chainRange;     // Tesla: chain distance
    float chainDamageMult;// Tesla: per-chain falloff
    float coneAngle;      // Flame: half-angle in degrees
    float burnDPS;        // Flame: burn damage per second
    float burnDuration;   // Flame: burn duration
    float arcHeightFactor;// Mortar: arc height ratio
    float projectileSize; // Visual size
    float overchargeMult; // First-shot bonus multiplier
    int pierceCount;      // Sniper: targets to pierce (0 = disabled, set by perk)
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
    // Laser beam target
    EntityID beamTarget;
    // Overcharge: track if first shot after cooldown
    bool cooldownReady;
} Tower;

extern const TowerConfig TOWER_CONFIGS[TOWER_TYPE_COUNT][TOWER_MAX_LEVEL];
extern const char *TOWER_NAMES[TOWER_TYPE_COUNT];

// --- Tower Behavior Registry ---

typedef int  (*TowerTargetFn)(const Tower *, const Enemy[], int, const Map *, RunModifiers *);
typedef void (*TowerFireFn)(Tower *, const TowerConfig *, Enemy[], int, Projectile[], int, GameState *, const Map *, RunModifiers *, float dt);
typedef void (*TowerDrawFn)(const Tower *, const TowerConfig *, const Enemy[], int);

typedef struct {
    TowerTargetFn   target;     // NULL = default TowerFindTarget
    TowerFireFn     fire;       // Required
    TowerDrawFn     drawEffect; // NULL = no special effect
} TowerBehavior;

extern const TowerBehavior TOWER_BEHAVIORS[TOWER_TYPE_COUNT];

void TowerPlace(Tower towers[], int maxTowers, TowerType type, GridPos pos,
                uint8_t ownerPlayer, GameState *gs, const Map *map);
void TowersUpdate(Tower towers[], int maxTowers, Enemy enemies[], int maxEnemies,
                  Projectile projectiles[], int maxProjectiles, GameState *gs,
                  const Map *map, RunModifiers *mods, float dt);
void TowersDraw(const Tower towers[], int maxTowers, int playerCount,
                const Enemy enemies[], int maxEnemies);
Tower *TowerFindByID(Tower towers[], int maxTowers, EntityID id);

#endif
