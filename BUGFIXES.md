# Bug log: from "doesn't build" to "IBM logo / Tetris ready"

This document records every bug found while getting the emulator to run, what
the symptom was, why it happened, how it was fixed, and what to do differently
next time. It is meant to be read alongside the commit that fixed them.

---

## 1. Build and platform

### 1.1 macOS link failure: `_glClear` / `_glDrawPixels` / `_glRasterPos2i`
**Symptom**
```
Undefined symbols: _glClear, _glDrawPixels, _glRasterPos2i
```
**Cause**
The Makefile used the `pkg-config` branch, which returns only `-lglfw`. On
macOS the OpenGL entry points live in the `OpenGL.framework`, not in `libglfw`.
The fallback branch had the frameworks, so the bug only appeared on machines
where GLFW *was* installed (i.e. every normal dev machine).

**Fix**
Link the frameworks on Darwin regardless of which GLFW branch was used:
```make
ifeq ($(UNAME_S),Darwin)
    LDLIBS += -framework OpenGL -framework Cocoa -framework IOKit
endif
```
**Avoid**
- Test the "dependency is installed" path, not just the fallback path.
- Run `make` in CI (or a fresh shell) after changing build flags.
- Read what `pkg-config --libs` actually prints; don't assume it includes the
  windowing/GL frameworks.

### 1.2 Duplicate symbols `_WIDTH` and `_HEIGHT`
**Symptom**
```
duplicate symbol '_WIDTH' in main.o and chip8.o
```
**Cause**
```c
const uint8_t HEIGHT = 32, WIDTH = 64; // in a header
```
In C (unlike C++), file-scope `const` has **external linkage** by default.
Because the header is included by both `main.c` and `chip8.c`, two definitions
were emitted.

This also caused `-Wgnu-folding-constant`: a `const` variable is not a constant
expression in C, so `display[WIDTH][HEIGHT]` was treated as a variable-length
array.

**Fix**
Use a macro (or `enum`) for compile-time constants:
```c
#define WIDTH 64
#define HEIGHT 32
```
**Avoid**
- In C headers, use `#define`, `enum`, or `static const` (internal linkage).
- Enable `-Wall -Wextra`; the VLA warning was the clue that the constant was not
  a real constant.

### 1.3 Missing/incorrect include guards
**Symptom**
Potential redefinition errors as the project grows; `stack.h` prototypes were
outside the guard.
**Cause**
`chip8.h` had no guard at all, and `stack.h` closed `#endif` before its
prototypes.
**Fix**
Wrap the entire header, including prototypes, in `#ifndef`/`#define`/`#endif`.
**Avoid**
- Treat every header as if it will be included twice.
- Compile a translation unit that includes the header twice as a quick check.

---

## 2. Initialization and memory

### 2.1 Segfault before the window opened
**Symptom**
The program died immediately, before `glfwInit()`.
**Cause**
```c
memset(chip, 0, sizeof(*chip)); // stack == NULL
chip->stack->sp = 0;            // NULL dereference
```
`Chip8.stack` was a pointer that was never allocated.

**Fix**
Store the stack by value:
```c
struct Stack stack;
```
and use `chip->stack.sp`, `push(&chip->stack, ...)`, `pop(&chip->stack)`.
This also removes a leak and a copy hazard.
**Avoid**
- `memset` to zero before initializing owned pointers.
- Prefer embedding small fixed-size objects over `malloc` unless you need
  dynamic size.
- Run under a debugger or ASan when a program dies before doing any work.

### 2.2 Fontset never loaded
**Symptom**
`FX29` (`I = font + digit * 5`) pointed at zeros; score digits rendered blank.
**Cause**
The 80-byte built-in font was not in memory at all.
**Fix**
```c
memcpy(chip->memory + FONT_START, fontset, sizeof(fontset)); // 0x050
```
and use `FONT_START` in `FX29`.
**Avoid**
- Treat the fontset as part of "ROM loading", not as an optional feature.
- Write a test that checks `FX29` returns an address inside the font block.

### 2.3 Oversized / empty ROM handling
**Symptom**
Silent truncation of large ROMs; confusing behavior on an empty file.
**Cause**
`fread` was capped at 4096 bytes and the size check compared the already
truncated length.
**Fix**
Read up to 4096, then explicitly reject `len == 0` and
`len > MEMORY_SIZE - PROGRAM_START`.
**Avoid**
- Check the return value of every `fread`/`fwrite`.
- Distinguish "read zero bytes" from "read everything".

