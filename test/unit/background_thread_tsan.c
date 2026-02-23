#include "test/jemalloc_test.h"

#include <errno.h>

#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define JEMALLOC_TEST_THREAD_SANITIZER 1
#  endif
#endif
#if defined(__SANITIZE_THREAD__)
#  define JEMALLOC_TEST_THREAD_SANITIZER 1
#endif

#ifndef JEMALLOC_TEST_THREAD_SANITIZER
/*
 * Simulate TSAN runtime presence for this test binary so jemalloc takes the
 * same boot path it would under -fsanitize=thread.
 */
void
__tsan_init(void) {
}
#endif

const char *malloc_conf =
    "background_thread:false,narenas:1,suppress_conf_warnings:true";

TEST_BEGIN(test_background_thread_disabled_under_tsan_runtime) {
	test_skip_if(!have_background_thread);

	bool enabled = true;
	size_t sz = sizeof(enabled);
	expect_d_eq(mallctl("background_thread", &enabled, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure");
	expect_false(enabled,
	    "background_thread should remain disabled under TSAN runtime");

	bool new_enabled = true;
	expect_d_eq(mallctl("background_thread", NULL, NULL, &new_enabled,
	    sizeof(new_enabled)), EINVAL,
	    "Enabling background_thread should fail under TSAN runtime");

	enabled = true;
	sz = sizeof(enabled);
	expect_d_eq(mallctl("background_thread", &enabled, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure");
	expect_false(enabled,
	    "background_thread should remain disabled after failed enable");
}
TEST_END

int
main(void) {
	return test_no_reentrancy(
	    test_background_thread_disabled_under_tsan_runtime);
}
