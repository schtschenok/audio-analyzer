#pragma once

#include <stdio.h>

#ifdef STRING_IMPLEMENTATION
#ifndef CORE_IMPLEMENTATION
#define CORE_IMPLEMENTATION
#endif
#endif
#include "core.h"

#ifdef STRING_IMPLEMENTATION
#ifndef ARENA_IMPLEMENTATION
#define ARENA_IMPLEMENTATION
#endif
#endif
#include "arena.h"

typedef struct {
    char* start;
    fu32 length;
    fu32 capacity;
} str_t;

str_t str_from_size(arena_t* arena, size_t length);

str_t str_from_cstr(arena_t* arena, const char* cstr);

bool str_valid(const str_t* str);

char* str_to_cstr(arena_t* arena, str_t str);

bool str_copy(str_t* destination, const str_t source);

bool str_eq(const str_t str1, const str_t str2);

bool str_eq_cstr(const str_t str, const char* cstr);

str_t str_slice(const str_t source, size_t start, size_t end);

str_t str_trim(const str_t str);

bool str_append(str_t* destination, const str_t source);

bool str_append_cstr(str_t* destination, const char* source);

str_t str_from_append(arena_t* arena, const str_t destination, const str_t source);

str_t str_from_append_cstr(arena_t* arena, const str_t destination, const char* source);

str_t str_from_i64(i64 i);
str_t str_from_i32(i32 i);
str_t str_from_i16(i16 i);
str_t str_from_i8(i8 i);
str_t str_from_u64(i64 i);
str_t str_from_u32(i32 i);
str_t str_from_u16(i16 i);
str_t str_from_u8(i8 i);

i64 str_to_i64(str_t str);
i32 str_to_i32(str_t str);
i16 str_to_i16(str_t str);
i8 str_to_i8(str_t str);
i64 str_to_u64(str_t str);
i32 str_to_u32(str_t str);
i16 str_to_u16(str_t str);
i8 str_to_u8(str_t str);

void str_write(const str_t str, FILE* file, bool newline);

// NOT SURE IF I EVEN NEED THIS AT ALL
// bool str_find_next_after(const str_t str, char character, i64* position);

bool str_tokenize(const str_t str, char separator, i64* context, str_t* token);

#ifdef STRING_IMPLEMENTATION

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

str_t str_from_size(arena_t* arena, const size_t length) {
    assert(arena_valid(arena));
    const void* ptr = arena_alloc(arena, length);
    if (ptr) {
        return (str_t){
            .start = (char*)ptr,
            .length = 0,
            .capacity = length
        };
    }
    return (str_t){
        .start = NULL,
        .length = 0,
        .capacity = 0
    };
}

str_t str_from_cstr(arena_t* arena, const char* cstr) {
    assert(arena_valid(arena));
    assert(cstr);

    const fu32 source_string_length = strlen(cstr);
    const void* ptr = arena_alloc(arena, source_string_length);

    memmove((char*)ptr, cstr, source_string_length);

    const str_t string = {
        .start = (char*)ptr,
        .capacity = source_string_length,
        .length = source_string_length
    };

    return string;
}

bool str_valid(const str_t* str) {
    if (!str || !str->start || str->length > str->capacity) {
        return false;
    }
    return true;
}

char* str_to_cstr(arena_t* arena, str_t str) {
    assert(str_valid(&str));

    char* cstr = arena_alloc(arena, str.length + 1);
    memmove(cstr, str.start, str.length);
    cstr[str.length] = '\0';
    return cstr;
}

bool str_copy(str_t* destination, const str_t source) {
    assert(str_valid(&source));
    assert(str_valid(destination));

    if (destination->capacity < source.length) {
        return false;
    }

    memmove(destination->start, source.start, source.length);
    destination->length = source.length;
    return true;
}

bool str_eq(const str_t str1, const str_t str2) {
    assert(str_valid(&str1));
    assert(str_valid(&str2));

    if (str1.length != str2.length) {
        return false;
    }

    if (str1.length == 0) {
        return true;
    }

    return memcmp(str1.start, str2.start, str1.length) == 0;
}

bool str_eq_cstr(const str_t str, const char* cstr) {
    assert(str_valid(&str));
    assert(cstr);

    if (str.length != strlen(cstr)) {
        return false;
    }

    if (str.length == 0) {
        return true;
    }

    return memcmp(str.start, cstr, str.length) == 0;
}

str_t str_slice(const str_t source, const size_t start, const size_t end) {
    assert(str_valid(&source));
    assert(end >= start);
    assert(start <= source.capacity);
    assert(end <= source.capacity);

    return (str_t){
        .start = source.start + start,
        .length = end - start,
        .capacity = source.capacity - start
    };
}

static const unsigned char trim_lut[256] = {
    [' '] = 1,
    ['\t'] = 1,
    ['\n'] = 1,
    ['\r'] = 1
};