### 2.4 `CLS` cleared 32 bytes, not 2048
**Symptom**
Old pixels survived a clear; screen garbage accumulated.
**Cause**
```c
memset(chip->display, false, sizeof(*chip->display));
```
`*chip->display` is one row (`WIDTH` bytes), not the whole array.
**Fix**
```c
memset(chip->display, 0, sizeof(chip->display));
```
**Avoid**
- `sizeof(array)` for the whole array, `sizeof(array[0])` for one element.
- Say the unit out loud: "I am clearing N bytes."

---

## 3. Display and opcodes

### 3.1 `DXYN` wrote to the wrong memory (and out of bounds)
**Symptom**
IBM logo appeared as a few stripes in the top-left corner; other struct fields
could be corrupted.
**Cause**
```c
uint8_t display[64][32];
*chip->display[(vy + row) * 64 + (vx + col)] ^= 1;
```
Three bugs at once:
1. `display` is 2-D, so `display[i]` is a **row pointer**; `*display[i]` is
   `display[i][0]`. Every pixel went to column 0.
2. The flat index `i` reached ~1464, but there are only 64 rows → OOB write into
   `pc`, `I`, and the rest of the struct.
3. The array was declared `[64][32]` (x-major) while the drawing code assumed
   `[32][64]` (y-major).

**Fix**
```c
uint8_t display[HEIGHT][WIDTH]; // [32][64]

uint8_t x = chip->v[(opcode & 0x0F00) >> 8] % WIDTH;
uint8_t y = chip->v[(opcode & 0x00F0) >> 4] % HEIGHT;

for (row ...) for (col ...) {
  if (spriteByte & (0x80 >> col)) {
    int px = (x + col) % WIDTH;
    int py = (y + row) % HEIGHT;
    if (chip->display[py][px]) chip->v[0xF] = 1;
    chip->display[py][px] ^= 1;
  }
}
```
**Avoid**
- Index 2-D arrays with `[row][col]`, never with a concatenated index unless the
  array is genuinely 1-D.
- Turn on `-fsanitize=address,undefined` during development; this bug would have
  been caught the first time the logo was drawn.
- Keep one coordinate convention (x = column, y = row) and use it everywhere.
- For CHIP-8, remember `DXYN` operands are **register numbers**, not values;
  take `v[X]` and `v[Y]` once and reuse them.

### 3.2 `DXYN` collision flag read after XOR / at the wrong cell
**Symptom**
`VF` unreliable; sprite collision detection broken.
**Cause**
The collision check used the same broken index and could test the wrong pixel.
**Fix**
Read the pixel **before** flipping it, at `display[py][px]`, and set `VF = 1`
only if it was already set.
**Avoid**
- Write the read/modify/write sequence in the order the hardware spec describes.
- Add a regression test: draw same sprite twice, expect pixel off and `VF == 1`.

### 3.3 `DXYN` had no edge handling
**Symptom**
Sprites near the right/bottom edge wrote outside the display.
**Fix**
Modulo-wrap both coordinates per pixel (`% WIDTH`, `% HEIGHT`).
**Avoid**
- CHIP-8 draws at 60+ FPS; an out-of-bounds write may only show up as a random
  crash much later. Fuzz the edges in tests.

### 3.4 `RET` truncated addresses to 8 bits
**Symptom**
Subroutine returns jumped to the wrong place; Tetris broke in strange ways.
**Cause**
```c
uint8_t pop(struct Stack *); // but addresses are 16-bit
```
**Fix**
`uint16_t pop(...)`, and `chip->pc = pop(...) & 0xFFF`.
**Avoid**
- Match the width of the value to its storage and to the hardware's address
  bus. A `pc` is 12-bit; the stack is 16-bit.
- Compile with `-Wconversion` occasionally to catch narrowing.

