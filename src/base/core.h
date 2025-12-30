#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS // NOLINT
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

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

static inline bool is_power_of_two(u64 x);
static inline u64 align_size(const u64 size, const u64 alignment);
static inline u64 get_page_size();

#ifdef CORE_IMPLEMENTATION

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static inline bool is_power_of_two(const u64 x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static inline u64 align_size(const u64 size, const u64 alignment) {
    assert(is_power_of_two(alignment));
    return (size + (alignment - 1)) & ~(alignment - 1);
}

static inline u64 get_page_size() {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#elif defined(__linux__)
    return getpagesize();
#else
    return 0;
#endif
}

#if defined(__GNUC__) || defined(__clang__)
#define int3() __asm__ volatile("int3")
#elif defined(_MSC_VER)
#define int3() __debugbreak()
#endif

#if defined(__SANITIZE_ADDRESS__) && !defined(__MINGW32__) // Cause MinGW doesn't seem to support ASAN
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

#endif
