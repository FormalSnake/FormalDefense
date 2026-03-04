#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>
#include "entity.h"
#include "game.h"
#include "map.h"

// ENet forward declarations
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

#define NET_PORT 7777
#define NET_DISCOVERY_PORT 7778
#define NET_MAX_PLAYERS 4
#define NET_PROTOCOL_VERSION 1
#define NET_SNAPSHOT_RATE (1.0f / 20.0f)
#define NET_MAX_USERNAME 16
#define NET_MAX_CHAT_MSG 128

// --- Network Mode ---

typedef enum {
    NET_MODE_NONE,
    NET_MODE_HOST,
    NET_MODE_CLIENT,
} NetMode;

// --- ENet Channels ---

#define NET_CHANNEL_RELIABLE   0
#define NET_CHANNEL_SNAPSHOT   1
#define NET_CHANNEL_CHAT       2
#define NET_CHANNEL_COUNT      3

// --- Message Types ---

typedef enum {
    // Lobby
    MSG_DISCOVERY_REQUEST = 1,
    MSG_DISCOVERY_RESPONSE,
    MSG_JOIN_REQUEST,
    MSG_JOIN_ACCEPT,
    MSG_JOIN_REJECT,
    MSG_LOBBY_STATE,
    MSG_GAME_START,
    MSG_PLAYER_DISCONNECT,
    // Actions (client -> host)
    MSG_ACTION_PLACE_TOWER,
    MSG_ACTION_UPGRADE_TOWER,
    MSG_ACTION_GIFT_GOLD,
    // Confirmations (host -> all)
    MSG_TOWER_PLACED,
    MSG_TOWER_UPGRADED,
    MSG_GOLD_GIFTED,
    // State
    MSG_GAME_STATE_SNAPSHOT,
    // Chat
    MSG_CHAT,
} MessageType;

// --- Packet Header ---

#pragma pack(push, 1)

typedef struct {
    uint8_t type;
    uint8_t version;
    uint16_t size;
} PacketHeader;

// --- Lobby Messages ---

typedef struct {
    PacketHeader header;
} DiscoveryRequest;

typedef struct {
    PacketHeader header;
    char hostName[NET_MAX_USERNAME];
    uint8_t playerCount;
    uint8_t maxPlayers;
    uint8_t inGame;
} DiscoveryResponse;

typedef struct {
    PacketHeader header;
    char username[NET_MAX_USERNAME];
} JoinRequest;

typedef struct {
    PacketHeader header;
    uint8_t playerIndex;
} JoinAccept;

typedef struct {
    PacketHeader header;
    char reason[32];
} JoinReject;

typedef struct {
    char username[NET_MAX_USERNAME];
    uint8_t connected;
} LobbyPlayerInfo;

typedef struct {
    PacketHeader header;
    uint8_t playerCount;
    LobbyPlayerInfo players[NET_MAX_PLAYERS];
} LobbyStateMsg;

typedef struct {
    PacketHeader header;
    uint8_t playerCount;
} GameStartMsg;

typedef struct {
    PacketHeader header;
    uint8_t playerIndex;
} PlayerDisconnectMsg;

// --- Action Messages ---

typedef struct {
    PacketHeader header;
    uint8_t towerType;
    int16_t gridX;
    int16_t gridZ;
} ActionPlaceTower;

typedef struct {
    PacketHeader header;
    EntityID towerID;
} ActionUpgradeTower;

typedef struct {
    PacketHeader header;
    uint8_t targetPlayer;
    int16_t amount;
} ActionGiftGold;

// --- Confirmation Messages ---

typedef struct {
    PacketHeader header;
    uint8_t ownerPlayer;
    uint8_t towerType;
    int16_t gridX;
    int16_t gridZ;
    EntityID towerID;
} TowerPlacedMsg;

typedef struct {
    PacketHeader header;
    EntityID towerID;
    uint8_t newLevel;
} TowerUpgradedMsg;

typedef struct {
    PacketHeader header;
    uint8_t fromPlayer;
    uint8_t toPlayer;
    int16_t amount;
} GoldGiftedMsg;

