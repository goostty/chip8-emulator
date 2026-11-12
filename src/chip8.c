#define _POSIX_C_SOURCE 200809L

#include "chip8.h"
#include <stdint.h>

static const OpHandler table[16] = {op_group0, op_group1, op_group2, op_group3,
                                    op_group4, op_group5, op_group6, op_group7,
                                    op_group8, op_group9, op_groupA, op_groupB,
                                    op_groupC, op_groupD, op_groupE, op_groupF};

static const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/* Standard CHIP-8 keypad layout:
 *   1 2 3 C        1 2 3 4
 *   4 5 6 D   ->   Q W E R
 *   7 8 9 E        A S D F
 *   A 0 B F        Z X C V
 * Index = CHIP-8 key, value = GLFW key. */
static const int keymap[16] = {
    GLFW_KEY_X, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, // 0 1 2 3
    GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_E, GLFW_KEY_A, // 4 5 6 7
    GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_Z, GLFW_KEY_C, // 8 9 A B
    GLFW_KEY_4, GLFW_KEY_R, GLFW_KEY_F, GLFW_KEY_V  // C D E F
};

/* Latched by key_callback, consumed by FX0A. Stores a CHIP-8 key index. */
static int pressed_key = -1;

static int glfw_key_to_chip8(int key) {
  for (int i = 0; i < 16; ++i) {
    if (keymap[i] == key)
      return i;
  }
  return -1;
}

uint32_t get_ticks_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void tick_timers(Chip8 *chip) {
  if (chip->delay_timer > 0)
    chip->delay_timer--;
  if (chip->sound_timer > 0)
    chip->sound_timer--;
}

int chip8_init(Chip8 *chip, FILE *f) {
  uint8_t buf[MEMORY_SIZE];
  size_t len = fread(buf, 1, sizeof(buf), f);
  fclose(f);

  if (len == 0) {
    fprintf(stderr, "ROM is empty or unreadable\n");
    return -1;
  }
  if (len > MEMORY_SIZE - PROGRAM_START) {
    fprintf(stderr, "ROM too large: %zu bytes (max %d)\n", len,
            MEMORY_SIZE - PROGRAM_START);
    return -1;
  }

  memset(chip, 0, sizeof(*chip));
  chip->pc = PROGRAM_START;
  chip->I = PROGRAM_START;
  chip->stack.sp = 0;

  memcpy(chip->memory + FONT_START, fontset, sizeof(fontset));
  memcpy(chip->memory + PROGRAM_START, buf, len);

  srand((unsigned)time(NULL));

  return 0;
}

void op_group0(Chip8 *chip, uint16_t opcode) {
  switch (opcode) {
  case 0x00E0: // CLS
    memset(chip->display, 0, sizeof(chip->display));
    break;
  case 0x00EE: // RET
    chip->pc = pop(&chip->stack) & 0xFFF;
    break;
  default:
    // 0NNN (machine-code call) is ignored by most interpreters.
    break;
  }
}

void op_group1(Chip8 *chip, uint16_t opcode) { chip->pc = opcode & 0xFFF; }

void op_group2(Chip8 *chip, uint16_t opcode) {
  push(&chip->stack, chip->pc);
  chip->pc = opcode & 0xFFF;
}

void op_group3(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  if (chip->v[x] == nn)
    chip->pc = (chip->pc + 2) & 0xFFF;
}

void op_group4(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  if (chip->v[x] != nn)
    chip->pc = (chip->pc + 2) & 0xFFF;
}

void op_group5(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;
  if ((opcode & 0x000F) == 0 && chip->v[x] == chip->v[y])
    chip->pc = (chip->pc + 2) & 0xFFF;
}

void op_group6(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  chip->v[x] = nn;
}

void op_group7(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  chip->v[x] += nn; // uint8_t intentionally wraps
}

void op_group8(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;
  uint8_t vy = chip->v[y];
  uint16_t sum = chip->v[x] + vy;

  switch (opcode & 0x000F) {
  case 0x0:
    chip->v[x] = vy;
    break;
  case 0x1:
    chip->v[x] |= vy;
    break;
  case 0x2:
    chip->v[x] &= vy;
    break;
  case 0x3:
    chip->v[x] ^= vy;
    break;
  case 0x4: // ADD, VF = carry
    chip->v[0xF] = (sum > 0xFF) ? 1 : 0;
    chip->v[x] = sum & 0xFF;
    break;
  case 0x5: // SUB, VF = NOT borrow
    chip->v[0xF] = (chip->v[x] >= vy) ? 1 : 0;
    chip->v[x] = chip->v[x] - vy;
    break;
  case 0x6: // SHR: VF is the bit shifted out
    chip->v[0xF] = chip->v[x] & 0x1;
    chip->v[x] >>= 1;
    break;
  case 0x7: // SUBN, VF = NOT borrow
    chip->v[0xF] = (vy >= chip->v[x]) ? 1 : 0;
    chip->v[x] = vy - chip->v[x];
    break;
  case 0xE: // SHL: VF is the bit shifted out
    chip->v[0xF] = (chip->v[x] & 0x80) >> 7;
    chip->v[x] <<= 1;
    break;
  default:
    fprintf(stderr, "Invalid opcode 8XY%X\n", opcode & 0x000F);
    break;
  }
}

