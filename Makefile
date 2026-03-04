CC = cc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags raylib)
LDFLAGS = $(shell pkg-config --libs raylib)
TARGET = formaldefense
SRCS = main.c game.c entity.c map.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) hello3d

.PHONY: run clean
