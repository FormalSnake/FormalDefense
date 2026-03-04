#include "game.h"
#include "entity.h"

const WaveConfig WAVE_CONFIGS[MAX_WAVES] = { {0} };

void GameStateInit(struct GameState *gs) {
    gs->gold = 250;
    gs->lives = 20;
    gs->currentWave = 0;
    gs->phase = PHASE_WAVE_COUNTDOWN;
    gs->waveCountdown = 5.0f;
    gs->spawnTimer = 0.0f;
    gs->currentGroup = 0;
    gs->spawnedInGroup = 0;
    gs->totalSpawned = 0;
    gs->totalToSpawn = 0;
    gs->waveActive = false;
}

void GameUpdateWave(struct GameState *gs, void *enemies, int maxEnemies, const Map *map, float dt) {
    (void)gs; (void)enemies; (void)maxEnemies; (void)map; (void)dt;
}
