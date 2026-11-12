CC      = cc
UNAME_S := $(shell uname -s)
TARGET  = chip8
SRC     = src/main.c src/chip8.c src/stack.c
TEST_SRC = tests/test_chip8.c src/chip8.c src/stack.c

CFLAGS  += -std=c11 -Wall -Wextra -O2

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
        LDLIBS += -L/usr/local/lib -lglfw
    endif
endif

# pkg-config only returns -lglfw, so OpenGL and Cocoa must be linked
# explicitly on macOS no matter which branch was taken above.
ifeq ($(UNAME_S),Darwin)
    LDLIBS += -framework OpenGL -framework Cocoa -framework IOKit
endif

# --- Build ---
all: $(TARGET)

$(TARGET): $(SRC) src/chip8.h src/stack.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

# --- Headless regression tests ---
test: $(TEST_SRC) src/chip8.h src/stack.h
	$(CC) $(CFLAGS) -Isrc -o tests/run_tests $(TEST_SRC) $(LDLIBS)
	./tests/run_tests

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
	rm -f $(TARGET) *.o tests/run_tests

.PHONY: all test clean install-deps
