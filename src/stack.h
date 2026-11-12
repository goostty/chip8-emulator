#ifndef STACK_H
#define STACK_H

#include <stdint.h>

#define STACK_DEPTH 16

struct Stack {
  uint16_t arr[STACK_DEPTH];
  uint16_t sp;
};

void push(struct Stack *stack, uint16_t addr);
uint16_t pop(struct Stack *stack);

#endif // STACK_H
