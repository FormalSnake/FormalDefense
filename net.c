#include "net.h"
#include "enet/enet.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Platform includes for discovery socket
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

DiscoveredGame g_discoveredGames[MAX_DISCOVERED_GAMES];
int g_discoveredGameCount = 0;
NetChatCallback g_netChatCallback = NULL;

// --- Helpers ---

static void PacketHeaderInit(PacketHeader *h, uint8_t type, uint16_t size)
{
    h->type = type;
    h->version = NET_PROTOCOL_VERSION;
    h->size = size;
}

static void NetSendReliable(ENetPeer *peer, const void *data, size_t size)
{
    ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, NET_CHANNEL_RELIABLE, packet);
}

static void NetSendUnreliable(ENetPeer *peer, const void *data, size_t size)
{
    ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, NET_CHANNEL_SNAPSHOT, packet);
}

static void NetSendChat_raw(ENetPeer *peer, const void *data, size_t size)
{
    ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, NET_CHANNEL_CHAT, packet);
}

static void NetBroadcastReliable(NetContext *ctx, const void *data, size_t size)
{
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->clientPeers[i])
            NetSendReliable(ctx->clientPeers[i], data, size);
    }
}

static void NetBroadcastUnreliable(NetContext *ctx, const void *data, size_t size)
{
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->clientPeers[i])
            NetSendUnreliable(ctx->clientPeers[i], data, size);
    }
}

static void NetBroadcastChatAll(NetContext *ctx, const void *data, size_t size)
{
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->clientPeers[i])
            NetSendChat_raw(ctx->clientPeers[i], data, size);
    }
}

// --- Init / Shutdown ---

bool NetInit(void)
{
    return enet_initialize() == 0;
}

void NetShutdown(void)
{
    enet_deinitialize();
}

void NetContextInit(NetContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = NET_MODE_NONE;
    ctx->discoverySocket = -1;
}

void NetContextDestroy(NetContext *ctx)
{
    if (ctx->discoverySocket >= 0) {
        close(ctx->discoverySocket);
        ctx->discoverySocket = -1;
    }
    if (ctx->host) {
        if (ctx->mode == NET_MODE_CLIENT && ctx->serverPeer) {
            enet_peer_disconnect_now(ctx->serverPeer, 0);
        }
        enet_host_destroy(ctx->host);
        ctx->host = NULL;
    }
    ctx->mode = NET_MODE_NONE;
}

// --- Host / Client Creation ---

bool NetHostCreate(NetContext *ctx, const char *username)
{
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = NET_PORT;

    ctx->host = enet_host_create(&addr, NET_MAX_PLAYERS - 1, NET_CHANNEL_COUNT, 0, 0);
    if (!ctx->host) return false;

    ctx->mode = NET_MODE_HOST;
    ctx->localPlayerIndex = 0;
    ctx->playerCount = 1;
    ctx->playerConnected[0] = true;
    strncpy(ctx->playerNames[0], username, NET_MAX_USERNAME - 1);
    ctx->playerNames[0][NET_MAX_USERNAME - 1] = '\0';
    ctx->snapshotTimer = 0.0f;
    ctx->inGame = false;

    return true;
}

bool NetClientCreate(NetContext *ctx)
{
    ctx->host = enet_host_create(NULL, 1, NET_CHANNEL_COUNT, 0, 0);
    if (!ctx->host) return false;

    ctx->mode = NET_MODE_CLIENT;
    return true;
}

