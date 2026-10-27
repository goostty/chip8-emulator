#include "chip8.h"
#include <stdint.h>

static const OpHandler table[16] = {op_group0, op_group1, op_group2, op_group3,
                                    op_group4, op_group5, op_group6, op_group7,
                                    op_group8, op_group9, op_groupA, op_groupB,
                                    op_groupC, op_groupD, op_groupE, op_groupF};

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
  chip->stack.sp = 0;

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

// Continue Here
void op_groupD(Chip8 *chip, uint16_t opcode) {}

void chip8_step(Chip8 *chip) {
  uint8_t h_byte = chip->memory[chip->pc];
  uint8_t l_byte = chip->memory[chip->pc + 1];
  uint8_t opcode = (h_byte << 8) | l_byte;

  chip->pc += 2;

  uint8_t nibble1 = (opcode >> 12) & 0xF;

  switch (nibble1) {
  case 0:
    table[0](chip, opcode);
    break;
  case 1:
    table[1](chip, opcode);
    break;
  case 2:
    table[2](chip, opcode);
    break;
  case 3:
    table[3](chip, opcode);
    break;
  case 4:
    table[4](chip, opcode);
    break;
  case 5:
    table[5](chip, opcode);
    break;
  case 6:
    table[6](chip, opcode);
    break;
  case 7:
    table[7](chip, opcode);
    break;
  case 8:
    table[8](chip, opcode);
    break;
  case 9:
    table[9](chip, opcode);
    break;
  case 0xA:
    table[0xA](chip, opcode);
    break;
  case 0xB:
    table[0xB](chip, opcode);
    break;
  case 0xC:
    table[0xC](chip, opcode);
    break;
  case 0xD:
    table[0xD](chip, opcode);
    break;
  case 0xE:
    table[0xE](chip, opcode);
    break;
  case 0xF:
    table[0xF](chip, opcode);
    break;

  default:
    fprintf(stderr, "Not a valid opcode");
    break;
  }
}
