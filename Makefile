CC = cc
CFLAGS = -Wall -Wextra -Ivendor/sokol -Ivendor/hmm -Ivendor/enet/include -Ishaders
OBJCFLAGS = $(CFLAGS) -fobjc-arc
FRAMEWORKS = -framework Metal -framework MetalKit -framework Cocoa -framework QuartzCore -framework AudioToolbox

TARGET = formaldefense
SRCS = main.c game.c entity.c map.c net.c lobby.c chat.c fd_gfx_sokol.c fd_input_sokol.c
OBJC_SRCS = fd_app_sokol.m
ENET_SRCS = vendor/enet/callbacks.c vendor/enet/compress.c vendor/enet/host.c \
            vendor/enet/list.c vendor/enet/packet.c vendor/enet/peer.c \
            vendor/enet/protocol.c vendor/enet/unix.c
OBJS = $(SRCS:.c=.o)
OBJC_OBJS = $(OBJC_SRCS:.m=.o)
ENET_OBJS = $(ENET_SRCS:.c=.o)

# Shader compiler
SHDC = vendor/sokol/sokol-shdc
SHADER_DIR = shaders

$(TARGET): $(OBJS) $(OBJC_OBJS) $(ENET_OBJS)
	$(CC) -o $@ $(OBJS) $(OBJC_OBJS) $(ENET_OBJS) $(FRAMEWORKS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.m
	$(CC) $(OBJCFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

# --- Editor ---
EDITOR_TARGET = editor
EDITOR_C_SRCS = editor.c map.c fd_gfx_sokol.c fd_input_sokol.c
EDITOR_OBJC_SRCS = editor_app_sokol.m
EDITOR_C_OBJS = $(EDITOR_C_SRCS:.c=.o)
EDITOR_OBJC_OBJS = $(EDITOR_OBJC_SRCS:.m=.o)

$(EDITOR_TARGET): $(EDITOR_C_OBJS) $(EDITOR_OBJC_OBJS)
	$(CC) -o $@ $(EDITOR_C_OBJS) $(EDITOR_OBJC_OBJS) $(FRAMEWORKS)

editor: $(EDITOR_TARGET)

# --- Shaders ---
shaders: $(SHADER_DIR)/ps1.glsl.h $(SHADER_DIR)/fullscreen.glsl.h $(SHADER_DIR)/unlit.glsl.h

$(SHADER_DIR)/%.glsl.h: $(SHADER_DIR)/%.glsl
	$(SHDC) --input $< --output $@ --slang metal_macos

clean:
	rm -f $(TARGET) $(EDITOR_TARGET) $(OBJS) $(OBJC_OBJS) $(ENET_OBJS) \
	      $(EDITOR_C_OBJS) $(EDITOR_OBJC_OBJS)

.PHONY: run clean editor shaders