bool NetClientConnect(NetContext *ctx, const char *hostAddr, const char *username)
{
    ENetAddress addr;
    enet_address_set_host(&addr, hostAddr);
    addr.port = NET_PORT;

    ctx->serverPeer = enet_host_connect(ctx->host, &addr, NET_CHANNEL_COUNT, 0);
    if (!ctx->serverPeer) return false;

    // Wait for connection (with timeout)
    ENetEvent event;
    if (enet_host_service(ctx->host, &event, 3000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        // Send join request
        JoinRequest req;
        PacketHeaderInit(&req.header, MSG_JOIN_REQUEST, sizeof(req));
        strncpy(req.username, username, NET_MAX_USERNAME - 1);
        req.username[NET_MAX_USERNAME - 1] = '\0';
        NetSendReliable(ctx->serverPeer, &req, sizeof(req));
        return true;
    }

    enet_peer_reset(ctx->serverPeer);
    ctx->serverPeer = NULL;
    return false;
}

// --- Host: Handle Incoming ---

static void HostHandleJoinRequest(NetContext *ctx, ENetPeer *peer, const JoinRequest *req)
{
    // Find free slot
    int slot = -1;
    for (int i = 1; i < NET_MAX_PLAYERS; i++) {
        if (!ctx->playerConnected[i]) {
            slot = i;
            break;
        }
    }

    if (slot < 0 || ctx->inGame) {
        JoinReject rej;
        PacketHeaderInit(&rej.header, MSG_JOIN_REJECT, sizeof(rej));
        strncpy(rej.reason, ctx->inGame ? "Game in progress" : "Server full", sizeof(rej.reason) - 1);
        NetSendReliable(peer, &rej, sizeof(rej));
        return;
    }

    ctx->playerConnected[slot] = true;
    ctx->clientPeers[slot] = peer;
    ctx->playerCount++;
    strncpy(ctx->playerNames[slot], req->username, NET_MAX_USERNAME - 1);
    ctx->playerNames[slot][NET_MAX_USERNAME - 1] = '\0';
    peer->data = (void *)(intptr_t)slot;

    // Send accept
    JoinAccept acc;
    PacketHeaderInit(&acc.header, MSG_JOIN_ACCEPT, sizeof(acc));
    acc.playerIndex = (uint8_t)slot;
    NetSendReliable(peer, &acc, sizeof(acc));

    // Broadcast lobby state to all
    NetBroadcastLobbyState(ctx);
}

static void HostHandleAction(NetContext *ctx, ENetPeer *peer, const uint8_t *data, size_t size,
                             GameState *gs, Enemy enemies[], Tower towers[],
                             Projectile projectiles[], Map *map)
{
    if (size < sizeof(PacketHeader)) return;
    const PacketHeader *hdr = (const PacketHeader *)data;
    int playerIdx = (int)(intptr_t)peer->data;
    (void)enemies;
    (void)projectiles;

    switch (hdr->type) {
    case MSG_ACTION_PLACE_TOWER: {
        if (size < sizeof(ActionPlaceTower)) return;
        const ActionPlaceTower *act = (const ActionPlaceTower *)data;
        GridPos pos = { act->gridX, act->gridZ };
        int cost = TOWER_CONFIGS[act->towerType][0].cost;

        if (act->towerType < TOWER_TYPE_COUNT &&
            MapCanPlaceTower(map, pos) &&
            gs->playerGold[playerIdx] >= cost) {

            gs->playerGold[playerIdx] -= cost;
            TowerPlace(towers, MAX_TOWERS, (TowerType)act->towerType, pos, (uint8_t)playerIdx, gs, map);
            map->tiles[pos.z][pos.x] = TILE_TOWER;

            // Find the tower we just placed to get its ID
            EntityID tid = ENTITY_ID_NONE;
            for (int i = MAX_TOWERS - 1; i >= 0; i--) {
                if (towers[i].active && towers[i].gridPos.x == pos.x && towers[i].gridPos.z == pos.z) {
                    tid = towers[i].id;
                    break;
                }
            }

            TowerPlacedMsg msg;
            PacketHeaderInit(&msg.header, MSG_TOWER_PLACED, sizeof(msg));
            msg.ownerPlayer = (uint8_t)playerIdx;
            msg.towerType = act->towerType;
            msg.gridX = act->gridX;
            msg.gridZ = act->gridZ;
            msg.towerID = tid;
            NetBroadcastReliable(ctx, &msg, sizeof(msg));
        }
        break;
    }
    case MSG_ACTION_UPGRADE_TOWER: {
        if (size < sizeof(ActionUpgradeTower)) return;
        const ActionUpgradeTower *act = (const ActionUpgradeTower *)data;
        Tower *t = TowerFindByID(towers, MAX_TOWERS, act->towerID);
        if (t && t->ownerPlayer == playerIdx && t->level < TOWER_MAX_LEVEL - 1) {
            int upgCost = TOWER_CONFIGS[t->type][t->level + 1].cost;
            if (gs->playerGold[playerIdx] >= upgCost) {
                gs->playerGold[playerIdx] -= upgCost;
                t->level++;

                TowerUpgradedMsg msg;
                PacketHeaderInit(&msg.header, MSG_TOWER_UPGRADED, sizeof(msg));
                msg.towerID = act->towerID;
                msg.newLevel = (uint8_t)t->level;
                NetBroadcastReliable(ctx, &msg, sizeof(msg));
            }
        }
        break;
    }
    case MSG_ACTION_GIFT_GOLD: {
        if (size < sizeof(ActionGiftGold)) return;
        const ActionGiftGold *act = (const ActionGiftGold *)data;
        if (act->targetPlayer < NET_MAX_PLAYERS &&
            act->targetPlayer != playerIdx &&
            ctx->playerConnected[act->targetPlayer] &&
            gs->playerGold[playerIdx] >= act->amount &&
            act->amount > 0) {

            gs->playerGold[playerIdx] -= act->amount;
            gs->playerGold[act->targetPlayer] += act->amount;

            GoldGiftedMsg msg;
            PacketHeaderInit(&msg.header, MSG_GOLD_GIFTED, sizeof(msg));
            msg.fromPlayer = (uint8_t)playerIdx;
            msg.toPlayer = act->targetPlayer;
            msg.amount = act->amount;
            NetBroadcastReliable(ctx, &msg, sizeof(msg));
        }
        break;
    }
    case MSG_CHAT: {
        if (size < sizeof(ChatMsg)) return;
        ChatMsg *chat = (ChatMsg *)data;
        chat->playerIndex = (uint8_t)playerIdx;
        NetBroadcastChatAll(ctx, chat, sizeof(ChatMsg));
        // Also show on host
        if (g_netChatCallback)
            g_netChatCallback(chat->playerIndex, ctx->playerNames[chat->playerIndex], chat->message);
        break;
    }
    default:
        break;
    }
}

static void HostHandleDisconnect(NetContext *ctx, ENetPeer *peer)
{
    int playerIdx = (int)(intptr_t)peer->data;
    if (playerIdx > 0 && playerIdx < NET_MAX_PLAYERS) {
        ctx->playerConnected[playerIdx] = false;
        ctx->clientPeers[playerIdx] = NULL;
        ctx->playerCount--;

        PlayerDisconnectMsg msg;
        PacketHeaderInit(&msg.header, MSG_PLAYER_DISCONNECT, sizeof(msg));
        msg.playerIndex = (uint8_t)playerIdx;
        NetBroadcastReliable(ctx, &msg, sizeof(msg));
    }
}

// --- Client: Handle Incoming ---

static void ClientHandlePacket(NetContext *ctx, const uint8_t *data, size_t size, int channel,
                               GameState *gs, Enemy enemies[], Tower towers[],
                               Projectile projectiles[], Map *map)
{
    if (size < sizeof(PacketHeader)) return;
    const PacketHeader *hdr = (const PacketHeader *)data;
    (void)channel;

    switch (hdr->type) {
    case MSG_JOIN_ACCEPT: {
        if (size < sizeof(JoinAccept)) return;
        const JoinAccept *acc = (const JoinAccept *)data;
        ctx->localPlayerIndex = acc->playerIndex;
        break;
    }
    case MSG_JOIN_REJECT: {
        // Connection rejected — disconnect
        enet_peer_disconnect_now(ctx->serverPeer, 0);
        ctx->serverPeer = NULL;
        break;
    }
    case MSG_LOBBY_STATE: {
        if (size < sizeof(LobbyStateMsg)) return;
        const LobbyStateMsg *lm = (const LobbyStateMsg *)data;
        ctx->playerCount = lm->playerCount;
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            strncpy(ctx->playerNames[i], lm->players[i].username, NET_MAX_USERNAME);
            ctx->playerConnected[i] = lm->players[i].connected;
        }
        strncpy(ctx->selectedMap, lm->selectedMap, MAX_MAP_NAME - 1);
        ctx->selectedMap[MAX_MAP_NAME - 1] = '\0';
        break;
    }
    case MSG_GAME_START: {
        if (size < sizeof(GameStartMsg)) return;
        const GameStartMsg *start = (const GameStartMsg *)data;
        ctx->inGame = true;
        ctx->playerCount = start->playerCount;
        strncpy(ctx->selectedMap, start->mapName, MAX_MAP_NAME - 1);
        ctx->selectedMap[MAX_MAP_NAME - 1] = '\0';
        break;
    }
    case MSG_MAP_DATA: {
        if (size < sizeof(MapDataMsg)) return;
        const MapDataMsg *mapMsg = (const MapDataMsg *)data;
        size_t expectedSize = sizeof(MapDataMsg) + mapMsg->dataSize;
        if (size < expectedSize) return;

        const char *mapData = (const char *)(data + sizeof(MapDataMsg));

        // Try loading from local maps/ first
        char localPath[256];
        snprintf(localPath, sizeof(localPath), "maps/%s.fdmap", mapMsg->mapName);
        if (!MapLoad(map, localPath)) {
            // Load from received data
            MapLoadFromBuffer(map, mapData, mapMsg->dataSize);
            // Save for next time
            FILE *f = fopen(localPath, "w");
            if (f) {
                fwrite(mapData, 1, mapMsg->dataSize, f);
                fclose(f);
            }
        }
        ctx->mapDataReceived = true;
        break;
    }
    case MSG_PLAYER_DISCONNECT: {
        if (size < sizeof(PlayerDisconnectMsg)) return;
        const PlayerDisconnectMsg *disc = (const PlayerDisconnectMsg *)data;
        if (disc->playerIndex < NET_MAX_PLAYERS) {
            ctx->playerConnected[disc->playerIndex] = false;
            ctx->playerCount--;
        }
        break;
    }
    case MSG_TOWER_PLACED: {
        if (size < sizeof(TowerPlacedMsg)) return;
        const TowerPlacedMsg *msg = (const TowerPlacedMsg *)data;
        // Apply locally (client gets this from snapshot too, but immediate feedback is nice)
        GridPos pos = { msg->gridX, msg->gridZ };
        // Only place if not already placed (avoid duplicate from snapshot)
        Tower *existing = TowerFindByID(towers, MAX_TOWERS, msg->towerID);
        if (!existing) {
            // Use a temporary gs to assign the correct ID
            uint16_t savedSeq = gs->nextEntitySeq;
            TowerPlace(towers, MAX_TOWERS, (TowerType)msg->towerType, pos, msg->ownerPlayer, gs, map);
            // Fix the ID to match host's
            for (int i = 0; i < MAX_TOWERS; i++) {
                if (towers[i].active && towers[i].gridPos.x == pos.x && towers[i].gridPos.z == pos.z
                    && towers[i].id != msg->towerID) {
                    towers[i].id = msg->towerID;
                    break;
                }
            }
            gs->nextEntitySeq = savedSeq; // restore since host manages IDs
            map->tiles[pos.z][pos.x] = TILE_TOWER;
        }
        break;
    }
    case MSG_TOWER_UPGRADED: {
        if (size < sizeof(TowerUpgradedMsg)) return;
        const TowerUpgradedMsg *msg = (const TowerUpgradedMsg *)data;
        Tower *t = TowerFindByID(towers, MAX_TOWERS, msg->towerID);
        if (t) t->level = msg->newLevel;
        break;
    }
    case MSG_GOLD_GIFTED: {
        if (size < sizeof(GoldGiftedMsg)) return;
        const GoldGiftedMsg *msg = (const GoldGiftedMsg *)data;
        if (msg->fromPlayer < NET_MAX_PLAYERS)
            gs->playerGold[msg->fromPlayer] -= msg->amount;
        if (msg->toPlayer < NET_MAX_PLAYERS)
            gs->playerGold[msg->toPlayer] += msg->amount;
        break;
    }
    case MSG_GAME_STATE_SNAPSHOT: {
        if (size < sizeof(GameStateSnapshot)) return;
        const GameStateSnapshot *snap = (const GameStateSnapshot *)data;

        // Update game state
        gs->phase = snap->phase;
        gs->currentWave = snap->currentWave;
        gs->lives = snap->lives;
        gs->waveCountdown = snap->waveCountdown;
        for (int i = 0; i < NET_MAX_PLAYERS; i++)
            gs->playerGold[i] = snap->playerGold[i];
        gs->gold = gs->playerGold[ctx->localPlayerIndex];

        // Parse entities from variable-length portion
        const uint8_t *ptr = data + sizeof(GameStateSnapshot);
        size_t remaining = size - sizeof(GameStateSnapshot);

        // Enemies
        size_t enemyDataSize = snap->enemyCount * sizeof(SnapshotEnemy);
        if (remaining < enemyDataSize) return;
        const SnapshotEnemy *snapEnemies = (const SnapshotEnemy *)ptr;

        // Mark all enemies inactive, then restore from snapshot
        for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
        for (int i = 0; i < snap->enemyCount && i < MAX_ENEMIES; i++) {
            const SnapshotEnemy *se = &snapEnemies[i];
            enemies[i].active = true;
            enemies[i].id = se->id;
            enemies[i].type = (EnemyType)se->type;
            enemies[i].worldPos.x = se->posX / 100.0f;
            enemies[i].worldPos.z = se->posZ / 100.0f;
            enemies[i].hp = se->hp / 10.0f;
            enemies[i].maxHp = se->maxHp / 10.0f;
            enemies[i].waypointIndex = se->waypointIndex;
            enemies[i].pathProgress = se->pathProgress / 255.0f;
            enemies[i].slowFactor = se->slowFactor / 255.0f;
            enemies[i].radius = ENEMY_CONFIGS[se->type].radius;
            enemies[i].color = ENEMY_CONFIGS[se->type].color;
            // Derive Y from elevation at current grid position + radius
            GridPos eGrid = MapWorldToGrid(enemies[i].worldPos);
            enemies[i].worldPos.y = MapGetElevationY(map, eGrid.x, eGrid.z) + enemies[i].radius;
        }
        ptr += enemyDataSize;
        remaining -= enemyDataSize;

        // Towers
        size_t towerDataSize = snap->towerCount * sizeof(SnapshotTower);
        if (remaining < towerDataSize) return;
        const SnapshotTower *snapTowers = (const SnapshotTower *)ptr;

        for (int i = 0; i < MAX_TOWERS; i++) towers[i].active = false;
        for (int i = 0; i < snap->towerCount && i < MAX_TOWERS; i++) {
            const SnapshotTower *st = &snapTowers[i];
            towers[i].active = true;
            towers[i].id = st->id;
            towers[i].type = (TowerType)st->type;
            towers[i].level = st->level;
            towers[i].gridPos = (GridPos){ st->gridX, st->gridZ };
            towers[i].worldPos = MapGridToWorldElevated(map, towers[i].gridPos);
            towers[i].worldPos.y += 0.35f;
            towers[i].ownerPlayer = st->ownerPlayer;
            towers[i].cooldownTimer = 0.0f;
            // Re-mark map tile
            map->tiles[st->gridZ][st->gridX] = TILE_TOWER;
        }
        ptr += towerDataSize;
        remaining -= towerDataSize;

        // Projectiles
        size_t projDataSize = snap->projectileCount * sizeof(SnapshotProjectile);
        if (remaining < projDataSize) return;
        const SnapshotProjectile *snapProjs = (const SnapshotProjectile *)ptr;

        for (int i = 0; i < MAX_PROJECTILES; i++) projectiles[i].active = false;
        for (int i = 0; i < snap->projectileCount && i < MAX_PROJECTILES; i++) {
            const SnapshotProjectile *sp = &snapProjs[i];
            projectiles[i].active = true;
            projectiles[i].id = sp->id;
            projectiles[i].position.x = sp->posX / 100.0f;
            projectiles[i].position.y = sp->posY / 100.0f;
            projectiles[i].position.z = sp->posZ / 100.0f;
            projectiles[i].targetEnemyID = sp->targetID;
            projectiles[i].ownerPlayer = sp->ownerPlayer;
        }
        break;
    }
    case MSG_CHAT: {
        if (size < sizeof(ChatMsg)) return;
        const ChatMsg *chat = (const ChatMsg *)data;
        if (g_netChatCallback)
            g_netChatCallback(chat->playerIndex, ctx->playerNames[chat->playerIndex], chat->message);
        break;
    }
    default:
        break;
    }
}

