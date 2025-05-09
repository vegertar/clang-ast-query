#pragma once

#include "string.h"
#include <stdio.h>

typedef STRING_PROPERTY_TYPE STRING_property_t;

typedef struct {
  HASH_size_t hash;
  STRING_property_t property;
  struct string elem;
} String;

typedef DECL_ARRAY(ANON, String) StringSet;

void StringSet_reserve(StringSet *ss, ARRAY_size_t n);

void StringSet_clear(StringSet *ss, int opt);

String *StringSet_add(StringSet *ss, const String *s);

void StringSet_dump(StringSet *ss, FILE *fp);

#define StringSet_for(ss, _i)                                                  \
  for (unsigned _i = 0, j = 0; _i < ss.n && j < ss.i; ++_i)                    \
    if (ss.data[_i].hash)
