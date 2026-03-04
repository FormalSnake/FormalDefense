#ifndef CHAT_H
#define CHAT_H

#include "net.h"
#include <stdbool.h>

#define CHAT_MAX_ENTRIES 20
#define CHAT_FADE_TIME 8.0f

typedef struct {
    char username[NET_MAX_USERNAME];
    char message[NET_MAX_CHAT_MSG];
    float age;
    uint8_t playerIndex;
} ChatEntry;

typedef struct {
    ChatEntry entries[CHAT_MAX_ENTRIES];
    int count;
    int head;  // circular buffer write index

    bool inputActive;
    char inputBuf[NET_MAX_CHAT_MSG];
    int inputLen;
} ChatState;

void ChatStateInit(ChatState *cs);
// Returns true if chat is consuming input (suppress other input)
bool ChatHandleInput(ChatState *cs, NetContext *ctx);
void ChatAddMessage(ChatState *cs, uint8_t playerIndex, const char *username, const char *message);
void ChatDraw(const ChatState *cs, int screenW, int screenH);
void ChatUpdate(ChatState *cs, float dt);

#endif
