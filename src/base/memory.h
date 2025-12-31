#pragma once

#ifdef MEMORY_IMPLEMENTATION
#ifndef CORE_IMPLEMENTATION
#define CORE_IMPLEMENTATION
#endif
#endif
#include "core.h"

static inline void* memory_map_anonymous(const u64 size, const bool populate);
static inline bool memory_unmap_anonymous(void* ptr, const u64 size);
static inline bool memory_dontneed(void* ptr, const u64 size);
static inline u8* memory_map_file(const i32 fd, const i64 size, const bool read, const bool write, const bool populate);
static inline bool memory_unmap_file(void* ptr, const u64 size);

#ifdef MEMORY_IMPLEMENTATION

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

static inline void* memory_map_anonymous(const u64 size, const bool populate) {
#if defined(_WIN32)
    void* alloc = VirtualAlloc(NULL, align_size(size, get_page_size()), populate ? MEM_RESERVE | MEM_COMMIT : MEM_RESERVE, PAGE_READWRITE);
    assert(alloc != NULL);
    return alloc;
#elif defined(__linux__)
    void* alloc = mmap(NULL,
                       align_size(size, get_page_size()),
                       PROT_READ | PROT_WRITE,
                       populate ? MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE : MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);
    assert(alloc != MAP_FAILED);
    return alloc;
#else
    return NULL;
#endif
}

static inline bool memory_unmap_anonymous(void* ptr, const u64 size) {
#if defined(_WIN32)
    const bool result = VirtualFree(ptr, 0, MEM_RELEASE);
    assert(result);
    return result;
#elif defined(__linux__)
    const bool result = munmap(ptr, size) == 0;
    assert(result);
    return result;
#else
    return false;
#endif
}

static inline bool memory_dontneed(void* ptr, const u64 size) {
#if defined(_WIN32)
    const bool result = VirtualAlloc(ptr, align_size(size, get_page_size()), MEM_DECOMMIT, PAGE_READWRITE) != NULL;
    assert(result);
    return result;
#elif defined(__linux__)
    const bool result = madvise(ptr, size, MADV_FREE) == 0;
    assert(result);
    return result;
#else
    return false;
#endif
}

static inline u8* memory_map_file(const i32 fd, const i64 size, const bool read, const bool write, const bool populate) {
    assert(fd >= 0 && size > 0 && (read || write));

#ifdef _WIN32
    HANDLE file = (HANDLE)_get_osfhandle(fd);
    if (file == NULL) {
        assert(false);
        return NULL;
    }

    HANDLE mapping = CreateFileMapping(file, NULL, write ? PAGE_READWRITE : (read ? PAGE_READONLY : PAGE_NOACCESS), 0, 0, NULL);
    if (mapping == NULL) {
        assert(false);
        return NULL;
    }

    u8* data = MapViewOfFile(mapping, write ? (read ? FILE_MAP_ALL_ACCESS : FILE_MAP_WRITE) : (read ? FILE_MAP_READ : 0), 0, 0, 0);
    CloseHandle(mapping);
    if (data == NULL) {
        assert(false);
        return NULL;
    }

    if (populate) {
        WIN32_MEMORY_RANGE_ENTRY range;
        range.VirtualAddress = data;
        range.NumberOfBytes = align_size(size, get_page_size());
        PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
    }

    return data;
#elif defined(__linux__)
    u8* data = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (data == MAP_FAILED) {
        assert(false);
        return NULL;
    }

    return data;
#else
    return NULL;
#endif
}

static inline bool memory_unmap_file(void* ptr, const u64 size) {
#ifdef _WIN32
    const bool result = UnmapViewOfFile(ptr);
    assert(result);
    return result;
#elif defined(__linux__)
    const bool result = munmap(ptr, size) == 0;
    assert(result);
    return result;
#else
    return false;
#endif
}

#endif
