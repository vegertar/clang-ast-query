#pragma once

#include "array.h"

enum input_kind {
  IK_C,
  IK_TEXT,
  IK_DATA,
  IK_NUMS,
};

struct input {
  int kind;
  char *file;
  char *tu;
  char **opts;
};

static inline void free_input(void *p) {
  struct input *f = p;
  // TODO:
}

DECL_ARRAY(input_list, struct input);
static inline IMPL_ARRAY_PUSH(input_list, struct input);
static inline IMPL_ARRAY_CLEAR(input_list, free_input);

extern struct input_list input_list;

#define clear_input() input_list_clear(&input_list, 1)

#define add_input(...) input_list_push(&input_list, (struct input)__VA_ARGS__)

#define add_input_if_empty(...)                                                \
  do {                                                                         \
    if (!input_list.i)                                                         \
      add_input(__VA_ARGS__);                                                  \
  } while (0)

#define foreach_input(item, ...)                                               \
  do {                                                                         \
    for (unsigned __i = 0; __i < input_list.i; ++__i) {                        \
      struct input item = input_list.data[__i];                                \
      __VA_ARGS__                                                              \
    }                                                                          \
  } while (0)

enum output_kind {
  OK_NIL,
  OK_TEXT,
  OK_DATA,
  OK_HTML,
  OK_NUMS,
};

struct output {
  int kind;
  char *file;
  unsigned char silent : 1;
  unsigned char noparse : 1;
};

struct error build_output(struct output o);