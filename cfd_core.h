#ifndef CFD_LIB_CORE_H
#define CFD_LIB_CORE_H

#ifndef CFD_LIB
#define CFD_LIB
#endif

#if defined(__linux__) || defined(__unix__)
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
#endif

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u32 b32;

#ifndef __cplusplus
    #ifndef true
        #define true 1
    #endif
    #ifndef false
        #define false 0
    #endif
#endif

#define KB(x) ((u64)(x) * 1024ULL)
#define MB(x) (KB(x) * 1024ULL)
#define GB(x) (MB(x) * 1024ULL)

#define NULL ((void *)0)

#ifdef _MSC_VER
#define force_inline __forceinline
#elif defined (__GNUC__)
#define force_inline inline __attribute__((always_inline))
#endif


typedef enum {
    CFD_LOG_LEVEL_INFO,
    CFD_LOG_LEVEL_WARNING,
    CFD_LOG_LEVEL_ERROR,
} CFD_Log_Level;

typedef struct CFD_Arena {
    u8 *buffer;
    u64 offset;
    u64 cap;
    b32 is_mmaped;
} CFD_Arena;

// LOGGING
typedef void (*CFD_Log_Fn)(CFD_Log_Level level, const char *fmt, va_list args);

#define cfd_info(fmt, ...)  cfd_log(CFD_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define cfd_warn(fmt, ...)  cfd_log(CFD_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define cfd_error(fmt, ...) cfd_log(CFD_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

CFD_LIB void cfd_log(CFD_Log_Level level, const char *fmt, ...);
CFD_LIB void cfd_set_logger(CFD_Log_Fn fn);
CFD_LIB void cfd_default_logger(CFD_Log_Level level, const char *fmt, va_list);


// ARENA
CFD_LIB b32 cfd_arena_init(CFD_Arena *arena, u64 size);
CFD_LIB void cfd_arena_init_from_buffer(CFD_Arena *arena, void *buffer, u64 size);
CFD_LIB void cfd_arena_reset(CFD_Arena *arena);
CFD_LIB b32 cfd_arena_destroy(CFD_Arena *arena);

static force_inline void *cfd_arena_alloc_aligned(CFD_Arena *arena, u64 size, u64 alignment) {
    u64 current_ptr = (u64)arena->buffer + arena->offset;
    u64 aligned_ptr = (current_ptr + (alignment - 1)) & ~(alignment - 1);
    u64 new_offset = (aligned_ptr - (uintptr_t)arena->buffer) + size;

    if (new_offset <= arena->cap) {
        arena->offset = new_offset;
        return (void *)aligned_ptr;
    }

    cfd_error("Arena out of memory! Requested: %zu, Available: %zu", size, arena->cap - arena->offset);
    return NULL;
}

static force_inline void *cfd_arena_alloc(CFD_Arena *arena, u64 size) {
    return cfd_arena_alloc_aligned(arena, size, 8);
}

static force_inline void *cfd_arena_alloc_zero_aligned(CFD_Arena *arena, u64 size, u64 alignment) {
    void *ptr = cfd_arena_alloc_aligned(arena, size, alignment);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

static force_inline void *cfd_arena_alloc_zero(CFD_Arena *arena, u64 size) {
    return cfd_arena_alloc_zero_aligned(arena, size, 8);
}

#define cfd_arena_push_type(arena, type) \
    (type *)cfd_arena_alloc_aligned((arena), sizeof(type), _Alignof(type))

#define cfd_arena_push_array(arena, type, count) \
    (type *)cfd_arena_alloc_aligned((arena), sizeof(type) * (count), _Alignof(type))

#define cfd_arena_push_array_zero(arena, type, count) \
    (type *)cfd_arena_alloc_zero_aligned((arena), sizeof(type) * (count), _Alignof(type))

#ifdef CFD_LIB_IMPLEMENTATION

#include <stdio.h>
#include <errno.h>

#ifdef __unix__
#include <sys/mman.h>
#endif

static CFD_Log_Fn g_logger = cfd_default_logger;

CFD_LIB void cfd_log(CFD_Log_Level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    g_logger(level, fmt, args);
    va_end(args);
}

CFD_LIB void cfd_set_logger(CFD_Log_Fn fn) {
    g_logger = fn ? fn : cfd_default_logger;
}

CFD_LIB void cfd_default_logger(CFD_Log_Level level, const char *fmt, va_list args) {
    switch(level) {
        case CFD_LOG_LEVEL_INFO:
            printf("[INFO]: ");
            vprintf(fmt, args);
            printf("\n");
            break;
        case CFD_LOG_LEVEL_WARNING:
            fprintf(stderr, "[WARNING]: ");
            vfprintf(stderr, fmt, args);
            fprintf(stderr, "\n");
            break;
        case CFD_LOG_LEVEL_ERROR:
            fprintf(stderr, "[ERROR]: ");
            vfprintf(stderr, fmt, args);
            fprintf(stderr, "\n");
            break;
        default:
            break;
    }
}


CFD_LIB b32 cfd_arena_init(CFD_Arena *arena, u64 size) {
#ifdef __unix__
    arena->buffer = (u8 *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (arena->buffer == MAP_FAILED) {
        cfd_error("mmap() failed: could not allocate virtual memory for arena allocator: %s", strerror(errno));
        return false;
    }
#else
#error "Only UNIX is supported yet"
#endif

    arena->offset = 0;
    arena->cap = size;
    arena->is_mmaped = true;

    return true;
}

CFD_LIB void cfd_arena_init_from_buffer(CFD_Arena *arena, void *buffer, u64 size) {
    if (!buffer) return;

    arena->buffer = (u8 *)buffer;
    arena->offset = 0;
    arena->cap = size;
    arena->is_mmaped = false;
}

CFD_LIB void cfd_arena_reset(CFD_Arena *arena) {
    arena->offset = 0;
}

CFD_LIB b32 cfd_arena_destroy(CFD_Arena *arena) {
#ifdef __unix__
    if (arena->is_mmaped && arena->buffer) {
        if (munmap(arena->buffer, arena->cap) != 0) {
            cfd_error("munmap() failed: could not unmap virtual memory for arena allocator: %s", strerror(errno));
            return false;
        }
    }
#else
#error "Only UNIX is supported yet"
#endif

    arena->buffer = NULL;
    arena->offset = 0;
    arena->cap = 0;
    arena->is_mmaped = false;
    return true;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_LIB_CORE_H */
