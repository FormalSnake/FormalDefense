#include "game.h"
#include "entity.h"
#include "progress.h"

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
    { .groups = {{ ENEMY_BASIC, 5 }}, .groupCount = 1, .spawnInterval = 1.5f, .bonusGold = 30 },
    { .groups = {{ ENEMY_BASIC, 8 }}, .groupCount = 1, .spawnInterval = 1.2f, .bonusGold = 35 },
    { .groups = {{ ENEMY_BASIC, 5 }, { ENEMY_FAST, 3 }}, .groupCount = 2, .spawnInterval = 1.0f, .bonusGold = 40 },
    { .groups = {{ ENEMY_FAST, 10 }}, .groupCount = 1, .spawnInterval = 0.8f, .bonusGold = 45 },
    { .groups = {{ ENEMY_BASIC, 6 }, { ENEMY_TANK, 2 }}, .groupCount = 2, .spawnInterval = 1.2f, .bonusGold = 55 },
    { .groups = {{ ENEMY_BASIC, 4 }}, .groupCount = 1, .spawnInterval = 1.5f, .bonusGold = 60 },

    // Act 2: Escalation (7-14)
    { .groups = {{ ENEMY_FAST, 8 }, { ENEMY_TANK, 3 }}, .groupCount = 2, .spawnInterval = 1.0f, .bonusGold = 70 },
    { .groups = {{ ENEMY_BASIC, 12 }, { ENEMY_FAST, 5 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 80 },
    { .groups = {{ ENEMY_TANK, 5 }, { ENEMY_FAST, 8 }}, .groupCount = 2, .spawnInterval = 0.9f, .bonusGold = 90 },
    { .groups = {{ ENEMY_BASIC, 10 }, { ENEMY_FAST, 8 }, { ENEMY_TANK, 3 }}, .groupCount = 3, .spawnInterval = 0.7f, .bonusGold = 100 },
    { .groups = {{ ENEMY_BASIC, 6 }, { ENEMY_FAST, 2 }}, .groupCount = 2, .spawnInterval = 1.2f, .bonusGold = 80 },
    { .groups = {{ ENEMY_FAST, 20 }}, .groupCount = 1, .spawnInterval = 0.5f, .bonusGold = 110 },
    { .groups = {{ ENEMY_TANK, 6 }, { ENEMY_BASIC, 10 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 120 },
    { .groups = {{ ENEMY_BASIC, 8 }}, .groupCount = 1, .spawnInterval = 1.0f, .bonusGold = 100 },

    // Act 3: Endgame (15-20)
    { .groups = {{ ENEMY_TANK, 8 }, { ENEMY_FAST, 6 }}, .groupCount = 2, .spawnInterval = 0.7f, .bonusGold = 140 },
    { .groups = {{ ENEMY_FAST, 25 }}, .groupCount = 1, .spawnInterval = 0.4f, .bonusGold = 150 },
    { .groups = {{ ENEMY_BASIC, 12 }, { ENEMY_FAST, 10 }, { ENEMY_TANK, 5 }}, .groupCount = 3, .spawnInterval = 0.6f, .bonusGold = 160 },
    { .groups = {{ ENEMY_TANK, 10 }, { ENEMY_BASIC, 4 }}, .groupCount = 2, .spawnInterval = 0.8f, .bonusGold = 170 },
    { .groups = {{ ENEMY_BASIC, 15 }, { ENEMY_FAST, 12 }, { ENEMY_TANK, 6 }}, .groupCount = 3, .spawnInterval = 0.5f, .bonusGold = 180 },
    { .groups = {{ ENEMY_FAST, 10 }, { ENEMY_TANK, 8 }, { ENEMY_BASIC, 15 }, { ENEMY_FAST, 8 }}, .groupCount = 4, .spawnInterval = 0.4f, .bonusGold = 250 },
};

void GameStateInit(GameState *gs, Difficulty difficulty, RunModifiers *mods)
{
    const DifficultyConfig *dc = &DIFFICULTY_CONFIGS[difficulty];
    int startGold = dc->startingGold;
    int startLives = dc->startingLives;

    if (mods) {
        startGold += mods->bonusStartingGold;
        startLives += mods->bonusStartingLives;
    }

    gs->gold = startGold;
    gs->lives = startLives;
    gs->maxLives = startLives;
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
    gs->greedSpeedMult = (mods) ? mods->greedSpeedMultiplier : 1.0f;

    // Multiplayer defaults (single-player compatible)
    gs->playerCount = 1;
    gs->hpMultiplier = dc->hpMultiplier;
    gs->countMultiplier = dc->countMultiplier;
    gs->goldPerKill = (int)(10 * dc->goldMultiplier);
    gs->nextEntitySeq = 1;
    for (int i = 0; i < 4; i++) gs->playerGold[i] = 0;
    gs->playerGold[0] = startGold;

    gs->endlessActive = false;
    gs->endlessWave = 0;
}

void GameStateInitMultiplayer(GameState *gs, int playerCount, Difficulty difficulty, RunModifiers *mods)
{
    GameStateInit(gs, difficulty, mods);
    const DifficultyConfig *dc = &DIFFICULTY_CONFIGS[difficulty];
    int startGold = dc->startingGold;
    if (mods) startGold += mods->bonusStartingGold;

    gs->playerCount = playerCount;
    gs->hpMultiplier = dc->hpMultiplier * (1.0f + 0.5f * (playerCount - 1));
    gs->countMultiplier = dc->countMultiplier * (1.0f + 0.3f * (playerCount - 1));
    gs->goldPerKill = (int)(10 * dc->goldMultiplier) / playerCount;
    if (gs->goldPerKill < 3) gs->goldPerKill = 3;
    gs->lives = gs->maxLives + 5 * (playerCount - 1);
    gs->maxLives = gs->lives;
    for (int i = 0; i < playerCount; i++) gs->playerGold[i] = startGold;
}

bool GameAllEnemiesDead(const void *enemiesVoid, int maxEnemies)
{
    const Enemy *enemies = (const Enemy *)enemiesVoid;
    for (int i = 0; i < maxEnemies; i++) {
        if (enemies[i].active) return false;
    }
    return true;
}

void GameUpdateWave(GameState *gs, void *enemiesVoid, int maxEnemies, const Map *map,
                    RunModifiers *mods, float dt)
{
    Enemy *enemies = (Enemy *)enemiesVoid;
    const DifficultyConfig *dc = &DIFFICULTY_CONFIGS[gs->difficulty];

    if (gs->phase == PHASE_OVER || gs->phase == PHASE_VICTORY) return;

    // Second wind check
    if (gs->lives <= 0 && mods && mods->secondWind && !mods->secondWindUsed) {
        mods->secondWindUsed = true;
        gs->lives = 5;
    }

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

            // Get wave config (normal or endless)
            const WaveConfig *wc;
            WaveConfig endlessWave;
            if (gs->endlessActive && gs->currentWave >= MAX_WAVES) {
                EndlessGenerateWave(&endlessWave, gs->endlessWave);
                wc = &endlessWave;
            } else {
                wc = &WAVE_CONFIGS[gs->currentWave];
            }

            // Count total enemies in this wave (with count multiplier)
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

    // Get wave config
    const WaveConfig *wc;
    WaveConfig endlessWave;
    if (gs->endlessActive && gs->currentWave >= MAX_WAVES) {
        EndlessGenerateWave(&endlessWave, gs->endlessWave);
        wc = &endlessWave;
    } else {
        wc = &WAVE_CONFIGS[gs->currentWave];
    }

    // Spawn enemies
    if (gs->waveActive && gs->currentGroup < wc->groupCount) {
        gs->spawnTimer -= dt;
        if (gs->spawnTimer <= 0.0f) {
            const WaveGroup *grp = &wc->groups[gs->currentGroup];
            int scaledCount = (int)(grp->count * gs->countMultiplier);
            if (scaledCount < 1) scaledCount = 1;

            EnemySpawn(enemies, maxEnemies, (EnemyType)grp->enemyType, map, gs);

            // Apply greed speed multiplier to spawned enemy
            if (gs->greedSpeedMult > 1.0f) {
                for (int i = 0; i < maxEnemies; i++) {
                    if (enemies[i].active && enemies[i].waypointIndex == 0 &&
                        enemies[i].pathProgress == 0.0f) {
                        enemies[i].speed *= gs->greedSpeedMult;
                        enemies[i].baseSpeed *= gs->greedSpeedMult;
                        break;
                    }
                }
            }

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
        // Split bonus gold among players (with gold multiplier + wave bonus multiplier)
        float waveBonusMult = (mods) ? mods->waveBonusMultiplier : 1.0f;
        int bonusPerPlayer = (int)(wc->bonusGold * dc->goldMultiplier * waveBonusMult) / gs->playerCount;
        for (int i = 0; i < gs->playerCount; i++)
            gs->playerGold[i] += bonusPerPlayer;
        // Keep legacy gold in sync for single-player HUD
        gs->gold = gs->playerGold[0];

        gs->currentWave++;

        if (gs->endlessActive) {
            gs->endlessWave++;
            gs->phase = PHASE_WAVE_COUNTDOWN;
            gs->waveCountdown = 5.0f;
        } else if (gs->currentWave >= MAX_WAVES) {
            gs->phase = PHASE_VICTORY;
        } else {
            gs->phase = PHASE_WAVE_COUNTDOWN;
            gs->waveCountdown = 5.0f;
        }
    }
}
