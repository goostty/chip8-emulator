#include <stdint.h>

struct Stack {
  uint16_t arr[16];
  uint16_t sp;
};

void push(struct Stack stack, uint16_t addr);
void pop(struct Stack stack);
