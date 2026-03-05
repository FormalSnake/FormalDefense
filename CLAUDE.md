# Formal Defense — 3D Tower Defense Game

## Overview
A 3D tower defense game built with C and Raylib. Features geometric shapes, grid-based maps, data-driven tower/enemy configs, RTS-style camera controls, and LAN multiplayer for up to 4 players.

## Build Commands
```bash
make          # Build the game
make run      # Build and run
make editor   # Build the standalone map editor
make clean    # Remove build artifacts
```

## Architecture

| File | Role |
|------|------|
| `main.c` | Entry point, game loop, scene management (Menu/MapSelect/Difficulty/Shop/PerkSelect/Lobby/Game), camera controller, HUD, input dispatch, ability system |
| `game.h/c` | GameState, wave system (20 waves + endless), economy, game phase management (incl. PHASE_VICTORY), multiplayer scaling |
| `entity.h/c` | Tower (8 types), Enemy, Projectile structs + config tables + update/draw, EntityID system, player ownership, RunModifiers integration |
| `progress.h/c` | Meta-progression: PlayerProfile save/load, shop items (16), perks (14), abilities (3), RunModifiers, crystal economy, endless wave generation |
| `map.h/c` | Grid (20×15), tile types, waypoints, placement validation, rendering, `.fdmap` load/save, MapRegistry |
| `net.h/c` | ENet networking: host/client, packet protocol, LAN discovery, snapshot broadcast, map data sync |
| `lobby.h/c` | Multiplayer lobby: username entry, host/join flow, LAN browser, map name display |
| `chat.h/c` | In-game chat: circular buffer, input handling, fade-out rendering |
| `editor.c` | Standalone 2D map editor binary (separate from game, shares `map.h/c`) |
| `maps/` | Directory of `.fdmap` map files (scanned at runtime by `MapRegistry`) |
| `vendor/enet/` | Embedded ENet library (compiled from source) |

## Key Conventions
- **Struct-based OOP:** Each system uses a struct (Map, Tower, Enemy, etc.) with associated functions prefixed by the type name
- **Config tables:** Static const arrays of config structs define tower/enemy types and levels
- **Active-flag entity pools:** Fixed-size arrays with `bool active` flags — no dynamic allocation
- **Data-driven design:** Game balance tuned via config tables, not hardcoded in logic
- **EntityID system:** 16-bit IDs with type bits (top 2 bits = entity type, bottom 14 = sequence)
- **Host-authoritative networking:** Host runs simulation, clients send action requests, host validates and confirms
- **Scene state machine:** `SCENE_MENU → SCENE_MAP_SELECT → SCENE_DIFFICULTY_SELECT → SCENE_SHOP → SCENE_PERK_SELECT → SCENE_GAME` (multiplayer: `→ SCENE_LOBBY → SCENE_GAME`)
- **Data-driven maps:** `.fdmap` plain-text file format with waypoints + tile overrides; `MapTracePath` auto-derives path tiles from waypoints

## Game Design Reference

### Tower Types (8 total, 3 upgrade levels each)

**Starter towers** (always available):

| Type | Role | Color | L1 Stats |
|------|------|-------|----------|
| Cannon | Balanced damage, AoE at L3 | Gray shades | 40 dmg, 3.5 range, 1.0/s, $50 |
| Machine Gun | Fast fire, low damage | Orange/gold/yellow | 10 dmg, 3.0 range, 4.0/s, $40 |

**Unlockable towers** (purchased in crystal shop):

| Type | Role | Mechanic | Color | L1 Stats |
|------|------|----------|-------|----------|
| Sniper | Long range, huge damage | Projectile (can pierce with perk) | Blue shades | 80 dmg, 7.0 range, 0.5/s, $70 |
| Slow | Area slow effect | Projectile + AoE slow | Purple shades | 5 dmg, 3.5 range, 1.5/s, $60 |
| Laser | Continuous beam, single-target DPS | No projectile — beam to target, DPS per frame | Cyan | 25 DPS, 3.0 range, $65 |
| Mortar | Long range, large AoE, slow fire | Arcing parabolic projectile to fixed position | Brown/Tan | 60 dmg, 8.0 range, 0.4/s, 2.0 AoE, $80 |
| Tesla | Chain lightning, multi-target | Instant hit — chains to 3-4 nearest enemies (2.0 chain range) | Electric blue | 30 dmg, 4.0 range, 0.8/s, 3 chains, $75 |
| Flame | Short range cone, burn DoT | 60° cone attack, applies burn damage over time | Red/Orange | 15 dmg + 8 DPS burn (2s), 2.5 range, $55 |

**Unlock tree:** Cannon + MG free → Sniper/Slow/Laser/Flame (tier 1) → Mortar (needs Sniper) / Tesla (needs Slow)

### Enemy Types
| Type | HP | Speed | Color |
|------|-----|-------|-------|
| Basic | 100 | Slow | Green sphere |
| Fast | 60 | Fast | Lime sphere |
| Tank | 300 | Very slow | Dark green sphere |

### Economy
- Starting gold: varies by difficulty (150-300) + shop bonus
- Starting lives: varies by difficulty (10-30) + shop bonus
- 20 escalating waves (3 acts) + optional endless mode
- Crystal meta-currency earned per run for persistent progression

## Roguelike Meta-Progression

### Crystal Economy
Crystals are earned at the end of each run based on performance:
```
crystals = (wavesCompleted * 1.5 + livesBonus + victoryBonus) * difficultyMult
```
- `livesBonus = (livesRemaining / maxLives) * 20`
- `victoryBonus = won ? 10 : 0`
- Difficulty multipliers: Easy 0.5x, Normal 1.0x, Hard 2.0x, Nightmare 4.0x

