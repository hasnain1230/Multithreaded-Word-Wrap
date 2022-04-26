//
// Created by Hasnain Ali on 4/18/22.
//

#ifndef WORD_WRAP_UNBOUNDED_QUEUE_H
#define WORD_WRAP_UNBOUNDED_QUEUE_H
#include <stdbool.h>

struct Queue {
    bool jobComplete;
    struct Node *head, *tail;
    size_t queueSize;
    pthread_mutex_t lock;
    pthread_cond_t dequeueReady;
};

struct Queue *initQueue();
void *enqueue(struct Queue *queue, void *item, size_t itemSize);
void *dequeue(struct Queue *queue);
void jobComplete(struct Queue *queue);
bool isEmpty(struct Queue *queue);


#endif //WORD_WRAP_UNBOUNDED_QUEUE_H
