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

void checkMemoryAllocation(void *ptr) {
    if (ptr == NULL) {
        perror("Memory Allocation Failed: ");
        exit(2);
    }
}

void checkThreadFunction(int returnValThread) {
    if (returnValThread != 0) {
        perror("Threading Function Failed!");
        exit(2);
    }
}

struct Queue *initQueue() {
    struct Queue *queue = (struct Queue *) malloc(sizeof(struct Queue));
    checkMemoryAllocation(queue);

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

    checkThreadFunction(pthread_mutex_init(&queue->lock, NULL));
    checkThreadFunction(pthread_cond_init(&queue->dequeueReady, NULL));

    return queue;
}

void enqueue(struct Queue *queue, void *item, size_t itemSize) {
    checkThreadFunction(pthread_mutex_lock(&queue->lock));

    struct Node *tempNode = (struct Node *) malloc(sizeof(struct Node));
    checkMemoryAllocation(tempNode);

    tempNode->data = malloc(itemSize); // Don't forget your null terminator
    checkMemoryAllocation(tempNode->data);
    tempNode->data = memcpy(tempNode->data, item, itemSize);
    tempNode->dataSize = itemSize;
    tempNode->next = NULL;

    if (isEmpty(queue)) {
        queue->head = queue->tail = tempNode; // If empty, head and tail point to the same thing.
        queue->queueSize++;
        checkThreadFunction(pthread_cond_signal(&queue->dequeueReady));
        checkThreadFunction(pthread_mutex_unlock(&queue->lock));

        return;
    }

    queue->tail->next = tempNode; // tail points to old tempNode. This says old tempNode now points to new tempNode
    queue->tail = tempNode; // Current tail also needs to point to new tempNode

    queue->queueSize++;

    checkThreadFunction(pthread_cond_signal(&queue->dequeueReady));
    checkThreadFunction(pthread_mutex_unlock(&queue->lock));

    return;
}

void *dequeue(struct Queue *queue) {
    checkThreadFunction(pthread_mutex_lock(&queue->lock));

    while (isEmpty(queue)) {
        if (queue->queueDone) {
            checkThreadFunction(pthread_mutex_unlock(&queue->lock));
            return NULL;
        }

        queue->sleepingThreads++;

        while (queue->mainThreadTermination == false && queue->sleepingThreads == queue->numOfThreads && isEmpty(queue)) {
            checkThreadFunction(pthread_mutex_unlock(&queue->lock));
            jobComplete(queue);
            checkThreadFunction(pthread_mutex_lock(&queue->lock));
        }

        checkThreadFunction(pthread_cond_wait(&queue->dequeueReady, &queue->lock));

        queue->sleepingThreads--;
    }

    struct Node *tempNode = queue->head; // Get node to dequeue
    void *data = malloc(tempNode->dataSize); // Need to store the data, so we can use it later. Client is responsible for freeing this because it points to dynamic memory. This could be improved so the client doesn't have to do anything, but it's fine for now.
    checkMemoryAllocation(data);
    data = memcpy(data, tempNode->data, tempNode->dataSize);

    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(tempNode->data);
    free(tempNode);

    queue->queueSize--;

    checkThreadFunction(pthread_mutex_unlock(&queue->lock));

    return data;
}

void jobComplete(struct Queue *queue) {
    checkThreadFunction( pthread_mutex_lock(&queue->lock));

    queue->queueDone = true;
    checkThreadFunction(pthread_cond_broadcast(&queue->dequeueReady));

    checkThreadFunction(pthread_mutex_unlock(&queue->lock));
}

void mainThreadTermination(struct Queue *queue, int numThreads) { // Yes, this is a setter function like Java. I want to avoid using global variables since that is bad practice.
    checkThreadFunction(pthread_mutex_lock(&queue->lock));
    queue->mainThreadTermination = false;
    queue->numOfThreads = numThreads;
    checkThreadFunction(pthread_mutex_unlock(&queue->lock));
}

bool isEmpty(struct Queue *queue) {
    return queue->queueSize == 0;
}