static inline bool is_trim_char(const unsigned char c) {
    return trim_lut[c] != 0;
}

str_t str_trim(const str_t str) {
    assert(str_valid(&str));

    size_t start = 0;
    size_t end = str.length;

    while (start < end && is_trim_char(str.start[start])) {
        start++;
    }
    while (end > start && is_trim_char(str.start[end - 1])) {
        end--;
    }

    str_t result;
    result.start = (start == end) ? (str.start + end) : (str.start + start);
    result.length = end - start;
    result.capacity = str.length - start;
    return result;
}

bool str_append(str_t* destination, const str_t source) {
    assert(str_valid(destination));
    assert(str_valid(&source));

    if (destination->capacity - destination->length < source.length) {
        return false;
    }

    memmove(destination->start + destination->length, source.start, source.length);
    return true;
};

bool str_append_cstr(str_t* destination, const char* source) {
    assert(str_valid(destination));
    assert(source);

    const size_t source_length = strlen(source);

    if (destination->capacity - destination->length < source_length) {
        return false;
    }

    memmove(destination->start + destination->length, source, source_length);
    return true;
};

str_t str_from_append(arena_t* arena, const str_t destination, const str_t source) {
    assert(arena_valid(arena));
    assert(str_valid(&destination) && str_valid(&source));

    const size_t result_length = destination.length + source.length;
    str_t result = str_from_size(arena, result_length);
    memmove(result.start, destination.start, destination.length);
    memmove(result.start + destination.length, source.start, source.length);
    return result;
};

str_t str_from_append_cstr(arena_t* arena, const str_t destination, const char* source) {
    assert(arena_valid(arena));
    assert(str_valid(&destination) && source);

    const size_t source_length = strlen(source);
    const size_t result_length = destination.length + source_length;
    str_t result = str_from_size(arena, result_length);
    memmove(result.start, destination.start, destination.length);
    memmove(result.start + destination.length, source, source_length);
    return result;
};

// str_t str_from_i64(i64 i);

// str_t str_from_i32(i32 i);

// str_t str_from_i16(i16 i);

// str_t str_from_i8(i8 i);

// str_t str_from_u64(i64 i);;

// str_t str_from_u32(i32 i);

// str_t str_from_u16(i16 i);

// str_t str_from_u8(i8 i);

// i64 str_to_i64(str_t str);

// i32 str_to_i32(str_t str);

// i16 str_to_i16(str_t str);

// i8 str_to_i8(str_t str);

// i64 str_to_u64(str_t str);

// i32 str_to_u32(str_t str);

// i16 str_to_u16(str_t str);

// i8 str_to_u8(str_t str);

void str_write(const str_t str, FILE* file, const bool newline) {
    assert(str_valid(&str));

    fwrite(str.start, sizeof(char), str.length, file);

    if (newline) {
        fwrite("\n", 1, 1, file);
    }
}

// NOT SURE IF I EVEN NEED THIS AT ALL
// Position should be initialized as -1 to start from the beginning
// bool str_find_next_after(const str_t str, const char character, i64* position) {
//     assert(str_valid(&str));
//     assert(position);
//     assert(*position < str.length - 1); // TODO: Added "- 1", NEEDS TESTING
//     assert(*position >= -1);

//     if (*position >= str.length) {
//         return false;
//     }

//     while (*position < (i64)str.length) {
//         (*position)++;
//         if (str.start[*position] == character) {
//             while (*position < (i64)str.length && str.start[*position] == character) {
//                 (*position)++;
//             }
//             return true;
//         }
//     }
//     return false;
// }

// TODO: Can I do this better?
// Context should be initialized as -1 to start from the beginning
bool str_tokenize(const str_t str, const char separator, i64* context, str_t* token) {
    assert(str_valid(&str));
    assert(context);
    assert(*context >= -1);

    if (*context >= str.length) {
        return false;
    }

    i64 start_position = *context + 1;
    bool seen_non_sep = false;
    i64 first_sep_found = -1;
    i64 last_sep_found = -1;
    for (i64 position = start_position; position < str.length; position++) {
        if (str.start[position] == separator) {
            if (!seen_non_sep) {
                start_position++;
                continue;
            }
            if (first_sep_found == -1) {
                first_sep_found = position;
            }
            last_sep_found = position;
        } else {
            seen_non_sep = true;
            if (last_sep_found != -1) {
                break;
            }
            if (position == str.length - 1) {
                first_sep_found = str.length;
                last_sep_found = str.length;
                break;
            }
        }
    }

    if (first_sep_found == -1) {
        return false;
    }

    token->start = str.start + start_position;
    token->length = first_sep_found - start_position;
    token->capacity = token->length;

    *context = last_sep_found;

    return true;
}

#endif
