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
| `main.c` | Entry point, game loop, scene management (Menu/MapSelect/Lobby/Game), camera controller, HUD, input dispatch |
| `game.h/c` | GameState, wave system, economy, game phase management, multiplayer scaling |
| `entity.h/c` | Tower, Enemy, Projectile structs + config tables + update/draw, EntityID system, player ownership |
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
- **Scene state machine:** `SCENE_MENU → SCENE_MAP_SELECT → SCENE_LOBBY → SCENE_GAME`
- **Data-driven maps:** `.fdmap` plain-text file format with waypoints + tile overrides; `MapTracePath` auto-derives path tiles from waypoints

## Game Design Reference

### Tower Types (3 upgrade levels each)
| Type | Role | Color |
|------|------|-------|
| Cannon | High damage, AoE at L3 | Gray shades |
| Machine Gun | Fast fire, low damage | Orange/gold/yellow |
| Sniper | Long range, huge damage | Blue shades |
| Slow | Area slow effect | Purple shades |

### Enemy Types
| Type | HP | Speed | Color |
|------|-----|-------|-------|
| Basic | 100 | Slow | Green sphere |
| Fast | 60 | Fast | Lime sphere |
| Tank | 300 | Very slow | Dark green sphere |

### Economy
- Starting gold: 250
- Starting lives: 20
- 10 escalating waves

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
