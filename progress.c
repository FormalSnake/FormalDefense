#include "progress.h"
#include "game.h"
#include "entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Shop Item Configs ---

const ShopItemConfig SHOP_ITEMS[SHOP_ITEM_COUNT] = {
    [SHOP_DAMAGE_1]          = { "Hardened Rounds",      "+10% tower damage",        80,  -1 },
    [SHOP_DAMAGE_2]          = { "Armor-Piercing Rounds", "+15% tower damage (stacks)", 150, SHOP_DAMAGE_1 },
    [SHOP_STARTING_GOLD]     = { "War Chest",            "+50 starting gold",       100, -1 },
    [SHOP_STARTING_LIVES]    = { "Reinforced Walls",     "+3 starting lives",       120, -1 },
    [SHOP_TOWER_COST]        = { "Bulk Discount",        "-10% tower build cost",   100, -1 },
    [SHOP_GOLD_PER_KILL]     = { "Bounty Hunter",        "+15% gold per kill",      120, -1 },
    [SHOP_TOWER_RANGE]       = { "Optics Package",       "+10% tower range",        100, -1 },
    [SHOP_UNLOCK_SNIPER]     = { "Sniper Blueprint",     "Unlock Sniper tower",     120, -1 },
    [SHOP_UNLOCK_SLOW]       = { "Slow Field Blueprint", "Unlock Slow tower",       100, -1 },
    [SHOP_UNLOCK_LASER]      = { "Laser Blueprint",      "Unlock Laser tower",      150, -1 },
    [SHOP_UNLOCK_MORTAR]     = { "Mortar Blueprint",     "Unlock Mortar tower",     180, SHOP_UNLOCK_SNIPER },
    [SHOP_UNLOCK_TESLA]      = { "Tesla Blueprint",      "Unlock Tesla tower",      200, SHOP_UNLOCK_SLOW },
    [SHOP_UNLOCK_FLAME]      = { "Flame Blueprint",      "Unlock Flame tower",      150, -1 },
    [SHOP_ABILITY_AIRSTRIKE] = { "Airstrike Module",     "Unlock Airstrike (Q)",    100, -1 },
    [SHOP_ABILITY_GOLD_RUSH] = { "Gold Rush Module",     "Unlock Gold Rush (E)",     80, -1 },
    [SHOP_ABILITY_FORTIFY]   = { "Fortify Module",       "Unlock Fortify (R)",      100, -1 },
};

// --- Perk Configs ---

const PerkConfig PERK_CONFIGS[PERK_COUNT] = {
    [PERK_PENETRATING_ROUNDS] = { "Penetrating Rounds", "Sniper projectiles pierce 2 targets", false },
    [PERK_GLASS_CANNON]       = { "Glass Cannon",       "+30% damage, +20% tower cost",        true },
    [PERK_GREED]              = { "Greed",              "2x gold per kill, enemies 20% faster", true },
    [PERK_BUNKER_DOWN]        = { "Bunker Down",        "+5 starting lives, -30 starting gold", true },
    [PERK_RAPID_FIRE]         = { "Rapid Fire",         "+25% fire rate, -15% damage",          true },
    [PERK_EAGLE_EYE]          = { "Eagle Eye",          "+20% range, -10% fire rate",           true },
    [PERK_WAR_BONDS]          = { "War Bonds",          "+25% wave bonus gold",                 false },
    [PERK_PERMAFROST]         = { "Permafrost",         "Slow towers 40% more effective",       false },
    [PERK_TANK_BUSTER]        = { "Tank Buster",        "+50% damage to Tanks, -20% to Fast",   true },
    [PERK_FIELD_ENGINEER]     = { "Field Engineer",     "Tower upgrades cost 20% less",         false },
    [PERK_EXPLOSIVE_TIPS]     = { "Explosive Tips",     "All projectiles +0.5 AoE radius",      false },
    [PERK_SECOND_WIND]        = { "Second Wind",        "First time lives=0, restore to 5",     false },
    [PERK_PREFAB_TOWERS]      = { "Prefab Towers",      "Towers cost 15% less, -10% range",     true },
    [PERK_OVERCHARGE]         = { "Overcharge",         "First shot after cooldown +40% damage", false },
};

// --- Player Profile ---

void PlayerProfileInit(PlayerProfile *profile)
{
    memset(profile, 0, sizeof(PlayerProfile));
    profile->magic = SAVE_MAGIC;
    profile->version = SAVE_VERSION;
}

bool PlayerProfileLoad(PlayerProfile *profile, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        PlayerProfileInit(profile);
        return false;
    }

    size_t read = fread(profile, sizeof(PlayerProfile), 1, f);
    fclose(f);

    if (read != 1 || profile->magic != SAVE_MAGIC) {
        PlayerProfileInit(profile);
        return false;
    }

    return true;
}

bool PlayerProfileSave(const PlayerProfile *profile, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    size_t written = fwrite(profile, sizeof(PlayerProfile), 1, f);
    fclose(f);

    return written == 1;
}

