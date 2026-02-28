#ifndef CFD_LIB_CORE_H
#define CFD_LIB_CORE_H

#ifndef CFD_LIB
#define CFD_LIB
#endif

#ifndef CFD_INTERNAL
#define CFD_INTERNAL static
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

typedef float f32;
typedef double f64;

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

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifdef _MSC_VER
#define force_inline __forceinline
#elif defined (__GNUC__)
#define force_inline inline __attribute__((always_inline))
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

#ifdef __cplusplus
    #include <cstddef>
    #define align_of(T) alignof(T)
#else
    #include <stdalign.h>
    #define align_of(T) _Alignof(T)
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

#define CFD_CHECK_NULL(ptr) \
    if (unlikely(!(ptr))) { \
        cfd_error("%s: parameter '%s' is NULL!", __func__, #ptr); \
        return false; \
    }

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

    if (likely(new_offset <= arena->cap)) {
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
    if (likely(ptr)) memset(ptr, 0, size);
    return ptr;
}

static force_inline void *cfd_arena_alloc_zero(CFD_Arena *arena, u64 size) {
    return cfd_arena_alloc_zero_aligned(arena, size, 8);
}

#define cfd_arena_push_type(arena, type) \
    (type *)cfd_arena_alloc_aligned((arena), sizeof(type), align_of(type))

#define cfd_arena_push_type_zero(arena, type) \
    (type *)cfd_arena_alloc_zero_aligned((arena), sizeof(type), align_of(type))

#define cfd_arena_push_array(arena, type, count) \
    (type *)cfd_arena_alloc_aligned((arena), sizeof(type) * (count), align_of(type))

#define cfd_arena_push_array_zero(arena, type, count) \
    (type *)cfd_arena_alloc_zero_aligned((arena), sizeof(type) * (count), align_of(type))


// FILE HANDLING

CFD_LIB b32 cfd_file_slurp(char *filename, CFD_File *file);
CFD_LIB b32 cfd_file_free(CFD_File *file);
CFD_LIB void cfd_dirname(const char *path, char *dest);
CFD_LIB Str8 cfd_file_readline(CFD_File *file);

#define IS_CFD_FILE_EOF(f) (f->cur >= f->size)


// STRING HANDLING

#define Str8_Fmt "%.*s"
#define str8_arg(s) (int)(s).len, (s).buffer

static force_inline Str8 Str8_From_Zstr(u8 *txt, u64 len) { Str8 res = { (u8*)txt, len }; return res; }
#define str8_lit(s) Str8_From_Zstr((u8 *)s, sizeof(s) - 1)

CFD_LIB b32 str8_is_all_digits(Str8 s);
CFD_LIB s32 str8_to_s32(Str8 s);
CFD_LIB f32 str8_to_f32(Str8 s);

static force_inline Str8 str8_copy(CFD_Arena *arena, Str8 s) {
    Str8 result;

    result.len = s.len;
    result.buffer = (u8 *)cfd_arena_alloc_aligned(arena, s.len, 1);

    memcpy(result.buffer, s.buffer, s.len);

    return result;
}


static force_inline b32 str8_equals(Str8 lhs, Str8 rhs) {
    if (lhs.len != rhs.len) return false;

    for (u64 i = 0; i < lhs.len; ++i)
        if (lhs.buffer[i] != rhs.buffer[i])
            return false;

    return true;
}

static force_inline b32 str8_ends_with(Str8 s, Str8 suffix) {
    if (unlikely(s.len < suffix.len)) return false;
    Str8 end = { s.buffer + s.len - suffix.len, suffix.len };
    return str8_equals(end, suffix);
}


static force_inline Str8 str8_ltrim(Str8 s) {
    Str8 result = s;

    while (result.len > 0 && (result.buffer[0] == ' ' || result.buffer[0] == '\t')) {
        ++result.buffer;
        --result.len;
    }

    return result;
}

static force_inline Str8 str8_rtrim(Str8 s) {
    Str8 result = s;

    while (result.len > 0 && (result.buffer[result.len - 1] == ' ' || result.buffer[result.len - 1] == '\t'))
        --result.len;

    return result;
}

static force_inline u32 str8_to_u32(Str8 s) {
    u32 n = 0;

    while (s.len-- && *s.buffer >= '0' && *s.buffer <= '9')
        n = n * 10 + *s.buffer++ - '0';

    return n;
}

#ifdef CFD_LIB_IMPLEMENTATION

#include <stdio.h>
#include <errno.h>

#if defined(__unix__)
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
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
#if defined(__unix__)
    arena->buffer = (u8 *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (arena->buffer == MAP_FAILED) {
        cfd_error("mmap() failed: could not allocate virtual memory for arena allocator: %s", strerror(errno));
        return false;
    }
#elif defined(_WIN32)
    arena->buffer = (u8 *)VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (arena->buffer == NULL) {
        cfd_error("VirtualAlloc() failed: could not allocate virtual memory. Error code: %lu", GetLastError());
        return false;
    }
#else
#error "Not supported OS"
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
    if (arena->is_mmaped && arena->buffer) {
#if defined(__unix__)
        if (munmap(arena->buffer, arena->cap) != 0) {
            cfd_error("munmap() failed: could not unmap virtual memory for arena allocator: %s", strerror(errno));
            return false;
        }

#elif defined(_WIN32)
        if (VirtualFree(arena->buffer, 0, MEM_RELEASE) == 0) {
            cfd_error("VirtualFree() failed. Error code: %lu", GetLastError());
            return false;
        }
#else
#error "Not supported OS"
#endif
    }

    arena->buffer = NULL;
    arena->offset = 0;
    arena->cap = 0;
    arena->is_mmaped = false;
    return true;
}

CFD_LIB b32 cfd_file_slurp(char *filename, CFD_File *file) {
#if defined(__unix__)
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

    file->size = (u64)sb.st_size;

    file->buffer = (u8 *)mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file->buffer == MAP_FAILED) {
        cfd_error("mmap() failed: %s", strerror(errno));
        close(fd);
        return false;
    }

    close(fd);
#elif defined(_WIN32)
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        cfd_error("CreateFileA() failed for '%s'. Error: %lu", filename, GetLastError());
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        cfd_error("GetFileSizeEx() failed. Error: %lu", GetLastError());
        CloseHandle(hFile);
        return false;
    }

    file->size = (u64)fileSize.QuadPart;

    if (file->size == 0) {
        cfd_error("File '%s' is empty.", filename);
        CloseHandle(hFile);
        return false;
    }

    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapping == NULL) {
        cfd_error("CreateFileMappingA() failed. Error: %lu", GetLastError());
        CloseHandle(hFile);
        return false;
    }

    file->buffer = (u8 *)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

    CloseHandle(hMapping);
    CloseHandle(hFile);

    if (file->buffer == NULL) {
        cfd_error("MapViewOfFile() failed. Error: %lu", GetLastError());
        return false;
    }