Saved to `profile.fdsave` (binary format with magic `0x46445356`).

### Crystal Shop (16 items, ~2530 crystals to fully unlock)

**Stat Boosts:**
| Item | Effect | Cost | Prereq |
|------|--------|------|--------|
| Hardened Rounds | +10% tower damage | 80 | — |
| Armor-Piercing Rounds | +15% tower damage (stacks) | 150 | Hardened Rounds |
| War Chest | +50 starting gold | 100 | — |
| Reinforced Walls | +3 starting lives | 120 | — |
| Bulk Discount | -10% tower build cost | 100 | — |
| Bounty Hunter | +15% gold per kill | 120 | — |
| Optics Package | +10% tower range | 100 | — |

**Tower Unlocks:** Sniper ($120), Slow ($100), Laser ($150), Mortar ($180, needs Sniper), Tesla ($200, needs Slow), Flame ($150)

**Ability Unlocks:** Airstrike ($100), Gold Rush ($80), Fortify ($100)

### Perks (pick 1 from 3 random at run start)
| Perk | Effect | Trade-off? |
|------|--------|------------|
| Penetrating Rounds | Sniper pierces 2 targets | No |
| Glass Cannon | +30% damage, +20% tower cost | Yes |
| Greed | 2x gold/kill, enemies 20% faster | Yes |
| Bunker Down | +5 lives, -30 starting gold | Yes |
| Rapid Fire | +25% fire rate, -15% damage | Yes |
| Eagle Eye | +20% range, -10% fire rate | Yes |
| War Bonds | +25% wave bonus gold | No |
| Permafrost | Slow towers 40% more effective | No |
| Tank Buster | +50% to Tanks, -20% to Fast | Yes |
| Field Engineer | Upgrades cost 20% less | No |
| Explosive Tips | All projectiles +0.5 AoE | No |
| Second Wind | Restore to 5 lives on first death | No |
| Prefab Towers | Towers -15% cost, -10% range | Yes |
| Overcharge | First shot after cooldown +40% damage | No |

### Active Abilities
| Ability | Key | Effect | Cooldown |
|---------|-----|--------|----------|
| Airstrike | Q | 200 damage to all enemies in 3.0 radius at cursor | 45s |
| Gold Rush | E | 2x gold from kills for 10s | 60s |
| Fortify | R | +5 lives instantly | 90s |

### Endless Mode
After wave 20 victory, "Continue (Endless)" button appears. Procedurally generated waves using 5 rotating archetypes (mixed, fast rush, tank assault, swarm, everything). Enemy counts scale +10% per wave, spawn interval tightens (min 0.15s). Bonus gold: 200 + 20 per endless wave.

### RunModifiers System (`progress.h`)
`RunModifiers` struct aggregates all multipliers from shop + perk into a single struct passed to game systems:
- `damageMultiplier`, `rangeMultiplier`, `fireRateMultiplier`, `towerCostMultiplier`, `upgradeCostMultiplier`
- `goldPerKillMultiplier`, `waveBonusMultiplier`, `slowEffectMultiplier`
- `tankDamageMultiplier`, `fastDamageMultiplier`, `aoeBonus`
- `bonusStartingGold`, `bonusStartingLives`
- Flags: `sniperPierce`, `secondWind`, `overcharge`, `goldRushActive`
- `towerUnlocked[8]`, `abilityUnlocked[3]`

## Multiplayer

### Networking
- ENet on port 7777, 3 channels: reliable (0), snapshot (1), chat (2)
- 20 Hz state snapshots from host to clients
- LAN discovery via UDP broadcast on port 7778

### Lobby
- 4-phase state machine: `LOBBY_CHOOSE → LOBBY_HOST_WAIT / LOBBY_JOIN_BROWSE → LOBBY_JOIN_WAIT`
- Username entry, direct IP connect, and LAN game browser

### Economy Scaling
- Per-player gold tracked in `playerGold[4]`
- HP and count multipliers based on player count (`hpMultiplier`, `countMultiplier`)
- Gift gold mechanic: send 25g to another player

### Map Sync
- Host selects map in `SCENE_MAP_SELECT` before lobby opens
- `MSG_MAP_DATA` sends full `.fdmap` content as reliable packet on game start
- Clients try local `maps/` first by name; if missing, auto-download and persist

### Player Colors
- `PLAYER_COLORS[4]` — used for tower tinting, chat names, lobby UI, and HUD indicators

## Map System

### `.fdmap` File Format
Plain-text, human-readable. Waypoints define the path; tiles section stores only obstacles/overrides. Spawn/base derived from first/last waypoint.
```
name Desert Crossing
size 20 15

waypoints
0 7
4 7
...
end

tiles
2 4 obstacle
end
```

### Map API
- `MapLoad(map, filePath)` / `MapSave(map, filePath)` — file I/O
- `MapLoadFromBuffer(map, data, len)` / `MapSerialize(map, buf, bufSize)` — buffer variants (for networking)
- `MapRegistryScan(reg, directory)` — scans a directory for `.fdmap` files (max 16)
- `MapInit(map)` — hardcoded fallback map (used when no file found)

### Map Editor (`make editor`)
Standalone 2D grid editor. Tools: Empty, Obstacle, Waypoint Add, Waypoint Delete. Save/Load `.fdmap` files to `maps/` directory.
