#include "lobby.h"
#include "ui.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

void LobbyStateInit(LobbyState *ls)
{
    memset(ls, 0, sizeof(*ls));
    ls->phase = LOBBY_CHOOSE;
    strncpy(ls->username, "Player", NET_MAX_USERNAME);
    ls->usernameLen = 6;
    ls->selectedGame = -1;
}

// Text input is now handled via UITextInput from ui.h

// --- Update ---

void LobbyUpdate(LobbyState *ls, NetContext *ctx)
{
    switch (ls->phase) {
    case LOBBY_CHOOSE:
        if (ls->editingUsername) {
            UITextInput unInput = { ls->username, &ls->usernameLen, USERNAME_MAX_LEN, true };
            UITextInputUpdate(&unInput);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB))
                ls->editingUsername = false;
        }
        break;

    case LOBBY_HOST_WAIT:
        NetDiscoveryPoll(ctx);
        NetPoll(ctx, NULL, NULL, NULL, NULL, NULL);
        break;

    case LOBBY_JOIN_BROWSE:
        if (ls->editingIP) {
            UITextInput ipInput = { ls->directIP, &ls->directIPLen, 63, true };
            UITextInputUpdate(&ipInput);
        }
        NetDiscoveryPoll(ctx);
        NetPoll(ctx, NULL, NULL, NULL, NULL, NULL);
        break;

    case LOBBY_JOIN_WAIT:
        NetPoll(ctx, NULL, NULL, NULL, NULL, NULL);
        break;
    }
}

// Draw helpers now use ui.h components

// --- Draw ---

