#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include <string.h>

static void PerkSelectDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    Vector2 mouse = GetMousePosition();
    bool isMultiplayer = (app->netCtx.mode != NET_MODE_NONE);

    // In multiplayer, poll network for perk offered/result messages
    if (isMultiplayer) {
        NetPoll(&app->netCtx, &app->gs, app->enemies, app->towers, app->projectiles, &app->map);
    }

    // Client waiting for perk offer from host
    if (isMultiplayer && app->netCtx.mode == NET_MODE_CLIENT && !app->netCtx.perkVotingActive && app->netCtx.perkResult == -2) {
        BeginDrawing();
        ClearBackground((Color){ 20, 22, 28, 255 });
        UIDrawCenteredText("Waiting for host...", screenW / 2, 200, 28, LIGHTGRAY);
        EndDrawing();
        return;
    }

    // Client: sync offered perks from netCtx
    if (isMultiplayer && app->netCtx.perkVotingActive) {
        for (int i = 0; i < 3; i++)
            app->perkOffered[i] = app->netCtx.perkOfferedIDs[i];
    }

    // Check if perk result arrived (multiplayer)
    if (isMultiplayer && app->netCtx.perkResult != -2) {
        // Result is in: apply perk and start game
        if (app->netCtx.perkResult >= 0)
            RunModifiersApplyPerk(&app->runMods, app->netCtx.perkResult);

        // Init multiplayer game state now
        GameStateInitMultiplayer(&app->gs, app->netCtx.playerCount,
                                (Difficulty)app->netCtx.selectedDifficulty, &app->runMods);
        memset(app->enemies, 0, sizeof(app->enemies));
        memset(app->towers, 0, sizeof(app->towers));
        memset(app->projectiles, 0, sizeof(app->projectiles));
        CameraControllerInit(&g_camCtrl);
        CameraControllerUpdate(&g_camCtrl, &app->camera, 0.0f);
        app->selectedTowerType = -1;
        app->selectedTowerIdx = -1;

        // Reset voting state for next time
        app->netCtx.perkResult = -2;
        app->netCtx.perkVotingActive = false;

        SceneManagerTransition(app->sceneManager, SCENE_GAME);
        return;
    }

    BeginDrawing();
    ClearBackground((Color){ 20, 22, 28, 255 });

    if (isMultiplayer) {
        UIDrawCenteredText("Perk Vote", screenW / 2, 20, 36, WHITE);
        // Show vote status
        int votesNeeded = app->netCtx.playerCount;
        int votesCast = 0;
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            if (app->netCtx.playerConnected[i] && app->netCtx.perkVotes[i] != 255)
                votesCast++;
        }
        UIDrawCenteredText(TextFormat("Votes: %d / %d", votesCast, votesNeeded),
                          screenW / 2, 58, 18, LIGHTGRAY);
    } else {
        UIDrawCenteredText("Choose a Perk", screenW / 2, 40, 36, WHITE);
        UIDrawCenteredText("Or skip for no perk", screenW / 2, 82, 16, LIGHTGRAY);
    }

    int cardW = 220, cardH = 140;
    int gap = 30;
    int totalW = 3 * cardW + 2 * gap;
    int startX = (screenW - totalW) / 2;
    int cardY = 120;

    // Track if local player already voted
    bool localVoted = false;
    uint8_t localVote = 255;
    if (isMultiplayer) {
        int localIdx = app->netCtx.localPlayerIndex;
        localVote = app->netCtx.perkVotes[localIdx];
        localVoted = (localVote != 255);
    }

    for (int p = 0; p < 3; p++) {
        int pid = app->perkOffered[p];
        const PerkConfig *pc = &PERK_CONFIGS[pid];
        int cx = startX + p * (cardW + gap);
        Rectangle cardRect = { (float)cx, (float)cardY, (float)cardW, (float)cardH };
        bool hover = CheckCollisionPointRec(mouse, cardRect);

        bool isLocalChoice = (isMultiplayer && localVoted && localVote == p);
        Color bg = isLocalChoice ? (Color){40,60,80,255} :
                   hover ? (Color){50,60,80,255} : (Color){35,40,50,200};
        Color border = isLocalChoice ? (Color){100,180,255,255} :
                       hover ? (Color){100,150,220,255} : (Color){60,70,90,200};
        DrawRectangleRec(cardRect, bg);
        DrawRectangleLinesEx(cardRect, 2, border);

        DrawText(pc->name, cx + 10, cardY + 10, 20, WHITE);
        DrawText(pc->description, cx + 10, cardY + 40, 14, (Color){200,200,200,255});

        if (pc->hasTradeoff)
            DrawText("Trade-off", cx + 10, cardY + cardH - 24, 12, (Color){255,180,80,255});
        else
            DrawText("Pure buff", cx + 10, cardY + cardH - 24, 12, (Color){100,200,100,255});

        // Vote indicators (multiplayer): colored dots for each player who voted this option
        if (isMultiplayer) {
            int dotX = cx + 10;
            int dotY = cardY + cardH - 40;
            for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                if (!app->netCtx.playerConnected[i]) continue;
                if (app->netCtx.perkVotes[i] == p) {
                    DrawCircle(dotX, dotY, 5, PLAYER_COLORS[i]);
                    dotX += 14;
                }
            }
        }

        // Click to vote/select
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (isMultiplayer) {
                if (!localVoted) {
                    NetSendPerkVote(&app->netCtx, (uint8_t)p);
                }
            } else {
                RunModifiersApplyPerk(&app->runMods, pid);
                GameSceneReset(app);
                SceneManagerTransition(app->sceneManager, SCENE_GAME);
            }
        }
    }

    // Skip button
    static const UIStyle skipStyle = {
        .bgNormal = { 50, 45, 40, 255 }, .bgHover = { 70, 60, 50, 255 },
        .bgPressed = { 40, 35, 30, 255 }, .bgDisabled = { 40, 40, 40, 255 },
        .bgSelected = { 60, 55, 45, 255 },
        .border = { 150, 130, 100, 200 }, .borderHover = { 170, 150, 120, 255 },
        .text = WHITE, .textDisabled = { 100, 100, 100, 255 }, .borderWidth = 2.0f,
    };
    int skipW = 140, skipH = 40;
    int skipX = (screenW - skipW) / 2;
    int skipY = cardY + cardH + 30;

    if (isMultiplayer && localVoted) {
        // Already voted, show waiting
        UIDrawCenteredText("Vote submitted. Waiting...", screenW / 2, skipY + 10, 18, LIGHTGRAY);
    } else {
        UIButtonResult skipBtn = UIButton(skipX, skipY, skipW, skipH, "Skip", 22, &skipStyle);
        if (skipBtn.clicked) {
            if (isMultiplayer) {
                if (!localVoted)
                    NetSendPerkVote(&app->netCtx, 255); // skip vote
            } else {
                GameSceneReset(app);
                SceneManagerTransition(app->sceneManager, SCENE_GAME);
            }
        }
    }

    EndDrawing();

    // Host: check if all votes are in, then resolve
    if (isMultiplayer && app->netCtx.mode == NET_MODE_HOST && app->netCtx.perkVotingActive) {
        int expectedVotes = app->netCtx.playerCount;
        if ((int)app->netCtx.perkVoteCount >= expectedVotes) {
            // Tally votes
            int counts[3] = {0};
            int skipCount = 0;
            for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                if (!app->netCtx.playerConnected[i]) continue;
                uint8_t v = app->netCtx.perkVotes[i];
                if (v < 3) counts[v]++;
                else skipCount++;
            }

            // Find max voted option
            int maxVotes = skipCount; // skip competes with perk choices
            int winner = -1; // -1 = skip
            for (int i = 0; i < 3; i++) {
                if (counts[i] > maxVotes) {
                    maxVotes = counts[i];
                    winner = i;
                }
            }

            // Tiebreak: random among tied
            if (winner >= 0) {
                int tiedCount = 0;
                int tied[3];
                for (int i = 0; i < 3; i++) {
                    if (counts[i] == maxVotes)
                        tied[tiedCount++] = i;
                }
                if (skipCount == maxVotes)
                    tiedCount++; // skip also tied, but we handle below

                if (tiedCount > 1 && skipCount < maxVotes) {
                    // Tiebreak among perk choices only
                    winner = tied[GetRandomValue(0, tiedCount - 1)];
                } else if (skipCount == maxVotes) {
                    // Skip tied with perks — include skip in tiebreak
                    int allTied[4];
                    int allCount = 0;
                    for (int i = 0; i < 3; i++) {
                        if (counts[i] == maxVotes)
                            allTied[allCount++] = i;
                    }
                    allTied[allCount++] = -1; // skip
                    winner = allTied[GetRandomValue(0, allCount - 1)];
                }
            }

            int8_t resultPerkID = (winner >= 0) ? (int8_t)app->perkOffered[winner] : -1;
            NetSendPerkResult(&app->netCtx, resultPerkID);
        }
    }

    // Single-player ESC
    if (!isMultiplayer && IsKeyPressed(KEY_ESCAPE)) {
        SceneManagerTransition(app->sceneManager, SCENE_SHOP);
    }
}

Scene ScenePerkSelectCreate(void)
{
    return (Scene){
        .name = "PerkSelect",
        .draw = PerkSelectDraw,
    };
}
