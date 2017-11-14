#ifndef TPE_PROTOS_STACK_H
#define TPE_PROTOS_STACK_H

struct stack * stack_new();

void * stack_push(struct stack *s, void * data);

void * stack_pop(struct stack *s, void * data);

void * stack_peek(struct stack *s, void * data);

bool stack_is_empty(struct stack *s);

int stack_size(struct stack *s);

void stack_destroy(struct stack *s);

#endif //TPE_PROTOS_STACK_H