### 3.5 Shift flags (`8XY6` / `8XYE`)
**Symptom**
Random game logic errors after a shift; `VF` always 0 or wrong.
**Cause**
```c
chip->v[x] >>= 1;
chip->v[0xF] = (chip->v[x] & 0x000F) >> 2; // bit already shifted out
// and:
chip->v[x] <<= 1;
chip->v[0xF] = (chip->v[x] & 0x0F00) >> 8;  // VX is 8-bit: mask is always 0
```
**Fix**
Capture the outgoing bit **before** shifting:
```c
chip->v[0xF] = chip->v[x] & 1;   // SHR
chip->v[x] >>= 1;
chip->v[0xF] = chip->v[x] >> 7;  // SHL, before shift
chip->v[x] <<= 1;
```
**Avoid**
- Read the spec's exact order ("VF = least significant bit of VX **before**
  the shift").
- Test both the bit-set and bit-clear cases.

### 3.6 `EX9E` / `EXA1` skipped three instructions
**Symptom**
Conditional skips jumped over too much code.
**Cause**
`chip8_step` already adds 2 to `pc`; the handlers added another 4 (or 2 in the
non-skip path).
**Fix**
Handlers add only `+2` when the condition is true, and `+0` otherwise.
**Avoid**
- Document the invariant: "when an opcode handler runs, PC already points at the
  next instruction."
- Write tests around every skip opcode.

### 3.7 `F` group fell through all cases
**Symptom**
`FX07` blocked while waiting for a key; timers were overwritten; `FX33` also
wrote `V0..VX` to memory.
**Cause**
Every `case` was missing `break`.
**Fix**
Add `break` to every case (and wrap declarations in braces after a label).
**Avoid**
- Compile with `-Wimplicit-fallthrough` (clang/GCC).
- Make each `case` a small block with its own braces; it makes missing breaks
  visually obvious.
- This is a classic C footgun: consider `switch` only for dense, trivial
  mappings.

### 3.8 `FX65` was implemented as `FX66`
**Symptom**
Loading registers from memory silently did nothing.
**Cause**
The CHIP-8 load opcode is `FX65`; the code had `case 0x66`.
**Fix**
Rename the case to `0x65`.
**Avoid**
- Transcribe opcode tables from a reference and diff against them.
- Add one test per opcode; the typo would have failed immediately.

### 3.9 `CXNN` produced only 0 or 1
**Symptom**
"Random" pieces/positions were almost always the first value.
**Cause**
```c
rand() % sizeof(uint16_t) // == rand() % 2
```
**Fix**
`rand() % 256`, and seed once in `chip8_init`, not on every opcode.
**Avoid**
- Don't use `sizeof` where you mean a value range.
- Check a random function's output range with a quick histogram.

### 3.10 `BNNN` was off by one instruction
**Symptom**
Jumps computed from `V0` landed 2 bytes late.
**Cause**
`pc += nnn + V0` adds to the already-advanced PC.
**Fix**
`pc = (nnn + V0) & 0xFFF`.
**Avoid**
- For jumps, assign `pc`; for skips, add to `pc`. Keep those two categories
  separate in your head.

### 3.11 `FX1E` overflow check and 12-bit I
**Symptom**
`I` could exceed the addressable 0xFFF range; `VF` semantics were ambiguous.
**Fix**
```c
uint16_t sum = chip->I + chip->v[x];
chip->v[0xF] = sum > 0xFFF;
chip->I = sum & 0xFFF;
```
**Avoid**
- Mask addresses after arithmetic.
- Look up the specific overflow rule; implementations differ.

---

## 4. Rendering

### 4.1 Pixels were 0/1, so the screen was black
**Symptom**
Window opened, but the logo was invisible (very dark gray at best).
**Cause**
`glDrawPixels(..., GL_LUMINANCE, ..., display)` treats `1` as `1/255`
intensity. The emulator stores `0/1`.
**Fix**
Expand to `0/255` in a staging buffer before drawing:
```c
pixels[y][x] = chip->display[y][x] ? 255 : 0;
```
**Avoid**
- Know the units of the data you hand to a graphics API.
- Keep emulator state (booleans) separate from presentation (colors).

### 4.2 64×32 was drawn at 1:1
**Symptom**
A tiny image in the corner of an 800×600 window.
**Fix**
Pick the largest integer scale that fits the framebuffer, then center:
```c
int scale = fb_width / WIDTH;
if (fb_height / HEIGHT < scale) scale = fb_height / HEIGHT;

glPixelZoom((float)scale, (float)-scale);
glWindowPos2i((fb_width - WIDTH * scale) / 2, ...);
```
**Avoid**
- Use a scale factor derived from the actual framebuffer size rather than magic
  numbers.