void LobbyDraw(LobbyState *ls, NetContext *ctx, int screenW, int screenH)
{
    (void)screenH;
    Vector2 mouse = GetMousePosition();
    int cx = screenW / 2;

    switch (ls->phase) {
    case LOBBY_CHOOSE: {
        UIDrawCenteredText("Multiplayer", cx, 60, 40, WHITE);

        // Username
        DrawText("Username:", cx - 140, 140, 18, LIGHTGRAY);
        UITextInput unDisplay = { ls->username, &ls->usernameLen, USERNAME_MAX_LEN, ls->editingUsername };
        UITextInputDraw(&unDisplay, cx - 140, 165, 280, 32);
        Rectangle unRect = { (float)(cx - 140), 165.0f, 280.0f, 32.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ls->editingUsername = CheckCollisionPointRec(mouse, unRect);
        }

        // Host / Join buttons
        UIButtonResult hostBtn = UIButton(cx - 100, 230, 200, 45, "Host Game", 24, &UI_STYLE_NEUTRAL);
        if (hostBtn.clicked && ls->usernameLen > 0) {
            ls->hostRequested = true;
        }

        UIButtonResult joinBtn = UIButton(cx - 100, 290, 200, 45, "Join Game", 24, &UI_STYLE_NEUTRAL);
        if (joinBtn.clicked && ls->usernameLen > 0) {
            NetInit();
            if (NetClientCreate(ctx)) {
                NetDiscoveryStart(ctx);
                ls->phase = LOBBY_JOIN_BROWSE;
            }
        }

        if (UIButton(cx - 100, 370, 200, 45, "Back", 24, &UI_STYLE_NEUTRAL).clicked) {
            ls->backRequested = true;
        }
        break;
    }

    case LOBBY_HOST_WAIT: {
        UIDrawCenteredText("Hosting Game", cx, 50, 36, WHITE);
        DrawText("Waiting for players...", cx - 100, 100, 18, LIGHTGRAY);

        // Show selected map
        DrawText(TextFormat("Map: %s", ctx->selectedMap[0] ? ctx->selectedMap : "None"),
                 cx - 150, 125, 16, (Color){ 180, 200, 255, 255 });

        // Player list
        UIDrawPlayerList(cx - 150, 150, 300, NET_MAX_PLAYERS,
                         (const char (*)[16])ctx->playerNames, ctx->playerConnected,
                         PLAYER_COLORS, -1);

        // Start button
        if (UIButton(cx - 100, 320, 200, 45, "Start Game", 24, &UI_STYLE_PRIMARY).clicked) {
            NetSendGameStart(ctx);
        }

        if (UIButton(cx - 100, 380, 200, 40, "Cancel", 20, &UI_STYLE_NEUTRAL).clicked) {
            NetDiscoveryStop(ctx);
            NetContextDestroy(ctx);
            NetShutdown();
            ls->phase = LOBBY_CHOOSE;
        }
        break;
    }

    case LOBBY_JOIN_BROWSE: {
        UIDrawCenteredText("Join Game", cx, 50, 36, WHITE);
        DrawText("LAN Games:", cx - 150, 100, 18, LIGHTGRAY);

        // Game list
        if (g_discoveredGameCount == 0) {
            DrawText("Searching...", cx - 50, 135, 16, DARKGRAY);
        }
        for (int i = 0; i < g_discoveredGameCount; i++) {
            int gy = 130 + i * 40;
            bool selected = (ls->selectedGame == i);
            Color bg = selected ? (Color){ 50, 70, 90, 255 } : (Color){ 30, 35, 45, 200 };
            DrawRectangle(cx - 180, gy, 360, 35, bg);
            DrawRectangleLines(cx - 180, gy, 360, 35, (Color){ 70, 70, 80, 200 });
            DrawText(TextFormat("%s (%d/%d) - %s",
                    g_discoveredGames[i].hostName,
                    g_discoveredGames[i].playerCount,
                    g_discoveredGames[i].maxPlayers,
                    g_discoveredGames[i].address),
                    cx - 170, gy + 8, 16, WHITE);

            Rectangle gRect = { (float)(cx - 180), (float)gy, 360.0f, 35.0f };
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, gRect))
                ls->selectedGame = i;
        }

        int baseY = 130 + (g_discoveredGameCount > 0 ? g_discoveredGameCount : 1) * 40 + 20;

        // Direct IP entry
        DrawText("Or enter IP:", cx - 150, baseY, 16, LIGHTGRAY);
        UITextInput ipDisplay = { ls->directIP, &ls->directIPLen, 63, ls->editingIP };
        UITextInputDraw(&ipDisplay, cx - 150, baseY + 22, 220, 28);
        Rectangle ipRect = { (float)(cx - 150), (float)(baseY + 22), 220.0f, 28.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ls->editingIP = CheckCollisionPointRec(mouse, ipRect);
        }

        // Connect button
        UIButtonResult connBtn = UIButton(cx + 80, baseY + 20, 80, 30, "Connect", 16, &UI_STYLE_NEUTRAL);
        if (connBtn.clicked) {
            const char *addr = NULL;
            if (ls->directIPLen > 0) {
                addr = ls->directIP;
            } else if (ls->selectedGame >= 0 && ls->selectedGame < g_discoveredGameCount) {
                addr = g_discoveredGames[ls->selectedGame].address;
            }
            if (addr) {
                NetDiscoveryStop(ctx);
                if (NetClientConnect(ctx, addr, ls->username)) {
                    ls->phase = LOBBY_JOIN_WAIT;
                }
            }
        }

        if (UIButton(cx - 100, baseY + 70, 200, 40, "Cancel", 20, &UI_STYLE_NEUTRAL).clicked) {
            NetDiscoveryStop(ctx);
            NetContextDestroy(ctx);
            NetShutdown();
            ls->phase = LOBBY_CHOOSE;
        }
        break;
    }

    case LOBBY_JOIN_WAIT: {
        UIDrawCenteredText("Lobby", cx, 50, 36, WHITE);
        DrawText("Waiting for host to start...", cx - 120, 100, 18, LIGHTGRAY);

        // Show selected map
        DrawText(TextFormat("Map: %s", ctx->selectedMap[0] ? ctx->selectedMap : "..."),
                 cx - 150, 125, 16, (Color){ 180, 200, 255, 255 });

        // Player list
        UIDrawPlayerList(cx - 150, 150, 300, NET_MAX_PLAYERS,
                         (const char (*)[16])ctx->playerNames, ctx->playerConnected,
                         PLAYER_COLORS, ctx->localPlayerIndex);

        if (UIButton(cx - 100, 310, 200, 40, "Disconnect", 20, &UI_STYLE_NEUTRAL).clicked) {
            NetContextDestroy(ctx);
            NetShutdown();
            ls->phase = LOBBY_CHOOSE;
        }

        // Check if host disconnected
        if (!ctx->serverPeer) {
            NetContextDestroy(ctx);
            NetShutdown();
            ls->phase = LOBBY_CHOOSE;
        }
        break;
    }
    }
}

bool LobbyGameStarted(LobbyState *ls, NetContext *ctx)
{
    if (ctx->mode == NET_MODE_HOST && ctx->inGame) return true;
    if (ctx->mode == NET_MODE_CLIENT && ctx->inGame) return true;
    (void)ls;
    return false;
}

bool LobbyBackPressed(LobbyState *ls)
{
    if (ls->backRequested) {
        ls->backRequested = false;
        return true;
    }
    return false;
}
