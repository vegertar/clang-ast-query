#pragma once

#define DECL_ARRAY(name, type)                                 \
  struct name {                                                \
    unsigned n;                                                \
    unsigned i;                                                \
    type *data;                                                \
  }

#define DECL_ARRAY_METHOD(name, method, ...)                   \
  struct name * name##_##method(struct name *p, ##__VA_ARGS__)

#define IMPL_ARRAY_PUSH(name, type)                            \
  struct name * name##_push(struct name *p, type item) {       \
    if (p->i == p->n) {                                        \
      unsigned n = p->n ? 2 * p->n : 1;                        \
      type *data = (type *)realloc(p->data, sizeof(type) * n); \
      assert(data);                                            \
      p->data = data;                                          \
      p->n = n;                                                \
    }                                                          \
    p->data[p->i++] = item;                                    \
    return p;                                                  \
  }

#define IMPL_ARRAY_CLEAR(name, f)                              \
  struct name * name##_clear(struct name *p, int destroy) {    \
    if (destroy == 0 || destroy == 1) {                        \
      for (unsigned i = 0; i < p->i; ++i) {                    \
        f(&p->data[i]);                                        \
      }                                                        \
    }                                                          \
    p->i = 0;                                                  \
    if (destroy && p->n) {                                     \
      free(p->data);                                           \
      p->n = 0;                                                \
      p->data = NULL;                                          \
    }                                                          \
    return p;                                                  \
  }

#define IMPL_ARRAY_BSEARCH(name, f)                            \
  _Bool name##_bsearch(const struct name *p,                   \
                       const void *v,                          \
                       unsigned *i) {                          \
    if (!v) {                                                  \
      return 0;                                                \
    }                                                          \
                                                               \
    unsigned begin = 0;                                        \
    unsigned end = p->i;                                       \
                                                               \
    while (begin < end) {                                      \
      const unsigned mid = begin + (end - begin) / 2;          \
      const int d = f(v, &p->data[mid]);                       \
      if (d == 0) {                                            \
        if (i) *i = mid;                                       \
        return 1;                                              \
      }                                                        \
      if (d < 0) {                                             \
        end = mid;                                             \
      } else {                                                 \
        begin = mid + 1;                                       \
      }                                                        \
    }                                                          \
                                                               \
    if (i) *i = begin;                                         \
    return 0;                                                  \
  }
