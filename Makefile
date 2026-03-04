CC = cc
CFLAGS = $(shell pkg-config --cflags raylib)
LDFLAGS = $(shell pkg-config --libs raylib)
TARGET = hello3d

$(TARGET): main.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
