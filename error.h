#pragma once

#include "pp.h"

#include <stdint.h>

enum errset : uint16_t {
#include "errset-inc.h"
};

struct error {
  enum errset es;
  int ec; // implementation specific error codes, e.g. errno
};

static inline struct error next_error(struct error err1, struct error err2) {
  return err1.es != ES_OK ? err1 : err2;
}

static inline const char *get_error_name(struct error err) {
  switch (err.es) {
#define ES_ENUM(name, ...)                                                     \
  case ES_##name:                                                              \
    return #name;

#include "errset-inc.h"
  }

  return (void *)0;
}

#define require(x, fmt, ...)                                                   \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d:%s:" fmt " REQUIRE %s\n", __FILE__, __LINE__,     \
              __func__, __VA_OPT__(, ) __VA_ARGS__ __VA_OPT__(, ) #x);         \
      return (struct error){ES_REQUIRE};                                       \
    }                                                                          \
  } while (0)