- Keep the aspect ratio: 64:32 = 2:1.

### 4.3 Image was upside down
**Symptom**
Display row 0 appeared at the bottom.
**Cause**
OpenGL's raster origin is bottom-left; the CHIP-8 framebuffer's row 0 is the
top.
**Fix**
Negative Y zoom and a raster position at the top-left of the destination:
```c
glPixelZoom(scale, -scale);
glRasterPos2i(x, y_top);
```
**Avoid**
- Write down the origin/orientation of every coordinate system (CHIP-8 display,
  GL window, texture).
- A one-pixel test sprite in a corner makes orientation bugs obvious.

### 4.4 OpenGL 3.0 hint removed `glDrawPixels`
**Symptom**
Potential blank window / invalid operation on macOS.
**Cause**
`glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)` requests a core-profile-era
context on macOS, where `glDrawPixels`/`GL_LUMINANCE` are not available.
**Fix**
Request legacy 2.1 (or drop the hint) when using `glDrawPixels`:
```c
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
```
**Avoid**
- Pick the rendering API first, then the context version.
- If you need core 3.3+, render a textured quad with shaders (more code, but
  future-proof and portable).

### 4.5 Deprecation warnings
**Fix**
`#define GL_SILENCE_DEPRECATION` before including `OpenGL/gl.h` on macOS.
**Avoid**
- Silence only the warning you have consciously accepted; don't blanket-disable
  deprecations you haven't evaluated.

### 4.6 Black window: the raster position was clipped
**Symptom**
The window opened and the program ran, but the content was completely dark for
both the IBM logo and Tetris. `glGetError()` reported `GL_NO_ERROR`.

**Cause**
```c
glRasterPos2i(80, 460); // with the default identity matrices
```
`glRasterPos*` takes **object coordinates**, which are transformed by the
current modelview/projection matrices. With the default identity matrices,
valid object coordinates are NDC `[-1, 1]`. `(80, 460)` is far outside that
range, so the raster position was marked invalid and `glDrawPixels` silently
drew nothing. Nothing to do with the ROM emulation.

**Fix**
Use `glWindowPos2i` (GL 1.4+), which takes framebuffer coordinates directly and
bypasses the transformation pipeline. Also query the **framebuffer** size, not
the window size, because Retina displays report 2× dimensions, then pick the
largest integer scale that fits and center the image:
```c
int fb_width, fb_height;
glfwGetFramebufferSize(window, &fb_width, &fb_height);
glViewport(0, 0, fb_width, fb_height);

int scale = fb_width / WIDTH;
if (fb_height / HEIGHT < scale) scale = fb_height / HEIGHT;
if (scale < 1) scale = 1;

glPixelZoom((float)scale, (float)-scale); // -Y so row 0 is at the top
glWindowPos2i((fb_width - WIDTH * scale) / 2,
              (fb_height + HEIGHT * scale) / 2);
glDrawPixels(WIDTH, HEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
```
**Avoid**
- `glRasterPos` is not a "draw at pixel (x, y)" call in a default GL context.
  Either set an orthographic projection first or use `glWindowPos`.
- `glGetError` does **not** report a clipped raster position, so this fails
  silently. Verify with `glReadPixels` (read the back buffer and count non-zero
  pixels) when the screen looks empty.
- Window size ≠ framebuffer size. On Retina/Metal, use
  `glfwGetFramebufferSize` for viewport and pixel coordinates.
- When a visual feature fails, test the render path in isolation (draw a solid
  white quad or all-white `glDrawPixels`) before blaming the emulator core.

---

## 5. Timing and execution speed

### 5.1 Timers never decremented
**Symptom**
Delay timer never reached zero; Tetris animation/gravity froze.
**Cause**
```c
while (...) {
  uint32_t last_time = get_ticks_ms(); // reset every iteration
  uint32_t now = get_ticks_ms();
  accumulator += now - last_time;      // ~0
  ...
}
```
`last_time` was local to the loop, so the delta was always ≈ 0.

