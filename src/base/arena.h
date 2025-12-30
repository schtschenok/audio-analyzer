#pragma once

#ifdef ARENA_IMPLEMENTATION
#ifndef CORE_IMPLEMENTATION
#define CORE_IMPLEMENTATION
#endif
#ifndef MEMORY_IMPLEMENTATION
#define MEMORY_IMPLEMENTATION
#endif
#endif
#include "core.h"
#include "memory.h"

typedef struct {
    uptr start;
    u64 capacity;
    u64 position;
} arena_t;

static inline arena_t arena_make(u64 size);
static inline bool arena_valid(const arena_t* arena);
static inline void* arena_alloc(arena_t* arena, u64 size);
static inline void* arena_alloc_aligned(arena_t* arena, u64 size, u64 alignment);
static inline void arena_clear(arena_t* arena);
static inline void arena_release(arena_t* arena);
static inline bool arena_delete(arena_t* arena);

#ifdef ARENA_IMPLEMENTATION

#include <assert.h>

static inline arena_t arena_make(const u64 size) {
    assert(size > 0);

    const u64 aligned_size = align_size(size, get_page_size());

    const void* arena_start = memory_map_anonymous(aligned_size, false);

    if (arena_start == NULL) {
        return (arena_t){ .start = 0,
                          .capacity = 0,
                          .position = 0 };
    }

    ASAN_POISON_MEMORY_REGION((void*)arena_start, aligned_size);

    const arena_t arena = {
        .start = (uptr)arena_start,
        .capacity = aligned_size,
        .position = 0
    };
    return arena;
}

static inline bool arena_valid(const arena_t* arena) {
    if (!arena || !arena->start || !arena->capacity || arena->capacity < arena->position) {
        return false;
    }
    return true;
}

static inline void* arena_alloc(arena_t* arena, const u64 size) {
    assert(arena_valid(arena));

    return arena_alloc_aligned(arena, size, DEFAULT_ALIGNMENT);
}

static inline void* arena_alloc_aligned(arena_t* arena, const u64 size, const u64 alignment) {
    assert(arena_valid(arena));
    assert(is_power_of_two(alignment));

    const u64 aligned_size = align_size(size, alignment);

    if (arena->capacity - arena->position < aligned_size) {
        return NULL;
    }

    void* ptr = (void*)(arena->start + align_size(arena->position, alignment));
    arena->position += aligned_size;

#if defined(_WIN64)
    VirtualAlloc(ptr, aligned_size, MEM_COMMIT, PAGE_READWRITE);
#endif

    ASAN_UNPOISON_MEMORY_REGION(ptr, aligned_size);

    return ptr;
}

static inline void arena_clear(arena_t* arena) {
    assert(arena_valid(arena));

    ASAN_POISON_MEMORY_REGION((void*)arena->start, arena->capacity);

    arena->position = 0;
}

static inline void arena_release(arena_t* arena) {
    assert(arena_valid(arena));

    memory_dontneed((void*)arena->start, arena->capacity);

    ASAN_POISON_MEMORY_REGION((void*)arena->start, arena->capacity);

    arena->position = 0;
}

static inline bool arena_delete(arena_t* arena) {
    assert(arena_valid(arena));

    ASAN_POISON_MEMORY_REGION((void*)arena->start, arena->capacity);

    const bool result = memory_unmap_anonymous((void*)arena->start, arena->capacity);

    arena->start = 0;
    arena->capacity = 0;
    arena->position = 0;

    return result;
}

#endif
