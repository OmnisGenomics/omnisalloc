#include "test/jemalloc_test.h"

#include <thread>

class ThreadExitAllocator {
public:
	~ThreadExitAllocator() {
		void *p = mallocx(8, MALLOCX_ARENA(0));
		if (p != nullptr) {
			dallocx(p, MALLOCX_ARENA(0));
		}
	}
};

static void
touch_tls_destructor(void) {
	thread_local ThreadExitAllocator tls_allocator;
	/*
	 * Ensure the thread_local object is materialized on this thread so its
	 * destructor executes during thread teardown.
	 */
	(void)&tls_allocator;
}

TEST_BEGIN(test_thread_exit_mallocx_arena0) {
	for (size_t i = 0; i < 100; i++) {
		std::thread worker(touch_tls_destructor);
		worker.join();
	}
}
TEST_END

int
main() {
	return test(
	    test_thread_exit_mallocx_arena0);
}
