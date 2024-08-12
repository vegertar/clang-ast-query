#pragma once

#include "string.h"
#include <stdio.h>

typedef struct {
  HASH_size_t hash;
  struct string elem;
} String;

typedef DECL_ARRAY(ANON, String) StringSet;

void StringSet_reserve(StringSet *ss, ARRAY_size_t n);

void StringSet_clear(StringSet *ss, int opt);

const String *StringSet_add(StringSet *ss, const String *s);

void StringSet_dump(StringSet *ss, FILE *fp);