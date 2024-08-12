#ifdef USE_TEST

#include "test.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

static int help(const char *kind, void **p, const struct __tinfo *q) {
  fprintf(stderr, "Available %s(s):\n", kind);

  const char *last_file = NULL;
  int n = 0;
  while (*++p) {
    ++n;
    const char *file = q[n].file;
    if (file != last_file) {
      fprintf(stderr, "  %s:\n", file);
      last_file = file;
    }
    fprintf(stderr, "    ");
    if (q[n].func)
      fprintf(stderr, "%s:", q[n].func);
    fprintf(stderr, "%5d:%s\n", q[n].line, q[n].name);
  }

  return 0;
}

static int run_test(const char *option);
__test_t test __testcall = run_test;

static const struct __tinfo testinfo __testinfo;

int test_help() { return help("TEST", (void **)&test, &testinfo); }

static int run_test(const char *option) {
  int failed = 0;
  int n = 0;
  void **p = (void **)&test;
  const struct __tinfo *q = &testinfo;

  struct winsize w;
  ioctl(2, TIOCGWINSZ, &w);

  while (*++p) {
    ++n;
    fprintf(stderr, "TEST(%s)", q[n].name);
    int c = ((__test_t)*p)(NULL);
    if (c) {
      ++failed;
      fprintf(stderr, ":FAILED\n");
    } else {
      // clear the line
      fprintf(stderr, "\r%*s\r", w.ws_col, "");
    }
  }

  fprintf(stderr, "RAN %d tests, %d PASSED\n", n, n - failed);
  return failed;
}

static struct __tinfo toggleinfo __toggleinfo;
static struct __tinfo *togglehold __togglehold = &toggleinfo;

int toggle_help() { return help("TOGGLE", (void **)&togglehold, &toggleinfo); }

int toggle(const char *option) {
  int n = 0;
  void **p = (void **)&togglehold;
  struct __tinfo *q = &toggleinfo;

  while (*++p) {
    ++n;
    if (!option || strcmp(option, q[n].name) == 0)
      q[n].enabled = 1;
  }
  return 0;
}

#endif // USE_TEST