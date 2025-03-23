#include "array.h"
#include "test.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t proper_capacity(size_t n) {
  if (!n)
    return 0;
  size_t lower_bits = 0;
  size_t m = n;
  while ((m >>= 1))
    ++lower_bits;
  size_t lower = 1 << lower_bits;
  return lower == n ? lower : (lower << 1);
}

TEST(proper_capacity, {
  ASSERT(proper_capacity(0) == 0);
  ASSERT(proper_capacity(1) == 1);
  ASSERT(proper_capacity(2) == 2);
  ASSERT(proper_capacity(3) == 4);
  ASSERT(proper_capacity(4) == 4);
})

ARRAY_t *ARRAY_reserve(ARRAY_t *p, size_t size, ARRAY_size_t n) {
  if (p->n < n) {
    void *data = realloc(p->data, size * n);
    assert(data);
    p->data = data;
    p->n = n;
  }
  return p;
}

TEST(ARRAY_reserve, {
  ARRAY_t seq = {};
  ARRAY_reserve(&seq, sizeof(int), 1);
  ASSERT(seq.data);
  ASSERT(seq.i == 0);
  ASSERT(seq.n == 1);
  free(seq.data);
})

ARRAY_t *ARRAY_set(ARRAY_t *p, size_t size, ARRAY_size_t at, const void *src,
                   ARRAY_size_t nmem, ARRAY_move_t init) {
  ARRAY_size_t i = at + nmem;

  if (p->n < i)
    ARRAY_reserve(p, size, proper_capacity(i));

  if (p->i < i) {
    if (p->i < at)
      memset((char *)p->data + p->i * size, 0, (at - p->i) * size);
    p->i = i;
  }
  if (src) {
    (init ? init : memcpy)((char *)p->data + at * size, src, size * nmem);
  }
  return p;
}

TEST(ARRAY_set, {
  ARRAY_t seq = {};
  for (int i = 0; i < 10; ++i)
    ARRAY_set(&seq, sizeof(i), i, &i, 1, NULL);
  ASSERT(seq.i == 10);
  ASSERT(seq.n == 16);

  // allocate a slot
  ARRAY_set(&seq, sizeof(int), 100, NULL, 1, NULL);

  ASSERT(seq.i == 101);
  ASSERT(seq.n == 128);
  // all middle elements are initialized to 0
  for (int i = 10; i < 100; ++i)
    ASSERT(*((int *)seq.data + i) == 0);

  free(seq.data);
})

ARRAY_t *ARRAY_insert(ARRAY_t *p, size_t size, ARRAY_size_t at, const void *src,
                      ARRAY_size_t nmem, ARRAY_init_t init, ARRAY_move_t move) {
  if (at >= p->i)
    return ARRAY_set(p, size, at, src, nmem, init);

  ARRAY_size_t length = p->i;
  // allocate slots
  ARRAY_set(p, size, length, NULL, nmem, init);
  char *dst = (char *)p->data + at * size;
  (move ? move : memmove)(dst + size * nmem, dst, (length - at) * size);
  (init ? init : memcpy)(dst, src, size * nmem);
  return p;
}

TEST(ARRAY_insert, {
  ARRAY_t seq = {};
  for (int i = 0; i < 10; ++i)
    ARRAY_set(&seq, sizeof(i), i, &i, 1, NULL);

  int a[] = {-1, -2, -3, -4};
  ARRAY_insert(&seq, sizeof(int), 4, a, 4, NULL, NULL);
  ASSERT(seq.i == 14);
  for (int i = 0; i < 4; ++i)
    ASSERT(*((int *)seq.data + i) == i);
  for (int i = 0; i < 4; ++i)
    ASSERT(*((int *)seq.data + i + 4) == a[i]);
  for (int i = 4; i < 10; ++i)
    ASSERT(*((int *)seq.data + i + 4) == i);

  free(seq.data);
})

ARRAY_t *ARRAY_clear(ARRAY_t *p, size_t size, ARRAY_destroy_t destroy,
                     enum array_destroy_option option) {
  if (destroy && option != ARRAY_DESTROY_CONTAINER_ONLY) {
    for (ARRAY_size_t i = 0; i < p->i; ++i) {
      destroy((char *)p->data + i * size);
    }
  }
  p->i = 0;
  if (option != ARRAY_DESTROY_ELEMENTS_ONLY && p->n) {
    free(p->data);
    p->n = 0;
    p->data = NULL;
  }
  return p;
}

