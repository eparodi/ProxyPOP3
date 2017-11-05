#ifndef TPE_PROTOS_QUEUE_H
#define TPE_PROTOS_QUEUE_H

#include <stdbool.h>

struct queue * new_queue(void);

void enqueue(struct queue *q, void *data);

void * dequeue(struct queue *q);

bool is_empty(struct queue *q);

int size(struct queue *q);

void destroy_queue(struct queue *q);


#endif //TPE_PROTOS_QUEUE_H
