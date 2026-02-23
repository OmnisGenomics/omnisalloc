#include "test/jemalloc_test.h"

#ifndef _WIN32

#define DTOR_KEY_COUNT 5
#define DTOR_REENTRY_INDEX 3

static pthread_key_t dtor_keys[DTOR_KEY_COUNT];

static void
dtor_chain_callback(void *arg) {
	uint32_t *idxp = (uint32_t *)arg;
	if (idxp == NULL) {
		return;
	}

	uint32_t idx = *idxp;
	if (idx >= DTOR_KEY_COUNT) {
		return;
	}

	/*
	 * Trigger allocator re-entry in the final pthread destructor iteration.
	 * This regresses the stale TSD reuse pattern from issue #2804.
	 */
	if (idx == DTOR_REENTRY_INDEX) {
		void *p = mallocx(2688, 0);
		if (p != NULL) {
			memset(p, 0x5, 2688);
			dallocx(p, 0);
		}
	}

	*idxp = idx + 1;
	if (idx + 1 < DTOR_KEY_COUNT) {
		if (pthread_setspecific(dtor_keys[idx + 1], idxp) != 0) {
			abort();
		}
	}
}

static void
dtor_chain_keys_init(void) {
	for (size_t i = 0; i < DTOR_KEY_COUNT; i++) {
		expect_d_eq(pthread_key_create(&dtor_keys[i], dtor_chain_callback),
		    0, "Failed to create pthread key %zu", i);
	}
}

static void
dtor_chain_keys_cleanup(void) {
	for (size_t i = 0; i < DTOR_KEY_COUNT; i++) {
		expect_d_eq(pthread_key_delete(dtor_keys[i]), 0,
		    "Failed to delete pthread key %zu", i);
	}
}

static void *
thread_trigger_dtor_chain(void *arg) {
	(void)arg;

	void *p = mallocx(8, 0);
	if (p != NULL) {
		dallocx(p, 0);
	}

	uint32_t idx = 0;
	expect_d_eq(pthread_setspecific(dtor_keys[0], &idx), 0,
	    "Failed to start pthread destructor chain");
	return NULL;
}

static void *
thread_post_exit_alloc(void *arg) {
	(void)arg;

	void *p = mallocx(1024, 0);
	if (p != NULL) {
		dallocx(p, 0);
	}
	return NULL;
}

TEST_BEGIN(test_thread_exit_tsd_reinit) {
	dtor_chain_keys_init();

	for (size_t i = 0; i < 200; i++) {
		thd_t thd;

		thd_create(&thd, thread_trigger_dtor_chain, NULL);
		thd_join(thd, NULL);

		thd_create(&thd, thread_post_exit_alloc, NULL);
		thd_join(thd, NULL);
	}

	dtor_chain_keys_cleanup();
}
TEST_END

#else

TEST_BEGIN(test_thread_exit_tsd_reinit) {
	test_skip("requires pthread key destructors");
}
TEST_END

#endif

int
main(void) {
	return test(
	    test_thread_exit_tsd_reinit);
}
