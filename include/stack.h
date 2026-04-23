#ifndef H_STACK
#define H_STACK

#include <stdint.h>
#include <stdbool.h>

#define STACK_MAX 1024

typedef struct{
	uint64_t elem[STACK_MAX];
	int size;	
} Stack;

void stack_init(Stack *stack);
bool stack_push(Stack *stack, uint64_t elem);
uint64_t stack_pop(Stack *stack);

#endif