//
// Created by Hasnain Ali on 4/18/22.
//

#ifndef WORD_WRAP_UNBOUNDED_QUEUE_H
#define WORD_WRAP_UNBOUNDED_QUEUE_H
#include <stdbool.h>

struct Node {
    char *data;
    struct Node *next;
};

struct Queue {
    struct Node *head, *tail;
    pthread_mutex_t lock;
    pthread_cond_t enqueueReady;
};

struct Queue *initQueue();
char *enqueue(struct Queue *queue, char *item);
char *dequeue(struct Queue *queue);
bool isEmpty(struct Queue *queue);


#endif //WORD_WRAP_UNBOUNDED_QUEUE_H
