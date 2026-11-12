#include "chip8.h"
#include <stdio.h>

static int failures = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s\n", msg);                                             \
      failures++;                                                              \
    }                                                                          \
  } while (0)

static void fresh(Chip8 *c) {
  memset(c, 0, sizeof(*c));
  c->pc = PROGRAM_START;
}

static void test_cls(void) {
  Chip8 c;
  fresh(&c);
  c.display[31][63] = 1;
  c.display[0][0] = 1;
  op_group0(&c, 0x00E0);
  CHECK(c.display[31][63] == 0 && c.display[0][0] == 0,
        "00E0 clears the entire display");
}

static void test_call_ret(void) {
  Chip8 c;
  fresh(&c);
  c.pc = 0x234;
  op_group2(&c, 0x2345);
  CHECK(c.pc == 0x345, "2NNN jumps to NNN");
  CHECK(c.stack.sp == 1 && c.stack.arr[0] == 0x234,
        "2NNN pushes the return address");

  op_group0(&c, 0x00EE);
  CHECK(c.pc == 0x234, "00EE restores the full 16-bit PC");
  CHECK(c.stack.sp == 0, "00EE pops the stack");
}

static void test_ld_add(void) {
  Chip8 c;
  fresh(&c);
  op_group6(&c, 0x6AFF);
  CHECK(c.v[0xA] == 0xFF, "6XNN loads a byte");
  op_group7(&c, 0x7A02);
  CHECK(c.v[0xA] == 0x01, "7XNN wraps at 8 bits");
}

static void test_alu(void) {
  Chip8 c;
  fresh(&c);

  c.v[1] = 0xFF;
  c.v[2] = 0x02;
  op_group8(&c, 0x8124);
  CHECK(c.v[1] == 0x01 && c.v[0xF] == 1, "8XY4 sets carry");

  fresh(&c);
  c.v[1] = 0x01;
  c.v[2] = 0x02;
  op_group8(&c, 0x8125);
  CHECK(c.v[1] == 0xFF && c.v[0xF] == 0, "8XY5 sets borrow flag to 0");

  fresh(&c);
  c.v[1] = 0x81;
  op_group8(&c, 0x8126);
  CHECK(c.v[1] == 0x40 && c.v[0xF] == 1, "8XY6 shifts right, VF = old LSB");

  fresh(&c);
  c.v[1] = 0x81;
  op_group8(&c, 0x812E);
  CHECK(c.v[1] == 0x02 && c.v[0xF] == 1, "8XYE shifts left, VF = old MSB");

  fresh(&c);
  c.v[1] = 0x40;
  op_group8(&c, 0x812E);
  CHECK(c.v[1] == 0x80 && c.v[0xF] == 0, "8XYE does not set VF on 0 bit");
}

static void test_draw(void) {
  Chip8 c;
  fresh(&c);
  c.I = 0x300;
  c.memory[0x300] = 0x80; // single leftmost pixel
  c.v[0] = 5;
  c.v[1] = 3;

  op_groupD(&c, 0xD011);
  CHECK(c.display[3][5] == 1, "DXYN sets the pixel at (x,y)");
  CHECK(c.v[0xF] == 0, "DXYN reports no collision on a blank pixel");

  op_groupD(&c, 0xD011);
  CHECK(c.display[3][5] == 0, "DXYN XORs an existing pixel off");
  CHECK(c.v[0xF] == 1, "DXYN reports collision when a pixel was set");
}

static void test_draw_wrap(void) {
  Chip8 c;
  fresh(&c);
  c.I = 0x300;
  c.memory[0x300] = 0x40; // pixel in column 1
  c.v[0] = 63;
  c.v[1] = 0;

  op_groupD(&c, 0xD011);
  CHECK(c.display[0][0] == 1, "DXYN wraps x past the right edge");
}

static void test_skip(void) {
  Chip8 c;
  fresh(&c);
  c.v[2] = 5;
  c.key[5] = 1;
  c.pc = 0x202;

  op_groupE(&c, 0xE29E);
  CHECK(c.pc == 0x204, "EX9E skips exactly one instruction when pressed");

  fresh(&c);
  c.v[2] = 5;
  c.key[5] = 0;
  c.pc = 0x202;
  op_groupE(&c, 0xE2A1);
  CHECK(c.pc == 0x204, "EXA1 skips when the key is not pressed");
}

static void test_font_bcd(void) {
  Chip8 c;
  fresh(&c);
  c.v[3] = 0x0A;
  op_groupF(&c, 0xF329);
  CHECK(c.I == FONT_START + 0x0A * 5, "FX29 points at the digit's font sprite");

  fresh(&c);
  c.I = 0x400;
  c.v[3] = 234;
  op_groupF(&c, 0xF333);
  CHECK(c.memory[0x400] == 2 && c.memory[0x401] == 3 &&
            c.memory[0x402] == 4,
        "FX33 writes hundreds/tens/ones");
}

static void test_store_load(void) {
  Chip8 c;
  fresh(&c);
  c.I = 0x400;
  for (int i = 0; i < 4; ++i)
    c.v[i] = (uint8_t)(i + 1);

  op_groupF(&c, 0xF355);
  CHECK(c.memory[0x400] == 1 && c.memory[0x403] == 4,
        "FX55 stores V0..VX into memory");

  for (int i = 0; i < 4; ++i)
    c.v[i] = 0;
  op_groupF(&c, 0xF365);
  CHECK(c.v[0] == 1 && c.v[3] == 4, "FX65 loads V0..VX from memory");
  CHECK(c.I == 0x400, "FX55/FX65 leave I unchanged");
}

static void test_key_wait(void) {
  Chip8 c;
  fresh(&c);
  c.pc = 0x202;

  // No key latched: FX0A must rewind the PC and retry.
  op_groupF(&c, 0xF30A);
  CHECK(c.pc == 0x200, "FX0A rewinds PC when no key is available");

  // Simulate a press through the real callback (key 5 = W).
  key_callback(NULL, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  c.pc = 0x202;
  op_groupF(&c, 0xF30A);
  CHECK(c.v[3] == 5, "FX0A stores the CHIP-8 key index, not the GLFW code");
  CHECK(c.pc == 0x202, "FX0A does not rewind after a key is consumed");
}

static void test_jump_and_timers(void) {
  Chip8 c;
  fresh(&c);
  c.v[0] = 3;
  op_groupB(&c, 0xB300);
  CHECK(c.pc == 0x303, "BNNN jumps to NNN + V0");

  fresh(&c);
  c.delay_timer = 2;
  c.sound_timer = 1;
  tick_timers(&c);
  CHECK(c.delay_timer == 1, "delay timer decrements");
  tick_timers(&c);
  CHECK(c.delay_timer == 0 && c.sound_timer == 0,
        "timers stop at zero");
}

int main(void) {
  printf("Running CHIP-8 tests...\n");
  test_cls();
  test_call_ret();
  test_ld_add();
  test_alu();
  test_draw();
  test_draw_wrap();
  test_skip();
  test_font_bcd();
  test_store_load();
  test_key_wait();
  test_jump_and_timers();

  if (failures == 0) {
    printf("All tests passed.\n");
    return 0;
  }
  printf("%d test(s) failed.\n", failures);
  return 1;
}
