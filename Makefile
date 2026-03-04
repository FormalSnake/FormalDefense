CC = cc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags raylib) -Ivendor/enet/include
LDFLAGS = $(shell pkg-config --libs raylib)
TARGET = formaldefense
SRCS = main.c game.c entity.c map.c net.c lobby.c chat.c
ENET_SRCS = vendor/enet/callbacks.c vendor/enet/compress.c vendor/enet/host.c \
            vendor/enet/list.c vendor/enet/packet.c vendor/enet/peer.c \
            vendor/enet/protocol.c vendor/enet/unix.c
OBJS = $(SRCS:.c=.o)
ENET_OBJS = $(ENET_SRCS:.c=.o)

$(TARGET): $(OBJS) $(ENET_OBJS)
	$(CC) -o $@ $(OBJS) $(ENET_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) $(ENET_OBJS) hello3d

.PHONY: run clean
