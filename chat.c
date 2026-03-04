#include "chat.h"
#include "entity.h"
#include "fd_gfx.h"
#include "fd_input.h"
#include "fd_app.h"
#include <string.h>
#include <stdio.h>

void ChatStateInit(ChatState *cs)
{
    memset(cs, 0, sizeof(*cs));
}

void ChatAddMessage(ChatState *cs, uint8_t playerIndex, const char *username, const char *message)
{
    int idx = cs->head;
    cs->head = (cs->head + 1) % CHAT_MAX_ENTRIES;
    if (cs->count < CHAT_MAX_ENTRIES) cs->count++;

    ChatEntry *e = &cs->entries[idx];
    e->playerIndex = playerIndex;
    strncpy(e->username, username, NET_MAX_USERNAME - 1);
    e->username[NET_MAX_USERNAME - 1] = '\0';
    strncpy(e->message, message, NET_MAX_CHAT_MSG - 1);
    e->message[NET_MAX_CHAT_MSG - 1] = '\0';
    e->age = 0.0f;
}

void ChatUpdate(ChatState *cs, float dt)
{
    for (int i = 0; i < cs->count; i++) {
        int idx = (cs->head - cs->count + i + CHAT_MAX_ENTRIES) % CHAT_MAX_ENTRIES;
        cs->entries[idx].age += dt;
    }
}

bool ChatHandleInput(ChatState *cs, NetContext *ctx)
{
    if (!cs->inputActive) {
        if (IsKeyPressed(KEY_T) && ctx->mode != NET_MODE_NONE) {
            cs->inputActive = true;
            cs->inputLen = 0;
            cs->inputBuf[0] = '\0';
            return true;
        }
        return false;
    }

    // Chat input is active
    if (IsKeyPressed(KEY_ESCAPE)) {
        cs->inputActive = false;
        return true;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (cs->inputLen > 0) {
            NetSendChat(ctx, cs->inputBuf);
        }
        cs->inputActive = false;
        return true;
    }

    // Text input
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (cs->inputLen < NET_MAX_CHAT_MSG - 1 && ch >= 32 && ch < 127) {
            cs->inputBuf[cs->inputLen++] = (char)ch;
            cs->inputBuf[cs->inputLen] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && cs->inputLen > 0) {
        cs->inputLen--;
        cs->inputBuf[cs->inputLen] = '\0';
    }

    return true; // Suppress other input while typing
}

void ChatDraw(const ChatState *cs, int screenW, int screenH)
{
    (void)screenW;
    int chatX = 10;
    int chatBaseY = screenH - 80; // Above bottom bar
    int lineH = 18;

    // Draw recent messages (from oldest to newest)
    int shown = 0;
    for (int i = 0; i < cs->count; i++) {
        int idx = (cs->head - cs->count + i + CHAT_MAX_ENTRIES) % CHAT_MAX_ENTRIES;
        const ChatEntry *e = &cs->entries[idx];

        // Fade after CHAT_FADE_TIME, hide after CHAT_FADE_TIME + 2
        float alpha = 1.0f;
        if (!cs->inputActive) {
            if (e->age > CHAT_FADE_TIME + 2.0f) continue;
            if (e->age > CHAT_FADE_TIME)
                alpha = 1.0f - (e->age - CHAT_FADE_TIME) / 2.0f;
        }

        int y = chatBaseY - (cs->count - i) * lineH;
        if (y < 40) continue;

        Color nameCol = PLAYER_COLORS[e->playerIndex % MAX_PLAYERS];
        nameCol.a = (unsigned char)(alpha * 255);
        Color msgCol = WHITE;
        msgCol.a = (unsigned char)(alpha * 255);

        DrawText(TextFormat("[%s]: %s", e->username, e->message),
                chatX, y, 14, msgCol);
        // Draw name part with player color
        char namePart[32];
        snprintf(namePart, sizeof(namePart), "[%s]", e->username);
        DrawText(namePart, chatX, y, 14, nameCol);
        shown++;
    }

    // Draw input box
    if (cs->inputActive) {
        int inputY = chatBaseY + 4;
        FdDrawRect(chatX, inputY, 400, 22, (Color){ 0, 0, 0, 180 });
        FdDrawRectLines(chatX, inputY, 400, 22, (Color){ 100, 200, 100, 200 });
        FdDrawText(FdTextFormat("> %s_", cs->inputBuf), chatX + 4, inputY + 3, 14, WHITE);
    } else if (shown == 0) {
        // Show hint
    }
}
