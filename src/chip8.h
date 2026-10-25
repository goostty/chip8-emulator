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
  uint8_t v_registers[16];
} Chip8;

int chip8_init(Chip8 *chip, FILE *f);
void chip8_step(Chip8 *chip);
