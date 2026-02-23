#include "test/jemalloc_test.h"

/*
 * Simulate a TSAN-linked binary.  Mutexes should still use normal locking once
 * allocator bootstrap has completed.
 */
void
__tsan_init(void) {
}

TEST_BEGIN(test_mutex_preboot_scope) {
	tsd_t *tsd = tsd_fetch();
	expect_ptr_not_null(tsd, "Expected initialized TSD");

	malloc_init_t saved_state = malloc_init_state;
	malloc_init_state = malloc_init_a0_initialized;
	expect_true(malloc_mutex_use_preboot_path(TSDN_NULL),
	    "Expected preboot path while allocator init is incomplete");
	expect_true(malloc_mutex_use_preboot_path(tsd_tsdn(tsd)),
	    "Expected preboot path while allocator init is incomplete");
	malloc_init_state = saved_state;

	expect_false(malloc_mutex_use_preboot_path(TSDN_NULL),
	    "Null tsdn after init should not force the preboot path");
	expect_false(malloc_mutex_use_preboot_path(tsd_tsdn(tsd)),
	    "Preboot mutex path must not remain enabled post-initialization");
}
TEST_END

int
main(void) {
	return test(
	    test_mutex_preboot_scope);
}
