#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "unbounded_queue.h"

struct Node { // These are private and not meant to be accessed by client code.
    char *data;
    struct Node *next;
};

struct Queue {
    struct Node *head, *tail;
    pthread_mutex_t lock;
    pthread_cond_t enqueueReady;
};

struct Queue *initQueue() {
    struct Queue *queue = (struct Queue *) malloc(sizeof(struct Queue));

    if (queue == NULL) {
        perror("Init Queue Memory Allocation Failed:");
        exit(2);
    }

    queue->head = queue->tail = NULL;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->enqueueReady, NULL);

    return queue;
}

char *enqueue(struct Queue *queue, char *item) {
    //pthread_mutex_lock(&queue->lock); // FIXME: CHECK RETURN VALUES OF ALL THIS! IF IT FAILS,


    struct Node *tempNode = (struct Node *) malloc(sizeof(struct Node));

    tempNode->data = malloc(strlen(item) + 1); // Don't forget your null terminator
    strcpy(tempNode->data, item); // Because some code in the main client code was messing with memory pointers, I decided to just copy the string, so I can have my own version of the file paths that I managed.
    tempNode->next = NULL;

    if (isEmpty(queue)) {
        queue->head = queue->tail = tempNode; // If empty, head and tail point to the same thing.
        return tempNode->data;
    }

    queue->tail->next = tempNode; // tail points to old tempNode. This says old tempNode now points to new tempNode
    queue->tail = tempNode; // Current tail also needs to point to new tempNode

    //pthread_mutex_unlock(&queue->lock);

    return item;
}

char *dequeue(struct Queue *queue) {
    if (isEmpty(queue)) {
        return NULL;
    }

    struct Node *tempNode = queue->head; // Get node to dequeue
    char *data = tempNode->data; // Need to store the data so we can use it later. Client is responsible for freeing this because it points to dynamic memory. This could be improved so the client doesn't have to do anything, but it's fine for now.

    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(tempNode);

    return data;
}

bool isEmpty(struct Queue *queue) {
    return (queue->head == NULL);
}
