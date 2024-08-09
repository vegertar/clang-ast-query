#include "string.h"
#include "test.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct string *string_reserve(struct string *p, size_t n) {
  if (p->flag == 2) {
    ARRAY_reserve((void *)p, sizeof(p->data[0]), proper_capacity(n + 1));
  } else if (p->flag == 1 || p->flag == 0 && sizeof(p->s) - 1 < n) {
    struct string heap = {.flag = 2};
    ARRAY_reserve((void *)&heap, sizeof(heap.data[0]), proper_capacity(n + 1));
    size_t len = string_len(p);
    memcpy(heap.data, string_get(p), len);
    heap.i = len;
    heap.data[len] = 0;
    *p = heap;
  }

  return p;
}

struct string *string_set(struct string *p, size_t i, const char *s, size_t n) {
  size_t old_len = string_len(p);
  size_t new_len = i + n;

  string_reserve(p, new_len);
  assert(p->flag != 1);

  char *start = (char *)string_get(p);

  if (old_len < new_len) {
    if (old_len < i)
      memset(start + old_len, 0, i - old_len);
    if (p->flag == 0)
      p->size = new_len;
    else
      p->i = new_len;
    start[new_len] = 0;
  }

  if (s)
    memcpy(start + i, s, n);

  return p;
}

struct string *string_insert(struct string *p, size_t i, const char *s,
                             size_t n) {
  size_t old_len = string_len(p);
  if (i >= old_len)
    return string_set(p, i, s, n);

  // allocate slots
  string_set(p, old_len, NULL, n);
  assert(p->flag != 1);

  char *dst = (char *)string_get(p) + i;
  memmove(dst + n, dst, old_len - i);
  memcpy(dst, s, n);
  return p;
}

struct string *string_clear(struct string *p, int opt) {
  switch (p->flag) {
  case 0:
    p->size = 0;
    p->s[0] = 0;
    break;
  case 1:
    p->data = NULL;
    p->i = 0;
    break;
  case 2:
    ARRAY_clear((void *)p, sizeof(p->data[0]), NULL, opt);
    if (p->data)
      p->data[0] = 0;
    break;
  }

  return p;
}

#define STR(s) s, sizeof(s) - 1

TEST(string, {
  struct string s = string_create(STR("hello"));
  ASSERT(strcmp(string_get(&s), "hello") == 0);
  ASSERT(string_len(&s) == 5);
  ASSERT(s.flag == 0);

  struct string l = string_literal("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  ASSERT(string_len(&l) == 26);
  ASSERT(l.flag == 1);

  struct string h = string_create(STR("0123456789abcdefghijklmnopqrstuvwxyz"));
  ASSERT(strcmp(string_get(&h), "0123456789abcdefghijklmnopqrstuvwxyz") == 0);
  ASSERT(string_len(&h) == 36);
  ASSERT(h.flag == 2);

  string_set(&s, 0, string_get(&h), string_len(&h));
  ASSERT(strcmp(string_get(&s), string_get(&h)) == 0);
  ASSERT(string_len(&s) == string_len(&h));
  ASSERT(s.flag == 2);
  string_clear(&s, 1);
  ASSERT(string_len(&s) == 0);
  ASSERT(string_get(&s) == NULL);

  string_clear(&h, 0);
  ASSERT(strcmp(string_get(&h), "") == 0);
  ASSERT(string_len(&h) == 0);
  ASSERT(h.flag == 2);
  ASSERT(h.n == 64);
  ASSERT(h.i == 0);
  string_clear(&h, 1);
  ASSERT(string_get(&h) == NULL);
  ASSERT(h.n == 0);
});