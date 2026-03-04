#include "game.h"
#include "entity.h"

// --- Difficulty Configs ---

const DifficultyConfig DIFFICULTY_CONFIGS[DIFFICULTY_COUNT] = {
    [DIFFICULTY_EASY] = {
        .name = "Easy", .color = { 100, 220, 100, 255 },
        .hpMultiplier = 0.7f, .countMultiplier = 0.8f, .speedMultiplier = 0.85f,
        .goldMultiplier = 1.3f, .spawnIntervalScale = 1.15f,
        .startingGold = 300, .startingLives = 30,
    },
    [DIFFICULTY_NORMAL] = {
        .name = "Normal", .color = { 220, 220, 100, 255 },
        .hpMultiplier = 1.0f, .countMultiplier = 1.0f, .speedMultiplier = 1.0f,
        .goldMultiplier = 1.0f, .spawnIntervalScale = 1.0f,
        .startingGold = 250, .startingLives = 20,
    },
    [DIFFICULTY_HARD] = {
        .name = "Hard", .color = { 220, 120, 50, 255 },
        .hpMultiplier = 1.4f, .countMultiplier = 1.3f, .speedMultiplier = 1.15f,
        .goldMultiplier = 0.8f, .spawnIntervalScale = 0.85f,
        .startingGold = 200, .startingLives = 15,
    },
    [DIFFICULTY_NIGHTMARE] = {
        .name = "Nightmare", .color = { 220, 50, 50, 255 },
        .hpMultiplier = 2.0f, .countMultiplier = 1.6f, .speedMultiplier = 1.3f,
        .goldMultiplier = 0.6f, .spawnIntervalScale = 0.7f,
        .startingGold = 150, .startingLives = 10,
    },
};

// --- 20 Waves (3 Acts) ---

