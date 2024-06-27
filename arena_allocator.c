#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "arena_allocator.h"

#define MEMORY_ARENA_NUMBER  3
#define MEMORY_ARENA_CAPACITY 4096

enum MEMORY_ARENA_STATUS {
    FREE,
    ALLOCATED
};

typedef struct memory_arena {
    char* arena_pointer;
    enum  MEMORY_ARENA_STATUS status;
} memory_arena;

typedef struct arena_allocator {
    char base[(sizeof(char) * MEMORY_ARENA_CAPACITY * MEMORY_ARENA_NUMBER) + sizeof(char) * MEMORY_ARENA_NUMBER];
    memory_arena* arenas;
} arena_allocator;

arena_allocator* init_arena_allocator()
{
    arena_allocator* arena = malloc(sizeof(arena_allocator));
    arena->arenas = realloc(arena->arenas, (sizeof(memory_arena) * MEMORY_ARENA_NUMBER));

    if (arena == NULL) return NULL;

    arena->arenas[0].arena_pointer = &arena->base[0];
    arena->arenas[1].arena_pointer = &arena->base[2 + MEMORY_ARENA_CAPACITY * 1];
    arena->arenas[2].arena_pointer = &arena->base[3 + MEMORY_ARENA_CAPACITY * 2];

    return arena;
}

char* allocate_memory_area(arena_allocator* arena)
{
    for (int i = 0; i < MEMORY_ARENA_NUMBER; i++)
    {
        if (arena->arenas[i].status == FREE)
        {
            arena->arenas[i].status = ALLOCATED;
            return arena->arenas[i].arena_pointer;
        }
    }

    return NULL;
}

void free_memory_area(arena_allocator* arena, char* memory_arena)
{
    for (int i = 0; i < MEMORY_ARENA_NUMBER; i++)
    {
        if (arena->arenas[i].arena_pointer == memory_arena)
        {
            arena->arenas[i].status = FREE;
            return;
        }
    }
}