// --- Poll ---

void NetPoll(NetContext *ctx, GameState *gs, Enemy enemies[], Tower towers[],
             Projectile projectiles[], Map *map)
{
    if (!ctx->host) return;

    ENetEvent event;
    while (enet_host_service(ctx->host, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            if (ctx->mode == NET_MODE_HOST) {
                // Peer connected, wait for join request
            }
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            if (ctx->mode == NET_MODE_HOST) {
                if (event.packet->dataLength >= sizeof(PacketHeader)) {
                    const PacketHeader *hdr = (const PacketHeader *)event.packet->data;
                    if (hdr->type == MSG_JOIN_REQUEST && event.packet->dataLength >= sizeof(JoinRequest)) {
                        HostHandleJoinRequest(ctx, event.peer, (const JoinRequest *)event.packet->data);
                    } else {
                        HostHandleAction(ctx, event.peer, event.packet->data, event.packet->dataLength,
                                        gs, enemies, towers, projectiles, map);
                    }
                }
            } else {
                ClientHandlePacket(ctx, event.packet->data, event.packet->dataLength,
                                  event.channelID, gs, enemies, towers, projectiles, map);
            }
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            if (ctx->mode == NET_MODE_HOST) {
                HostHandleDisconnect(ctx, event.peer);
            } else {
                // Host disconnected
                ctx->serverPeer = NULL;
            }
            break;
        case ENET_EVENT_TYPE_NONE:
            break;
        }
    }
}

