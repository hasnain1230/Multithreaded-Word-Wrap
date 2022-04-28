#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "unbounded_queue.h"

struct Node { // These are private and not meant to be accessed by client code.
    void *data;
    size_t dataSize;
    struct Node *next;
};

struct Queue {
    bool queueDone, mainThreadTermination;
    struct Node *head, *tail;
    size_t sleepingThreads, queueSize, numOfThreads;;
    pthread_mutex_t lock;
    pthread_cond_t dequeueReady;
};

struct Queue *initQueue() {
    struct Queue *queue = (struct Queue *) malloc(sizeof(struct Queue));

    if (queue == NULL) {
        perror("Init Queue Memory Allocation Failed:");
        exit(2);
    }

    queue->head = queue->tail = NULL;
    queue->queueDone = false;
    queue->sleepingThreads = 0;
    queue->queueSize = 0;

    queue->mainThreadTermination = true; // By default, at least
    queue->numOfThreads = -1; // This is -1 unless main thread termination is false.

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->dequeueReady, NULL);

    return queue;
}

void *enqueue(struct Queue *queue, void *item, size_t itemSize) {
    pthread_mutex_lock(&queue->lock); // FIXME: CHECK RETURN VALUES OF ALL THIS! IF IT FAILS,

    struct Node *tempNode = (struct Node *) malloc(sizeof(struct Node));

    tempNode->data = malloc(itemSize); // Don't forget your null terminator
    tempNode->data = memcpy(tempNode->data, item, itemSize);
    tempNode->dataSize = itemSize;
    tempNode->next = NULL;

    if (isEmpty(queue)) {
        queue->head = queue->tail = tempNode; // If empty, head and tail point to the same thing.
        queue->queueSize++;
        pthread_cond_signal(&queue->dequeueReady);
        pthread_mutex_unlock(&queue->lock);
        return tempNode->data;
    }

    queue->tail->next = tempNode; // tail points to old tempNode. This says old tempNode now points to new tempNode
    queue->tail = tempNode; // Current tail also needs to point to new tempNode

    queue->queueSize++;

    pthread_cond_signal(&queue->dequeueReady);
    pthread_mutex_unlock(&queue->lock); // FIXME: Check return value

    return item;
}

void *dequeue(struct Queue *queue) {
    pthread_mutex_lock(&queue->lock); // FIXME: CHECK RETURN VALUES OF ALL THIS! IF IT FAILS, EXIT

    while (isEmpty(queue)) {
        if (queue->queueDone) {
            pthread_mutex_unlock(&queue->lock);
            return NULL;
        }

        queue->sleepingThreads++;

        while (queue->mainThreadTermination == false && queue->sleepingThreads == queue->numOfThreads && isEmpty(queue)) {
            pthread_mutex_unlock(&queue->lock);
            jobComplete(queue);
            pthread_mutex_lock(&queue->lock);
        }

        pthread_cond_wait(&queue->dequeueReady, &queue->lock);

        queue->sleepingThreads--;
    }

    struct Node *tempNode = queue->head; // Get node to dequeue
    void *data = malloc(tempNode->dataSize); // Need to store the data, so we can use it later. Client is responsible for freeing this because it points to dynamic memory. This could be improved so the client doesn't have to do anything, but it's fine for now.
    data = memcpy(data, tempNode->data, tempNode->dataSize);

    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(tempNode->data);
    free(tempNode);

    queue->queueSize--;

    pthread_mutex_unlock(&queue->lock); // FIXME: CHECK RETURN VALUES OF ALL THIS! IF IT FAILS,

    return data;
}

void jobComplete(struct Queue *queue) {
    pthread_mutex_lock(&queue->lock);

    queue->queueDone = true;
    pthread_cond_broadcast(&queue->dequeueReady);

    pthread_mutex_unlock(&queue->lock);
}

void mainThreadTermination(struct Queue *queue, int numThreads) { // Yes, this is a setter function like Java. I want to avoid using global variables since that is bad practice.
    pthread_mutex_lock(&queue->lock);
    queue->mainThreadTermination = false;
    queue->numOfThreads = numThreads;
    pthread_mutex_unlock(&queue->lock);
}

bool isEmpty(struct Queue *queue) {
    return queue->queueSize == 0;
}