**Fix**
Keep the timestamp across iterations (already had a field for it):
```c
chip->last_time = get_ticks_ms(); // before the loop
...
uint32_t now = get_ticks_ms();
chip->accumulator += now - chip->last_time;
chip->last_time = now;
```
Decrement timers at 60 Hz, not once per CPU instruction.
**Avoid**
- Time deltas require state that survives the loop iteration.
- Clamp the accumulator to avoid a huge catch-up after a debugger pause.
- Use `uint32_t` subtraction: unsigned wrap gives the correct delta.

### 5.2 CPU ran at thousands of instructions per second
**Symptom**
IBM logo flickered; Tetris unplayably fast.
**Cause**
No instruction budget per frame.
**Fix**
`STEPS_PER_FRAME 10` at 60 Hz ≈ 600 instructions/sec (typical CHIP-8 range is
~500–1000). Timers stay at 60 Hz.
**Avoid**
- Separate CPU frequency, timer frequency, and render frequency in your head;
  they are three different clocks.
- Make the budget a named constant so it can be tuned per ROM.

---

## 6. Input

### 6.1 Keypad mapping was out of bounds and shifted
**Symptom**
Most keys did nothing; some keys mapped to the wrong CHIP-8 key; undefined
behavior from reading past an 8-element array.
**Cause**
```c
int glfw_keys[] = {8 entries};
for (int byte = 1; byte <= 0xF; ++byte)
  chip->key[byte] = glfwGetKey(window, glfw_keys[byte]) == GLFW_PRESS;
```
Indices 8..15 read past the array, index 0 was never set, and the layout was not
the CHIP-8 keypad.

**Fix**
One 16-entry table, index = CHIP-8 key:
```c
static const int keymap[16] = {
    GLFW_KEY_X, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, // 0 1 2 3
    GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_E, GLFW_KEY_A, // 4 5 6 7
    GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_Z, GLFW_KEY_C, // 8 9 A B
    GLFW_KEY_4, GLFW_KEY_R, GLFW_KEY_F, GLFW_KEY_V  // C D E F
};
```
**Avoid**
- Draw the keypad on paper and tick off all 16 entries.
- Prefer a table whose size is explicit (`keymap[16]`), and compile with
  `-fsanitize=bounds`.
- If two arrays must stay in sync, derive one from the other or assert their
  sizes.

### 6.2 `FX0A` stored a raw GLFW keycode
**Symptom**
Waiting for a key stored values like 87 (GLFW `W`) instead of 5.
**Cause**
`key_callback` wrote `key` directly into `pressed_key`.
**Fix**
Map via the same table in the callback, latch the CHIP-8 index, consume it in
`FX0A` (`pressed_key = -1`), and rewind `pc` by 2 when no key is ready.
**Avoid**
- Never let framework-specific values cross into emulator state.
- Convert at the boundary (callback) once.
- Make the "wait" non-blocking; blocking inside an opcode prevents timers and
  window events from running.

---

## 7. Tooling and repo hygiene

- **Empty test file.** `tests/test_chip8.c` had no tests. Added a headless
  regression suite (`make test`) covering display, ALU flags, skips, stack,
  font, BCD, store/load, input, and timers.
- **Committed Linux binary.** `my_game` (x86-64 ELF) was tracked in a macOS
  repo. Removed and added to `.gitignore` along with `chip8`, `*.o`, and
  `tests/run_tests`.
- **Hardcoded ROM path.** `../roms/IBM Logo.ch8` only worked from one directory.
  Replaced with `argv[1]` and a usage message.
- **Missing error handling.** `malloc`, `chip8_init`, and `fopen` results are
  now checked; every early exit releases what it owns.
- **No bounds checks on `pc`/`I`.** `chip8_step` masks `pc` to 12 bits and
  wraps operand fetches; memory accesses mask to 0xFFF.
- **Stack over/underflow was silent.** Added diagnostics and a safe return
  value instead of reading/writing outside the 16-entry array.

---

## 8. How to avoid these classes of bugs

1. **Compile like you mean it.**
   `-std=c11 -Wall -Wextra` is the minimum. For emulator work, add
   `-fsanitize=address,undefined,bounds` in a separate debug target. Most of
   the bugs above (NULL deref, OOB display write, stack OOB) would have been
   caught immediately.

2. **Write one test per opcode.**
   The empty test file was the biggest missed opportunity. Opcodes are pure
   functions of `(Chip8*, opcode)`; test them directly without opening a
   window. `make test` now does this in under a second.

