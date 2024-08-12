#pragma once

#if defined(USE_TEST) || defined(USE_TOGGLE)

struct __tinfo {
  const char *name;
  const char *file;
  const char *func;
  unsigned line;
  _Bool enabled;
};

#endif // USE_TEST || USE_TOGGLE

#ifdef USE_TEST
#define __testcall __attribute__((__section__(".testcall.test")))
#define __testinfo __attribute__((__section__(".testinfo.test")))

int test_help();

typedef int (*__test_t)(const char *option);
extern __test_t test;

#define TEST(name, ...)                                                        \
  static int __test_##name(const char *option);                                \
  __test_t __testcall_##name __testcall = __test_##name;                       \
  const struct __tinfo __testinfo_##name __testinfo = {#name, __FILE__, NULL,  \
                                                       __LINE__};              \
  static int __test_##name(const char *option) {                               \
    __VA_ARGS__                                                                \
    return 0;                                                                  \
  }
#define RUN_TEST(option) (test ? test(option) : -1)

#define ASSERT_FMT(value, fmt, ...)                                            \
  do {                                                                         \
    if (!(value)) {                                                            \
      fprintf(stderr, ":%s:%d:%s" fmt, __FILE__, __LINE__, #value,             \
              ##__VA_ARGS__);                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT(value) ASSERT_FMT(value, "")
#else
#define TEST(...)
#define RUN_TEST(...) (-1)
#endif // USE_TEST

#ifdef USE_TOGGLE
#define __togglehold __attribute__((__section__(".togglehold.toggle")))
#define __toggleinfo __attribute__((__section__(".toggleinfo.toggle")))

int toggle_help();
int toggle(const char *option);

#define TOGGLE(name, ...)                                                      \
  do {                                                                         \
    static struct __tinfo __toggleinfo_##name __toggleinfo = {                 \
        #name, __FILE__, __func__, __LINE__};                                  \
    static struct __tinfo *__togglehold_##name __togglehold =                  \
        &__toggleinfo_##name;                                                  \
    if (__togglehold_##name->enabled) {                                        \
      __VA_ARGS__;                                                             \
    }                                                                          \
  } while (0)

#else
#define TOGGLE(...)
#define toggle_help(...) (-1)
#define toggle(...) (-1)
#endif // USE_TOGGLE