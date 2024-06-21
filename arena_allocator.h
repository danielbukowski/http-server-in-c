
typedef struct arena_allocator arena_allocator;

arena_allocator* init_arena_allocator();

char* allocate_memory_area(arena_allocator* arena);

void free_memory_area(arena_allocator* arena, char* memory_arena);