// --- Run Modifiers ---

void RunModifiersInit(RunModifiers *mods, const PlayerProfile *profile)
{
    memset(mods, 0, sizeof(RunModifiers));

    // Defaults
    mods->damageMultiplier = 1.0f;
    mods->rangeMultiplier = 1.0f;
    mods->fireRateMultiplier = 1.0f;
    mods->towerCostMultiplier = 1.0f;
    mods->upgradeCostMultiplier = 1.0f;
    mods->goldPerKillMultiplier = 1.0f;
    mods->waveBonusMultiplier = 1.0f;
    mods->slowEffectMultiplier = 1.0f;
    mods->tankDamageMultiplier = 1.0f;
    mods->fastDamageMultiplier = 1.0f;
    mods->greedSpeedMultiplier = 1.0f;
    mods->activePerk = PERK_NONE;

    // Starter towers always unlocked
    mods->towerUnlocked[0] = true; // TOWER_CANNON
    mods->towerUnlocked[1] = true; // TOWER_MACHINEGUN

    if (!profile) return;

    // Apply shop purchases
    if (profile->shopPurchased[SHOP_DAMAGE_1])
        mods->damageMultiplier *= 1.10f;
    if (profile->shopPurchased[SHOP_DAMAGE_2])
        mods->damageMultiplier *= 1.15f;
    if (profile->shopPurchased[SHOP_STARTING_GOLD])
        mods->bonusStartingGold += 50;
    if (profile->shopPurchased[SHOP_STARTING_LIVES])
        mods->bonusStartingLives += 3;
    if (profile->shopPurchased[SHOP_TOWER_COST])
        mods->towerCostMultiplier *= 0.90f;
    if (profile->shopPurchased[SHOP_GOLD_PER_KILL])
        mods->goldPerKillMultiplier *= 1.15f;
    if (profile->shopPurchased[SHOP_TOWER_RANGE])
        mods->rangeMultiplier *= 1.10f;

    // Tower unlocks
    if (profile->shopPurchased[SHOP_UNLOCK_SNIPER])
        mods->towerUnlocked[2] = true; // TOWER_SNIPER
    if (profile->shopPurchased[SHOP_UNLOCK_SLOW])
        mods->towerUnlocked[3] = true; // TOWER_SLOW
    if (profile->shopPurchased[SHOP_UNLOCK_LASER])
        mods->towerUnlocked[4] = true; // TOWER_LASER
    if (profile->shopPurchased[SHOP_UNLOCK_MORTAR])
        mods->towerUnlocked[5] = true; // TOWER_MORTAR
    if (profile->shopPurchased[SHOP_UNLOCK_TESLA])
        mods->towerUnlocked[6] = true; // TOWER_TESLA
    if (profile->shopPurchased[SHOP_UNLOCK_FLAME])
        mods->towerUnlocked[7] = true; // TOWER_FLAME

    // Ability unlocks
    if (profile->shopPurchased[SHOP_ABILITY_AIRSTRIKE])
        mods->abilityUnlocked[ABILITY_AIRSTRIKE] = true;
    if (profile->shopPurchased[SHOP_ABILITY_GOLD_RUSH])
        mods->abilityUnlocked[ABILITY_GOLD_RUSH] = true;
    if (profile->shopPurchased[SHOP_ABILITY_FORTIFY])
        mods->abilityUnlocked[ABILITY_FORTIFY] = true;
}

void RunModifiersApplyPerk(RunModifiers *mods, int perkID)
{
    mods->activePerk = perkID;

    switch (perkID) {
    case PERK_PENETRATING_ROUNDS:
        mods->sniperPierce = true;
        break;
    case PERK_GLASS_CANNON:
        mods->damageMultiplier *= 1.30f;
        mods->towerCostMultiplier *= 1.20f;
        break;
    case PERK_GREED:
        mods->goldPerKillMultiplier *= 2.0f;
        mods->greedSpeedMultiplier = 1.20f;
        break;
    case PERK_BUNKER_DOWN:
        mods->bonusStartingLives += 5;
        mods->bonusStartingGold -= 30;
        break;
    case PERK_RAPID_FIRE:
        mods->fireRateMultiplier *= 1.25f;
        mods->damageMultiplier *= 0.85f;
        break;
    case PERK_EAGLE_EYE:
        mods->rangeMultiplier *= 1.20f;
        mods->fireRateMultiplier *= 0.90f;
        break;
    case PERK_WAR_BONDS:
        mods->waveBonusMultiplier *= 1.25f;
        break;
    case PERK_PERMAFROST:
        mods->slowEffectMultiplier *= 1.40f;
        break;
    case PERK_TANK_BUSTER:
        mods->tankDamageMultiplier *= 1.50f;
        mods->fastDamageMultiplier *= 0.80f;
        break;
    case PERK_FIELD_ENGINEER:
        mods->upgradeCostMultiplier *= 0.80f;
        break;
    case PERK_EXPLOSIVE_TIPS:
        mods->aoeBonus += 0.5f;
        break;
    case PERK_SECOND_WIND:
        mods->secondWind = true;
        break;
    case PERK_PREFAB_TOWERS:
        mods->towerCostMultiplier *= 0.85f;
        mods->rangeMultiplier *= 0.90f;
        break;
    case PERK_OVERCHARGE:
        mods->overcharge = true;
        break;
    default:
        break;
    }
}

