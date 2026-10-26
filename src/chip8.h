#include "stack.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint8_t memory[4096];
  bool display[64][32];
  struct Stack stack;
  uint16_t pc;
  uint16_t I;
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint8_t v[16];
} Chip8;

typedef void (*OpHandler)(Chip8 *chip, uint16_t opcode);

int chip8_init(Chip8 *chip, FILE *f);
void chip8_step(Chip8 *chip);

// Op Table Functions
void op_group0(Chip8 *chip, uint16_t opcode);
void op_group1(Chip8 *chip, uint16_t opcode);
void op_group2(Chip8 *chip, uint16_t opcode);
void op_group3(Chip8 *chip, uint16_t opcode);
void op_group4(Chip8 *chip, uint16_t opcode);
void op_group5(Chip8 *chip, uint16_t opcode);
void op_group6(Chip8 *chip, uint16_t opcode);
void op_group7(Chip8 *chip, uint16_t opcode);
void op_group8(Chip8 *chip, uint16_t opcode);
void op_group9(Chip8 *chip, uint16_t opcode);
void op_groupA(Chip8 *chip, uint16_t opcode);
void op_groupB(Chip8 *chip, uint16_t opcode);
void op_groupC(Chip8 *chip, uint16_t opcode);
void op_groupD(Chip8 *chip, uint16_t opcode);
void op_groupE(Chip8 *chip, uint16_t opcode);
void op_groupF(Chip8 *chip, uint16_t opcode);