// --- Send Actions (Client) ---

void NetSendPlaceTower(NetContext *ctx, TowerType type, GridPos pos)
{
    ActionPlaceTower act;
    PacketHeaderInit(&act.header, MSG_ACTION_PLACE_TOWER, sizeof(act));
    act.towerType = (uint8_t)type;
    act.gridX = (int16_t)pos.x;
    act.gridZ = (int16_t)pos.z;

    if (ctx->mode == NET_MODE_CLIENT && ctx->serverPeer) {
        NetSendReliable(ctx->serverPeer, &act, sizeof(act));
    }
}

void NetSendUpgradeTower(NetContext *ctx, EntityID towerID)
{
    ActionUpgradeTower act;
    PacketHeaderInit(&act.header, MSG_ACTION_UPGRADE_TOWER, sizeof(act));
    act.towerID = towerID;

    if (ctx->mode == NET_MODE_CLIENT && ctx->serverPeer) {
        NetSendReliable(ctx->serverPeer, &act, sizeof(act));
    }
}

void NetSendGiftGold(NetContext *ctx, uint8_t targetPlayer, int amount)
{
    ActionGiftGold act;
    PacketHeaderInit(&act.header, MSG_ACTION_GIFT_GOLD, sizeof(act));
    act.targetPlayer = targetPlayer;
    act.amount = (int16_t)amount;

    if (ctx->mode == NET_MODE_CLIENT && ctx->serverPeer) {
        NetSendReliable(ctx->serverPeer, &act, sizeof(act));
    }
}

