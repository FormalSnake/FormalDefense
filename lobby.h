#ifndef LOBBY_H
#define LOBBY_H

#include "net.h"
#include <stdbool.h>

#define USERNAME_MAX_LEN 15

typedef enum {
    LOBBY_CHOOSE,     // Choose host or join
    LOBBY_HOST_WAIT,  // Hosting, waiting for players
    LOBBY_JOIN_BROWSE,// Browsing for games
    LOBBY_JOIN_WAIT,  // Connected to host, waiting for start
} LobbyPhase;

typedef struct {
    LobbyPhase phase;
    char username[NET_MAX_USERNAME];
    int usernameLen;
    bool editingUsername;
    int selectedGame;
    char directIP[64];
    int directIPLen;
    bool editingIP;
} LobbyState;

void LobbyStateInit(LobbyState *ls);
void LobbyUpdate(LobbyState *ls, NetContext *ctx);
void LobbyDraw(LobbyState *ls, NetContext *ctx, int screenW, int screenH);
bool LobbyGameStarted(LobbyState *ls, NetContext *ctx);
bool LobbyBackPressed(LobbyState *ls);

#endif
