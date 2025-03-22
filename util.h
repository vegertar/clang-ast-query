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
  assert(out);
  if ((*out = fopen(filename, mode)))
    return (struct error){};

  fprintf(stderr, "%s: open('%s') error: %s\n", __func__, filename,
          strerror(errno));
  return (struct error){ES_FILE_OPEN, errno};
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

// This function expands a given input path to the absolute one. Note that if
// the input itself is already an absolute path, it will return it directly to
// avoid unnecessary copies.
const char *expand_path(const char *cwd, unsigned cwd_len, const char *in,
                        char *const restrict out, unsigned out_cap);

bool starts_with(const char *s, const char *starting);

bool ends_with(const char *s, const char *ending);

const char *rands(char buf[], unsigned cap);

unsigned __int128 hash(const void *key, int len);
