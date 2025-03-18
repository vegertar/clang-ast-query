#pragma once

#include "pp.h"

#include <stdint.h>

enum errset : uint16_t {
#include "error-inc.h"
};

struct error {
  enum errset es;
  int ec; // implementation specific error codes, e.g. errno
};

static inline struct error next_error(struct error err1, struct error err2) {
  return err1.es != ES_OK ? err1 : err2;
}