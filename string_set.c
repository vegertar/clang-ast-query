#include "string_set.h"
#include "test.h"
#include <assert.h>
#include <string.h>

static HASH_size_t StringSet_hash(const void *self, size_t size, const void *v);
static int StringSet_compare(const void *v, const void *element, size_t size);
static void *StringSet_init(void *dst, const void *src, size_t size);

void StringSet_reserve(StringSet *ss, ARRAY_size_t n) {
  assert(ss->i == 0 && "The string set is dirty");
  ARRAY_reserve((void *)ss, sizeof(String), n);
  memset(ss->data, 0, sizeof(String) * ss->n);
}

void StringSet_clear(StringSet *ss, int opt) {
  for (unsigned i = 0; i < ss->n && ss->i; ++i) {
    if (ss->data[i].hash) {
      string_clear(&ss->data[i].elem, opt);
      ss->i--;
      ss->data[i].hash = 0;
    }
  }
  ARRAY_clear((void *)ss, sizeof(String), NULL, opt);
}

String *StringSet_add(StringSet *ss, const String *s) {
  size_t size = ss->i;
  String *y = ARRAY_hput((void *)ss, sizeof(String), StringSet_compare,
                         StringSet_hash, NULL, NULL, s, StringSet_init);
  // Update the property if it's not a new string
  if (y && size == ss->i)
    y->property |= s->property;
  return y;
}

HASH_size_t StringSet_hash(const void *self, size_t size, const void *v) {
  const String *x = v;
  return x->hash;
}

int StringSet_compare(const void *v, const void *element, size_t size) {
  const String *x = v;
  const String *y = element;
  size_t nx = string_len(&x->elem);
  size_t ny = string_len(&y->elem);
  size_t n = nx < ny ? nx : ny;
  int d = strncmp(string_get(&x->elem), string_get(&y->elem), n);
  return d ? d : nx - ny;
}

void *StringSet_init(void *dst, const void *src, size_t size) {
  const String *x = src;
  String *y = dst;
  y->hash = x->hash;
  y->property = x->property;
  y->elem = string_dup(&x->elem);
  return y;
}

void StringSet_dump(StringSet *ss, FILE *fp) {
  if (fp == NULL)
    fp = stderr;
  for (unsigned i = 0, j = 0; i < ss->n && j < ss->i; ++i) {
    if (ss->data[i].hash)
      fprintf(fp, "%6u:%-6u:%12u:%s\n", ++j, i, ss->data[i].hash,
              string_get(&ss->data[i].elem));
  }
}

TEST(StringSet, {
  StringSet ss = {};
  StringSet_reserve(&ss, 17);
  ASSERT(ss.n == 17);

  const char *cases[] = {
      // 1
      "data",
      "data",
      "data",
      "data",
      "data",
      "data",
      "data",
      // 2
      "FILE *restrict",
      // 3
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      // 4
      "const struct rlimit64 *",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      // 5
      "__timeout",
      // 6
      "int (*)(const char *restrict, struct stat *restrict)",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      "/usr/include/x86_64-linux-gnu/bits/sched.h",
      // 7
      "sigsuspend",
      // 8
      "__xstat64",
      // 9
      "cmsghdr",
      // 10
      "int (int, struct mmsghdr *, unsigned int, int)",
      // 11
      "void *(*volatile)(size_t, size_t, const void *)",
      // 12
      "epoll_data_t",
      "epoll_data_t",
      // 13
      "u_char *(ngx_pool_t *, ngx_str_t *)",
      // 14
      "ngx_shm_t *",
      "ngx_shm_t *",
      // 15
      "void (ngx_err_t, const char *, ...)",
      "void (ngx_err_t, const char *, ...)",
      "data",
  };

  for (unsigned i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
    struct string s = string_static(cases[i], strlen(cases[i]));
    String x = {string_hash(&s), 0, s};
    const String *y = StringSet_add(&ss, &x);
    TOGGLE(dump_string_set_if_full, {
      if (!y) {
        fprintf(stderr, "\n---\n");
        StringSet_dump(&ss, stderr);
        fprintf(stderr, "---\n");
      }
    });
    ASSERT(y, "The string set is full at %uth case", i);
    ASSERT(string_owned(&y->elem));
  }

  ASSERT(ss.i == 15);
  StringSet_clear(&ss, 1);
  ASSERT(ss.i == 0 && ss.n == 0);
});