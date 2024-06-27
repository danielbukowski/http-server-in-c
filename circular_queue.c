#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "circular_queue.h"

#define QUEUE_CAPACITY 1024

typedef struct circular_queue {
	int elements[QUEUE_CAPACITY];
	int front;
	int rear;
} circular_queue;

static bool isFull(circular_queue* queue) 
{
	return ((queue->front == queue->rear + 1) || (queue->front == 0 && queue->rear == QUEUE_CAPACITY - 1));
}

static bool isEmpty(circular_queue* queue) 
{
	return (queue->front == -1);
}

circular_queue* init_queue() 
{
	circular_queue* queue = malloc(sizeof(circular_queue));

	if (queue == NULL) return NULL;

	queue->front = -1;
	queue->rear = -1;

	return queue;
}

bool enqueue(circular_queue* queue, int element) 
{
	if (isFull(queue)) return false;

	if (queue->front == -1) queue->front = 0;

	queue->rear = (queue->rear + 1) % QUEUE_CAPACITY;
	queue->elements[queue->rear] = element;

	return true;
}

int dequeue(circular_queue* queue) 
{
	if (isEmpty(queue)) return -1;

	int element = queue->elements[queue->front];

	if (queue->front == queue->rear) 
	{
		queue->front = -1;
		queue->rear = -1;
		return element;
	}

	queue->front = (queue->front + 1) % QUEUE_CAPACITY;

	return element;
}