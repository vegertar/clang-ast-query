#ifdef USE_TEST

#include "test.h"
#include <stdio.h>

static int run_test();
int (*test)(void) __test_call = run_test;

static int run_test() {
  static const char *name __test_name = NULL;
  int failed = 0;
  int n = 0;
  void **p = (void **)&test;
  const char **q = &name;
  while (*++p) {
    ++n;
    fprintf(stderr, "TEST(%s)", q[n]);
    int c = ((int (*)(void))*p)();
    if (c) {
      ++failed;
      fprintf(stderr, ":FAILED\n");
    } else {
      // clear the line
      fprintf(stderr, "\r%120s\r", "");
    }
  }

  fprintf(stderr, "RAN %d tests, %d PASSED\n", n, n - failed);
  return failed;
}

#endif // USE_TEST