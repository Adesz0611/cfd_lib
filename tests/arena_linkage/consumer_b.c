#include "cfd_core.h"
#include <assert.h>
#include <stdio.h>
static unsigned error_count;
static char last_error[256];
static void capture(CFD_Log_Level level, const char *fmt, va_list args) {
    assert(level == CFD_LOG_LEVEL_ERROR);
    ++error_count;
    vsnprintf(last_error, sizeof(last_error), fmt, args);
}
void test_oom(u64 requested) {
    _Alignas(64) u8 storage[64];
    CFD_Arena arena = {0};
    cfd_arena_init_from_buffer(&arena, storage, sizeof(storage));
    assert(cfd_arena_alloc(&arena, 8) == storage);
    u64 saved = arena.offset;
    error_count = 0;
    cfd_set_logger(capture);
    assert(cfd_arena_alloc_aligned(&arena, requested, 16) == NULL);
    assert(arena.offset == saved && error_count == 1);
    assert(strcmp(last_error, "Arena out of memory! Requested: 65, Available: 56") == 0);
    assert(cfd_arena_alloc_zero(&arena, requested) == NULL);
    assert(arena.offset == saved && error_count == 2);
    assert(cfd_arena_push_array(&arena, u64, 9) == NULL);
    assert(arena.offset == saved && error_count == 3);
    /* Exercise formatting beyond 32 bits without performing a huge allocation. */
    assert(cfd_arena_alloc(&arena, 0x100000001ULL) == NULL);
    assert(arena.offset == saved && error_count == 4);
    assert(strcmp(last_error, "Arena out of memory! Requested: 4294967297, Available: 56") == 0);
    cfd_set_logger(NULL);
    assert(cfd_arena_destroy(&arena));
    storage[0] = 1; /* borrowed backing remains caller-owned */
    assert(storage[0] == 1);
}
