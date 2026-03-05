#include "progress.h"
#include "game.h"
#include "entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Shop Item Configs ---

const ShopItemConfig SHOP_ITEMS[SHOP_ITEM_COUNT] = {
    [SHOP_DAMAGE_1]          = { "Hardened Rounds",      "+10% tower damage",        80,  -1, SHOP_CAT_STAT_BOOST },
    [SHOP_DAMAGE_2]          = { "Armor-Piercing Rounds", "+15% tower damage (stacks)", 150, SHOP_DAMAGE_1, SHOP_CAT_STAT_BOOST },
    [SHOP_STARTING_GOLD]     = { "War Chest",            "+50 starting gold",       100, -1, SHOP_CAT_STAT_BOOST },
    [SHOP_STARTING_LIVES]    = { "Reinforced Walls",     "+3 starting lives",       120, -1, SHOP_CAT_STAT_BOOST },
    [SHOP_TOWER_COST]        = { "Bulk Discount",        "-10% tower build cost",   100, -1, SHOP_CAT_STAT_BOOST },
    [SHOP_GOLD_PER_KILL]     = { "Bounty Hunter",        "+15% gold per kill",      120, -1, SHOP_CAT_STAT_BOOST },
    [SHOP_TOWER_RANGE]       = { "Optics Package",       "+10% tower range",        100, -1, SHOP_CAT_STAT_BOOST },
    [SHOP_UNLOCK_SNIPER]     = { "Sniper Blueprint",     "Unlock Sniper tower",     120, -1, SHOP_CAT_TOWER_UNLOCK },
    [SHOP_UNLOCK_SLOW]       = { "Slow Field Blueprint", "Unlock Slow tower",       100, -1, SHOP_CAT_TOWER_UNLOCK },
    [SHOP_UNLOCK_LASER]      = { "Laser Blueprint",      "Unlock Laser tower",      150, -1, SHOP_CAT_TOWER_UNLOCK },
    [SHOP_UNLOCK_MORTAR]     = { "Mortar Blueprint",     "Unlock Mortar tower",     180, SHOP_UNLOCK_SNIPER, SHOP_CAT_TOWER_UNLOCK },
    [SHOP_UNLOCK_TESLA]      = { "Tesla Blueprint",      "Unlock Tesla tower",      200, SHOP_UNLOCK_SLOW, SHOP_CAT_TOWER_UNLOCK },
    [SHOP_UNLOCK_FLAME]      = { "Flame Blueprint",      "Unlock Flame tower",      150, -1, SHOP_CAT_TOWER_UNLOCK },
    [SHOP_ABILITY_AIRSTRIKE] = { "Airstrike Module",     "Unlock Airstrike (Q)",    100, -1, SHOP_CAT_ABILITY },
    [SHOP_ABILITY_GOLD_RUSH] = { "Gold Rush Module",     "Unlock Gold Rush (E)",     80, -1, SHOP_CAT_ABILITY },
    [SHOP_ABILITY_FORTIFY]   = { "Fortify Module",       "Unlock Fortify (R)",      100, -1, SHOP_CAT_ABILITY },
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

// --- Modifier Deltas ---

void RunModifiersApplyDeltas(RunModifiers *mods, const ModifierDeltas *d)
{
    mods->damageMultiplier      *= d->damageMultiplier;
    mods->rangeMultiplier       *= d->rangeMultiplier;
    mods->fireRateMultiplier    *= d->fireRateMultiplier;
    mods->towerCostMultiplier   *= d->towerCostMultiplier;
    mods->upgradeCostMultiplier *= d->upgradeCostMultiplier;
    mods->goldPerKillMultiplier *= d->goldPerKillMultiplier;
    mods->waveBonusMultiplier   *= d->waveBonusMultiplier;
    mods->slowEffectMultiplier  *= d->slowEffectMultiplier;
    mods->tankDamageMultiplier  *= d->tankDamageMultiplier;
    mods->fastDamageMultiplier  *= d->fastDamageMultiplier;
    mods->greedSpeedMultiplier  *= d->greedSpeedMultiplier;
    mods->bonusStartingGold     += d->bonusStartingGold;
    mods->bonusStartingLives    += d->bonusStartingLives;
    mods->aoeBonus              += d->aoeBonus;
    if (d->sniperPierce) mods->sniperPierce = true;
    if (d->secondWind)   mods->secondWind = true;
    if (d->overcharge)   mods->overcharge = true;
}

// --- Perk Effects Table ---

const ModifierDeltas PERK_EFFECTS[PERK_COUNT] = {
    [PERK_PENETRATING_ROUNDS] = { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, true,false,false },
    [PERK_GLASS_CANNON]       = { 1.30f,1,1,1.20f,1,1,1,1,1,1,1, 0,0, 0, false,false,false },
    [PERK_GREED]              = { 1,1,1,1,1,2.0f,1,1,1,1,1.20f, 0,0, 0, false,false,false },
    [PERK_BUNKER_DOWN]        = { 1,1,1,1,1,1,1,1,1,1,1, -30,5, 0, false,false,false },
    [PERK_RAPID_FIRE]         = { 0.85f,1,1.25f,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false },
    [PERK_EAGLE_EYE]          = { 1,1.20f,0.90f,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false },
    [PERK_WAR_BONDS]          = { 1,1,1,1,1,1,1.25f,1,1,1,1, 0,0, 0, false,false,false },
    [PERK_PERMAFROST]         = { 1,1,1,1,1,1,1,1.40f,1,1,1, 0,0, 0, false,false,false },
    [PERK_TANK_BUSTER]        = { 1,1,1,1,1,1,1,1,1.50f,0.80f,1, 0,0, 0, false,false,false },
    [PERK_FIELD_ENGINEER]     = { 1,1,1,1,0.80f,1,1,1,1,1,1, 0,0, 0, false,false,false },
    [PERK_EXPLOSIVE_TIPS]     = { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0.5f, false,false,false },
    [PERK_SECOND_WIND]        = { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,true,false },
    [PERK_PREFAB_TOWERS]      = { 1,0.90f,1,0.85f,1,1,1,1,1,1,1, 0,0, 0, false,false,false },
    [PERK_OVERCHARGE]         = { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,true },
};

// --- Shop Effects Table ---

// Tower type indices matching TowerType enum
#define TT_SNIPER 2
#define TT_SLOW   3
#define TT_LASER  4
#define TT_MORTAR 5
#define TT_TESLA  6
#define TT_FLAME  7

const ShopItemEffect SHOP_EFFECTS[SHOP_ITEM_COUNT] = {
    [SHOP_DAMAGE_1]          = { { 1.10f,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, -1 },
    [SHOP_DAMAGE_2]          = { { 1.15f,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, -1 },
    [SHOP_STARTING_GOLD]     = { { 1,1,1,1,1,1,1,1,1,1,1, 50,0, 0, false,false,false }, -1, -1 },
    [SHOP_STARTING_LIVES]    = { { 1,1,1,1,1,1,1,1,1,1,1, 0,3, 0, false,false,false }, -1, -1 },
    [SHOP_TOWER_COST]        = { { 1,1,1,0.90f,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, -1 },
    [SHOP_GOLD_PER_KILL]     = { { 1,1,1,1,1,1.15f,1,1,1,1,1, 0,0, 0, false,false,false }, -1, -1 },
    [SHOP_TOWER_RANGE]       = { { 1,1.10f,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, -1 },
    [SHOP_UNLOCK_SNIPER]     = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, TT_SNIPER, -1 },
    [SHOP_UNLOCK_SLOW]       = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, TT_SLOW, -1 },
    [SHOP_UNLOCK_LASER]      = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, TT_LASER, -1 },
    [SHOP_UNLOCK_MORTAR]     = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, TT_MORTAR, -1 },
    [SHOP_UNLOCK_TESLA]      = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, TT_TESLA, -1 },
    [SHOP_UNLOCK_FLAME]      = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, TT_FLAME, -1 },
    [SHOP_ABILITY_AIRSTRIKE] = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, ABILITY_AIRSTRIKE },
    [SHOP_ABILITY_GOLD_RUSH] = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, ABILITY_GOLD_RUSH },
    [SHOP_ABILITY_FORTIFY]   = { { 1,1,1,1,1,1,1,1,1,1,1, 0,0, 0, false,false,false }, -1, ABILITY_FORTIFY },
};

#undef TT_SNIPER
#undef TT_SLOW
#undef TT_LASER
#undef TT_MORTAR
#undef TT_TESLA
#undef TT_FLAME

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

    // Apply shop purchases via data table
    for (int i = 0; i < SHOP_ITEM_COUNT; i++) {
        if (!profile->shopPurchased[i]) continue;
        const ShopItemEffect *eff = &SHOP_EFFECTS[i];
        RunModifiersApplyDeltas(mods, &eff->modifiers);
        if (eff->towerUnlockType >= 0)
            mods->towerUnlocked[eff->towerUnlockType] = true;
        if (eff->abilityUnlockID >= 0)
            mods->abilityUnlocked[eff->abilityUnlockID] = true;
    }
}

void RunModifiersApplyPerk(RunModifiers *mods, int perkID)
{
    if (perkID < 0 || perkID >= PERK_COUNT) return;
    mods->activePerk = perkID;
    RunModifiersApplyDeltas(mods, &PERK_EFFECTS[perkID]);
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
