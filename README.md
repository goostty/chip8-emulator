# chip8-emulator

A CHIP-8 emulator written in C, built as a way to learn the basics of computer
architecture. It uses GLFW for the window/input and OpenGL for the framebuffer.

## Build

```sh
make            # builds ./chip8
make test       # runs the headless opcode/display regression tests
make clean
```

On macOS, GLFW can be installed with `brew install glfw`. The Makefile links the
required OpenGL/Cocoa frameworks automatically.

On Fedora/Linux, `make install-deps` installs `glfw`/`glfw-devel`.

## Run

```sh
./chip8 "roms/IBM Logo.ch8"
./chip8 "roms/your-game.ch8"
```

Any ROM you have the right to use can be placed in `roms/`.

## Controls

| CHIP-8 key | Keyboard |
| ---------- | -------- |
| 1 2 3 C    | 1 2 3 4  |
| 4 5 6 D    | Q W E R  |
| 7 8 9 E    | A S D F  |
| A 0 B F    | Z X C V  |

## Layout

```
src/
  main.c    window, timing, rendering, ROM loading
  chip8.c   CPU, opcodes, timers, input mapping
  chip8.h   Chip8 state and public API
  stack.c   call stack
  stack.h
tests/
  test_chip8.c   headless regression tests
roms/
  IBM Logo.ch8
```

## Notes

- CPU: ~600 instructions/second (10 steps per 60 Hz frame).
- Timers: decremented at 60 Hz.
- Display: 64×32 monochrome, scaled by the largest integer factor that fits and
  centered in the window (Retina-aware via `glfwGetFramebufferSize`).

See [BUGFIXES.md](BUGFIXES.md) for the full list of bugs found while bringing
this up, their root causes, and the lessons for avoiding them.
