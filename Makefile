CC = cc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags raylib) -Ivendor/enet/include
LDFLAGS = $(shell pkg-config --libs raylib)
TARGET = formaldefense
SRCS = main.c scene.c scene_menu.c scene_mapselect.c scene_difficulty.c scene_shop.c \
       scene_perkselect.c scene_lobby.c scene_game.c \
       game.c entity.c map.c net.c lobby.c chat.c settings.c progress.c ui.c
ENET_SRCS = vendor/enet/callbacks.c vendor/enet/compress.c vendor/enet/host.c \
            vendor/enet/list.c vendor/enet/packet.c vendor/enet/peer.c \
            vendor/enet/protocol.c vendor/enet/unix.c
OBJS = $(SRCS:.c=.o)
ENET_OBJS = $(ENET_SRCS:.c=.o)

$(TARGET): $(OBJS) $(ENET_OBJS)
	$(CC) -o $@ $(OBJS) $(ENET_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

EDITOR_TARGET = editor
EDITOR_SRCS = editor.c map.c
EDITOR_OBJS = $(EDITOR_SRCS:.c=.o)

run: $(TARGET)
	./$(TARGET)

$(EDITOR_TARGET): $(EDITOR_OBJS)
	$(CC) -o $@ $(EDITOR_OBJS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(EDITOR_TARGET) $(OBJS) $(ENET_OBJS) $(EDITOR_OBJS) hello3d

.PHONY: run clean editor
