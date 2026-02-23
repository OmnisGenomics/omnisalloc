#include "test/jemalloc_test.h"

/* Threshold: 2 << 20 = 2097152. */
#define HUGE_DIRTY_DECAY_MS 500
#define HUGE_MUZZY_DECAY_MS -1
const char *malloc_conf =
    "oversize_threshold:2097152,dirty_decay_ms:500,muzzy_decay_ms:-1,"
    "background_thread:false";

#define HUGE_SZ (2 << 20)
#define SMALL_SZ (8)

static size_t huge_purge_forced_calls;
static extent_hooks_t huge_counting_hooks;

static bool
purge_forced_count(extent_hooks_t *extent_hooks, void *addr, size_t sz,
    size_t offset, size_t length, unsigned arena_ind) {
	huge_purge_forced_calls++;
	return ehooks_default_extent_hooks.purge_forced(extent_hooks, addr, sz,
	    offset, length, arena_ind);
}

static ssize_t
arena_decay_ms_read_ctl(unsigned arena, const char *state) {
	char cmd[64];
	ssize_t decay_ms;
	size_t decay_sz = sizeof(decay_ms);

	malloc_snprintf(cmd, sizeof(cmd), "arena.%u.%s_decay_ms", arena, state);
	expect_d_eq(mallctl(cmd, &decay_ms, &decay_sz, NULL, 0), 0,
	    "Unexpected mallctl() failure: %s", cmd);
	return decay_ms;
}

