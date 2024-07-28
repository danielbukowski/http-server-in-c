#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h> 

#include "arena_allocator.h"

enum MEMORY_ARENA_STATUS {
    FREE,
    ALLOCATED
};

typedef struct memory_arena {
    char*                     arena_pointer;
    enum MEMORY_ARENA_STATUS  status;
} memory_arena;

typedef struct arena_allocator {
    char*         base;
    int           memory_arena_number;
    memory_arena* memory_arenas;
} arena_allocator;

arena_allocator* init_arena_allocator(size_t memory_arena_capacity, int memory_arena_number)
{
    size_t base_size = (sizeof(char) * memory_arena_capacity * memory_arena_number) + (sizeof(char) * memory_arena_number);

    arena_allocator* arena = malloc(sizeof(arena_allocator));
    if (arena == NULL) return NULL;
    
    arena->memory_arena_number = memory_arena_number;

    char* new_base_ptr = realloc(arena->base, base_size);
    if (new_base_ptr == NULL) {
        free(arena);
        return NULL;
    }
    arena->base = new_base_ptr;

    memory_arena* new_memory_arenas_ptr = realloc(arena->memory_arenas, (sizeof(memory_arena) * arena->memory_arena_number));
    if (new_memory_arenas_ptr == NULL) {
        free(arena);
        return NULL;
    }
    arena->memory_arenas = new_memory_arenas_ptr;

    memset(arena->base, '\0', base_size);

    arena->memory_arenas[0].arena_pointer = arena->base;
    for (int i = 1; i < arena->memory_arena_number; i++)
    {
        arena->memory_arenas[i].arena_pointer = arena->base + (1 + i ) + (memory_arena_capacity * i);
    }

    return arena;
}

char* allocate_memory_area(arena_allocator* arena_allocator)
{
    for (int i = 0; i < arena_allocator->memory_arena_number; i++)
    {
        if (arena_allocator->memory_arenas[i].status == FREE)
        {
            arena_allocator->memory_arenas[i].status = ALLOCATED;
            return arena_allocator->memory_arenas[i].arena_pointer;
        }
    }

    return NULL;
}

void free_memory_area(arena_allocator* arena_allocator, char* memory_arena)
{
    for (int i = 0; i < arena_allocator->memory_arena_number; i++)
    {
        if (arena_allocator->memory_arenas[i].arena_pointer == memory_arena)
        {
            arena_allocator->memory_arenas[i].status = FREE;
            return;
        }
    }
}