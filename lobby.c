#include "lobby.h"
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

// --- Text Input Helper ---

static void HandleTextInput(char *buf, int *len, int maxLen)
{
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (*len < maxLen && ch >= 32 && ch < 127) {
            buf[*len] = (char)ch;
            (*len)++;
            buf[*len] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && *len > 0) {
        (*len)--;
        buf[*len] = '\0';
    }
}

// --- Update ---

void LobbyUpdate(LobbyState *ls, NetContext *ctx)
{
    switch (ls->phase) {
    case LOBBY_CHOOSE:
        if (ls->editingUsername) {
            HandleTextInput(ls->username, &ls->usernameLen, USERNAME_MAX_LEN);
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
            HandleTextInput(ls->directIP, &ls->directIPLen, 63);
        }
        NetDiscoveryPoll(ctx);
        NetPoll(ctx, NULL, NULL, NULL, NULL, NULL);
        break;

    case LOBBY_JOIN_WAIT:
        NetPoll(ctx, NULL, NULL, NULL, NULL, NULL);
        break;
    }
}

// --- Draw Helpers ---

static bool DrawButton(int x, int y, int w, int h, const char *text, int fontSize, Vector2 mouse)
{
    Rectangle rect = { (float)x, (float)y, (float)w, (float)h };
    bool hover = CheckCollisionPointRec(mouse, rect);
    Color bg = hover ? (Color){ 60, 80, 100, 255 } : (Color){ 40, 50, 65, 255 };
    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 2, (Color){ 100, 140, 180, 200 });
    int tw = MeasureText(text, fontSize);
    DrawText(text, x + (w - tw) / 2, y + (h - fontSize) / 2, fontSize, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void DrawTextBox(int x, int y, int w, int h, const char *text, bool active, Vector2 mouse)
{
    Rectangle rect = { (float)x, (float)y, (float)w, (float)h };
    (void)mouse;
    Color border = active ? (Color){ 100, 200, 100, 255 } : (Color){ 80, 80, 80, 255 };
    DrawRectangleRec(rect, (Color){ 25, 25, 30, 255 });
    DrawRectangleLinesEx(rect, 2, border);
    DrawText(text, x + 6, y + (h - 16) / 2, 16, WHITE);
    if (active) {
        int tw = MeasureText(text, 16);
        DrawText("_", x + 6 + tw, y + (h - 16) / 2, 16, (Color){ 100, 200, 100, 255 });
    }
}

// --- Draw ---

void LobbyDraw(LobbyState *ls, NetContext *ctx, int screenW, int screenH)
{
    (void)screenH;
    Vector2 mouse = GetMousePosition();
    int cx = screenW / 2;

    switch (ls->phase) {
    case LOBBY_CHOOSE: {
        const char *title = "Multiplayer";
        int titleW = MeasureText(title, 40);
        DrawText(title, cx - titleW / 2, 60, 40, WHITE);

        // Username
        DrawText("Username:", cx - 140, 140, 18, LIGHTGRAY);
        DrawTextBox(cx - 140, 165, 280, 32, ls->username, ls->editingUsername, mouse);
        Rectangle unRect = { (float)(cx - 140), 165.0f, 280.0f, 32.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ls->editingUsername = CheckCollisionPointRec(mouse, unRect);
        }

        // Host / Join buttons
        if (DrawButton(cx - 100, 230, 200, 45, "Host Game", 24, mouse)) {
            if (ls->usernameLen > 0) {
                ls->hostRequested = true;
            }
        }

        if (DrawButton(cx - 100, 290, 200, 45, "Join Game", 24, mouse)) {
            if (ls->usernameLen > 0) {
                NetInit();
                if (NetClientCreate(ctx)) {
                    NetDiscoveryStart(ctx);
                    ls->phase = LOBBY_JOIN_BROWSE;
                }
            }
        }

        if (DrawButton(cx - 100, 370, 200, 45, "Back", 24, mouse)) {
            ls->backRequested = true;
        }
        break;
    }

    case LOBBY_HOST_WAIT: {
        const char *title = "Hosting Game";
        int titleW = MeasureText(title, 36);
        DrawText(title, cx - titleW / 2, 50, 36, WHITE);

        DrawText("Waiting for players...", cx - 100, 100, 18, LIGHTGRAY);

        // Show selected map
        DrawText(TextFormat("Map: %s", ctx->selectedMap[0] ? ctx->selectedMap : "None"),
                 cx - 150, 125, 16, (Color){ 180, 200, 255, 255 });

        // Player list
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            int py = 150 + i * 35;
            if (ctx->playerConnected[i]) {
                Color col = PLAYER_COLORS[i];
                DrawRectangle(cx - 150, py, 300, 30, (Color){ 30, 40, 50, 200 });
                DrawText(TextFormat("P%d: %s", i + 1, ctx->playerNames[i]),
                        cx - 140, py + 6, 18, col);
            } else {
                DrawRectangle(cx - 150, py, 300, 30, (Color){ 20, 20, 25, 150 });
                DrawText(TextFormat("P%d: ---", i + 1), cx - 140, py + 6, 18, DARKGRAY);
            }
        }

        // Start button
        if (DrawButton(cx - 100, 320, 200, 45, "Start Game", 24, mouse)) {
            NetSendGameStart(ctx);
        }

        if (DrawButton(cx - 100, 380, 200, 40, "Cancel", 20, mouse)) {
            NetDiscoveryStop(ctx);
            NetContextDestroy(ctx);
            NetShutdown();
            ls->phase = LOBBY_CHOOSE;
        }
        break;
    }

    case LOBBY_JOIN_BROWSE: {
        const char *title = "Join Game";
        int titleW = MeasureText(title, 36);
        DrawText(title, cx - titleW / 2, 50, 36, WHITE);

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
        DrawTextBox(cx - 150, baseY + 22, 220, 28, ls->directIP, ls->editingIP, mouse);
        Rectangle ipRect = { (float)(cx - 150), (float)(baseY + 22), 220.0f, 28.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ls->editingIP = CheckCollisionPointRec(mouse, ipRect);
        }

        // Connect button
        if (DrawButton(cx + 80, baseY + 20, 80, 30, "Connect", 16, mouse)) {
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

        if (DrawButton(cx - 100, baseY + 70, 200, 40, "Cancel", 20, mouse)) {
            NetDiscoveryStop(ctx);
            NetContextDestroy(ctx);
            NetShutdown();
            ls->phase = LOBBY_CHOOSE;
        }
        break;
    }

    case LOBBY_JOIN_WAIT: {
        const char *title = "Lobby";
        int titleW = MeasureText(title, 36);
        DrawText(title, cx - titleW / 2, 50, 36, WHITE);

        DrawText("Waiting for host to start...", cx - 120, 100, 18, LIGHTGRAY);

        // Show selected map
        DrawText(TextFormat("Map: %s", ctx->selectedMap[0] ? ctx->selectedMap : "..."),
                 cx - 150, 125, 16, (Color){ 180, 200, 255, 255 });

        // Player list
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            int py = 150 + i * 35;
            if (ctx->playerConnected[i]) {
                Color col = PLAYER_COLORS[i];
                DrawRectangle(cx - 150, py, 300, 30, (Color){ 30, 40, 50, 200 });
                DrawText(TextFormat("P%d: %s%s", i + 1, ctx->playerNames[i],
                        i == ctx->localPlayerIndex ? " (You)" : ""),
                        cx - 140, py + 6, 18, col);
            } else {
                DrawRectangle(cx - 150, py, 300, 30, (Color){ 20, 20, 25, 150 });
                DrawText(TextFormat("P%d: ---", i + 1), cx - 140, py + 6, 18, DARKGRAY);
            }
        }

        if (DrawButton(cx - 100, 310, 200, 40, "Disconnect", 20, mouse)) {
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
