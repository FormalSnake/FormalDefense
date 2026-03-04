# Formal Defense — 3D Tower Defense Game

## Overview
A 3D tower defense game built with C and Raylib. Features geometric shapes, grid-based maps, data-driven tower/enemy configs, and RTS-style camera controls.

## Build Commands
```bash
make        # Build the project
make run    # Build and run
make clean  # Remove build artifacts
```

## Architecture

| File | Role |
|------|------|
| `main.c` | Entry point, game loop, camera controller, HUD, input dispatch |
| `game.h/c` | GameState, wave system, economy, game phase management |
| `entity.h/c` | Tower, Enemy, Projectile structs + config tables + update/draw |
| `map.h/c` | Grid (20×15), tile types, waypoints, placement validation, rendering |

## Key Conventions
- **Struct-based OOP:** Each system uses a struct (Map, Tower, Enemy, etc.) with associated functions prefixed by the type name
- **Config tables:** Static const arrays of config structs define tower/enemy types and levels
- **Active-flag entity pools:** Fixed-size arrays with `bool active` flags — no dynamic allocation
- **Data-driven design:** Game balance tuned via config tables, not hardcoded in logic

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
