#pragma once

#include "string.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif // !PATH_MAX

#define ALT(x, y) (x ? x : y)

#define ESCAPE(escape, in, n, out)                                             \
  do {                                                                         \
    size_t j = 0, k = 0;                                                       \
                                                                               \
    do {                                                                       \
      while (j < n && !strchr(escape, in[j]))                                  \
        ++j;                                                                   \
                                                                               \
      string_append(out, in + k, j - k);                                       \
      if (j == n)                                                              \
        break;                                                                 \
                                                                               \
      string_push(out, '\\');                                                  \
      k = j++;                                                                 \
    } while (k < n);                                                           \
  } while (0)

static inline FILE *open_file(const char *filename, const char *mode) {
  FILE *fp = fopen(filename, mode);
  if (!fp) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, filename,
            strerror(errno));
  }
  return fp;
}

static inline void close_file(FILE *fp) {
  if (fp)
    fclose(fp);
}

int reads(FILE *fp, struct string *s, const char *escape);

const char *expand_path(const char *cwd, unsigned n, const char *in, char *out);

_Bool starts_with(const char *s, const char *starting);

_Bool ends_with(const char *s, const char *ending);

const char *rands(char buf[], unsigned cap);

unsigned __int128 hash(const void *key, int len);
