typedef struct memory_arena memory_arena;

memory_arena* init_memory_arena();

void* allocate_chunk(memory_arena* memory_arena);

void free_chunk(memory_arena* memory_arena, void* chunk);