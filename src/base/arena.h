#pragma once

#ifdef ARENA_IMPLEMENTATION
#ifndef CORE_IMPLEMENTATION
#define CORE_IMPLEMENTATION
#endif
#endif
#include "core.h"

typedef struct {
    uptr start;
    u64 capacity;
    u64 position;
} arena_t;

arena_t arena_make(u64 size);
static inline bool arena_valid(const arena_t* arena);
static inline void* arena_alloc(arena_t* arena, u64 size);
static inline void* arena_alloc_aligned(arena_t* arena, u64 size, u64 alignment);
static inline void arena_clear(arena_t* arena);
static inline void arena_release(arena_t* arena);
bool arena_delete(arena_t* arena);

#ifdef ARENA_IMPLEMENTATION

#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include "sanitizer/asan_interface.h"
#define ASAN_POISON_MEMORY_REGION(addr, size) \
    __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
    __asan_unpoison_memory_region((addr), (size))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) \
    ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
    ((void)(addr), (void)(size))
#endif

arena_t arena_make(const u64 size) {
    assert(size > 0);

    const u64 aligned_size = align_size(size, getpagesize());

    const void* arena_start = mmap(NULL,
                                   aligned_size,
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS,
                                   -1,
                                   0);
    if (arena_start == MAP_FAILED) {
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

    void* ptr = (void*)(arena->start + arena->position);
    arena->position += aligned_size;

    ASAN_UNPOISON_MEMORY_REGION((void*)ptr, aligned_size);

    return ptr;
}

static inline void arena_clear(arena_t* arena) {
    assert(arena_valid(arena));

    ASAN_POISON_MEMORY_REGION((void*)arena->start, arena->capacity);

    arena->position = 0;
}

static inline void arena_release(arena_t* arena) {
    assert(arena_valid(arena));

    madvise((void*)arena->start, arena->capacity, MADV_DONTNEED);

    ASAN_POISON_MEMORY_REGION((void*)arena->start, arena->capacity);

    arena->position = 0;
}

bool arena_delete(arena_t* arena) {
    assert(arena_valid(arena));

    ASAN_POISON_MEMORY_REGION((void*)arena->start, arena->capacity);

    const bool result = munmap((void*)arena->start, arena->capacity) == 0;

    arena->start = 0;
    arena->capacity = 0;
    arena->position = 0;

    return result;
}

#endif
