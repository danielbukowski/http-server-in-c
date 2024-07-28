typedef struct arena_allocator arena_allocator;

arena_allocator* init_arena_allocator(size_t memory_arena_capacity, int memory_arena_number);

char* allocate_memory_area(arena_allocator* arena_allocator);

void free_memory_area(arena_allocator* arena_allocator, char* memory_arena);