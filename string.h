#pragma once

#include "array.h"
#include <stdlib.h>

#define ANON
#define HSTR DECL_ARRAY(ANON, char)

struct string {
  union {
    HSTR;
    struct {
      char s[sizeof(HSTR) + 7];
      unsigned char flag : 2; // 0: on stack, 1: static, 2: on heap
      unsigned char size : 6; // the length of the on-stack string
    };
  };
};

_Static_assert(sizeof(struct string) == sizeof(HSTR) + 8,
               "Undefined struct string");
_Static_assert(sizeof(((struct string *)0)->s) <= (2 << 6),
               "The size field is tool small");

struct string *string_reserve(struct string *p, size_t n);
struct string *string_set(struct string *p, size_t i, const char *s, size_t n);
struct string *string_insert(struct string *p, size_t i, const char *s,
                             size_t n);
struct string *string_clear(struct string *p, int opt);

static inline struct string string_create(const char *s, size_t n) {
  struct string string = {0};
  string_set(&string, 0, s, n);
  return string;
}

static inline struct string string_static(const char *s, size_t n) {
  struct string string = {.flag = 1};
  string.data = (void *)s;
  string.i = n;
  return string;
}

#define string_literal(s) string_static(s, sizeof(s) - 1)

static inline const char *string_get(struct string *p) {
  return p->flag == 0 ? p->s : p->data;
}

static inline size_t string_len(struct string *p) {
  return p->flag == 0 ? p->size : p->i;
}

#undef HSTR
#undef ANON
