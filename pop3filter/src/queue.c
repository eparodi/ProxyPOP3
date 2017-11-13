#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

struct queue {
    struct queue_node 	*first, *last;
    struct queue_node   *current;   //usado para recorrer la queue
    int 				size;
};

struct queue_node {
    void *data;
    struct queue_node *next;
};

struct queue * queue_new() {
    struct queue *ret = malloc(sizeof(*ret));

    if (ret == NULL) {
        return NULL;
    }

    ret->first      = ret->last = NULL;
    ret->current    = NULL;
    ret->size       = 0;

    return ret;
}

static struct queue_node * new_node(void *data) {
    struct queue_node *ret = malloc(sizeof(*ret));

    if (ret == NULL) {
        return NULL;
    }

    ret->data = data;
    ret->next = NULL;

    return ret;
}

void queue_add(struct queue *q, void *data) {
    struct queue_node *last = q->last;

    if (data == NULL) {
        return;
    }

    if (last == NULL) {
        last = new_node(data);	// queue vacia
        q->first = q->last = last;
        q->current = q->first;  // seteo el puntero para recorrer
    } else {
        last->next = new_node(data);
        q->last = last->next;
    }

    q->size++;
}

void * queue_remove(struct queue *q) {
    if (queue_is_empty(q)) {
        return NULL;
    }

    struct queue_node *first = q->first;
    void * ret = first->data;

    q->first = first->next;
    free(first);
    q->size--;

    if (q->first == NULL) {
        q->last = NULL;
        q->current = NULL;
    }

    return ret;
}

void * queue_peek(struct queue *q) {
    void * ret = NULL;

    if (q->first != NULL) {
        ret = q->first->data;
    }

    return ret;
}

bool queue_is_empty(struct queue *q) {
    return q->size == 0;
}

int queue_size(struct queue *q) {
    return q->size;
}

void * queue_get_next(struct queue *q) {
    void * ret;

    if (q->current != NULL) {
        ret = q->current->data;
        q->current = q->current->next;
    } else {    // termine de recorrer o la queue estaba vacia
        q->current = q->first;
        ret = NULL;
    }

    return ret;
}

void queue_destroy(struct queue *q) {

    //TODO free nodes
    free(q);
}
