#pragma once

#include "error.h"
#include "string.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static inline struct error open_file(const char *filename, const char *mode,
                                     FILE **out) {
  FILE *fp = fopen(filename, mode);
  if (!fp) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, filename,
            strerror(errno));
    return (struct error){ES_FILE_OPEN, errno};
  }

  assert(out);
  *out = fp;
  return (struct error){};
}

static inline struct error close_file(FILE *fp) {
  return fp && fclose(fp) ? (struct error){ES_FILE_CLOSE, errno}
                          : (struct error){};
}

static inline struct error rename_file(const char *from, const char *to) {
  return rename(from, to) ? (struct error){ES_FILE_RENAME, errno}
                          : (struct error){};
}

static inline struct error unlink_file(const char *file) {
  return unlink(file) ? (struct error){ES_FILE_UNLINK, errno}
                      : (struct error){};
}

struct error reads(FILE *fp, struct string *s, const char *escape);

const char *expand_path(const char *cwd, unsigned n, const char *in, char *out);

_Bool starts_with(const char *s, const char *starting);

_Bool ends_with(const char *s, const char *ending);

const char *rands(char buf[], unsigned cap);

unsigned __int128 hash(const void *key, int len);
