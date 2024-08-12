#pragma once

#include "murmur3.h"
#include <stddef.h>
#include <stdint.h>

#ifndef ARRAY_SIZE_TYPE
#define ARRAY_SIZE_TYPE unsigned
#endif // ARRAY_SIZE_TYPE

#ifndef HASH_WIDTH
#define HASH_WIDTH 32
#endif

#if HASH_WIDTH == 32
#define HASH_SIZE_TYPE uint32_t
#elif HASH_WIDTH == 128
#define HASH_SIZE_TYPE unsigned __int128
#else
#error "Unsupported HASH_WIDTH: either 32 or 128"
#endif

#ifndef HASH_SEED
#define HASH_SEED 496789
#endif

#ifndef ANON
#define ANON
#endif

#define DECL_ARRAY(name, type)                                                 \
  struct name {                                                                \
    type *data;                                                                \
    ARRAY_size_t n, i;                                                         \
  }

#define IMPL_ARRAY_RESERVE(name, type)                                         \
  struct name *name##_reserve(struct name *p, ARRAY_size_t n) {                \
    return (struct name *)ARRAY_reserve((ARRAY_t *)p, sizeof(type), n);        \
  }

#define IMPL_ARRAY_SET(name, type)                                             \
  struct name *name##_set(struct name *p, ARRAY_size_t i, type item) {         \
    return (struct name *)ARRAY_set((ARRAY_t *)p, sizeof(type), i, &item, 1,   \
                                    NULL);                                     \
  }

#define IMPL_ARRAY_PUSH(name, type)                                            \
  struct name *name##_push(struct name *p, type item) {                        \
    return (struct name *)ARRAY_set((ARRAY_t *)p, sizeof(type), p->i, &item,   \
                                    1, NULL);                                  \
  }

#define IMPL_ARRAY_APPEND(name, type)                                          \
  struct name *name##_append(struct name *p, const type *src,                  \
                             ARRAY_size_t len) {                               \
    return (struct name *)ARRAY_set((ARRAY_t *)p, sizeof(type), p->i, src,     \
                                    len, NULL);                                \
  }

#define IMPL_ARRAY_CLEAR(name, destroy)                                        \
  struct name *name##_clear(struct name *p, int option) {                      \
    return (struct name *)ARRAY_clear((ARRAY_t *)p, sizeof(p->data[0]),        \
                                      destroy, option);                        \
  }

#define IMPL_ARRAY_BSEARCH(name, compare)                                      \
  _Bool name##_bsearch(const struct name *p, const void *v, ARRAY_size_t *i) { \
    return ARRAY_bsearch((ARRAY_t *)p, sizeof(p->data[0]), compare, v, i);     \
  }

#define IMPL_ARRAY_BADD(name, init)                                            \
  _Bool name##_badd(struct name *p, const void *v, ARRAY_size_t *i) {          \
    ARRAY_size_t j = -1;                                                       \
    const _Bool found = name##_bsearch(p, v, &j);                              \
    assert(found || j != -1);                                                  \
    if (!found)                                                                \
      ARRAY_insert((ARRAY_t *)p, sizeof(p->data[0]), j, v, 1, init, NULL);     \
    if (i)                                                                     \
      *i = j;                                                                  \
    return !found;                                                             \
  }

typedef ARRAY_SIZE_TYPE ARRAY_size_t;
typedef HASH_SIZE_TYPE HASH_size_t;
typedef DECL_ARRAY(ARRAY_base, void) ARRAY_t;

typedef HASH_size_t (*ARRAY_hash_t)(const ARRAY_t *p, size_t sz, const void *v);
typedef HASH_size_t (*ARRAY_rehash_t)(const ARRAY_t *p, size_t sz, size_t i);
typedef const void *(*ARRAY_access_t)(const ARRAY_t *p, size_t sz, size_t i);
typedef int (*ARRAY_compare_t)(const void *v, const void *element, size_t sz);
typedef void *(*ARRAY_init_t)(void *dst, const void *v, size_t sz);
typedef void *(*ARRAY_move_t)(void *dst, const void *v, size_t sz);
typedef void (*ARRAY_destroy_t)(void *v);

enum array_destroy_option {
  ARRAY_DESTROY_ELEMENTS_ONLY,
  ARRAY_DESTROY_ALL,
  ARRAY_DESTROY_CONTAINER_ONLY,
};

size_t proper_capacity(size_t n);

ARRAY_t *ARRAY_reserve(ARRAY_t *p, size_t size, ARRAY_size_t n);
ARRAY_t *ARRAY_set(ARRAY_t *p, size_t size, ARRAY_size_t at, const void *src,
                   ARRAY_size_t nmem, ARRAY_move_t init);
ARRAY_t *ARRAY_insert(ARRAY_t *p, size_t size, ARRAY_size_t at, const void *src,
                      ARRAY_size_t nmem, ARRAY_init_t init, ARRAY_move_t move);
ARRAY_t *ARRAY_clear(ARRAY_t *p, size_t size, void (*destroy)(void *),
                     enum array_destroy_option option);
_Bool ARRAY_bsearch(const ARRAY_t *p, size_t size, ARRAY_compare_t compare,
                    const void *v, ARRAY_size_t *i);

const void *ARRAY_hput(ARRAY_t *p, size_t size, ARRAY_compare_t compare,
                       ARRAY_hash_t hash, ARRAY_rehash_t rehash,
                       ARRAY_access_t access, const void *v, ARRAY_move_t init);
const void *ARRAY_hget(const ARRAY_t *p, size_t size, ARRAY_compare_t compare,
                       ARRAY_hash_t hash, ARRAY_rehash_t rehash,
                       ARRAY_access_t access, const void *v,
                       ARRAY_size_t *slot);

static inline HASH_size_t ARRAY_hash(const ARRAY_t *p, size_t size,
                                     const void *v) {
  HASH_size_t out;
#if HASH_WIDTH == 32
  MurmurHash3_x86_32(v, size, HASH_SEED, &out);
#else
  MurmurHash3_x64_128(v, size, HASH_SEED, &out);
#endif
  return out;
}

static inline HASH_size_t ARRAY_rehash(const ARRAY_t *p, size_t size,
                                       size_t i) {
  return i + 1;
}

static inline const void *ARRAY_access(const ARRAY_t *p, size_t size,
                                       size_t i) {
  return (char *)p->data + i * size;
}