void NetSendChat(NetContext *ctx, const char *message)
{
    ChatMsg msg;
    PacketHeaderInit(&msg.header, MSG_CHAT, sizeof(msg));
    msg.playerIndex = ctx->localPlayerIndex;
    strncpy(msg.message, message, NET_MAX_CHAT_MSG - 1);
    msg.message[NET_MAX_CHAT_MSG - 1] = '\0';

    if (ctx->mode == NET_MODE_CLIENT && ctx->serverPeer) {
        NetSendChat_raw(ctx->serverPeer, &msg, sizeof(msg));
    } else if (ctx->mode == NET_MODE_HOST) {
        // Host: broadcast to clients and invoke local callback
        NetBroadcastChatAll(ctx, &msg, sizeof(msg));
        if (g_netChatCallback)
            g_netChatCallback(msg.playerIndex, ctx->playerNames[msg.playerIndex], msg.message);
    }
}

void NetSendGameStart(NetContext *ctx)
{
    if (ctx->mode != NET_MODE_HOST) return;

    GameStartMsg msg;
    PacketHeaderInit(&msg.header, MSG_GAME_START, sizeof(msg));
    msg.playerCount = (uint8_t)ctx->playerCount;
    memset(msg.mapName, 0, sizeof(msg.mapName));
    strncpy(msg.mapName, ctx->selectedMap, MAX_MAP_NAME - 1);
    NetBroadcastReliable(ctx, &msg, sizeof(msg));
    ctx->inGame = true;
}

