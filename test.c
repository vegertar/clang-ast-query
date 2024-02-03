#ifdef USE_TEST

#include "test.h"
#include <stdio.h>

static int run_test();
int (*test)(void) __test_call = run_test;

static int run_test() {
  static const char *name __test_name = NULL;
  int c = 0;
  unsigned failed = 0;
  unsigned n = 0;
  void **p = (void **)&test;
  const char **q = &name;
  while (*++p) {
    ++n;
    fprintf(stderr, "TEST(%s)", q[n]);
    c = ((int (*)(void))*p)();
    if (c) {
      ++failed;
      fprintf(stderr, ":FAILED\n");
    } else {
      // clear the line
      fprintf(stderr, "\r%120s\r", "");
    }
  }

  fprintf(stderr, "RAN %u tests, %u PASSED\n", n, n - failed);
  return c;
}

#endif // USE_TEST