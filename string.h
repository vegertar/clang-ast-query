#pragma once

#include "array.h"
#include <stdlib.h>

// On-heap string
#define HSTR DECL_ARRAY(ANON, char)

enum string_flag {
  STRING_FLAG_ON_STACK,
  STRING_FLAG_STATIC,
  STRING_FLAG_ON_HEAP,
  STRING_FLAG_LITERAL,
};

#define STRING_MASK_STATIC 1

struct string {
  union {
    HSTR;
    struct {
      char s[23]; // the buffer of the on-stack string, with ending zero
      unsigned char size : 5; // the value of strlen(s)
      unsigned char flag : 2; // 0: on-stack, 1: static, 2: on-heap, 3: literal
      unsigned char reserved : 1;
    };
  };
};

typedef ARRAY_size_t string_size_t;

// Include the ending zero byte.
#define STRING_BUFSIZ_ON_STACK sizeof(((struct string *)0)->s)

static_assert(sizeof(struct string) <= 24,
              "It should be able to copy at low cost");
static_assert(STRING_BUFSIZ_ON_STACK >= sizeof(HSTR),
              "It should be large enough to not be polluted");

struct string *string_reserve(struct string *p, string_size_t n);
struct string *string_set(struct string *p, string_size_t i, const char *s,
                          string_size_t n);
struct string *string_insert(struct string *p, string_size_t i, const char *s,
                             string_size_t n);
struct string *string_clear(struct string *p, int opt);

static inline struct string string_create(const char *s, string_size_t n,
                                          unsigned flag) {
  struct string string = {};
  if (flag & STRING_MASK_STATIC) {
    string.flag = flag;
    string.data = (void *)s;
    string.i = n;
  } else {
    string_set(&string, 0, s, n);
  }
  return string;
}

#define string_from(s, n) string_create(s, n, 0)
#define string_static(s, n) string_create(s, n, STRING_FLAG_STATIC)
#define string_literal(s) string_create(s, sizeof(s) - 1, STRING_FLAG_LITERAL)

static inline const char *string_get(const struct string *p) {
  return p->flag == STRING_FLAG_ON_STACK ? p->s : p->data;
}

static inline string_size_t string_len(const struct string *p) {
  return p->flag == STRING_FLAG_ON_STACK ? p->size : p->i;
}

static inline bool string_owned(const struct string *p) {
  return (p->flag & STRING_MASK_STATIC) == 0;
}

static inline struct string string_dup(const struct string *p) {
  return p->flag == STRING_FLAG_LITERAL
             ? *p
             : string_create(string_get(p), string_len(p), 0);
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