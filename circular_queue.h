typedef struct circular_queue circular_queue;

static bool isFull(circular_queue* queue);

static bool isEmpty(circular_queue* queue);

circular_queue* init_queue(int queue_request_capacity);

bool enqueue(circular_queue* queue, int element);

int dequeue(circular_queue* queue);