#include "util.h"
#include "murmur3.h"
#include "test.h"

#include <assert.h>
#include <stdlib.h>

int reads(FILE *fp, struct string *s, const char *escape) {
  assert(fp && s);

  char buffer[BUFSIZ];
  size_t n = 0;
  while ((n = fread(buffer, 1, sizeof(buffer), fp))) {
    string_reserve(s, s->i + n + 1);

    if (escape)
      ESCAPE(escape, buffer, n, s);
    else
      string_append(s, buffer, n);

    s->data[s->i] = 0;
  }

  if (ferror(fp)) {
    fprintf(stderr, "%s: fread error\n", __func__);
    return -1;
  }

  return 0;
}

const char *expand_path(const char *cwd, unsigned n, const char *in,
                        char *out) {
  assert(n < PATH_MAX);
  if (!in || *in == '/')
    return in;
  while (n > 0 && cwd[n - 1] == '/')
    --n;

  size_t i = 0;
  while (in[i] && in[i] != '/')
    ++i;
  if (!i)
    return out;

  if (in[0] == '.' && i <= 2) {
    if (in[1] == '.') {
      while (n > 0 && cwd[n - 1] != '/')
        --n;
    }
    memcpy(out, cwd, n);
  } else {
    assert(n + 1 + i < PATH_MAX);
    memcpy(out, cwd, n);
    out[n++] = '/';
    memcpy(out + n, in, i);
    n += i;
  }

  out[n] = 0;
  return expand_path(out, n, in + i + (in[i] == '/'), out);
}

TEST(expand_path, {
  char path[PATH_MAX];
  ASSERT(expand_path(NULL, 0, NULL, NULL) == NULL);
  ASSERT(strcmp(expand_path(NULL, 0, "a", path), "/a") == 0);
  ASSERT(strcmp(expand_path(NULL, 0, "/a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/", 1, "a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/", 1, "./a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/", 1, "../a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "/a", NULL), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "a", path), "/tmp/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, ".", path), "/tmp") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "./a", path), "/tmp/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "././a", path), "/tmp/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "../a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, ".././a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "./../a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp/x/y", 10, "./../a", path), "/tmp/x/a") == 0);
})

_Bool starts_with(const char *s, const char *starting) {
  if (!s || !starting)
    return 0;

  int i = 0;
  while (s[i] && s[i] == starting[i])
    ++i;
  return starting[i] == 0;
}

TEST(starts_with, {
  ASSERT(!starts_with(NULL, NULL));
  ASSERT(starts_with("/", ""));
  ASSERT(starts_with("/", "/"));
  ASSERT(starts_with("/abc", "/ab"));
  ASSERT(!starts_with("/abc", "/abcd"));
});

_Bool ends_with(const char *s, const char *ending) {
  int n = s ? strlen(s) : 0;
  int m = ending ? strlen(ending) : 0;
  return m == 0 || n >= m && strcmp(s + n - m, ending) == 0;
}

TEST(ends_with, {
  ASSERT(ends_with("", ""));
  ASSERT(ends_with("/", ""));
  ASSERT(ends_with("/abc", "abc"));
  ASSERT(!ends_with("/abc", "/"));
});

const char *rands(char buf[], unsigned cap) {
  static const char s[] = "0123456789"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz";
  if (!buf || !cap)
    return NULL;

  int v = rand();
  const size_t n = cap - 1;
  size_t i = 0;
  while (v > 0 && i < n) {
    buf[i++] = s[v % (sizeof(s) - 1)];
    v /= 3;
  }
  buf[i] = 0;
  return buf;
}

unsigned __int128 hash(const void *key, int len) {
  unsigned __int128 out;
  MurmurHash3_x64_128(key, len, 496789, &out);
  return out;
}