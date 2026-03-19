#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include "raymath.h"
#include <string.h>
#include <stdio.h>

#define BOTTOM_BAR_HEIGHT 60
#define BTN_WIDTH 120
#define BTN_HEIGHT 45
#define BTN_MARGIN 10
#define INFO_PANEL_W 200
#define INFO_PANEL_H 160

static void GameSceneDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    float dt = GetFrameTime();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    Vector2 mouse = GetMousePosition();

    // --- UI hit test (bottom bar takes priority) ---
    bool mouseInUI = (mouse.y >= screenH - BOTTOM_BAR_HEIGHT);

    if (app->selectedTowerIdx >= 0) {
        Rectangle infoRect = { (float)(screenW - INFO_PANEL_W - 10), 40.0f,
                               (float)INFO_PANEL_W, (float)INFO_PANEL_H };
        if (CheckCollisionPointRec(mouse, infoRect))
            mouseInUI = true;
    }

    // --- Chat input ---
    bool chatActive = ChatHandleInput(&app->chatState, &app->netCtx);
    ChatUpdate(&app->chatState, dt);

    // --- ESC: deselect, pause, or resume ---
    if (!chatActive && !app->settingsState.open && IsKeyPressed(KEY_ESCAPE)) {
        if (app->netCtx.mode != NET_MODE_NONE) {
            if (app->localPaused) {
                app->localPaused = false;
            } else if (app->selectedTowerType >= 0 || app->selectedTowerIdx >= 0) {
                app->selectedTowerType = -1;
                app->selectedTowerIdx = -1;
            } else {
                app->localPaused = true;
            }
        } else {
            if (app->gs.phase == PHASE_PAUSED) {
                app->gs.phase = app->phaseBeforePause;
            } else if (app->selectedTowerType >= 0 || app->selectedTowerIdx >= 0) {
                app->selectedTowerType = -1;
                app->selectedTowerIdx = -1;
            } else if (app->gs.phase != PHASE_OVER) {
                app->phaseBeforePause = app->gs.phase;
                app->gs.phase = PHASE_PAUSED;
            }
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        app->selectedTowerType = -1;
        app->selectedTowerIdx = -1;
    }

    // --- Mouse ground position ---
    Vector3 mouseGround = {0};
    bool mouseOnGround = GetMouseGroundPos(app->camera, &app->map, &mouseGround);
    GridPos mouseGrid = {-1, -1};
    bool canPlace = false;

    if (mouseOnGround) {
        mouseGrid = MapWorldToGrid(mouseGround);
        if (app->selectedTowerType >= 0)
            canPlace = MapCanPlaceTower(&app->map, mouseGrid);
    }

    // Build list of unlocked tower types
    int unlockedTowers[TOWER_TYPE_COUNT];
    int unlockedCount = 0;
    for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
        if (app->runMods.towerUnlocked[i])
            unlockedTowers[unlockedCount++] = i;
    }

    int localPI = app->netCtx.mode != NET_MODE_NONE ? app->netCtx.localPlayerIndex : 0;

    if (app->gs.phase != PHASE_PAUSED) {

    // --- Camera ---
    if (!chatActive && (!mouseInUI || IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D)))
        CameraControllerUpdate(&g_camCtrl, &app->camera, dt);

    // --- Ability cooldown ticking ---
    for (int a = 0; a < ABILITY_COUNT; a++) {
        if (app->runMods.abilityTimer[a] > 0.0f)
            app->runMods.abilityTimer[a] -= dt;
    }

    // --- Ability input ---
    if (!chatActive && app->gs.phase == PHASE_PLAYING) {
        if (app->runMods.abilityUnlocked[ABILITY_AIRSTRIKE] && app->runMods.abilityTimer[ABILITY_AIRSTRIKE] <= 0.0f
            && IsKeyPressed(KEY_Q) && mouseOnGround) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (!app->enemies[j].active) continue;
                if (Vector3Distance(mouseGround, app->enemies[j].worldPos) <= 3.0f) {
                    app->enemies[j].hp -= 200.0f;
                    if (app->enemies[j].hp <= 0.0f) {
                        app->enemies[j].active = false;
                        app->gs.playerGold[localPI] += app->gs.goldPerKill;
                        app->gs.gold = app->gs.playerGold[0];
                    }
                }
            }
            app->runMods.abilityTimer[ABILITY_AIRSTRIKE] = ABILITY_AIRSTRIKE_COOLDOWN;
        }
        if (app->runMods.abilityUnlocked[ABILITY_GOLD_RUSH] && app->runMods.abilityTimer[ABILITY_GOLD_RUSH] <= 0.0f
            && IsKeyPressed(KEY_E) && !app->runMods.goldRushActive) {
            app->runMods.goldRushActive = true;
            app->runMods.abilityCooldown[ABILITY_GOLD_RUSH] = ABILITY_GOLD_RUSH_DURATION;
        }
        if (app->runMods.abilityUnlocked[ABILITY_FORTIFY] && app->runMods.abilityTimer[ABILITY_FORTIFY] <= 0.0f
            && IsKeyPressed(KEY_R)) {
            app->gs.lives += 5;
            app->runMods.abilityTimer[ABILITY_FORTIFY] = ABILITY_FORTIFY_COOLDOWN;
        }
    }

    // Gold Rush duration tracking
    if (app->runMods.goldRushActive) {
        app->runMods.abilityCooldown[ABILITY_GOLD_RUSH] -= dt;
        if (app->runMods.abilityCooldown[ABILITY_GOLD_RUSH] <= 0.0f) {
            app->runMods.goldRushActive = false;
            app->runMods.abilityTimer[ABILITY_GOLD_RUSH] = ABILITY_GOLD_RUSH_COOLDOWN;
        }
    }

    // --- Bottom bar button clicks ---
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= screenH - BOTTOM_BAR_HEIGHT) {
        for (int i = 0; i < unlockedCount; i++) {
            int ttype = unlockedTowers[i];
            int bx = BTN_MARGIN + i * (BTN_WIDTH + BTN_MARGIN);
            int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
            Rectangle btnRect = { (float)bx, (float)by, (float)BTN_WIDTH, (float)BTN_HEIGHT };
            if (CheckCollisionPointRec(mouse, btnRect)) {
                int cost = (int)(TOWER_CONFIGS[ttype][0].cost * app->runMods.towerCostMultiplier);
                if (app->gs.playerGold[localPI] >= cost) {
                    app->selectedTowerType = ttype;
                    app->selectedTowerIdx = -1;
                }
            }
        }
    }

    // --- Tower selection keys (1-8) ---
    {
        int keys[] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT };
        for (int i = 0; i < unlockedCount && i < 8; i++) {
            if (IsKeyPressed(keys[i])) {
                app->selectedTowerType = unlockedTowers[i];
                app->selectedTowerIdx = -1;
            }
        }
    }

    // --- Left click in 3D area ---
    if (!mouseInUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && app->gs.phase != PHASE_OVER) {
        if (app->selectedTowerType >= 0 && canPlace) {
            int cost = (int)(TOWER_CONFIGS[app->selectedTowerType][0].cost * app->runMods.towerCostMultiplier);
            if (app->gs.playerGold[localPI] >= cost) {
                if (app->netCtx.mode == NET_MODE_CLIENT) {
                    NetSendPlaceTower(&app->netCtx, (TowerType)app->selectedTowerType, mouseGrid);
                } else {
                    app->gs.playerGold[localPI] -= cost;
                    app->gs.gold = app->gs.playerGold[0];
                    TowerPlace(app->towers, MAX_TOWERS, (TowerType)app->selectedTowerType, mouseGrid,
                              (uint8_t)localPI, &app->gs, &app->map);
                    app->map.tiles[mouseGrid.z][mouseGrid.x] = TILE_TOWER;
                    MapBuildMesh(&app->gameMapMesh, &app->map, app->ps1Shader);
                }
            }
        } else if (app->selectedTowerType < 0 && mouseOnGround) {
            app->selectedTowerIdx = -1;
            for (int i = 0; i < MAX_TOWERS; i++) {
                if (!app->towers[i].active) continue;
                if (app->towers[i].gridPos.x == mouseGrid.x && app->towers[i].gridPos.z == mouseGrid.z) {
                    app->selectedTowerIdx = i;
                    break;
                }
            }
        }
    }

    } // end PHASE_PAUSED guard

    // --- Network polling ---
    if (app->netCtx.mode != NET_MODE_NONE) {
        NetPoll(&app->netCtx, &app->gs, app->enemies, app->towers, app->projectiles, &app->map);

        if (app->netCtx.mode == NET_MODE_CLIENT && !app->netCtx.serverPeer) {
            NetContextDestroy(&app->netCtx);
            NetShutdown();
            NetContextInit(&app->netCtx);
            app->localPaused = false;
            SceneManagerTransition(app->sceneManager, SCENE_MENU);
            return;
        }
    }

    // --- Update game systems ---
    if (app->netCtx.mode != NET_MODE_CLIENT &&
        app->gs.phase != PHASE_OVER && app->gs.phase != PHASE_PAUSED) {
        GameUpdateWave(&app->gs, app->enemies, MAX_ENEMIES, &app->map, &app->runMods, dt);
        EnemiesUpdate(app->enemies, MAX_ENEMIES, &app->map, &app->gs, dt);
        TowersUpdate(app->towers, MAX_TOWERS, app->enemies, MAX_ENEMIES, app->projectiles, MAX_PROJECTILES,
                     &app->gs, &app->map, &app->runMods, dt);
        ProjectilesUpdate(app->projectiles, MAX_PROJECTILES, app->enemies, MAX_ENEMIES, &app->gs, &app->runMods, dt);
    }

    // --- Broadcast snapshots ---
    if (app->netCtx.mode == NET_MODE_HOST) {
        app->netCtx.snapshotTimer += dt;
        if (app->netCtx.snapshotTimer >= NET_SNAPSHOT_RATE) {
            app->netCtx.snapshotTimer -= NET_SNAPSHOT_RATE;
            NetBroadcastSnapshot(&app->netCtx, &app->gs, app->enemies, app->towers, app->projectiles);
        }
    }

    // Deselect tower if it became inactive
    if (app->selectedTowerIdx >= 0 && !app->towers[app->selectedTowerIdx].active)
        app->selectedTowerIdx = -1;

    // Rebuild map mesh if towers changed
    {
        int towerCount = 0;
        for (int i = 0; i < MAX_TOWERS; i++)
            if (app->towers[i].active) towerCount++;
        if (towerCount != app->lastTowerCount) {
            app->lastTowerCount = towerCount;
            MapBuildMesh(&app->gameMapMesh, &app->map, app->ps1Shader);
        }
    }

    // =========================
    // DRAW — 3D to low-res render target
    // =========================
    BeginTextureMode(app->renderTarget);
    ClearBackground((Color){ 30, 30, 35, 255 });

    BeginMode3D(app->camera);
        DrawSkybox(app->camera);
        DrawWater(app->waterShader, GetShaderLocation(app->waterShader, "time"), app->totalTime);
        MapDrawMesh(&app->gameMapMesh);

        // Draw trees on grass tiles
        BeginShaderMode(app->ps1Shader);
            for (int i = 0; i < app->gameTreeCount; i++) {
                float s = app->gameTrees[i].scale * 0.08f;
                DrawModelEx(app->treeModel, app->gameTrees[i].position,
                            (Vector3){0,1,0}, app->gameTrees[i].rotation,
                            (Vector3){s,s,s}, WHITE);
            }
        EndShaderMode();

        BeginShaderMode(app->ps1Shader);
            // Batched blob shadows
            {
                BlobShadowEntry shadowEntries[MAX_BLOB_SHADOWS];
                int shadowCount = 0;
                for (int i = 0; i < MAX_TOWERS; i++) {
                    if (!app->towers[i].active) continue;
                    Vector3 tp = app->towers[i].worldPos;
                    shadowEntries[shadowCount++] = (BlobShadowEntry){
                        tp.x, tp.z, 0.45f,
                        MapGetElevationY(&app->map, app->towers[i].gridPos.x, app->towers[i].gridPos.z)
                    };
                }
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!app->enemies[i].active) continue;
                    Vector3 ep = app->enemies[i].worldPos;
                    GridPos eg = MapWorldToGrid(ep);
                    shadowEntries[shadowCount++] = (BlobShadowEntry){
                        ep.x, ep.z, app->enemies[i].radius * 1.2f,
                        MapGetElevationY(&app->map, eg.x, eg.z)
                    };
                }
                DrawBlobShadowsBatched(shadowEntries, shadowCount);
            }

            // Grid hover highlight
            if (mouseOnGround && app->selectedTowerType >= 0 &&
                mouseGrid.x >= 0 && mouseGrid.x < MAP_WIDTH &&
                mouseGrid.z >= 0 && mouseGrid.z < MAP_HEIGHT) {
                Vector3 ghostPos = MapGridToWorldElevated(&app->map, mouseGrid);
                ghostPos.y += 0.35f;
                Color ghostCol = canPlace ? (Color){ 0, 255, 0, 100 } : (Color){ 255, 0, 0, 100 };
                DrawCubeV(ghostPos, (Vector3){ 0.7f, 0.7f, 0.7f }, ghostCol);

                if (canPlace) {
                    float range = TOWER_CONFIGS[app->selectedTowerType][0].range;
                    Vector3 rangeCenter = MapGridToWorldElevated(&app->map, mouseGrid);
                    DrawRangeCircle(rangeCenter, range, (Color){ 255, 255, 255, 80 });
                }
            }

            TowersDraw(app->towers, MAX_TOWERS, app->gs.playerCount, app->enemies, MAX_ENEMIES);

            // Range indicator for selected tower
            if (app->selectedTowerIdx >= 0 && app->towers[app->selectedTowerIdx].active) {
                const Tower *st = &app->towers[app->selectedTowerIdx];
                float tElevY = MapGetElevationY(&app->map, st->gridPos.x, st->gridPos.z);
                float range = TOWER_CONFIGS[st->type][st->level].range;
                Vector3 rc = st->worldPos;
                rc.y = tElevY;
                DrawRangeCircle(rc, range, (Color){ 255, 255, 100, 150 });
            }

            EnemiesDraw(app->enemies, MAX_ENEMIES, app->sphereModel);
            ProjectilesDraw(app->projectiles, MAX_PROJECTILES, app->sphereModel);
        EndShaderMode();
    EndMode3D();
    EndTextureMode();

    // =========================
    // Full-res output
    // =========================
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(app->renderTarget.texture,
        (Rectangle){ 0, 0, (float)app->rtW, -(float)app->rtH },
        (Rectangle){ 0, 0, (float)screenW, (float)screenH },
        (Vector2){ 0, 0 }, 0.0f, WHITE);

    EnemiesDrawHUD(app->enemies, MAX_ENEMIES, app->camera);

    // --- Top bar ---
    DrawRectangle(0, 0, screenW, 32, (Color){ 0, 0, 0, 180 });
    DrawText(TextFormat("Gold: %d", app->gs.playerGold[localPI]), 10, 7, 20, GOLD);
    DrawText(TextFormat("Lives: %d", app->gs.lives), 170, 7, 20,
             app->gs.lives > 5 ? RED : MAROON);
    if (app->gs.endlessActive)
        DrawText(TextFormat("Wave: %d (Endless +%d)", app->gs.currentWave + 1, app->gs.endlessWave), 330, 7, 20, (Color){180,100,255,255});
    else
        DrawText(TextFormat("Wave: %d/%d", app->gs.currentWave + 1, MAX_WAVES), 330, 7, 20, WHITE);
    {
        const DifficultyConfig *hdc = &DIFFICULTY_CONFIGS[app->gs.difficulty];
        DrawText(hdc->name, 510, 7, 20, hdc->color);
    }
    if (app->runMods.activePerk >= 0 && app->runMods.activePerk < PERK_COUNT) {
        DrawText(PERK_CONFIGS[app->runMods.activePerk].name, 640, 7, 16, (Color){200,180,100,255});
    }
    DrawFPS(screenW - 90, 7);

    // --- Wave countdown ---
    if (app->gs.phase == PHASE_WAVE_COUNTDOWN) {
        UIDrawCenteredText(TextFormat("Next wave in %.1f", app->gs.waveCountdown),
                          screenW / 2, 80, 28, YELLOW);
    }

    // --- Ability HUD ---
    {
        const char *abilityNames[] = { "Airstrike [Q]", "Gold Rush [E]", "Fortify [R]" };
        int abX = screenW - 160;
        int abY = 40;
        for (int a = 0; a < ABILITY_COUNT; a++) {
            if (!app->runMods.abilityUnlocked[a]) continue;
            bool onCooldown = app->runMods.abilityTimer[a] > 0.0f;
            Color abBg = onCooldown ? (Color){40,30,30,200} : (Color){30,40,50,200};
            DrawRectangle(abX, abY, 150, 28, abBg);
            DrawRectangleLines(abX, abY, 150, 28, (Color){80,80,100,200});
            Color abTxt = onCooldown ? (Color){100,80,80,255} : WHITE;
            DrawText(abilityNames[a], abX + 5, abY + 6, 14, abTxt);
            if (onCooldown) {
                DrawText(TextFormat("%.0fs", app->runMods.abilityTimer[a]),
                         abX + 120, abY + 6, 14, (Color){180,80,80,255});
            } else if (a == ABILITY_GOLD_RUSH && app->runMods.goldRushActive) {
                DrawText("ACTIVE", abX + 100, abY + 6, 12, GOLD);
            }
            abY += 32;
        }
    }

    // --- Bottom bar ---
    DrawRectangle(0, screenH - BOTTOM_BAR_HEIGHT, screenW, BOTTOM_BAR_HEIGHT, (Color){ 20, 20, 25, 220 });
    for (int ui = 0; ui < unlockedCount; ui++) {
        int i = unlockedTowers[ui];
        int bx = BTN_MARGIN + ui * (BTN_WIDTH + BTN_MARGIN);
        int by = screenH - BOTTOM_BAR_HEIGHT + (BOTTOM_BAR_HEIGHT - BTN_HEIGHT) / 2;
        int cost = (int)(TOWER_CONFIGS[i][0].cost * app->runMods.towerCostMultiplier);
        bool affordable = app->gs.playerGold[localPI] >= cost;

        Color btnBg = (app->selectedTowerType == i) ? (Color){ 80, 120, 80, 255 } :
                      affordable ? (Color){ 50, 50, 60, 255 } : (Color){ 40, 40, 40, 255 };
        Color btnFg = affordable ? WHITE : (Color){ 100, 100, 100, 255 };
        Color costCol = affordable ? GOLD : (Color){ 120, 80, 80, 255 };

        DrawRectangle(bx, by, BTN_WIDTH, BTN_HEIGHT, btnBg);
        DrawRectangleLines(bx, by, BTN_WIDTH, BTN_HEIGHT, (Color){ 100, 100, 100, 200 });
        DrawRectangle(bx + 4, by + 4, 12, 12, TOWER_CONFIGS[i][0].color);
        DrawText(TOWER_NAMES[i], bx + 20, by + 4, 16, btnFg);
        DrawText(TextFormat("$%d", cost), bx + 20, by + 24, 14, costCol);
        DrawText(TextFormat("[%d]", ui + 1), bx + BTN_WIDTH - 28, by + 28, 12, (Color){120,120,120,255});
    }

    // --- Tower info panel ---
    if (app->selectedTowerIdx >= 0 && app->towers[app->selectedTowerIdx].active) {
        const Tower *st = &app->towers[app->selectedTowerIdx];
        const TowerConfig *cfg = &TOWER_CONFIGS[st->type][st->level];
        int px = screenW - INFO_PANEL_W - 10;
        int py = 40;

        DrawRectangle(px, py, INFO_PANEL_W, INFO_PANEL_H, (Color){ 20, 20, 30, 230 });
        DrawRectangleLines(px, py, INFO_PANEL_W, INFO_PANEL_H, (Color){ 100, 100, 100, 200 });

        DrawText(TextFormat("%s (Lv %d)", TOWER_NAMES[st->type], st->level + 1),
                 px + 8, py + 6, 18, cfg->color);
        DrawText(TextFormat("Damage: %.0f", cfg->damage),   px + 8, py + 30, 14, WHITE);
        DrawText(TextFormat("Range:  %.1f", cfg->range),    px + 8, py + 48, 14, WHITE);
        DrawText(TextFormat("Rate:   %.1f/s", cfg->fireRate), px + 8, py + 66, 14, WHITE);

        float tElevY = MapGetElevationY(&app->map, st->gridPos.x, st->gridPos.z);
        if (tElevY > 0.0f)
            DrawText(TextFormat("Elev:   +%.1f rng", tElevY), px + 8, py + 84, 14, (Color){100,200,255,255});

        int extraY = (tElevY > 0.0f) ? 102 : 84;
        if (cfg->aoeRadius > 0.0f)
            DrawText(TextFormat("AoE:    %.1f", cfg->aoeRadius), px + 8, py + extraY, 14, ORANGE);
        if (cfg->slowFactor < 1.0f)
            DrawText(TextFormat("Slow:   %.0f%%", (1.0f - cfg->slowFactor) * 100.0f), px + 8, py + extraY, 14, PURPLE);

        // Upgrade button
        if (st->level < TOWER_MAX_LEVEL - 1) {
            int upgCost = (int)(TOWER_CONFIGS[st->type][st->level + 1].cost * app->runMods.upgradeCostMultiplier);
            bool canUpg = app->gs.playerGold[localPI] >= upgCost;
            int ubx = px + 8, uby = py + INFO_PANEL_H - 30;
            int ubw = INFO_PANEL_W - 16, ubh = 24;

            Color ubCol = canUpg ? (Color){ 60, 100, 60, 255 } : (Color){ 50, 50, 50, 255 };
            DrawRectangle(ubx, uby, ubw, ubh, ubCol);
            DrawRectangleLines(ubx, uby, ubw, ubh, (Color){ 120, 120, 120, 200 });

            DrawText(TextFormat("Upgrade $%d", upgCost), ubx + 8, uby + 4, 16,
                     canUpg ? GOLD : (Color){ 100, 100, 100, 255 });

            if (canUpg && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Rectangle upgRect = { (float)ubx, (float)uby, (float)ubw, (float)ubh };
                if (CheckCollisionPointRec(mouse, upgRect)) {
                    if (app->netCtx.mode == NET_MODE_CLIENT) {
                        NetSendUpgradeTower(&app->netCtx, app->towers[app->selectedTowerIdx].id);
                    } else {
                        app->gs.playerGold[localPI] -= upgCost;
                        app->gs.gold = app->gs.playerGold[0];
                        app->towers[app->selectedTowerIdx].level++;
                    }
                }
            }
        } else {
            DrawText("MAX LEVEL", px + 8, py + INFO_PANEL_H - 26, 16, (Color){ 180, 180, 50, 255 });
        }
    }

    // --- Placement hint ---
    if (app->selectedTowerType >= 0) {
        DrawText(TextFormat("Placing: %s  (Right-click to cancel)",
                 TOWER_NAMES[app->selectedTowerType]), 10, screenH - BOTTOM_BAR_HEIGHT - 24, 16, YELLOW);
    }

    // --- Multiplayer Player List & Gift UI ---
    if (app->netCtx.mode != NET_MODE_NONE && app->gs.playerCount > 1) {
        int plX = 10, plY = 40;
        DrawRectangle(plX, plY, 180, 30 * app->gs.playerCount + 5, (Color){ 0, 0, 0, 150 });
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            if (!app->netCtx.playerConnected[i]) continue;
            int py = plY + 3 + i * 30;
            Color pCol = PLAYER_COLORS[i];
            bool isLocal = (i == localPI);
            DrawText(TextFormat("P%d: %s %s$%d", i + 1,
                    app->netCtx.playerNames[i],
                    isLocal ? "(You)" : "",
                    app->gs.playerGold[i]),
                    plX + 5, py, 14, pCol);

            if (!isLocal && i != localPI) {
                int gbX = plX + 140;
                Rectangle giftBtn = { (float)gbX, (float)py, 35.0f, 18.0f };
                bool giftHover = CheckCollisionPointRec(mouse, giftBtn);
                DrawRectangleRec(giftBtn, giftHover ? (Color){60,80,60,255} : (Color){40,50,40,255});
                DrawText("$25", gbX + 3, py + 2, 12, GOLD);
                if (giftHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && app->gs.playerGold[localPI] >= 25) {
                    if (app->netCtx.mode == NET_MODE_CLIENT) {
                        NetSendGiftGold(&app->netCtx, (uint8_t)i, 25);
                    } else {
                        app->gs.playerGold[localPI] -= 25;
                        app->gs.playerGold[i] += 25;
                    }
                }
            }
        }

        if (app->selectedTowerIdx >= 0 && app->towers[app->selectedTowerIdx].active) {
            uint8_t owner = app->towers[app->selectedTowerIdx].ownerPlayer;
            DrawText(TextFormat("Owner: %s", app->netCtx.playerNames[owner]),
                    screenW - INFO_PANEL_W - 2, 28, 14, PLAYER_COLORS[owner]);
        }
    }

    // --- Pause Menu ---
    if (app->gs.phase == PHASE_PAUSED || app->localPaused) {
        DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });

        UIDrawCenteredText("PAUSED", screenW / 2, screenH / 2 - 100, 60, WHITE);

        int pBtnW = 200, pBtnH = 45;
        int pBtnX = (screenW - pBtnW) / 2;
        int resumeY = screenH / 2 - 20;

        UIButtonResult resumeBtn = UIButton(pBtnX, resumeY, pBtnW, pBtnH, "Resume", 24, &UI_STYLE_PRIMARY);

        static const UIStyle settingsStyle = {
            .bgNormal = { 48, 38, 78, 255 }, .bgHover = { 70, 55, 110, 255 },
            .bgPressed = { 38, 28, 60, 255 }, .bgDisabled = { 40, 40, 40, 255 },
            .bgSelected = { 60, 48, 90, 255 },
            .border = { 140, 110, 200, 200 }, .borderHover = { 160, 130, 220, 255 },
            .text = WHITE, .textDisabled = { 100, 100, 100, 255 }, .borderWidth = 2.0f,
        };
        int pSetY = resumeY + pBtnH + 12;
        UIButtonResult setBtn = UIButton(pBtnX, pSetY, pBtnW, pBtnH, "Settings", 24, &settingsStyle);

        int pmMenuY = pSetY + pBtnH + 12;
        UIButtonResult menuBtn = UIButton(pBtnX, pmMenuY, pBtnW, pBtnH, "Main Menu", 24, &UI_STYLE_SECONDARY);

        int pQuitY = pmMenuY + pBtnH + 12;
        UIButtonResult quitBtn = UIButton(pBtnX, pQuitY, pBtnW, pBtnH, "Quit", 24, &UI_STYLE_DANGER);

        SettingsDraw(&app->settingsState, screenW, screenH);

        // Pause menu settings update
        int pauseSettingsResult = SettingsUpdate(&app->settingsState);
        if (pauseSettingsResult == 1) {
            app->settings = app->settingsState.pending;
            SettingsSave(&app->settings, "settings.cfg");

            UnloadRenderTexture(app->renderTarget);
            app->rtW = screenW / app->settings.ps1Downscale;
            app->rtH = screenH / app->settings.ps1Downscale;
            app->renderTarget = LoadRenderTexture(app->rtW, app->rtH);
            SetTextureFilter(app->renderTarget.texture, TEXTURE_FILTER_POINT);
            float resolution[2] = { (float)app->rtW, (float)app->rtH };
            SetShaderValue(app->ps1Shader, app->locResolution, resolution, SHADER_UNIFORM_VEC2);
            SetShaderValue(app->waterShader, app->wLocResolution, resolution, SHADER_UNIFORM_VEC2);

            if (app->settings.vsync) SetWindowState(FLAG_VSYNC_HINT);
            else ClearWindowState(FLAG_VSYNC_HINT);

            if (app->settings.fullscreen == FULLSCREEN_WINDOWED) {
                if (IsWindowFullscreen()) ToggleFullscreen();
                else if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                SetWindowSize(RESOLUTION_PRESETS[app->settings.resolutionIdx].width,
                              RESOLUTION_PRESETS[app->settings.resolutionIdx].height);
            } else if (app->settings.fullscreen == FULLSCREEN_BORDERLESS) {
                if (IsWindowFullscreen()) ToggleFullscreen();
                if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
            } else if (app->settings.fullscreen == FULLSCREEN_EXCLUSIVE) {
                if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
                if (!IsWindowFullscreen()) ToggleFullscreen();
            }

            g_camCtrl.panSpeed = app->settings.camPanSpeed;
            g_camCtrl.rotSpeed = app->settings.camRotSpeed;
            g_camCtrl.zoomSpeed = app->settings.camZoomSpeed;
        }

        if (!app->settingsState.open) {
            if (resumeBtn.clicked) {
                if (app->localPaused) app->localPaused = false;
                else app->gs.phase = app->phaseBeforePause;
            } else if (setBtn.clicked) {
                SettingsOpen(&app->settingsState, &app->settings);
            } else if (menuBtn.clicked) {
                app->localPaused = false;
                if (app->netCtx.mode != NET_MODE_NONE) {
                    NetContextDestroy(&app->netCtx);
                    NetShutdown();
                    NetContextInit(&app->netCtx);
                }
                SceneManagerTransition(app->sceneManager, SCENE_MENU);
            } else if (quitBtn.clicked) {
                if (app->netCtx.mode != NET_MODE_NONE) {
                    NetContextDestroy(&app->netCtx);
                    NetShutdown();
                }
                CloseWindow();
                return;
            }
        }
    }

    // --- Chat overlay ---
    if (app->netCtx.mode != NET_MODE_NONE) {
        ChatDraw(&app->chatState, screenW, screenH);
    }

    // --- Game Over / Victory Screen ---
    if (app->gs.phase == PHASE_OVER || app->gs.phase == PHASE_VICTORY) {
        DrawRectangle(0, 0, screenW, screenH, (Color){ 0, 0, 0, 150 });
        bool won = (app->gs.phase == PHASE_VICTORY);
        const char *msg = won ? "VICTORY!" : "GAME OVER";
        Color msgCol = won ? GOLD : RED;
        UIDrawCenteredText(msg, screenW / 2, screenH / 2 - 80, 60, msgCol);

        const char *sub = won ?
            TextFormat("All %d waves cleared!", app->gs.endlessActive ? app->gs.currentWave : MAX_WAVES) :
            TextFormat("Survived to wave %d/%d", app->gs.currentWave + 1,
                      app->gs.endlessActive ? app->gs.currentWave + 1 : MAX_WAVES);
        UIDrawCenteredText(sub, screenW / 2, screenH / 2 - 10, 24, WHITE);

        int crystalsEarned = CrystalsCalculate(app->gs.currentWave, app->gs.lives, app->gs.maxLives,
                                                (int)app->gs.difficulty, won);
        UIDrawCenteredText(TextFormat("Crystals earned: +%d", crystalsEarned),
                          screenW / 2, screenH / 2 + 20, 22, (Color){180,100,255,255});

        UIDrawCenteredText("Press R to restart | ESC for Main Menu",
                          screenW / 2, screenH / 2 + 55, 18, LIGHTGRAY);

        // Endless mode button
        int nextBtnY = screenH / 2 + 80;
        if (won && !app->gs.endlessActive) {
            int eBtnW = 220, eBtnH = 40;
            int eBtnX = (screenW - eBtnW) / 2;
            UIButtonResult endlessBtn = UIButton(eBtnX, nextBtnY, eBtnW, eBtnH, "Continue (Endless)", 20, &UI_STYLE_SECONDARY);
            nextBtnY += eBtnH + 10;

            if (endlessBtn.clicked) {
                app->gs.endlessActive = true;
                app->gs.endlessWave = 0;
                app->gs.phase = PHASE_WAVE_COUNTDOWN;
                app->gs.waveCountdown = 5.0f;
            }
        }

        // Main Menu button
        int mbW = 180, mbH = 40;
        int mbX = (screenW - mbW) / 2;
        UIButtonResult goMenuBtn = UIButton(mbX, nextBtnY, mbW, mbH, "Main Menu", 22, &UI_STYLE_NEUTRAL);

        // Save crystals (once)
        if (!app->crystalsSaved) {
            int earned = CrystalsCalculate(app->gs.currentWave, app->gs.lives, app->gs.maxLives,
                                           (int)app->gs.difficulty, won);
            app->profile.crystals += earned;
            app->profile.totalRuns++;
            if (won) app->profile.totalWins++;
            PlayerProfileSave(&app->profile, "profile.fdsave");
            app->crystalsSaved = true;
        }

        // Restart
        if (IsKeyPressed(KEY_R)) {
            app->crystalsSaved = false;
            bool reloaded = false;
            for (int i = 0; i < app->mapRegistry.count; i++) {
                if (strcmp(app->mapRegistry.names[i], app->map.name) == 0) {
                    reloaded = MapLoad(&app->map, app->mapRegistry.paths[i]);
                    break;
                }
            }
            if (!reloaded) MapInit(&app->map);
            MapBuildMesh(&app->gameMapMesh, &app->map, app->ps1Shader);
            MapPlaceTrees(app->gameTrees, &app->gameTreeCount, &app->map, 42);
            RunModifiersInit(&app->runMods, &app->profile);
            app->endlessState = (EndlessState){0};
            GameStateInit(&app->gs, app->selectedDifficulty, &app->runMods);
            memset(app->enemies, 0, sizeof(app->enemies));
            memset(app->towers, 0, sizeof(app->towers));
            memset(app->projectiles, 0, sizeof(app->projectiles));
            app->selectedTowerType = -1;
            app->selectedTowerIdx = -1;
        }

        // Back to menu
        bool goMenu = IsKeyPressed(KEY_ESCAPE) || goMenuBtn.clicked;
        if (goMenu) {
            app->crystalsSaved = false;
            if (app->netCtx.mode != NET_MODE_NONE) {
                NetContextDestroy(&app->netCtx);
                NetShutdown();
                NetContextInit(&app->netCtx);
            }
            SceneManagerTransition(app->sceneManager, SCENE_MENU);
        }
    }

    EndDrawing();
}

Scene SceneGameCreate(void)
{
    return (Scene){
        .name = "Game",
        .draw = GameSceneDraw,
    };
}