void NetSendMapData(NetContext *ctx, const Map *map)
{
    if (ctx->mode != NET_MODE_HOST) return;

    char mapBuf[4096];
    int dataLen = MapSerialize(map, mapBuf, sizeof(mapBuf));
    if (dataLen <= 0) return;

    size_t totalSize = sizeof(MapDataMsg) + dataLen;
    uint8_t *packet = malloc(totalSize);
    if (!packet) return;

    MapDataMsg *msg = (MapDataMsg *)packet;
    PacketHeaderInit(&msg->header, MSG_MAP_DATA, (uint16_t)totalSize);
    memset(msg->mapName, 0, sizeof(msg->mapName));
    strncpy(msg->mapName, map->name, MAX_MAP_NAME - 1);
    msg->dataSize = (uint16_t)dataLen;
    memcpy(packet + sizeof(MapDataMsg), mapBuf, dataLen);

    NetBroadcastReliable(ctx, packet, totalSize);
    free(packet);
}

void NetBroadcastLobbyState(NetContext *ctx)
{
    if (ctx->mode != NET_MODE_HOST) return;

    LobbyStateMsg lm;
    PacketHeaderInit(&lm.header, MSG_LOBBY_STATE, sizeof(lm));
    lm.playerCount = (uint8_t)ctx->playerCount;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        strncpy(lm.players[i].username, ctx->playerNames[i], NET_MAX_USERNAME);
        lm.players[i].connected = ctx->playerConnected[i] ? 1 : 0;
    }
    memset(lm.selectedMap, 0, sizeof(lm.selectedMap));
    strncpy(lm.selectedMap, ctx->selectedMap, MAX_MAP_NAME - 1);
    NetBroadcastReliable(ctx, &lm, sizeof(lm));
}

