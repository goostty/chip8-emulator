#include "stack.h"

void push(struct Stack *stack, uint16_t addr) {
  stack->arr[stack->sp] = addr;
  stack->sp++;
}

void pop(struct Stack *stack) { stack->sp--; }
