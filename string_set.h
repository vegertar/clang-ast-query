#pragma once

#include "string.h"
#include <stdio.h>

#define STRING_PROPERTY_TEXT 0U
#define STRING_PROPERTY_FILE 1U
#define STRING_PROPERTY_IDENTIFIER 2U

typedef struct {
  HASH_size_t hash;
  uint8_t property;
  struct string elem;
} String;

typedef DECL_ARRAY(ANON, String) StringSet;

void StringSet_reserve(StringSet *ss, ARRAY_size_t n);

void StringSet_clear(StringSet *ss, int opt);

const String *StringSet_add(StringSet *ss, const String *s);

void StringSet_dump(StringSet *ss, FILE *fp);