#include "game.h"
#include "entity.h"

// 10 escalating waves
const WaveConfig WAVE_CONFIGS[MAX_WAVES] = {
    // Wave 1: 5 basic
    { .groups = {{ ENEMY_BASIC, 5 }}, .groupCount = 1, .spawnInterval = 1.5f, .bonusGold = 30 },
    // Wave 2: 8 basic
    { .groups = {{ ENEMY_BASIC, 8 }}, .groupCount = 1, .spawnInterval = 1.2f, .bonusGold = 40 },
    // Wave 3: 5 basic + 3 fast
    { .groups = {{ ENEMY_BASIC, 5 }, { ENEMY_FAST, 3 }}, .groupCount = 2, .spawnInterval = 1.0f, .bonusGold = 50 },
    // Wave 4: 10 fast
    { .groups = {{ ENEMY_FAST, 10 }}, .groupCount = 1, .spawnInterval = 0.8f, .bonusGold = 60 },
    // Wave 5: 6 basic + 2 tank
    { .groups = {{ ENEMY_BASIC, 6 }, { ENEMY_TANK, 2 }}, .groupCount = 2, .spawnInterval = 1.2f, .bonusGold = 80 },
    // Wave 6: 8 fast + 3 tank
    { .groups = {{ ENEMY_FAST, 8 }, { ENEMY_TANK, 3 }}, .groupCount = 2, .spawnInterval = 1.0f, .bonusGold = 90 },
    // Wave 7: 12 basic + 5 fast
    { .groups = {{ ENEMY_BASIC, 12 }, { ENEMY_FAST, 5 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 100 },
    // Wave 8: 5 tank + 8 fast
    { .groups = {{ ENEMY_TANK, 5 }, { ENEMY_FAST, 8 }}, .groupCount = 2, .spawnInterval = 0.9f, .bonusGold = 120 },
    // Wave 9: 10 basic + 8 fast + 4 tank
    { .groups = {{ ENEMY_BASIC, 10 }, { ENEMY_FAST, 8 }, { ENEMY_TANK, 4 }}, .groupCount = 3, .spawnInterval = 0.7f, .bonusGold = 150 },
    // Wave 10: 8 fast + 6 tank + 12 basic
    { .groups = {{ ENEMY_FAST, 8 }, { ENEMY_TANK, 6 }, { ENEMY_BASIC, 12 }}, .groupCount = 3, .spawnInterval = 0.6f, .bonusGold = 200 },
};

void GameStateInit(GameState *gs)
{
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

    // Multiplayer defaults (single-player compatible)
    gs->playerCount = 1;
    gs->hpMultiplier = 1.0f;
    gs->countMultiplier = 1.0f;
    gs->goldPerKill = 10;
    gs->nextEntitySeq = 1;
    for (int i = 0; i < 4; i++) gs->playerGold[i] = 0;
    gs->playerGold[0] = 250;
}

void GameStateInitMultiplayer(GameState *gs, int playerCount)
{
    GameStateInit(gs);
    gs->playerCount = playerCount;
    gs->hpMultiplier = 1.0f + 0.5f * (playerCount - 1);
    gs->countMultiplier = 1.0f + 0.3f * (playerCount - 1);
    gs->goldPerKill = 10 / playerCount;
    if (gs->goldPerKill < 3) gs->goldPerKill = 3;
    gs->lives = 20 + 5 * (playerCount - 1);
    for (int i = 0; i < playerCount; i++) gs->playerGold[i] = 250;
}

bool GameAllEnemiesDead(const void *enemiesVoid, int maxEnemies)
{
    const Enemy *enemies = (const Enemy *)enemiesVoid;
    for (int i = 0; i < maxEnemies; i++) {
        if (enemies[i].active) return false;
    }
    return true;
}

void GameUpdateWave(GameState *gs, void *enemiesVoid, int maxEnemies, const Map *map, float dt)
{
    Enemy *enemies = (Enemy *)enemiesVoid;

    if (gs->phase == PHASE_OVER) return;

    // Wave countdown phase
    if (gs->phase == PHASE_WAVE_COUNTDOWN) {
        gs->waveCountdown -= dt;
        if (gs->waveCountdown <= 0.0f) {
            gs->phase = PHASE_PLAYING;
            gs->waveActive = true;
            gs->spawnTimer = 0.0f;
            gs->currentGroup = 0;
            gs->spawnedInGroup = 0;
            gs->totalSpawned = 0;

            // Count total enemies in this wave (with count multiplier)
            const WaveConfig *wc = &WAVE_CONFIGS[gs->currentWave];
            gs->totalToSpawn = 0;
            for (int g = 0; g < wc->groupCount; g++) {
                int scaledCount = (int)(wc->groups[g].count * gs->countMultiplier);
                if (scaledCount < 1) scaledCount = 1;
                gs->totalToSpawn += scaledCount;
            }
        }
        return;
    }

    if (gs->phase != PHASE_PLAYING) return;

    const WaveConfig *wc = &WAVE_CONFIGS[gs->currentWave];

    // Spawn enemies
    if (gs->waveActive && gs->currentGroup < wc->groupCount) {
        gs->spawnTimer -= dt;
        if (gs->spawnTimer <= 0.0f) {
            const WaveGroup *grp = &wc->groups[gs->currentGroup];
            int scaledCount = (int)(grp->count * gs->countMultiplier);
            if (scaledCount < 1) scaledCount = 1;

            EnemySpawn(enemies, maxEnemies, (EnemyType)grp->enemyType, map, gs);
            gs->spawnedInGroup++;
            gs->totalSpawned++;
            gs->spawnTimer = wc->spawnInterval;

            if (gs->spawnedInGroup >= scaledCount) {
                gs->currentGroup++;
                gs->spawnedInGroup = 0;
            }

            if (gs->currentGroup >= wc->groupCount) {
                gs->waveActive = false;
            }
        }
    }

    // Check wave complete
    if (!gs->waveActive && gs->totalSpawned >= gs->totalToSpawn &&
        GameAllEnemiesDead(enemies, maxEnemies)) {
        // Split bonus gold among players
        int bonusPerPlayer = wc->bonusGold / gs->playerCount;
        for (int i = 0; i < gs->playerCount; i++)
            gs->playerGold[i] += bonusPerPlayer;
        // Keep legacy gold in sync for single-player HUD
        gs->gold = gs->playerGold[0];

        gs->currentWave++;

        if (gs->currentWave >= MAX_WAVES) {
            gs->phase = PHASE_OVER;
        } else {
            gs->phase = PHASE_WAVE_COUNTDOWN;
            gs->waveCountdown = 5.0f;
        }
    }
}
