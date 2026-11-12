#ifndef CHIP8_H
#define CHIP8_H

#include "stack.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Use macros/enums for compile-time array bounds. A `static const` variable
 * is NOT a constant expression in C, which forced the compiler to treat the
 * display as a variable-length array. */
#define WIDTH 64
#define HEIGHT 32
#define MEMORY_SIZE 4096
#define PROGRAM_START 0x200
#define FONT_START 0x050

typedef struct {
  uint8_t memory[MEMORY_SIZE];
  uint8_t display[HEIGHT][WIDTH];
  struct Stack stack;
  uint16_t pc;
  uint16_t I;
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint8_t v[16];
  uint8_t key[16];
  uint32_t last_time;
  uint32_t accumulator;
} Chip8;

typedef void (*OpHandler)(Chip8 *chip, uint16_t opcode);

int chip8_init(Chip8 *chip, FILE *f);
void chip8_step(Chip8 *chip);
void chip8_keys_update(Chip8 *chip, GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods);
void tick_timers(Chip8 *chip);
uint32_t get_ticks_ms(void);

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

#endif // CHIP8_H
