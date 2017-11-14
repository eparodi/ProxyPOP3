#ifndef TPE_PROTOS_QUEUE_H
#define TPE_PROTOS_QUEUE_H

#include <stdbool.h>

struct queue * queue_new(void);

void queue_add(struct queue *q, void *data);

void * queue_remove(struct queue *q);

void * queue_peek(struct queue *q);

bool queue_is_empty(struct queue *q);

int queue_size(struct queue *q);

void * queue_get_next(struct queue *q);

void queue_destroy(struct queue *q);

#endif //TPE_PROTOS_QUEUE_H
