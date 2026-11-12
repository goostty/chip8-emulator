#include <stdint.h>

#ifndef STACK_H
#define STACK_H

struct Stack {
  uint16_t arr[16];
  uint16_t sp;
};

#endif

void push(struct Stack *stack, uint16_t addr);
void pop(struct Stack *stack);
