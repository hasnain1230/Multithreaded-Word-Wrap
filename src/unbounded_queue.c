#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "unbounded_queue.h"

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


    struct Node *temp = (struct Node *) malloc(sizeof(struct Node));

    temp->data = malloc(strlen(item) + 1);
    strcpy(temp->data, item);
    temp->next = NULL;

    if (isEmpty(queue)) {
        queue->head = queue->tail = temp;
        return temp->data;
    }

    queue->tail->next = temp; // tail points to old temp. This says old temp now points to new temp
    queue->tail = temp;

    //pthread_mutex_unlock(&queue->lock);

    return item;
}

char *dequeue(struct Queue *queue) {
    if (isEmpty(queue)) {
        return NULL;
    }

    struct Node *temp = queue->head;
    char *data = temp->data;

    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(temp);

    return data;
}

bool isEmpty(struct Queue *queue) {
    return (queue->head == NULL);
}
