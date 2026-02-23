#include "test/jemalloc_test.h"

#include "jemalloc/internal/jemalloc_internal_externs.h"

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define JEMALLOC_TEST_ADDRESS_SANITIZER 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define JEMALLOC_TEST_ADDRESS_SANITIZER 1
#endif

#ifndef JEMALLOC_TEST_ADDRESS_SANITIZER
/*
 * Simulate ASAN runtime presence so allocator bootstrap takes the same
 * conservative config-file path as sanitizer-linked binaries.
 */
void
__asan_init(void) {
}
#endif

TEST_BEGIN(test_malloc_conf_bootstrap_sanitizer_runtime) {
#ifdef _WIN32
	bool windows = true;
#else
	bool windows = false;
#endif
	test_skip_if(windows);

	expect_true(malloc_conf_unsafe_bootstrap_runtime_present(),
	    "Expected sanitizer runtime detection to be enabled");
}
TEST_END

int
main(void) {
	return test_no_reentrancy(
	    test_malloc_conf_bootstrap_sanitizer_runtime);
}
