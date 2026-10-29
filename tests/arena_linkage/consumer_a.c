#include "cfd_core.h"
#include <assert.h>
#include <stdint.h>
void test_success(void) {
    CFD_Arena arena = {0};
    assert(cfd_arena_init(&arena, MB(1)));
    assert(cfd_arena_alloc_aligned(&arena, 1, 1));
    f64 *values = cfd_arena_push_array_zero(&arena, f64, 17);
    assert(values && (uintptr_t)values % align_of(f64) == 0);
    for (u32 i = 0; i < 17; ++i) assert(values[i] == 0.0);
    values[16] = 3.0;
    assert(values[16] == 3.0);
    assert(cfd_arena_alloc_zero(&arena, 8));
    assert(cfd_arena_align(&arena, 32));
    assert(arena.offset % 32 == 0);
    cfd_arena_reset(&arena);
    assert(arena.offset == 0);
    assert(cfd_arena_destroy(&arena));
    assert(arena.buffer == NULL && arena.cap == 0);
}