// --- Crystal Earning ---

int CrystalsCalculate(int wavesCompleted, int livesRemaining, int maxLives,
                      int difficulty, bool won)
{
    float livesBonus = 0.0f;
    if (maxLives > 0)
        livesBonus = ((float)livesRemaining / (float)maxLives) * 20.0f;
    float victoryBonus = won ? 10.0f : 0.0f;

    float difficultyMult = 1.0f;
    switch (difficulty) {
    case 0: difficultyMult = 0.5f; break;  // Easy
    case 1: difficultyMult = 1.0f; break;  // Normal
    case 2: difficultyMult = 2.0f; break;  // Hard
    case 3: difficultyMult = 4.0f; break;  // Nightmare
    default: break;
    }

    return (int)((wavesCompleted * 1.5f + livesBonus + victoryBonus) * difficultyMult);
}

// --- Perk Selection ---

void PerkSelectRandom(int offered[3], unsigned int seed)
{
    // Fisher-Yates shuffle on indices 0..PERK_COUNT-1, pick first 3
    int indices[PERK_COUNT];
    for (int i = 0; i < PERK_COUNT; i++) indices[i] = i;

    // Simple LCG for reproducible randomness from seed
    unsigned int rng = seed;
    for (int i = PERK_COUNT - 1; i > 0; i--) {
        rng = rng * 1103515245 + 12345;
        int j = (int)((rng >> 16) % (unsigned int)(i + 1));
        int tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    offered[0] = indices[0];
    offered[1] = indices[1];
    offered[2] = indices[2];
}

// --- Endless Wave Generation ---

void EndlessGenerateWave(WaveConfig *out, int endlessWaveNum)
{
    // 5 rotating archetypes
    int archetype = endlessWaveNum % 5;
    float scaleFactor = 1.0f + 0.10f * endlessWaveNum; // +10% per wave

    memset(out, 0, sizeof(WaveConfig));

    switch (archetype) {
    case 0: // Mixed
        out->groups[0] = (WaveGroup){ ENEMY_BASIC, (int)(12 * scaleFactor) };
        out->groups[1] = (WaveGroup){ ENEMY_FAST,  (int)(8 * scaleFactor) };
        out->groups[2] = (WaveGroup){ ENEMY_TANK,  (int)(4 * scaleFactor) };
        out->groupCount = 3;
        break;
    case 1: // Fast rush
        out->groups[0] = (WaveGroup){ ENEMY_FAST, (int)(25 * scaleFactor) };
        out->groupCount = 1;
        break;
    case 2: // Tank assault
        out->groups[0] = (WaveGroup){ ENEMY_TANK, (int)(10 * scaleFactor) };
        out->groups[1] = (WaveGroup){ ENEMY_BASIC, (int)(5 * scaleFactor) };
        out->groupCount = 2;
        break;
    case 3: // Swarm
        out->groups[0] = (WaveGroup){ ENEMY_BASIC, (int)(30 * scaleFactor) };
        out->groups[1] = (WaveGroup){ ENEMY_FAST,  (int)(15 * scaleFactor) };
        out->groupCount = 2;
        break;
    case 4: // Everything
        out->groups[0] = (WaveGroup){ ENEMY_FAST,  (int)(15 * scaleFactor) };
        out->groups[1] = (WaveGroup){ ENEMY_TANK,  (int)(8 * scaleFactor) };
        out->groups[2] = (WaveGroup){ ENEMY_BASIC, (int)(15 * scaleFactor) };
        out->groups[3] = (WaveGroup){ ENEMY_FAST,  (int)(10 * scaleFactor) };
        out->groupCount = 4;
        break;
    }

    // Spawn interval tightens (min 0.15s)
    float interval = 0.4f - 0.01f * endlessWaveNum;
    if (interval < 0.15f) interval = 0.15f;
    out->spawnInterval = interval;

    out->bonusGold = 200 + 20 * endlessWaveNum;
}

// --- Shop ---

bool ShopCanPurchase(const PlayerProfile *profile, ShopItemID item)
{
    if (item < 0 || item >= SHOP_ITEM_COUNT) return false;
    if (profile->shopPurchased[item]) return false;
    if (profile->crystals < SHOP_ITEMS[item].cost) return false;

    // Check prerequisite
    int prereq = SHOP_ITEMS[item].prereq;
    if (prereq >= 0 && !profile->shopPurchased[prereq]) return false;

    return true;
}

void ShopPurchase(PlayerProfile *profile, ShopItemID item)
{
    if (!ShopCanPurchase(profile, item)) return;
    profile->crystals -= SHOP_ITEMS[item].cost;
    profile->shopPurchased[item] = true;
}