3. **Keep an invariant comment on `chip8_step`.**
   "PC has already advanced by 2 when a handler runs." Every skip/jump bug came
   from forgetting this. Documenting the invariant once prevents a whole class
   of off-by-2 errors.

4. **Separate the three clocks.**
   CPU (instructions/sec), timers (60 Hz), and rendering (60 Hz) are different.
   Never decrement timers per instruction; never step the CPU per timer tick.

5. **Use the type that matches the hardware.**
   Addresses are 12-bit; registers are 8-bit; the stack stores 16-bit return
   addresses. A `uint8_t` return address silently works in small tests and
   fails in real ROMs.

6. **Index 2-D arrays with `[row][col]` only.**
   If you catch yourself writing `arr[y * width + x]`, either make the array
   genuinely 1-D or stop and use `arr[y][x]`. Unit-test the corners.

7. **Test the ROM, not just the build.**
   A headless harness that runs N steps and dumps the framebuffer (see the
   verification section) would have shown the logo bug without a window. Keep
   one around for every ROM you debug.

8. **Never trust framework values inside the core.**
   Convert GLFW keycodes, timestamps, and window handles at the boundary.
   The `Chip8` struct should not know what GLFW is, apart from the update
   function that is explicitly about input.

9. **Read the spec's exact ordering.**
   The shift-flag bugs and the `DXYN` collision bug were both "did it in the
   wrong order" bugs. Write the spec sentence next to the code.

10. **Look at the actual output.**
    Dump the framebuffer as ASCII before adding a renderer. If the image is
    wrong in ASCII, no amount of OpenGL will save it.

---

## 9. Verification performed

```console
$ make
cc ... -o chip8 ...
$ make test
Running CHIP-8 tests...
All tests passed.
```

Headless IBM logo run after 60 instructions reaches the ROM's idle loop
(`pc=0x228`) and produces the expected 8-band IBM logo in the framebuffer.

The windowed binary was run for 3 seconds with `roms/IBM Logo.ch8`; it opened a
2.1 GL context, rendered, and exited only because the test alarm killed it
(no crash, no GL error).

## 10. Known limitations / not implemented

These were out of scope for the bug-fix pass but are worth knowing:

- **Sound**: `sound_timer` ticks down but no audio is emitted. Add a small
  square-wave generator (e.g. miniaudio/SDL) if you want the Tetris beep.
- **SCHIP / CHIP-48 extensions**: 128×64 display, `00CN` scroll, `00FB` etc.
  The current core is plain CHIP-8.
- **`8XY6`/`8XYE` operand semantics**: implemented as classic CHIP-8
  (`VX = VX >> 1`). CHIP-48 shifts `VY` into `VX`; if a specific ROM behaves
  oddly, add a quirk flag.
- **`FX55`/`FX65`**: keep `I` unchanged (modern behavior). Original CHIP-8
  increments `I` by `X + 1`; again, a quirk flag is the clean way to support
  both.

---

## 11. Appendix: the codebase, explained as if you wrote it

This is the design rationale for the final code — not "what was fixed" but
"why the surviving design is shaped this way." Read it as the notes you would
have made while writing each part.

### 11.1 Module map and data flow

```
main.c      window, event loop, three clocks, rendering        (the "host")
chip8.c     CPU fetch/decode/execute, opcode handlers, input    (the "core")
stack.c     call stack, nothing else
chip8.h     the one shared header: state + public API
tests/      headless opcode/display regression tests
```

One frame of the loop in `main` is:

```
poll events → refresh key state → accumulate wall-clock time
→ tick_timers() once per 16 ms bucket → chip8_step() × 10
→ convert 0/1 framebuffer to 0/255 → GL blit → swap buffers
```

The core (`chip8.c`) never knows about rendering, and rendering never touches
`Chip8` state except reading `display`. The only GLFW dependency in `chip8.c`
is input, isolated in `chip8_keys_update` and `key_callback`.

### 11.2 The `Chip8` struct

```c
uint8_t memory[4096];        // 12-bit address space
uint8_t display[HEIGHT][WIDTH]; // [32][64], row-major, row 0 = top
struct Stack stack;          // by value: one owner, no malloc/free
uint16_t pc;                 // masked to 12 bits everywhere
uint16_t I;                  // also 12-bit
uint8_t delay_timer, sound_timer; // decremented at 60 Hz, never per opcode
uint8_t v[16];               // V0..VF, VF is special (flags)
uint8_t key[16];             // polled key state (for EX9E/EXA1)
uint32_t last_time, accumulator;  // host-side timing bookkeeping
```

