#include <stdio.h>
#include <stdbool.h>

#define QUEUE_SIZE 10

typedef struct circular_queue circular_queue;

bool isFull(circular_queue* queue);

bool isEmpty(circular_queue* queue);

circular_queue* init_queue();

bool enqueue(circular_queue* queue, int element);

bool dequeue(circular_queue* queue);