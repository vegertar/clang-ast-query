#pragma once

#include "array.h"
#include <stdlib.h>

#define HSTR DECL_ARRAY(ANON, char)

enum string_flag {
  STRING_FLAG_ON_STACK,
  STRING_FLAG_STATIC,
  STRING_FLAG_ON_HEAP,
  STRING_FLAG_LITERAL,
};

struct string {
  union {
    HSTR;
    struct {
      char s[sizeof(HSTR) + 7];
      unsigned char flag : 2; // 0: on-stack, 1: static, 2: on-heap, 3: literal
      unsigned char size : 6; // the length of the on-stack string
    };
  };
};

typedef ARRAY_size_t string_size_t;

// Include the ending zero byte.
#define STRING_BUFSIZ_ON_STACK sizeof(((struct string *)0)->s)

static_assert(sizeof(struct string) <= 24,
              "The struct string is not cheap copying");
static_assert(sizeof(struct string) == sizeof(HSTR) + 8,
              "Undefined struct string");
static_assert(STRING_BUFSIZ_ON_STACK <= (2 << 6),
              "The size field is tool small");

struct string *string_reserve(struct string *p, string_size_t n);
struct string *string_set(struct string *p, string_size_t i, const char *s,
                          string_size_t n);
struct string *string_insert(struct string *p, string_size_t i, const char *s,
                             string_size_t n);
struct string *string_clear(struct string *p, int opt);

static inline struct string string_create(const char *s, string_size_t n,
                                          unsigned flag) {
  struct string string = {};
  if (flag & 1) {
    string.flag = flag;
    string.data = (void *)s;
    string.i = n;
  } else {
    string_set(&string, 0, s, n);
  }
  return string;
}

#define string_static(s, n) string_create(s, n, 1)
#define string_literal(s) string_create(s, sizeof(s) - 1, 3)

static inline const char *string_get(const struct string *p) {
  return p->flag == 0 ? p->s : p->data;
}

static inline string_size_t string_len(const struct string *p) {
  return p->flag == 0 ? p->size : p->i;
}

static inline bool string_owned(const struct string *p) {
  return (p->flag & 1) == 0;
}

static inline struct string string_dup(const struct string *p) {
  return p->flag == 3 ? *p : string_create(string_get(p), string_len(p), 0);
}

static inline struct string *string_append(struct string *p, const char *s,
                                           size_t n) {
  return string_set(p, p->i, s, n);
}

static inline struct string *string_push(struct string *p, char c) {
  return string_set(p, p->i, &c, 1);
}

static inline HASH_size_t string_hash(const struct string *p) {
  return ARRAY_hash(NULL, string_len(p), string_get(p));
}

#undef HSTR