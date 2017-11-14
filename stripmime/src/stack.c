#include "stack.h"

struct stack {
    struct stack_node *first, *last;
    int size;
};

struct stack_node {
    void *data;
    struct stack_node *next;
};

struct stack * stack_new() {
    struct stack *ret = malloc(sizeof())
}

void * stack_push(struct stack *s, void * data);

void * stack_pop(struct stack *s, void * data);

void * stack_peek(struct stack *s, void * data);

bool stack_is_empty(struct stack *s);

int stack_size(struct stack *s);

void stack_destroy(struct stack *s);