const WaveConfig WAVE_CONFIGS[MAX_WAVES] = {
    // Act 1: Introduction (1-6)
    // Wave 1: 5 basic
    { .groups = {{ ENEMY_BASIC, 5 }}, .groupCount = 1, .spawnInterval = 1.5f, .bonusGold = 30 },
    // Wave 2: 8 basic
    { .groups = {{ ENEMY_BASIC, 8 }}, .groupCount = 1, .spawnInterval = 1.2f, .bonusGold = 35 },
    // Wave 3: 5 basic + 3 fast
    { .groups = {{ ENEMY_BASIC, 5 }, { ENEMY_FAST, 3 }}, .groupCount = 2, .spawnInterval = 1.0f, .bonusGold = 40 },
    // Wave 4: 10 fast
    { .groups = {{ ENEMY_FAST, 10 }}, .groupCount = 1, .spawnInterval = 0.8f, .bonusGold = 45 },
    // Wave 5: 6 basic + 2 tank
    { .groups = {{ ENEMY_BASIC, 6 }, { ENEMY_TANK, 2 }}, .groupCount = 2, .spawnInterval = 1.2f, .bonusGold = 55 },
    // Wave 6: breather — 4 basic
    { .groups = {{ ENEMY_BASIC, 4 }}, .groupCount = 1, .spawnInterval = 1.5f, .bonusGold = 60 },

    // Act 2: Escalation (7-14)
    // Wave 7: 8 fast + 3 tank
    { .groups = {{ ENEMY_FAST, 8 }, { ENEMY_TANK, 3 }}, .groupCount = 2, .spawnInterval = 1.0f, .bonusGold = 70 },
    // Wave 8: 12 basic + 5 fast
    { .groups = {{ ENEMY_BASIC, 12 }, { ENEMY_FAST, 5 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 80 },
    // Wave 9: 5 tank + 8 fast
    { .groups = {{ ENEMY_TANK, 5 }, { ENEMY_FAST, 8 }}, .groupCount = 2, .spawnInterval = 0.9f, .bonusGold = 90 },
    // Wave 10: 10 basic + 8 fast + 3 tank
    { .groups = {{ ENEMY_BASIC, 10 }, { ENEMY_FAST, 8 }, { ENEMY_TANK, 3 }}, .groupCount = 3, .spawnInterval = 0.7f, .bonusGold = 100 },
    // Wave 11: breather — 6 basic + 2 fast
    { .groups = {{ ENEMY_BASIC, 6 }, { ENEMY_FAST, 2 }}, .groupCount = 2, .spawnInterval = 1.2f, .bonusGold = 80 },
    // Wave 12: fast rush — 20 fast
    { .groups = {{ ENEMY_FAST, 20 }}, .groupCount = 1, .spawnInterval = 0.5f, .bonusGold = 110 },
    // Wave 13: 6 tank + 10 basic
    { .groups = {{ ENEMY_TANK, 6 }, { ENEMY_BASIC, 10 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 120 },
    // Wave 14: breather — 8 basic
    { .groups = {{ ENEMY_BASIC, 8 }}, .groupCount = 1, .spawnInterval = 1.0f, .bonusGold = 100 },

    // Act 3: Endgame (15-20)
    // Wave 15: 8 tank + 6 fast
    { .groups = {{ ENEMY_TANK, 8 }, { ENEMY_FAST, 6 }}, .groupCount = 2, .spawnInterval = 0.7f, .bonusGold = 140 },
    // Wave 16: speed blitz — 25 fast
    { .groups = {{ ENEMY_FAST, 25 }}, .groupCount = 1, .spawnInterval = 0.4f, .bonusGold = 150 },
    // Wave 17: 12 basic + 10 fast + 5 tank
    { .groups = {{ ENEMY_BASIC, 12 }, { ENEMY_FAST, 10 }, { ENEMY_TANK, 5 }}, .groupCount = 3, .spawnInterval = 0.6f, .bonusGold = 160 },
    // Wave 18: heavy tanks — 10 tank + 4 basic
    { .groups = {{ ENEMY_TANK, 10 }, { ENEMY_BASIC, 4 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 170 },
    // Wave 19: massive mixed — 15 basic + 12 fast + 6 tank
    { .groups = {{ ENEMY_BASIC, 15 }, { ENEMY_FAST, 12 }, { ENEMY_TANK, 6 }}, .groupCount = 3, .spawnInterval = 0.5f, .bonusGold = 180 },
    // Wave 20: final onslaught — 10 fast + 8 tank + 15 basic
    { .groups = {{ ENEMY_FAST, 10 }, { ENEMY_TANK, 8 }, { ENEMY_BASIC, 15 }, { ENEMY_FAST, 8 }}, .groupCount = 4, .spawnInterval = 0.4f, .bonusGold = 250 },
};

void GameStateInit(GameState *gs, Difficulty difficulty)
{
    const DifficultyConfig *dc = &DIFFICULTY_CONFIGS[difficulty];
    gs->gold = dc->startingGold;
    gs->lives = dc->startingLives;
    gs->currentWave = 0;
    gs->phase = PHASE_WAVE_COUNTDOWN;
    gs->waveCountdown = 5.0f;
    gs->spawnTimer = 0.0f;
    gs->currentGroup = 0;
    gs->spawnedInGroup = 0;
    gs->totalSpawned = 0;
    gs->totalToSpawn = 0;
    gs->waveActive = false;

    gs->difficulty = difficulty;
    gs->speedMultiplier = dc->speedMultiplier;

    // Multiplayer defaults (single-player compatible)
    gs->playerCount = 1;
    gs->hpMultiplier = dc->hpMultiplier;
    gs->countMultiplier = dc->countMultiplier;
    gs->goldPerKill = (int)(10 * dc->goldMultiplier);
    gs->nextEntitySeq = 1;
    for (int i = 0; i < 4; i++) gs->playerGold[i] = 0;
    gs->playerGold[0] = dc->startingGold;
}

void GameStateInitMultiplayer(GameState *gs, int playerCount, Difficulty difficulty)
{
    GameStateInit(gs, difficulty);
    const DifficultyConfig *dc = &DIFFICULTY_CONFIGS[difficulty];
    gs->playerCount = playerCount;
    gs->hpMultiplier = dc->hpMultiplier * (1.0f + 0.5f * (playerCount - 1));
    gs->countMultiplier = dc->countMultiplier * (1.0f + 0.3f * (playerCount - 1));
    gs->goldPerKill = (int)(10 * dc->goldMultiplier) / playerCount;
    if (gs->goldPerKill < 3) gs->goldPerKill = 3;
    gs->lives = dc->startingLives + 5 * (playerCount - 1);
    for (int i = 0; i < playerCount; i++) gs->playerGold[i] = dc->startingGold;
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
    const DifficultyConfig *dc = &DIFFICULTY_CONFIGS[gs->difficulty];

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
            gs->spawnTimer = wc->spawnInterval * dc->spawnIntervalScale;

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
        // Split bonus gold among players (with gold multiplier)
        int bonusPerPlayer = (int)(wc->bonusGold * dc->goldMultiplier) / gs->playerCount;
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