TEST(ARRAY_clear, {
  ARRAY_t seq = {};
  for (int i = 0; i < 10; ++i)
    ARRAY_set(&seq, sizeof(i), i, &i, 1, NULL);

  ARRAY_clear(&seq, sizeof(int), NULL, ARRAY_DESTROY_ELEMENTS_ONLY);
  ASSERT(!seq.i);
  ASSERT(seq.n);
  ASSERT(seq.data);

  ARRAY_clear(&seq, sizeof(int), NULL, ARRAY_DESTROY_ALL);
  ASSERT(!seq.n);
  ASSERT(!seq.data);
})

bool ARRAY_bsearch(const ARRAY_t *p, size_t size, ARRAY_compare_t compare,
                   const void *v, ARRAY_size_t *i) {
  if (!v)
    return 0;

  ARRAY_size_t begin = 0;
  ARRAY_size_t end = p->i;

  while (begin < end) {
    const ARRAY_size_t mid = begin + (end - begin) / 2;
    const int d = compare(v, (char *)p->data + mid * size, size);
    if (d == 0) {
      if (i)
        *i = mid;
      return 1;
    }
    if (d < 0) {
      end = mid;
    } else {
      begin = mid + 1;
    }
  }

  if (i)
    *i = begin;
  return 0;
}

void *ARRAY_hput(ARRAY_t *p, size_t size, ARRAY_compare_t compare,
                 ARRAY_hash_t hash, ARRAY_rehash_t rehash,
                 ARRAY_access_t access, const void *v, ARRAY_move_t init) {
  if (!v)
    return NULL;

  ARRAY_size_t i = 0;
  void *t = ARRAY_hget(p, size, compare, hash, rehash, access, v, &i);

  // Either found the target or the table is full
  if (t || i == p->n)
    return t;

  p->i++;
  return (init ? init : memcpy)((char *)p->data + i * size, v, size);
}

void *ARRAY_hget(ARRAY_t *p, size_t size, ARRAY_compare_t compare,
                 ARRAY_hash_t hash, ARRAY_rehash_t rehash,
                 ARRAY_access_t access, const void *v, ARRAY_size_t *slot) {
  if (!v)
    return NULL;

  assert(p->n && "Not initialized yet");

  if (!compare)
    compare = memcmp;
  if (!hash)
    hash = ARRAY_hash;
  if (!rehash)
    rehash = ARRAY_rehash;
  if (!access)
    access = ARRAY_access;

  HASH_size_t code = hash(p, size, v);
  ARRAY_size_t i = code % p->n;
  ARRAY_size_t end = i;

  void *element;
  HASH_size_t element_code;
  bool hash_collision;

  do {
    element = access(p, size, i);
    element_code = hash(p, size, element);
    hash_collision = 0;

    // An available slot
    if (element_code == 0) {
      if (slot)
        *slot = i;
      return NULL;
    }

    if (element_code == code) {
      // Found target
      if (compare(v, element, size) == 0) {
        if (slot)
          *slot = i;
        return element;
      }

      hash_collision = 1;
    }

    i = rehash(p, size, i, hash_collision) % p->n;
  } while (i != end);

  // Full
  if (slot)
    *slot = p->n;
  return NULL;
}

#ifdef USE_TEST

static int compare_int(const void *v, const void *element, size_t n) {
  return *(const int *)v - *(const int *)element;
}

#endif // USE_TEST

TEST(ARRAY_bsearch, {
  ARRAY_t seq = {};
  for (int i = 0; i < 100; ++i)
    ARRAY_set(&seq, sizeof(i), i, &i, 1, NULL);

  ARRAY_size_t j;
  bool found;
  int v;

  for (int i = 0; i < 100; ++i) {
    v = i;
    found = ARRAY_bsearch(&seq, sizeof(v), compare_int, &v, &j);
    ASSERT(found);
    ASSERT(j == i);
  }

  v = -1000;
  found = ARRAY_bsearch(&seq, sizeof(v), compare_int, &v, &j);
  ASSERT(!found);
  ASSERT(j == 0);

  v = 1000;
  found = ARRAY_bsearch(&seq, sizeof(v), compare_int, &v, &j);
  ASSERT(!found);
  ASSERT(j == 100);

  ARRAY_clear(&seq, sizeof(v), NULL, ARRAY_DESTROY_ALL);
})
