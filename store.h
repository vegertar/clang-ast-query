#pragma once

#include "error.h"
#include "pp.h"

struct error store_open(const char *db_file);
struct error store();
struct error store_close();

typedef bool (*query_meta_row_t)(const char *cwd, int cwd_len, const char *tu,
                                 int tu_len, void *obj);
struct error query_meta(query_meta_row_t row, void *obj);

typedef bool (*query_strings_row_t)(const char *key, int key_len,
                                    uint8_t property, uint32_t hash, void *obj);
struct error query_strings(uint8_t property, query_strings_row_t row,
                           void *obj);

typedef bool (*query_semantics_row_t)(unsigned begin_row, unsigned begin_col,
                                      unsigned end_row, unsigned end_col,
                                      const char *kind, const char *name,
                                      void *obj);
struct error query_semantics(unsigned src, query_semantics_row_t row,
                             void *obj);

typedef bool (*query_link_row_t)(unsigned begin_row, unsigned begin_col,
                                 unsigned end_row, unsigned end_col,
                                 unsigned link, void *obj);
struct error query_link(unsigned src, query_link_row_t row, void *obj);