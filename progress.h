#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdbool.h>
#include <stdint.h>

// --- Shop Items ---

typedef enum {
    SHOP_DAMAGE_1,
    SHOP_DAMAGE_2,
    SHOP_STARTING_GOLD,
    SHOP_STARTING_LIVES,
    SHOP_TOWER_COST,
    SHOP_GOLD_PER_KILL,
    SHOP_TOWER_RANGE,
    SHOP_UNLOCK_SNIPER,
    SHOP_UNLOCK_SLOW,
    SHOP_UNLOCK_LASER,
    SHOP_UNLOCK_MORTAR,
    SHOP_UNLOCK_TESLA,
    SHOP_UNLOCK_FLAME,
    SHOP_ABILITY_AIRSTRIKE,
    SHOP_ABILITY_GOLD_RUSH,
    SHOP_ABILITY_FORTIFY,
    SHOP_ITEM_COUNT, // 16 items (0-15)
} ShopItemID;

typedef struct {
    const char *name;
    const char *description;
    int cost;
    int prereq; // -1 = none, otherwise ShopItemID
} ShopItemConfig;

extern const ShopItemConfig SHOP_ITEMS[SHOP_ITEM_COUNT];

// --- Perks ---

typedef enum {
    PERK_NONE = -1,
    PERK_PENETRATING_ROUNDS,
    PERK_GLASS_CANNON,
    PERK_GREED,
    PERK_BUNKER_DOWN,
    PERK_RAPID_FIRE,
    PERK_EAGLE_EYE,
    PERK_WAR_BONDS,
    PERK_PERMAFROST,
    PERK_TANK_BUSTER,
    PERK_FIELD_ENGINEER,
    PERK_EXPLOSIVE_TIPS,
    PERK_SECOND_WIND,
    PERK_PREFAB_TOWERS,
    PERK_OVERCHARGE,
    PERK_COUNT,
} PerkID;

typedef struct {
    const char *name;
    const char *description;
    bool hasTradeoff;
} PerkConfig;

extern const PerkConfig PERK_CONFIGS[PERK_COUNT];

// --- Abilities ---

typedef enum {
    ABILITY_AIRSTRIKE,
    ABILITY_GOLD_RUSH,
    ABILITY_FORTIFY,
    ABILITY_COUNT,
} AbilityID;

// --- Player Profile (persistent save data) ---

#define SAVE_MAGIC 0x46445356
#define SAVE_VERSION 1

typedef struct {
    uint32_t magic;
    uint8_t version;
    int crystals;
    int totalRuns;
    int totalWins;
    bool shopPurchased[SHOP_ITEM_COUNT];
} PlayerProfile;

// --- Run Modifiers (computed per-run from profile + perk) ---

struct RunModifiers {
    // Multipliers
    float damageMultiplier;
    float rangeMultiplier;
    float fireRateMultiplier;
    float towerCostMultiplier;
    float upgradeCostMultiplier;
    float goldPerKillMultiplier;
    float waveBonusMultiplier;
    float slowEffectMultiplier;
    float tankDamageMultiplier;
    float fastDamageMultiplier;

    // Bonuses
    int bonusStartingGold;
    int bonusStartingLives;
    float aoeBonus;

    // Flags
    bool sniperPierce;
    bool secondWind;
    bool secondWindUsed;
    bool overcharge;
    bool goldRushActive;

    // Tower unlocks (index by TowerType)
    bool towerUnlocked[8]; // TOWER_TYPE_COUNT

    // Abilities
    bool abilityUnlocked[ABILITY_COUNT];
    float abilityCooldown[ABILITY_COUNT];
    float abilityTimer[ABILITY_COUNT];

    // Active perk
    int activePerk; // PerkID or PERK_NONE

    // Greed speed modifier (applied to enemy spawn)
    float greedSpeedMultiplier;
};

// --- Endless Mode ---

typedef struct {
    bool active;
    int endlessWave;
} EndlessState;

// --- Functions ---

void PlayerProfileInit(PlayerProfile *profile);
bool PlayerProfileLoad(PlayerProfile *profile, const char *path);
bool PlayerProfileSave(const PlayerProfile *profile, const char *path);

void RunModifiersInit(struct RunModifiers *mods, const PlayerProfile *profile);
void RunModifiersApplyPerk(struct RunModifiers *mods, int perkID);

int CrystalsCalculate(int wavesCompleted, int livesRemaining, int maxLives,
                      int difficulty, bool won);

void PerkSelectRandom(int offered[3], unsigned int seed);

// Endless wave generation — fills a wave config (from game.h forward-declared types)
// Returns groupCount, fills groups array
typedef struct WaveGroup WaveGroup;
typedef struct WaveConfig WaveConfig;
void EndlessGenerateWave(WaveConfig *out, int endlessWaveNum);

bool ShopCanPurchase(const PlayerProfile *profile, ShopItemID item);
void ShopPurchase(PlayerProfile *profile, ShopItemID item);

// Ability cooldowns (seconds)
#define ABILITY_AIRSTRIKE_COOLDOWN 45.0f
#define ABILITY_GOLD_RUSH_COOLDOWN 60.0f
#define ABILITY_GOLD_RUSH_DURATION 10.0f
#define ABILITY_FORTIFY_COOLDOWN   90.0f
#define ABILITY_FORTIFY_DURATION   15.0f

#endif