// --- Snapshot ---

void NetBroadcastSnapshot(NetContext *ctx, GameState *gs, Enemy enemies[],
                          Tower towers[], Projectile projectiles[])
{
    if (ctx->mode != NET_MODE_HOST) return;

    // Count active entities
    int ec = 0, tc = 0, pc = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active) ec++;
    for (int i = 0; i < MAX_TOWERS; i++) if (towers[i].active) tc++;
    for (int i = 0; i < MAX_PROJECTILES; i++) if (projectiles[i].active) pc++;

    size_t totalSize = sizeof(GameStateSnapshot) +
                       ec * sizeof(SnapshotEnemy) +
                       tc * sizeof(SnapshotTower) +
                       pc * sizeof(SnapshotProjectile);

    uint8_t *buf = malloc(totalSize);
    if (!buf) return;

    GameStateSnapshot *snap = (GameStateSnapshot *)buf;
    PacketHeaderInit(&snap->header, MSG_GAME_STATE_SNAPSHOT, (uint16_t)totalSize);
    snap->phase = (uint8_t)gs->phase;
    snap->currentWave = (uint8_t)gs->currentWave;
    snap->lives = (int16_t)gs->lives;
    snap->waveCountdown = gs->waveCountdown;
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        snap->playerGold[i] = (int16_t)gs->playerGold[i];
    snap->enemyCount = (uint8_t)ec;
    snap->towerCount = (uint8_t)tc;
    snap->projectileCount = (uint8_t)pc;

    uint8_t *ptr = buf + sizeof(GameStateSnapshot);

    // Enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        SnapshotEnemy *se = (SnapshotEnemy *)ptr;
        se->id = enemies[i].id;
        se->type = (uint8_t)enemies[i].type;
        se->posX = (int16_t)(enemies[i].worldPos.x * 100.0f);
        se->posZ = (int16_t)(enemies[i].worldPos.z * 100.0f);
        se->hp = (int16_t)(enemies[i].hp * 10.0f);
        se->maxHp = (int16_t)(enemies[i].maxHp * 10.0f);
        se->waypointIndex = (uint8_t)enemies[i].waypointIndex;
        se->pathProgress = (uint8_t)(enemies[i].pathProgress * 255.0f);
        se->slowFactor = (uint8_t)(enemies[i].slowFactor * 255.0f);
        ptr += sizeof(SnapshotEnemy);
    }

    // Towers
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!towers[i].active) continue;
        SnapshotTower *st = (SnapshotTower *)ptr;
        st->id = towers[i].id;
        st->type = (uint8_t)towers[i].type;
        st->level = (uint8_t)towers[i].level;
        st->gridX = (int16_t)towers[i].gridPos.x;
        st->gridZ = (int16_t)towers[i].gridPos.z;
        st->ownerPlayer = towers[i].ownerPlayer;
        ptr += sizeof(SnapshotTower);
    }

    // Projectiles
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        SnapshotProjectile *sp = (SnapshotProjectile *)ptr;
        sp->id = projectiles[i].id;
        sp->posX = (int16_t)(projectiles[i].position.x * 100.0f);
        sp->posY = (int16_t)(projectiles[i].position.y * 100.0f);
        sp->posZ = (int16_t)(projectiles[i].position.z * 100.0f);
        sp->targetID = projectiles[i].targetEnemyID;
        sp->ownerPlayer = projectiles[i].ownerPlayer;
        ptr += sizeof(SnapshotProjectile);
    }

    NetBroadcastUnreliable(ctx, buf, totalSize);
    free(buf);
}

// --- LAN Discovery ---

