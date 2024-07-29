#pragma once

#include <stddef.h>

#ifndef ARRAY_SIZE_TYPE
#define ARRAY_SIZE_TYPE unsigned
#endif // ARRAY_SIZE_TYPE

#ifndef ARRAY_SSIZE_TYPE
#define ARRAY_SSIZE_TYPE signed
#endif // ARRAY_SIZE_TYPE

#define DECL_ARRAY(name, type)                                                 \
  struct name {                                                                \
    ARRAY_size_t n;                                                            \
    ARRAY_size_t i;                                                            \
    type *data;                                                                \
  }

#define IMPL_ARRAY_RESERVE(name, type)                                         \
  struct name *name##_reserve(struct name *p, ARRAY_size_t n) {                \
    return (struct name *)ARRAY_reserve((struct ARRAY_base *)p, sizeof(type),  \
                                        n);                                    \
  }

#define IMPL_ARRAY_SET(name, type)                                             \
  struct name *name##_set(struct name *p, ARRAY_ssize_t i, type item) {        \
    return (struct name *)ARRAY_set((struct ARRAY_base *)p, sizeof(type), i,   \
                                    &item, 1, NULL);                           \
  }

#define IMPL_ARRAY_PUSH(name, type)                                            \
  struct name *name##_push(struct name *p, type item) {                        \
    return (struct name *)ARRAY_set((struct ARRAY_base *)p, sizeof(type),      \
                                    p->i, &item, 1, NULL);                     \
  }

#define IMPL_ARRAY_APPEND(name, type)                                          \
  struct name *name##_append(struct name *p, const type *src,                  \
                             ARRAY_size_t len) {                               \
    return (struct name *)ARRAY_set((struct ARRAY_base *)p, sizeof(type),      \
                                    p->i, src, len, NULL);                     \
  }

#define IMPL_ARRAY_CLEAR(name, destroy)                                        \
  struct name *name##_clear(struct name *p, int option) {                      \
    return (struct name *)ARRAY_clear((struct ARRAY_base *)p,                  \
                                      sizeof(p->data[0]), destroy, option);    \
  }

#define IMPL_ARRAY_BSEARCH(name, compare)                                      \
  _Bool name##_bsearch(const struct name *p, const void *v, ARRAY_size_t *i) { \
    return ARRAY_bsearch((struct ARRAY_base *)p, sizeof(p->data[0]), compare,  \
                         v, i);                                                \
  }

#define IMPL_ARRAY_BADD(name, init)                                            \
  _Bool name##_badd(struct name *p, const void *v, ARRAY_size_t *i) {          \
    ARRAY_size_t j = -1;                                                       \
    const _Bool found = name##_bsearch(p, v, &j);                              \
    assert(found || j != -1);                                                  \
    if (!found)                                                                \
      ARRAY_insert((struct ARRAY_base *)p, sizeof(p->data[0]), j, v, 1, init,  \
                   NULL);                                                      \
    if (i)                                                                     \
      *i = j;                                                                  \
    return !found;                                                             \
  }

typedef ARRAY_SIZE_TYPE ARRAY_size_t;
typedef ARRAY_SSIZE_TYPE ARRAY_ssize_t;
typedef int (*ARRAY_compare_t)(const void *v, const void *element, size_t n);
typedef void *(*ARRAY_init_t)(void *dst, const void *v, size_t n);
typedef void *(*ARRAY_move_t)(void *dst, const void *v, size_t n);
typedef void (*ARRAY_destroy_t)(void *v);

enum array_destroy_option {
  ARRAY_DESTROY_ELEMENTS_ONLY,
  ARRAY_DESTROY_ALL,
  ARRAY_DESTROY_CONTAINER_ONLY,
};

DECL_ARRAY(ARRAY_base, void);

struct ARRAY_base *ARRAY_reserve(struct ARRAY_base *p, ARRAY_size_t size,
                                 ARRAY_size_t n);
struct ARRAY_base *ARRAY_set(struct ARRAY_base *p, ARRAY_size_t size,
                             ARRAY_ssize_t at, const void *src,
                             ARRAY_size_t nmem, ARRAY_move_t init);
struct ARRAY_base *ARRAY_insert(struct ARRAY_base *p, ARRAY_size_t size,
                                ARRAY_ssize_t at, const void *src,
                                ARRAY_size_t nmem, ARRAY_init_t init,
                                ARRAY_move_t move);
struct ARRAY_base *ARRAY_clear(struct ARRAY_base *p, ARRAY_size_t size,
                               void (*destroy)(void *),
                               enum array_destroy_option option);
_Bool ARRAY_bsearch(const struct ARRAY_base *p, ARRAY_size_t size,
                    ARRAY_compare_t compare, const void *v, ARRAY_size_t *i);
