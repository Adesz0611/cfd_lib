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

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #define PATH_MAX MAX_PATH
#elif defined(__unix__)
    #include <limits.h>
#else
    #define PATH_MAX 512
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

typedef struct CFD_File {
    u8 *buffer;
    u64 size;
    u64 cur;
} CFD_File;

typedef struct Str8 {
    u8 *buffer;
    u64 len;
} Str8;


// LOGGING
typedef void (*CFD_Log_Fn)(CFD_Log_Level level, const char *fmt, va_list args);

#define cfd_info(...)  cfd_log(CFD_LOG_LEVEL_INFO, __VA_ARGS__)
#define cfd_warn(...)  cfd_log(CFD_LOG_LEVEL_WARNING, __VA_ARGS__)
#define cfd_error(...) cfd_log(CFD_LOG_LEVEL_ERROR, __VA_ARGS__)

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


// FILE HANDLING

CFD_LIB b32 cfd_file_slurp(char *filename, CFD_File *file);
CFD_LIB b32 cfd_file_free(CFD_File *file);
CFD_LIB void cfd_dirname(char *path, char *dest);
CFD_LIB Str8 cfd_file_readline(CFD_File *file);

#define IS_CFD_FILE_EOF(f) (f->cur >= f->size)


// STRING HANDLING

#define Str8_Fmt "%.*s"
#define Str8_Arg(s) (int)(s).len, (s).buffer

static force_inline Str8 Str8_From_Zstr(u8 *txt, u64 len) { Str8 res = { (u8*)txt, len }; return res; }
#define Str8_Lit(s) Str8_From_Zstr((u8 *)s, sizeof(s) - 1)

#ifdef CFD_LIB_IMPLEMENTATION

#include <stdio.h>
#include <errno.h>

#ifdef __unix__
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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

CFD_LIB b32 cfd_file_slurp(char *filename, CFD_File *file) {
#ifdef __unix__
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        cfd_error("failed to open file '%s' via open(): %s", filename, strerror(errno));
        return false;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        cfd_error("fstat() failed: %s", strerror(errno));
        close(fd);
        return false;
    }

    file->size = sb.st_size;

    file->buffer = (u8 *)mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file->buffer == MAP_FAILED) {
        cfd_error("mmap() failed: %s", strerror(errno));
        close(fd);
        return false;
    }

    close(fd);

#else

    FILE *f = fopen(filename, "rb");
    if (!f) {
        cfd_error("fopen() failed: %s", strerror(errno));
        return false;
    }

    fseek(f, 0, SEEK_END);
    file->size = ftell(f);
    rewind(f);

    file->buffer = (u8 *)malloc(file->size);
    if (!file->buffer) {
        cfd_error("malloc() failed: %s", strerror(errno));
        fclose(f);
        return false;
    }

    u64 bytes_read = fread(file->buffer, 1, file->size, f);
    if (bytes_read != file->size) {
        cfd_error("fread() failed: %s", strerror(errno));
        free(file->buffer);
        fclose(f);
        return false;
    }

    fclose(f);

#endif /* __unix__ */

    file->cur = 0;

    return true;
}

CFD_LIB b32 cfd_file_free(CFD_File *file) {
#ifdef __unix__
    if (munmap(file->buffer, file->size) != 0) {
        cfd_error("munmap() failed: could not unmap mapped file: %s", strerror(errno));
        return false;
    }
#else
    free(file->buffer);
#endif

    return true;
}

CFD_LIB void cfd_dirname(char *path, char *dest) {
    if (!path || !dest) {
        cfd_error("path or destination is null in cfd_dirname()!");
        return;
    }

    u64 path_len = strlen(path);
    if (path_len == 0) {
        dest[0] = '.';
        dest[1] = '\0';
        return;
    }

    if (path_len > PATH_MAX) {
        cfd_error("The length of path(%zu) is longer than OS PATH_MAX(%zu)!", path_len, PATH_MAX);
        return;
    }

    s64 last_slash_index = -1;
    for (s64 i = (s64)path_len - 1; i >= 0; --i) {
        if (path[i] == '/'
#ifdef _WIN32
            || path[i] == '\\'
#endif
           ) {
            last_slash_index = i;
            break;
        }
    }

    if (last_slash_index > 0) {
        memcpy(dest, path, last_slash_index);
        dest[last_slash_index] = '\0';
    } else if (last_slash_index == 0) {
        dest[0] = path[0];
        dest[1] = '\0';
    } else {
        dest[0] = '.';
        dest[1] = '\0';
    }
}

CFD_LIB Str8 cfd_file_readline(CFD_File *file) {
    if (file->cur >= file->size)
        return (Str8){0};

    Str8 line;
    line.buffer = file->buffer + file->cur;
    line.len = 0;

    while (file->cur < file->size) {
        u8 ch = file->buffer[file->cur];

        if (ch == '\r') {
            if (file->cur + 1 < file->size && file->buffer[file->cur + 1] == '\n')
                file->cur += 2;
            else
                file->cur++;

            break;
        }

        if (ch == '\n') {
            file->cur++;
            break;
        }

        line.len++;
        file->cur++;
    }

    return line;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_LIB_CORE_H */
