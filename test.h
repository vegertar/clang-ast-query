#pragma once

#ifdef USE_TEST
# define __test_call __attribute__ ((unused,__section__ (".testcall.test")))
# define __test_name __attribute__ ((unused,__section__ (".testname.test")))

extern int (*test)(void);

# define TEST(name, ...)                                             \
  static int test_##name();                                          \
  int (*__testcall_##name)(void) __test_call = test_##name;          \
  const char *__testname_##name __test_name = #name;                 \
  static int test_##name() {                                         \
    __VA_ARGS__                                                      \
    return 0;                                                        \
  }
# define RUN_TEST() test ? test() : (-1)

# define ASSERT_FMT(value, fmt, ...)                                 \
  do {                                                               \
    if (!(value)) {                                                  \
      fprintf(stderr, ":%s:%d:%s" fmt,                               \
              __FILE__, __LINE__, #value,                            \
              ##__VA_ARGS__);                                        \
      return 1;                                                      \
    }                                                                \
  } while (0)

# define ASSERT(value) ASSERT_FMT(value, "")
#else
# define TEST(...)
# define RUN_TEST(...)
#endif
