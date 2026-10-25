#include "chip8.h"

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

void chip8_step(Chip8 *chip) {
  uint8_t instruct = chip->memory[chip->pc];
  chip->pc += 2;
}