TEST_BEGIN(huge_decay_configuration) {
	unsigned arena;
	size_t arena_sz = sizeof(arena);

	void *ptr = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(ptr, "Fail to allocate huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena, &arena_sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_gt(arena, 0, "Huge allocation should not come from arena 0");

	expect_zd_eq(arena_decay_ms_read_ctl(arena, "dirty"),
	    HUGE_DIRTY_DECAY_MS,
	    "Huge arena dirty_decay_ms should follow configured default");
	expect_zd_eq(arena_decay_ms_read_ctl(arena, "muzzy"),
	    HUGE_MUZZY_DECAY_MS,
	    "Huge arena muzzy_decay_ms should follow configured default");
	dallocx(ptr, 0);
}
TEST_END

TEST_BEGIN(huge_no_immediate_purge_without_background_thread) {
	test_skip_if(is_background_thread_enabled());

	unsigned arena;
	size_t arena_sz = sizeof(arena);

	/* Force huge arena creation and discover its arena index. */
	void *ptr = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(ptr, "Failed to allocate huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena, &arena_sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_gt(arena, 0, "Huge allocation should not come from arena 0");
	dallocx(ptr, 0);

	/* Install counting hook for forced purge operations. */
	extent_hooks_t *old_hooks;
	size_t hooks_sz = sizeof(old_hooks);
	huge_counting_hooks = ehooks_default_extent_hooks;
	huge_counting_hooks.purge_forced = &purge_forced_count;
	extent_hooks_t *hooks = &huge_counting_hooks;
	char cmd[64];
	malloc_snprintf(cmd, sizeof(cmd), "arena.%u.extent_hooks", arena);
	expect_d_eq(mallctl(cmd, &old_hooks, &hooks_sz, &hooks, sizeof(hooks)), 0,
	    "Failed to install extent hooks on huge arena");

	/* Ensure stale state does not affect this test. */
	expect_d_eq(mallctl("arena.0.purge", NULL, NULL, NULL, 0), 0,
	    "Unexpected mallctl() failure");
	huge_purge_forced_calls = 0;

	/*
	 * With positive dirty_decay_ms and no background thread, huge frees
	 * should not force immediate purging on the caller thread.
	 */
	ptr = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(ptr, "Failed to allocate huge size");
	dallocx(ptr, 0);
	expect_zu_eq(huge_purge_forced_calls, 0,
	    "Unexpected immediate forced purge for huge free");

	/* Restore original hooks so later tests are unaffected. */
	expect_d_eq(mallctl(cmd, NULL, NULL, &old_hooks, sizeof(old_hooks)), 0,
	    "Failed to restore extent hooks on huge arena");
}
TEST_END

TEST_BEGIN(huge_bind_thread) {
	unsigned arena1, arena2;
	size_t sz = sizeof(unsigned);

	/* Bind to a manual arena. */
	expect_d_eq(mallctl("arenas.create", &arena1, &sz, NULL, 0), 0,
	    "Failed to create arena");
	expect_d_eq(mallctl("thread.arena", NULL, NULL, &arena1,
	    sizeof(arena1)), 0, "Fail to bind thread");

	void *ptr = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(ptr, "Fail to allocate huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_eq(arena1, arena2, "Wrong arena used after binding");
	dallocx(ptr, 0);

	/* Switch back to arena 0. */
	test_skip_if(have_percpu_arena &&
	    PERCPU_ARENA_ENABLED(opt_percpu_arena));
	arena2 = 0;
	expect_d_eq(mallctl("thread.arena", NULL, NULL, &arena2,
	    sizeof(arena2)), 0, "Fail to bind thread");
	ptr = mallocx(SMALL_SZ, MALLOCX_TCACHE_NONE);
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_eq(arena2, 0, "Wrong arena used after binding");
	dallocx(ptr, MALLOCX_TCACHE_NONE);

	/* Then huge allocation should use the huge arena. */
	ptr = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(ptr, "Fail to allocate huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_ne(arena2, 0, "Wrong arena used after binding");
	expect_u_ne(arena1, arena2, "Wrong arena used after binding");
	dallocx(ptr, 0);
}
TEST_END

TEST_BEGIN(huge_mallocx) {
	unsigned arena1, arena2;
	size_t sz = sizeof(unsigned);

	expect_d_eq(mallctl("arenas.create", &arena1, &sz, NULL, 0), 0,
	    "Failed to create arena");
	void *huge = mallocx(HUGE_SZ, MALLOCX_ARENA(arena1));
	expect_ptr_not_null(huge, "Fail to allocate huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &huge,
	    sizeof(huge)), 0, "Unexpected mallctl() failure");
	expect_u_eq(arena1, arena2, "Wrong arena used for mallocx");
	dallocx(huge, MALLOCX_ARENA(arena1));

	void *huge2 = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(huge, "Fail to allocate huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &huge2,
	    sizeof(huge2)), 0, "Unexpected mallctl() failure");
	expect_u_ne(arena1, arena2,
	    "Huge allocation should not come from the manual arena.");
	expect_u_ne(arena2, 0,
	    "Huge allocation should not come from the arena 0.");
	dallocx(huge2, 0);
}
TEST_END

TEST_BEGIN(huge_allocation) {
	unsigned arena1, arena2;

	void *ptr = mallocx(HUGE_SZ, 0);
	expect_ptr_not_null(ptr, "Fail to allocate huge size");
	size_t sz = sizeof(unsigned);
	expect_d_eq(mallctl("arenas.lookup", &arena1, &sz, &ptr, sizeof(ptr)),
	    0, "Unexpected mallctl() failure");
	expect_u_gt(arena1, 0, "Huge allocation should not come from arena 0");
	dallocx(ptr, 0);

	test_skip_if(have_percpu_arena &&
	    PERCPU_ARENA_ENABLED(opt_percpu_arena));

	ptr = mallocx(HUGE_SZ >> 1, 0);
	expect_ptr_not_null(ptr, "Fail to allocate half huge size");
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_ne(arena1, arena2, "Wrong arena used for half huge");
	dallocx(ptr, 0);

	ptr = mallocx(SMALL_SZ, MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(ptr, "Fail to allocate small size");
	expect_d_eq(mallctl("arenas.lookup", &arena2, &sz, &ptr,
	    sizeof(ptr)), 0, "Unexpected mallctl() failure");
	expect_u_ne(arena1, arena2,
	    "Huge and small should be from different arenas");
	dallocx(ptr, 0);
}
TEST_END

int
main(void) {
	return test(
	    huge_decay_configuration,
	    huge_no_immediate_purge_without_background_thread,
	    huge_allocation,
	    huge_mallocx,
	    huge_bind_thread);
}
