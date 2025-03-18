#pragma once

#include "pp.h"

#include <stdint.h>

#define ES(type, mask, ...)                                                    \
  ES_I1(PP_NARG(__VA_ARGS__))(NO(__VA_ARGS__), type, mask, __VA_ARGS__)
#define ES_I1(n) ES_I2(n)
#define ES_I2(n) ES##n

#define ES_CURR(order, n, type, mask, name, ...)                               \
  ES_##type##_##name = (mask) + (n - order + 1)

#define ES_NEXT(n, type, mask, name, ...) n, type, mask, __VA_ARGS__

#define ES0(n, type, mask, ...) ES_##type = mask
#define ES1(...) ES_CURR(1, __VA_ARGS__), ES0(__VA_ARGS__)
#define ES2(...) ES_CURR(2, __VA_ARGS__), ES1(ES_NEXT(__VA_ARGS__))
#define ES3(...) ES_CURR(3, __VA_ARGS__), ES2(ES_NEXT(__VA_ARGS__))
#define ES4(...) ES_CURR(4, __VA_ARGS__), ES3(ES_NEXT(__VA_ARGS__))
#define ES5(...) ES_CURR(5, __VA_ARGS__), ES4(ES_NEXT(__VA_ARGS__))
#define ES6(...) ES_CURR(6, __VA_ARGS__), ES5(ES_NEXT(__VA_ARGS__))
#define ES7(...) ES_CURR(7, __VA_ARGS__), ES6(ES_NEXT(__VA_ARGS__))
#define ES8(...) ES_CURR(8, __VA_ARGS__), ES7(ES_NEXT(__VA_ARGS__))

enum errset : uint16_t {
  ES_OK,
  ES_BUILDING_PROHIBITED,

  ES(FILE, 0x0100U, OPEN, CLOSE, READ, WRITE, RENAME, UNLINK),
  ES(REMARK, 0x0200U, NO_CLANG),
  ES(PARSE, 0x0300U, INIT, HALT),
  ES(STORE, 0x0400U, INIT, HALT),
  ES(RENDER, 0x0500U),
};

struct error {
  enum errset es;
  int ec; // implementation specific error codes, e.g. errno
};

static inline struct error next_error(struct error err1, struct error err2) {
  return err1.es != ES_OK ? err1 : err2;
}