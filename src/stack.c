#include "stack.h"
#include <stdio.h>

void push(struct Stack *stack, uint16_t addr) {
  if (stack->sp >= STACK_DEPTH) {
    fprintf(stderr, "CHIP-8 stack overflow (sp=%u)\n", stack->sp);
    stack->sp = STACK_DEPTH - 1;
  }
  stack->arr[stack->sp++] = addr;
}

uint16_t pop(struct Stack *stack) {
  if (stack->sp == 0) {
    fprintf(stderr, "CHIP-8 stack underflow\n");
    return 0;
  }
  return stack->arr[--stack->sp];
}
