#include "test/jemalloc_test.h"

#include <cstring>
#include <thread>

class ThreadExitAllocator {
public:
	~ThreadExitAllocator() {
		/*
		 * Regression pattern for Windows TLS teardown ordering: force
		 * allocation and overwrite in thread_local destructors.
		 */
		void *p = mallocx(2688, MALLOCX_ARENA(0));
		if (p != nullptr) {
			memset(p, 0x5, 2688);
			dallocx(p, MALLOCX_ARENA(0));
		}
	}
};

static void
touch_tls_destructor(void) {
	void *p = mallocx(8, MALLOCX_ARENA(0));
	if (p != nullptr) {
		dallocx(p, MALLOCX_ARENA(0));
	}

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
