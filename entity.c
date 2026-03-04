#include "entity.h"
#include "game.h"

const EnemyConfig ENEMY_CONFIGS[ENEMY_TYPE_COUNT] = {
    [ENEMY_BASIC] = { 100.0f, 2.0f, 0.3f, GREEN },
    [ENEMY_FAST]  = { 60.0f,  4.0f, 0.25f, LIME },
    [ENEMY_TANK]  = { 300.0f, 1.2f, 0.4f, DARKGREEN },
};

const char *TOWER_NAMES[TOWER_TYPE_COUNT] = { "Cannon", "MG", "Sniper", "Slow" };

const TowerConfig TOWER_CONFIGS[TOWER_TYPE_COUNT][TOWER_MAX_LEVEL] = { {0} };

void EnemySpawn(Enemy enemies[], int maxEnemies, EnemyType type, const Map *map) {
    (void)enemies; (void)maxEnemies; (void)type; (void)map;
}
void EnemiesUpdate(Enemy enemies[], int maxEnemies, const Map *map, GameState *gs, float dt) {
    (void)enemies; (void)maxEnemies; (void)map; (void)gs; (void)dt;
}
void EnemiesDraw(const Enemy enemies[], int maxEnemies, Camera3D camera) {
    (void)enemies; (void)maxEnemies; (void)camera;
}
void TowerPlace(Tower towers[], int maxTowers, TowerType type, GridPos pos) {
    (void)towers; (void)maxTowers; (void)type; (void)pos;
}
void TowersUpdate(Tower towers[], int maxTowers, Enemy enemies[], int maxEnemies,
                  Projectile projectiles[], int maxProjectiles, float dt) {
    (void)towers; (void)maxTowers; (void)enemies; (void)maxEnemies;
    (void)projectiles; (void)maxProjectiles; (void)dt;
}
void TowersDraw(const Tower towers[], int maxTowers) {
    (void)towers; (void)maxTowers;
}
void ProjectileSpawn(Projectile projectiles[], int maxProjectiles,
                     Vector3 origin, int targetIndex, float damage, float speed,
                     float slowFactor, float slowDuration, float aoeRadius, Color color) {
    (void)projectiles; (void)maxProjectiles; (void)origin; (void)targetIndex;
    (void)damage; (void)speed; (void)slowFactor; (void)slowDuration; (void)aoeRadius; (void)color;
}
void ProjectilesUpdate(Projectile projectiles[], int maxProjectiles,
                       Enemy enemies[], int maxEnemies, GameState *gs, float dt) {
    (void)projectiles; (void)maxProjectiles; (void)enemies; (void)maxEnemies; (void)gs; (void)dt;
}
void ProjectilesDraw(const Projectile projectiles[], int maxProjectiles) {
    (void)projectiles; (void)maxProjectiles;
}
