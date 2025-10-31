#pragma once

#include <stdint.h>
#ifdef ARRAY_IMPLEMENTATION
#ifndef CORE_IMPLEMENTATION
#define CORE_IMPLEMENTATION
#endif
#endif
#include "core.h"

#ifdef ARRAY_IMPLEMENTATION
#ifndef ARENA_IMPLEMENTATION
#define ARENA_IMPLEMENTATION
#endif
#endif
#include "arena.h"

typedef struct {
    u64 length;
    u64 capacity;
} array_header_t;

#define array_from_size(type, arena, size) (type*)__array_from_size(arena, sizeof(type) * size) // TODO: Maybe move away from headers to a struct, but definitely store sizeof(type)
void* __array_from_size(arena_t* arena, u64 size);

array_header_t* get_array_header(void* array);
u64 get_array_length(void* array);
u64 get_array_capacity(void* array);

#ifdef ARRAY_IMPLEMENTATION

void* __array_from_size(arena_t* arena, const u64 size) {
    const uptr start = (uptr)arena_alloc(arena, sizeof(array_header_t) + size);

    array_header_t* header = (array_header_t*)start;
    header->length = 0;
    header->capacity = size;

    return (void*)(start + sizeof(array_header_t));
}

array_header_t* get_array_header(void* array) {
    return (array_header_t*)(((uptr)(array)) - align_size(sizeof(array_header_t), DEFAULT_ALIGNMENT)); // TODO: Add a runtime check for header? 0xBEEEEEEF
}

u64 get_array_length(void* array) {
    return get_array_header(array)->length;
}

u64 get_array_capacity(void* array) {
    return get_array_header(array)->capacity;
}

#endif