Decisions:

- **Memory map**: `0x000–0x04F` reserved, `0x050` = 80-byte fontset, `0x200` =
  program start. `FX29` therefore computes `I = FONT_START + digit * 5`, and
  `chip8_init` installs the font *before* the ROM is copied, so a ROM that
  overwrites the font area is its own bug, not a mystery.
- **Display is `[32][64]`** (y-major) because the natural blit to
  `glDrawPixels` wants rows first, and the drawing code then uses `[y][x]`
  everywhere. The transposed declaration was one of the three ways `DXYN` was
  broken.
- **Stack embedded, not pointed at.** A `malloc`ed stack leaks, has to be
  threaded through every cleanup path, and enables aliasing bugs. `memset` to
  zero plus a by-value struct removes the whole class of "owned pointer"
  problems in this codebase.
- **`pc` is masked to 12 bits (`& 0xFFF`)** at every assignment point. The
  address bus is 12 bits; letting `pc` exceed 0xFFF would read out of bounds
  later. Masking at the boundary (in `chip8_step`, in jump/return handlers) is
  cheaper than checking bounds at every read.

### 11.3 The invariant that prevents a whole class of bugs

```c
void chip8_step(Chip8 *chip) {
  uint16_t pc = chip->pc & 0xFFF;
  uint16_t opcode = (chip->memory[pc] << 8) | chip->memory[(pc + 1) & 0xFFF];
  chip->pc = (pc + 2) & 0xFFF;   // advance FIRST
  table[opcode >> 12](chip, opcode); // then dispatch
}
```

**Invariant: when a handler runs, `pc` already points at the next
instruction.** Therefore:

- *Jumps* (`1NNN`, `2NNN`, `BNNN`, `00EE`) **assign** `pc`.
- *Conditional skips* (`3XNN`, `4XNN`, `5XY0`, `9XY0`, `EX9E`, `EXA1`) **add
  2** to `pc` when the condition is true, and add nothing otherwise.
- `FX0A` **subtracts 2** to re-run itself when no key is available.

Every skip/jump bug in the log was a violation of this invariant. That is why
the invariant is the single most important sentence in the codebase.

Why a dispatch table indexed by the top nibble, instead of a `switch` in one
giant function: each group is a separate function, testable on its own, and
`table[opcode >> 12]` is a one-instruction indirect call. The cost is that
function names (`op_group8`) encode grouping rather than semantics — the
spec-name comment in each handler is what you actually read.

### 11.4 Drawing (`DXYN`), the one opcode where every bug lived

```c
uint8_t x = chip->v[(opcode & 0x0F00) >> 8] % WIDTH;
uint8_t y = chip->v[(opcode & 0x00F0) >> 4] % HEIGHT;
chip->v[0xF] = 0;
for (row < height) {
  uint8_t spriteByte = chip->memory[(chip->I + row) & 0xFFF];
  for (col < 8) {
    if (spriteByte & (0x80 >> col)) {         // MSB first
      int px = (x + col) % WIDTH;             // wrap per pixel
      int py = (y + row) % HEIGHT;
      if (chip->display[py][px]) chip->v[0xF] = 1; // read BEFORE xor
      chip->display[py][px] ^= 1;
    }
  }
}
```

Three rules to keep straight, all learned the hard way:

1. `x`/`y` are **register numbers** in the encoding, so take `v[X]`/`v[Y]`
   once. Every other use of `v` in this handler must be a *value*, or you are
   indexing the register file with a value (e.g. `V12` when you meant the
   pixel at x=12).
2. Wrap **per pixel**, not per sprite: pre-modding `x` once does not help a
   sprite that crosses the edge.
3. Collision is "was the target pixel set before I wrote it", so read first,
   xor second. The order is the spec, not a style choice.

### 11.5 Input: one table, two consumers

```c
static const int keymap[16] = {
    GLFW_KEY_X, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, // 0 1 2 3
    GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_E, GLFW_KEY_A, // 4 5 6 7
    GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_Z, GLFW_KEY_C, // 8 9 A B
    GLFW_KEY_4, GLFW_KEY_R, GLFW_KEY_F, GLFW_KEY_V  // C D E F
};
```

