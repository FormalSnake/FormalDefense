#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "map.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declare RunModifiers
typedef struct RunModifiers RunModifiers;

typedef enum {
    PHASE_PLAYING,
    PHASE_WAVE_COUNTDOWN,
    PHASE_OVER,
    PHASE_PAUSED,
    PHASE_VICTORY,
} GamePhase;

#define MAX_WAVE_GROUPS 4

typedef struct WaveGroup {
    int enemyType;
    int count;
} WaveGroup;

typedef struct WaveConfig {
    WaveGroup groups[MAX_WAVE_GROUPS];
    int groupCount;
    float spawnInterval;
    int bonusGold;
} WaveConfig;

#define MAX_WAVES 20

typedef enum {
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD,
    DIFFICULTY_NIGHTMARE,
    DIFFICULTY_COUNT,
} Difficulty;

typedef struct {
    const char *name;
    Color color;
    float hpMultiplier;
    float countMultiplier;
    float speedMultiplier;
    float goldMultiplier;
    float spawnIntervalScale;
    int startingGold;
    int startingLives;
} DifficultyConfig;

extern const DifficultyConfig DIFFICULTY_CONFIGS[DIFFICULTY_COUNT];

struct GameState {
    int gold;              // Legacy single-player gold (alias for playerGold[0])
    int lives;
    int maxLives;          // For crystal calculation
    int currentWave;
    GamePhase phase;
    float waveCountdown;
    float spawnTimer;
    int currentGroup;
    int spawnedInGroup;
    int totalSpawned;
    int totalToSpawn;
    bool waveActive;

    // Difficulty
    Difficulty difficulty;
    float speedMultiplier;

    // Multiplayer fields
    int playerGold[4];
    int playerCount;
    float hpMultiplier;
    float countMultiplier;
    int goldPerKill;
    uint16_t nextEntitySeq;

    // Endless mode
    bool endlessActive;
    int endlessWave;

    // Greed speed multiplier from perk
    float greedSpeedMult;
};

extern const WaveConfig WAVE_CONFIGS[MAX_WAVES];

void GameStateInit(struct GameState *gs, Difficulty difficulty, RunModifiers *mods);
void GameStateInitMultiplayer(struct GameState *gs, int playerCount, Difficulty difficulty, RunModifiers *mods);

// Forward declare Enemy (defined in entity.h)
typedef struct Enemy_ Enemy_;
void GameUpdateWave(struct GameState *gs, void *enemies, int maxEnemies, const Map *map,
                    RunModifiers *mods, float dt);
bool GameAllEnemiesDead(const void *enemies, int maxEnemies);

#endif
