#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */ // DONE
        if (q == NULL || proc == NULL) return;
        if (q->size >= MAX_QUEUE_SIZE)
        {
                printf("Queue is full\n");
                return;
        }
        q->proc[q->size++] = proc;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */ // DONE
        if (q == NULL || q->size == 0) return NULL;
        struct pcb_t *proc = q->proc[0];
        q->size--;
        for(int i = 0; i < q->size; i++)
        {
                q->proc[i] = q->proc[i+1];
        }
	return proc;
}


