#include "test/jemalloc_test.h"

#ifndef _WIN32

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#endif

static const char *self_path = NULL;

#ifndef _WIN32

static int
run_child_with_conf(const char *conf, char *output, size_t output_len) {
	int pipefd[2];
	if (pipe(pipefd) != 0) {
		return -1;
	}

	pid_t pid = fork();
	if (pid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		if (dup2(pipefd[1], STDERR_FILENO) == -1) {
			_exit(127);
		}
		close(pipefd[1]);

		/* Set both forms to handle builds with and without je_ prefix. */
		if (setenv("MALLOC_CONF", conf, 1) != 0 ||
		    setenv("JE_MALLOC_CONF", conf, 1) != 0) {
			_exit(127);
		}

		execl(self_path, self_path, "--child", (char *)NULL);
		_exit(127);
	}

	close(pipefd[1]);
	size_t off = 0;
	while (off + 1 < output_len) {
		ssize_t n = read(pipefd[0], output + off, output_len - off - 1);
		if (n == 0) {
			break;
		}
		if (n < 0) {
			close(pipefd[0]);
			return -1;
		}
		off += (size_t)n;
	}
	output[off] = '\0';
	close(pipefd[0]);

	int status;
	if (waitpid(pid, &status, 0) < 0) {
		return -1;
	}
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

#endif

TEST_BEGIN(test_suppress_conf_warnings) {
#ifdef _WIN32
	bool windows = true;
#else
	bool windows = false;
#endif
	test_skip_if(windows);

#ifndef _WIN32
	char output[4096];

	int rc = run_child_with_conf("definitely_invalid_option:1", output,
	    sizeof(output));
	expect_d_eq(rc, 0, "Unexpected child failure without suppression");
	expect_ptr_not_null(strstr(output, "Invalid conf"),
	    "Expected invalid conf warning without suppression");

	rc = run_child_with_conf(
	    "suppress_conf_warnings:true,definitely_invalid_option:1", output,
	    sizeof(output));
	expect_d_eq(rc, 0, "Unexpected child failure with suppression");
	expect_ptr_null(strstr(output, "Invalid conf"),
	    "Expected invalid conf warning to be suppressed");

	rc = run_child_with_conf(
	    "definitely_invalid_option:1,suppress_conf_warnings:true", output,
	    sizeof(output));
	expect_d_eq(rc, 0,
	    "Unexpected child failure with suppression set later in config");
	expect_ptr_null(strstr(output, "Invalid conf"),
	    "Expected invalid conf warning to be suppressed regardless of "
	    "option order");
#endif
}
TEST_END

int
main(int argc, char **argv) {
	self_path = argv[0];
	if (argc > 1 && strcmp(argv[1], "--child") == 0) {
		void *p = malloc(8);
		if (p == NULL) {
			return 1;
		}
		free(p);
		return 0;
	}

	return test(
	    test_suppress_conf_warnings);
}