#else
#error "Not supported OS"
#endif /* __unix__ */

    file->cur = 0;

    return true;
}

CFD_LIB b32 cfd_file_free(CFD_File *file) {
    if (unlikely(file->buffer == NULL)) return true;

#if defined(__unix__)
    if (munmap(file->buffer, file->size) != 0) {
        cfd_error("munmap() failed: could not unmap mapped file: %s", strerror(errno));
        return false;
    }
#elif defined(_WIN32)
    if (!UnmapViewOfFile(file->buffer)) {
        cfd_error("UnmapViewOfFile() failed. Error: %lu", GetLastError());
        return false;
    }
#else
#error "Not supported OS"
#endif

    return true;
}

CFD_LIB void cfd_dirname(const char *path, char *dest) {
    if (!path || !dest) {
        cfd_error("path or destination is null in cfd_dirname()!");
        return;
    }

    u64 path_len = strlen(path);
    if (unlikely(path_len == 0)) {
        dest[0] = '.';
        dest[1] = '\0';
        return;
    }

    if (unlikely(path_len > PATH_MAX)) {
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
        memcpy(dest, path, (size_t)last_slash_index);
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
    if (file->cur >= file->size) {
        Str8 result = { NULL, 0 };
        return result;
    }

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

CFD_LIB b32 str8_is_all_digits(Str8 s) {
    if (unlikely(s.len == 0)) return false;

    for (u64 i = 0; i < s.len; ++i)
        if (s.buffer[i] < '0' || s.buffer[i] > '9')
            return false;

    return true;
}


CFD_LIB s32 str8_to_s32(Str8 s) {
    s32 n = 0, sign = 1;

    if (likely(s.len > 0)) {
        switch (s.buffer[0]) {
            case '-':
                sign = -1;
                --s.len;
                ++s.buffer;
                break;
            case '+':
                --s.len;
                ++s.buffer;
                break;
        }
    }

    while (s.len-- && *s.buffer >= '0' && *s.buffer <= '9')
        n = n * 10 + *s.buffer++ - '0';

    return n * sign;
}

CFD_LIB f32 str8_to_f32(Str8 s) {
    static const f64 pow10_table[] = {
        1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
        1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
        1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
    };

    if (!s.buffer || s.len == 0)
        return 0.0f;

    f32 sign = 1.0f;
    u64 i = 0;

    if (i < s.len && (s.buffer[i] == '-' || s.buffer[i] == '+')) {
        if (s.buffer[i] == '-') {
            sign = -1.0f;
        }
        i++;
    }

    f64 result = 0.0;
    while (i < s.len && s.buffer[i] >= '0' && s.buffer[i] <= '9') {
        result = result * 10.0 + (s.buffer[i] - '0');
        i++;
    }

    if (i < s.len && s.buffer[i] == '.') {
        i++;
        f64 frac_divisor = 1.0;
        while (i < s.len && s.buffer[i] >= '0' && s.buffer[i] <= '9') {
            result = result * 10.0 + (s.buffer[i] - '0');
            frac_divisor *= 10.0;
            i++;
        }
        result /= frac_divisor;
    }

    if (i < s.len && (s.buffer[i] == 'e' || s.buffer[i] == 'E')) {
        i++;
        s32 exp_sign = 1;
        s32 exp_value = 0;

        if (i < s.len && (s.buffer[i] == '-' || s.buffer[i] == '+')) {
            if (s.buffer[i] == '-') {
                exp_sign = -1;
            }
            i++;
        }

        while (i < s.len && s.buffer[i] >= '0' && s.buffer[i] <= '9') {
            exp_value = exp_value * 10 + (s.buffer[i] - '0');
            i++;
        }

        s32 exponent = exp_sign * exp_value;

        if (exponent >= -22 && exponent <= 22) {
            if (exponent >= 0)
                result *= pow10_table[exponent];
            else
                result /= pow10_table[-exponent];
        } else {
            // Fallback for exponents outside table range
            if (exponent > 0) {
                while (exponent--) result *= 10.0;
            } else {
                while (exponent++) result /= 10.0;
            }
        }
    }

    return sign * (f32)result;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_LIB_CORE_H */
