#include "stack.h"

void stack_init(Stack *stack){
	stack->size = 0;
}

bool stack_push(Stack *stack, uint64_t elem){
	if(stack->size == STACK_MAX) return false;
	stack->elem[stack->size] = elem;
	stack->size++;
	return true;
}

uint64_t stack_pop(Stack *stack){
	if(stack->size == 0) return 0;
	stack->size--;
	return stack->elem[stack->size];
}