// --- Snapshot Structs ---

typedef struct {
    EntityID id;
    uint8_t type;
    int16_t posX;       // worldPos.x * 100
    int16_t posZ;       // worldPos.z * 100
    int16_t hp;         // hp * 10
    int16_t maxHp;      // maxHp * 10
    uint8_t waypointIndex;
    uint8_t pathProgress; // 0-255
    uint8_t slowFactor;   // 0-255 (0=full slow, 255=no slow)
} SnapshotEnemy;

typedef struct {
    EntityID id;
    uint8_t type;
    uint8_t level;
    int16_t gridX;
    int16_t gridZ;
    uint8_t ownerPlayer;
} SnapshotTower;

typedef struct {
    EntityID id;
    int16_t posX;
    int16_t posY;
    int16_t posZ;
    EntityID targetID;
    uint8_t ownerPlayer;
} SnapshotProjectile;

typedef struct {
    PacketHeader header;
    uint8_t phase;
    uint8_t currentWave;
    int16_t lives;
    float waveCountdown;
    int16_t playerGold[NET_MAX_PLAYERS];
    uint8_t enemyCount;
    uint8_t towerCount;
    uint8_t projectileCount;
    // Followed by: SnapshotEnemy[], SnapshotTower[], SnapshotProjectile[]
} GameStateSnapshot;

// --- Chat Message ---

typedef struct {
    PacketHeader header;
    uint8_t playerIndex;
    char message[NET_MAX_CHAT_MSG];
} ChatMsg;

#pragma pack(pop)

// --- Net Context ---

typedef struct {
    NetMode mode;
    ENetHost *host;
    ENetPeer *serverPeer;         // Client only: connection to host
    ENetPeer *clientPeers[NET_MAX_PLAYERS]; // Host only: connected clients

    uint8_t localPlayerIndex;
    int playerCount;
    char playerNames[NET_MAX_PLAYERS][NET_MAX_USERNAME];
    bool playerConnected[NET_MAX_PLAYERS];

    float snapshotTimer;
    bool inGame;

    // Discovery (UDP)
    int discoverySocket;
    float discoveryTimer;
} NetContext;

// --- Chat callback ---
typedef void (*NetChatCallback)(uint8_t playerIndex, const char *username, const char *message);
extern NetChatCallback g_netChatCallback;

// --- API ---

bool NetInit(void);
void NetShutdown(void);

void NetContextInit(NetContext *ctx);
void NetContextDestroy(NetContext *ctx);

bool NetHostCreate(NetContext *ctx, const char *username);
bool NetClientCreate(NetContext *ctx);
bool NetClientConnect(NetContext *ctx, const char *hostAddr, const char *username);

void NetPoll(NetContext *ctx, GameState *gs, Enemy enemies[], Tower towers[],
             Projectile projectiles[], Map *map);

void NetSendPlaceTower(NetContext *ctx, TowerType type, GridPos pos);
void NetSendUpgradeTower(NetContext *ctx, EntityID towerID);
void NetSendGiftGold(NetContext *ctx, uint8_t targetPlayer, int amount);
void NetSendChat(NetContext *ctx, const char *message);
void NetSendGameStart(NetContext *ctx);

void NetBroadcastSnapshot(NetContext *ctx, GameState *gs, Enemy enemies[],
                          Tower towers[], Projectile projectiles[]);

// Discovery
void NetDiscoveryStart(NetContext *ctx);
void NetDiscoveryStop(NetContext *ctx);
void NetDiscoveryPoll(NetContext *ctx);
void NetDiscoveryRespond(NetContext *ctx);

// Discovered games (for lobby browser)
#define MAX_DISCOVERED_GAMES 8

typedef struct {
    char hostName[NET_MAX_USERNAME];
    char address[64];
    uint8_t playerCount;
    uint8_t maxPlayers;
    float lastSeen;
} DiscoveredGame;

extern DiscoveredGame g_discoveredGames[MAX_DISCOVERED_GAMES];
extern int g_discoveredGameCount;

#endif