void NetDiscoveryStart(NetContext *ctx)
{
    ctx->discoverySocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->discoverySocket < 0) return;

    int yes = 1;
    setsockopt(ctx->discoverySocket, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    setsockopt(ctx->discoverySocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // Non-blocking
    int flags = fcntl(ctx->discoverySocket, F_GETFL, 0);
    fcntl(ctx->discoverySocket, F_SETFL, flags | O_NONBLOCK);

    // Bind for receiving (if host)
    if (ctx->mode == NET_MODE_HOST) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(NET_DISCOVERY_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;
        bind(ctx->discoverySocket, (struct sockaddr *)&addr, sizeof(addr));
    }

    ctx->discoveryTimer = 0.0f;
}

void NetDiscoveryStop(NetContext *ctx)
{
    if (ctx->discoverySocket >= 0) {
        close(ctx->discoverySocket);
        ctx->discoverySocket = -1;
    }
}

void NetDiscoveryPoll(NetContext *ctx)
{
    if (ctx->discoverySocket < 0) return;

    // Send broadcast request periodically (client)
    if (ctx->mode != NET_MODE_HOST) {
        ctx->discoveryTimer += GetFrameTime();
        if (ctx->discoveryTimer >= 1.0f) {
            ctx->discoveryTimer = 0.0f;

            DiscoveryRequest req;
            PacketHeaderInit(&req.header, MSG_DISCOVERY_REQUEST, sizeof(req));

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(NET_DISCOVERY_PORT);
            dest.sin_addr.s_addr = INADDR_BROADCAST;
            sendto(ctx->discoverySocket, &req, sizeof(req), 0,
                   (struct sockaddr *)&dest, sizeof(dest));
        }
    }

    // Receive responses
    uint8_t buf[256];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    ssize_t n;

    while ((n = recvfrom(ctx->discoverySocket, buf, sizeof(buf), 0,
                         (struct sockaddr *)&from, &fromLen)) > 0) {
        if ((size_t)n < sizeof(PacketHeader)) continue;
        const PacketHeader *hdr = (const PacketHeader *)buf;

        if (ctx->mode == NET_MODE_HOST && hdr->type == MSG_DISCOVERY_REQUEST) {
            // Respond with our info
            DiscoveryResponse resp;
            PacketHeaderInit(&resp.header, MSG_DISCOVERY_RESPONSE, sizeof(resp));
            strncpy(resp.hostName, ctx->playerNames[0], NET_MAX_USERNAME);
            resp.playerCount = (uint8_t)ctx->playerCount;
            resp.maxPlayers = NET_MAX_PLAYERS;
            resp.inGame = ctx->inGame ? 1 : 0;

            sendto(ctx->discoverySocket, &resp, sizeof(resp), 0,
                   (struct sockaddr *)&from, sizeof(from));
        }
        else if (ctx->mode != NET_MODE_HOST && hdr->type == MSG_DISCOVERY_RESPONSE) {
            if ((size_t)n < sizeof(DiscoveryResponse)) continue;
            const DiscoveryResponse *resp = (const DiscoveryResponse *)buf;
            if (resp->inGame) continue;

            char addrStr[64];
            inet_ntop(AF_INET, &from.sin_addr, addrStr, sizeof(addrStr));

            // Update or add to discovered list
            int idx = -1;
            for (int i = 0; i < g_discoveredGameCount; i++) {
                if (strcmp(g_discoveredGames[i].address, addrStr) == 0) {
                    idx = i;
                    break;
                }
            }
            if (idx < 0 && g_discoveredGameCount < MAX_DISCOVERED_GAMES) {
                idx = g_discoveredGameCount++;
            }
            if (idx >= 0) {
                strncpy(g_discoveredGames[idx].hostName, resp->hostName, NET_MAX_USERNAME);
                strncpy(g_discoveredGames[idx].address, addrStr, sizeof(g_discoveredGames[idx].address));
                g_discoveredGames[idx].playerCount = resp->playerCount;
                g_discoveredGames[idx].maxPlayers = resp->maxPlayers;
                g_discoveredGames[idx].lastSeen = 0.0f;
            }
        }
    }

    // Age out old entries
    float dt = GetFrameTime();
    for (int i = 0; i < g_discoveredGameCount; i++) {
        g_discoveredGames[i].lastSeen += dt;
        if (g_discoveredGames[i].lastSeen > 5.0f) {
            g_discoveredGames[i] = g_discoveredGames[g_discoveredGameCount - 1];
            g_discoveredGameCount--;
            i--;
        }
    }
}