void op_group9(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;
  if ((opcode & 0x000F) == 0 && chip->v[x] != chip->v[y])
    chip->pc = (chip->pc + 2) & 0xFFF;
}

void op_groupA(Chip8 *chip, uint16_t opcode) {
  chip->I = opcode & 0xFFF;
}

void op_groupB(Chip8 *chip, uint16_t opcode) {
  uint16_t nnn = opcode & 0x0FFF;
  chip->pc = (nnn + chip->v[0]) & 0xFFF;
}

void op_groupC(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  chip->v[x] = (uint8_t)(rand() % 256) & nn;
}

void op_groupD(Chip8 *chip, uint16_t opcode) {
  uint8_t x = chip->v[(opcode & 0x0F00) >> 8] % WIDTH;
  uint8_t y = chip->v[(opcode & 0x00F0) >> 4] % HEIGHT;
  uint8_t height = opcode & 0x000F;

  chip->v[0xF] = 0;

  for (unsigned row = 0; row < height; ++row) {
    uint8_t spriteByte = chip->memory[(chip->I + row) & 0xFFF];

    for (unsigned col = 0; col < 8; ++col) {
      if (spriteByte & (0x80 >> col)) {
        unsigned px = (x + col) % WIDTH;
        unsigned py = (y + row) % HEIGHT;

        if (chip->display[py][px])
          chip->v[0xF] = 1; // collision detected before XOR

        chip->display[py][px] ^= 1;
      }
    }
  }
}

void op_groupE(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t key = chip->v[x] & 0x0F;
  uint8_t sub = opcode & 0x00FF;

  switch (sub) {
  case 0x9E: // skip if key pressed
    if (chip->key[key])
      chip->pc = (chip->pc + 2) & 0xFFF;
    break;
  case 0xA1: // skip if key not pressed
    if (!chip->key[key])
      chip->pc = (chip->pc + 2) & 0xFFF;
    break;
  default:
    fprintf(stderr, "Invalid opcode EX%02X\n", sub);
    break;
  }
}

void op_groupF(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t sub = opcode & 0x00FF;

  switch (sub) {
  case 0x07: // VX = delay timer
    chip->v[x] = chip->delay_timer;
    break;

  case 0x0A: // VX = next key press (non-blocking)
    if (pressed_key >= 0) {
      chip->v[x] = (uint8_t)pressed_key;
      pressed_key = -1;
    } else {
      // No key yet: re-run this instruction on the next step.
      chip->pc = (chip->pc - 2) & 0xFFF;
    }
    break;

  case 0x15: // delay timer = VX
    chip->delay_timer = chip->v[x];
    break;

  case 0x18: // sound timer = VX
    chip->sound_timer = chip->v[x];
    break;

  case 0x1E: { // I += VX, VF = carry past 0xFFF
    uint16_t sum = chip->I + chip->v[x];
    chip->v[0xF] = (sum > 0xFFF) ? 1 : 0;
    chip->I = sum & 0xFFF;
    break;
  }

  case 0x29: // I = font sprite for digit VX
    chip->I = FONT_START + (chip->v[x] & 0x0F) * 5;
    break;

  case 0x33: // BCD: hundreds, tens, ones
    chip->memory[chip->I] = chip->v[x] / 100;
    chip->memory[(chip->I + 1) & 0xFFF] = (chip->v[x] / 10) % 10;
    chip->memory[(chip->I + 2) & 0xFFF] = chip->v[x] % 10;
    break;

  case 0x55: // store V0..VX at I (I unchanged)
    for (int i = 0; i <= x; ++i)
      chip->memory[(chip->I + i) & 0xFFF] = chip->v[i];
    break;

  case 0x65: // load V0..VX from I (I unchanged)
    for (int i = 0; i <= x; ++i)
      chip->v[i] = chip->memory[(chip->I + i) & 0xFFF];
    break;

  default:
    fprintf(stderr, "Invalid opcode FX%02X\n", sub);
    break;
  }
}

void chip8_keys_update(Chip8 *chip, GLFWwindow *window) {
  for (int i = 0; i < 16; ++i)
    chip->key[i] = glfwGetKey(window, keymap[i]) == GLFW_PRESS;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  (void)window;
  (void)scancode;
  (void)mods;

  if (action == GLFW_PRESS) {
    int mapped = glfw_key_to_chip8(key);
    if (mapped >= 0)
      pressed_key = mapped;
  }
}

void chip8_step(Chip8 *chip) {
  uint16_t pc = chip->pc & 0xFFF;
  uint8_t h_byte = chip->memory[pc];
  uint8_t l_byte = chip->memory[(pc + 1) & 0xFFF];
  uint16_t opcode = (h_byte << 8) | l_byte;

  chip->pc = (pc + 2) & 0xFFF;

  uint8_t nibble1 = (opcode >> 12) & 0xF;
  table[nibble1](chip, opcode);
}
