CC      = cc
UNAME_S := $(shell uname -s)
TARGET  = chip8
SRC     = src/main.c

# --- GLFW detection & install ---
GLFW_OK := $(shell pkg-config --exists glfw3 2>/dev/null && echo yes)

ifeq ($(GLFW_OK),yes)
    CFLAGS  += $(shell pkg-config --cflags glfw3)
    LDLIBS  += $(shell pkg-config --libs glfw3)
else
    ifeq ($(UNAME_S),Linux)
        CFLAGS += -I/usr/local/include
        LDLIBS += -L/usr/local/lib -lglfw -lGL -ldl -lpthread
    endif
    ifeq ($(UNAME_S),Darwin)
        CFLAGS += -I/usr/local/include
        LDLIBS += -L/usr/local/lib -lglfw -framework Cocoa -framework OpenGL
    endif
endif

# --- Build ---
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

# --- Install GLFW if missing ---
install-deps:
ifeq ($(GLFW_OK),yes)
	@echo "GLFW already installed."
else
	@echo "GLFW not found. Installing..."
ifeq ($(UNAME_S),Linux)
	@sudo dnf upgrade && sudo dnf install glfw glfw-devel
endif
ifeq ($(UNAME_S),Darwin)
	@brew install glfw
endif
	@echo "Done. Run 'make' again."
endif

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean install-deps
