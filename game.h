#ifndef GAME_H
#define GAME_H

#include "map.h"
#include <stdbool.h>

typedef enum {
    PHASE_PLAYING,
    PHASE_WAVE_COUNTDOWN,
    PHASE_OVER,
    PHASE_PAUSED,
} GamePhase;

#define MAX_WAVE_GROUPS 4

typedef struct {
    int enemyType;
    int count;
} WaveGroup;

typedef struct {
    WaveGroup groups[MAX_WAVE_GROUPS];
    int groupCount;
    float spawnInterval;
    int bonusGold;
} WaveConfig;

#define MAX_WAVES 10

struct GameState {
    int gold;
    int lives;
    int currentWave;
    GamePhase phase;
    float waveCountdown;
    float spawnTimer;
    int currentGroup;
    int spawnedInGroup;
    int totalSpawned;
    int totalToSpawn;
    bool waveActive;
};

extern const WaveConfig WAVE_CONFIGS[MAX_WAVES];

void GameStateInit(struct GameState *gs);
void GameUpdateWave(struct GameState *gs, void *enemies, int maxEnemies, const Map *map, float dt);

#endif
