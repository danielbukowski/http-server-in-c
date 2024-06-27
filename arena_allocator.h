typedef struct arena_allocator arena_allocator;

arena_allocator* init_arena_allocator();

char* allocate_memory_area(arena_allocator* arena_allocator);

void free_memory_area(arena_allocator* arena_allocator, char* memory_arena);