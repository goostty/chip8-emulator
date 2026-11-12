#include "chip8.h"
#include <stdint.h>

static const OpHandler table[16] = {op_group0, op_group1, op_group2, op_group3,
                                    op_group4, op_group5, op_group6, op_group7,
                                    op_group8, op_group9, op_groupA, op_groupB,
                                    op_groupC, op_groupD, op_groupE, op_groupF};
int pressed_key = -1;

int chip8_init(Chip8 *chip, FILE *f) {
  uint8_t buf[4096];
  size_t len = fread(buf, 1, sizeof(buf), f);

  if (len > 4096 - 0x200) {
    fprintf(stderr, "Length of Binary exceeded the RAM's capacity");
    fclose(f);
    return -1;
  }

  // Initialize struct members
  memset(chip, 0, sizeof(*chip));
  chip->pc = 0x200;
  memcpy(chip->memory + 0x200, buf, len);
  chip->I = 0x200;
  chip->stack->sp = 0;

  fclose(f);

  // Return success
  return 0;
}

void op_group0(Chip8 *chip, uint16_t opcode) {
  uint8_t s_opcode = opcode & 0xF;
  if (s_opcode == 0xE) {
    pop(chip->stack);
  } else {
    memset(chip->display, false, sizeof(*chip->display));
  }
}

void op_group1(Chip8 *chip, uint16_t opcode) { chip->pc = opcode & 0xFFF; }

void op_group2(Chip8 *chip, uint16_t opcode) {
  push(chip->stack, chip->pc);
  chip->pc = opcode & 0xFFF;
}

void op_group3(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  if (chip->v[x] == nn) {
    chip->pc += 2;
  }
}

void op_group4(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  if (chip->v[x] != nn) {
    chip->pc += 2;
  }
}

void op_group5(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;
  if (chip->v[x] == chip->v[y]) {
    chip->pc += 2;
  }
}

void op_group6(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;

  chip->v[x] = nn;
}

void op_group7(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;

  chip->v[x] += nn;
}

void op_group8(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;
  uint8_t vy = chip->v[y];
  uint16_t sum = chip->v[x] + vy;

  switch (opcode & 0x000F) {
  case 0:
    chip->v[x] = chip->v[y];
    break;
  case 1:
    chip->v[x] |= vy;
    break;
  case 2:
    chip->v[x] &= vy;
    break;
  case 3:
    chip->v[x] ^= vy;
    break;
  case 4:
    chip->v[0xF] = (sum > 0xFF) ? 1 : 0;
    chip->v[x] = sum & 0xFF;
    break;
  case 5:
    chip->v[0xF] = (chip->v[x] >= vy) ? 1 : 0;
    chip->v[x] = (chip->v[x] - vy);
    break;
  case 6:
    chip->v[x] >>= 1;
    chip->v[0xF] = (chip->v[x] & 0x000F) >> 2;
    break;
  case 7:
    chip->v[0xF] = (vy >= chip->v[x]) ? 1 : 0;
    chip->v[x] = vy - chip->v[x];
    break;
  case 0xE:
    chip->v[x] <<= 1;
    chip->v[0xF] = (chip->v[x] & 0x0F00) >> 8;
    break;
  default:
    fprintf(stderr, "Not a valid opcode (group 8)");
    break;
  }
}

void op_group9(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;

  if (chip->v[x] != chip->v[y]) {
    chip->pc += 2;
  }
}

void op_groupA(Chip8 *chip, uint16_t opcode) {
  uint16_t nnn = opcode & 0x0FFF;
  chip->I = nnn;
}

void op_groupB(Chip8 *chip, uint16_t opcode) {
  uint16_t nnn = opcode & 0x0FFF;
  chip->pc += (nnn + chip->v[0]);
}

void op_groupC(Chip8 *chip, uint16_t opcode) {
  srand(time(NULL));

  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t nn = opcode & 0x00FF;
  uint16_t rand_num = rand() % sizeof(uint16_t);

  chip->v[x] = (rand_num & nn);
}

void op_groupD(Chip8 *chip, uint16_t opcode) {
  chip->v[0xF] = 0;
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;
  uint8_t height = opcode & 0x000F;

  // Outer Loop: Rows (height), Inner Loop, pixels in each row
  for (unsigned int row = 0; row < height; ++row) {
    uint8_t spriteByte = chip->memory[chip->I + row];

    for (unsigned int col = 0; col < 8; ++col) {
      uint8_t spritePixel = spriteByte & (0x80 >> col);

      if (spritePixel) {
        if (*chip->display[(chip->v[y] + row) * 64 + (chip->v[x] + col)] == 1) {
          chip->v[0xF] = 1;
        }

        // Flip pixel value
        *(chip->display[(chip->v[y] + row) * 64 + (chip->v[x] + col)]) ^= 1;
      }
    }
  }
}

void op_groupE(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode >> 8) & 0x0F;
  uint8_t sub = (opcode >> 4) & 0x0F;

  if (sub == 0x9) {
    if (chip->key[chip->v[x]])
      chip->pc += 4;
    else
      chip->pc += 2;

  } else if (sub == 0xA) {
    if (!chip->key[chip->v[x]])
      chip->pc += 4;
    else
      chip->pc += 2;

  } else {
    fprintf(stderr, "Not valid byte");
  }
}

// Continue Here
void op_groupF(Chip8 *chip, uint16_t opcode) {
  uint8_t x = (opcode >> 8) & 0x0F;
  uint8_t sub = opcode & 0xFF;

  switch (sub) {
  case 0x07:
    chip->v[x] = chip->delay_timer;
  case 0x0A:
    while (pressed_key == -1) {
      glfwWaitEvents();
    }
    chip->v[x] = pressed_key;
  case 0x15:
    chip->delay_timer = chip->v[x];
  case 0x18:
    chip->sound_timer = chip->v[x];
  case 0x1E:
    uint16_t old_I = chip->I;
    chip->I += chip->v[x];
    chip->v[0xF] = (chip->I < old_I) ? 1 : 0;
  case 0x29:
    chip->I = 0x050 + (chip->v[x] * 5);
  case 0x33:
    chip->memory[chip->I] = chip->v[x] / 100;
    chip->memory[chip->I + 1] = (chip->v[x] / 10) % 10;
    chip->memory[chip->I + 2] = chip->v[x] % 10;
    // I is NOT incremented
  case 0x55:
    for (int i = 0; i <= x; ++i) {
      chip->memory[chip->I + i] = chip->v[i];
    }
  case 0x66:
    for (int i = 0; i <= x; ++i) {
      chip->v[i] = chip->memory[chip->I + i];
    }
  }
}

void chip8_keys_update(Chip8 *chip, GLFWwindow *window) {
  int glfw_keys[] = {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_Q, GLFW_KEY_W,
                     GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_Z, GLFW_KEY_X};
  for (int byte = 0x1; byte <= 0xF; ++byte) {
    chip->key[byte] = glfwGetKey(window, glfw_keys[byte]) == GLFW_PRESS;
  }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (action == GLFW_PRESS) {
    pressed_key = key;
  }
}

void chip8_step(Chip8 *chip) {
  uint8_t h_byte = chip->memory[chip->pc];
  uint8_t l_byte = chip->memory[chip->pc + 1];
  uint16_t opcode = (h_byte << 8) | l_byte;

  chip->pc += 2;

  uint8_t nibble1 =
      opcode >> 12 & 0xF; // Bit shift takes more precedence than &
  table[nibble1](chip, opcode);
}
