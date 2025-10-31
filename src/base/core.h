#pragma once

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef bool b;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uintptr_t uptr;
typedef intptr_t iptr;
typedef ptrdiff_t ptrd;

typedef uint_fast8_t fu8;
typedef uint_fast16_t fu16;
typedef uint_fast32_t fu32;
typedef uint_fast64_t fu64;

typedef int_fast8_t fi8;
typedef int_fast16_t fi16;
typedef int_fast32_t fi32;
typedef int_fast64_t fi64;

typedef atomic_uint_least8_t a_u8;
typedef atomic_uint_least8_t a_u16;
typedef atomic_uint_least32_t a_u32;
typedef atomic_uint_least64_t a_u64;

typedef atomic_int_least8_t a_i8;
typedef atomic_int_least16_t a_i16;
typedef atomic_int_least32_t a_i32;
typedef atomic_int_least64_t a_i64;

typedef atomic_uintptr_t a_uptr;
typedef atomic_intptr_t a_iptr;
typedef atomic_ptrdiff_t a_ptrd;

typedef atomic_uint_fast8_t a_fu8;
typedef atomic_uint_fast16_t a_fu16;
typedef atomic_uint_fast32_t a_fu32;
typedef atomic_uint_fast64_t a_fu64;

typedef atomic_int_fast8_t a_fi8;
typedef atomic_int_fast16_t a_fi16;
typedef atomic_int_fast32_t a_fi32;
typedef atomic_int_fast64_t a_fi64;

typedef char c;
typedef unsigned char uc;
typedef atomic_char a_c;
typedef atomic_uchar a_uc;

typedef float f32;
typedef double f64;

#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)
#define TB(n) (((u64)(n)) << 40)

#define DEFAULT_ALIGNMENT 8

static inline bool is_power_of_two(size_t x);
static inline size_t align_size(const size_t size, const size_t alignment);

#ifdef CORE_IMPLEMENTATION

static inline bool is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static inline size_t align_size(const size_t size, const size_t alignment) {
    assert(is_power_of_two(alignment));
    return (size + (alignment - 1)) & ~(alignment - 1);
}

#endif