Index = CHIP-8 key, value = GLFW key. Two different consumers need two
different *kinds* of input, which is why the design has both:

- `chip8_keys_update` polls **level** (is it down right now) into
  `chip->key[16]` — that is what `EX9E`/`EXA1` need.
- `key_callback` latches **edges** (a fresh press) into the file-static
  `pressed_key` — that is what `FX0A` needs. `FX0A` consumes it and resets to
  `-1`, so a stale press cannot fire twice, and it rewinds `pc` instead of
  blocking. Blocking inside an opcode would freeze timers and the event loop.

The boundary rule: GLFW keycodes are converted at the callback, never stored.
The core only ever sees `0..15`.

### 11.6 The three clocks

- **CPU**: `STEPS_PER_FRAME = 10` at 60 Hz ≈ 600 instructions/sec (the
  classic range is ~500–1000).
- **Timers**: exactly 60 Hz, driven by a millisecond accumulator:

```c
chip->accumulator += now - chip->last_time;
chip->last_time = now;
if (chip->accumulator > 100) chip->accumulator = 100; // no spiral of death
while (chip->accumulator >= FRAME_MS) { tick_timers(chip); chip->accumulator -= FRAME_MS; }
```

  The accumulator pattern exists so timers tick at real 60 Hz regardless of
  how long the frame took, and the cap stops a debugger pause from causing a
  10-minute catch-up. `last_time` lives in the struct — the bug was having it
  as a local, which made the delta always ≈ 0.
- **Render**: 1 frame per loop iteration, paced by vsync
  (`glfwSwapInterval(1)`).

The mistake to avoid: conflating any two of these. Decrementing a timer per
CPU instruction makes game speed depend on CPU frequency; stepping the CPU per
timer tick makes it 60/sec.

### 11.7 Rendering: the framebuffer is not the window

- `chip->display` stores `0/1`. GL wants `0/255` for `GL_LUMINANCE`, so a
  staging buffer `pixels[HEIGHT][WIDTH]` converts every frame. Emulator state
  stays booleans; presentation concerns stay in `main.c`.
- `glWindowPos2i`, not `glRasterPos2i`: raster coordinates go through the
  projection (identity ⇒ valid range is NDC `[-1, 1]`, so pixel coordinates
  get clipped silently — `glGetError` stays clean). Window-position
  coordinates are framebuffer pixels.
- `glfwGetFramebufferSize`, not `glfwGetWindowSize`: on Retina the
  framebuffer is 2×. All GL pixel coordinates are in framebuffer pixels.
- `glPixelZoom(scale, -scale)` with negative Y so display row 0 lands at the
  top; GL's raster origin is bottom-left.
- Integer scale keeps each CHIP-8 pixel a solid block (no nearest-neighbor
  shimmer).
- `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)`: the default is 4, and a 64-wide
  row happens to be 4-aligned, but stating it explicitly means a 128×64
  (SCHIP) or any odd-width mode will not silently skew.
- `GL_SILENCE_DEPRECATION`: the legacy `glDrawPixels` path is deprecated on
  macOS but works; the alternative is a texture + shader quad, which was
  judged not worth the complexity for a 64×32 blit.

### 11.8 Tests, and what they are for

`tests/test_chip8.c` calls opcode handlers directly on a zeroed `Chip8` — no
window, no ROM, no GLFW event loop. Each test pins one *spec sentence*:

- `00EE` restores the **full 16-bit** return address (the `uint8_t pop` bug).
- `8XY6`/`8XYE` set `VF` from the bit **before** the shift (both directions,
  set and clear).
- `DXYN` writes to `(x, y)`, reports collision on the second draw, and wraps
  at the right edge.
- `EX9E`/`EXA1` skip exactly one instruction.
- `FX29` lands inside the font block; `FX33` writes hundreds/tens/ones.
- `FX55`/`FX65` leave `I` unchanged.
- `FX0A` rewinds `pc` when no key is latched, stores the **CHIP-8** key index
  when one is, and does not rewind after consuming.
- `BNNN` jumps to `NNN + V0`; timers stop at zero.

The value of these is not coverage percentage: it is that the suite runs in
under a second, headless, so every fix can be proven without opening a window
or eyeballing a framebuffer.
