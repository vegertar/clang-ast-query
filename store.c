#include "store.h"
#include "parse.h"
#include "test.h"
#include "util.h"

#include <sqlite3.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_AST_LEVEL
#define MAX_AST_LEVEL 255
#endif // !MAX_AST_LEVEL

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int err;

static int store_semantics();

int store_init(const char *db_file) {
  OPEN_DB(db_file);
  EXEC_SQL("PRAGMA synchronous = OFF");
  EXEC_SQL("PRAGMA journal_mode = MEMORY");
  return err;
}

int store() {
  if (err)
    return err;

  EXEC_SQL("BEGIN TRANSACTION");
  err += store_semantics();
  EXEC_SQL("END TRANSACTION");

  return err;
}

void store_halt() { CLOSE_DB(); }

static int store_semantics() {
  EXEC_SQL("CREATE TABLE semantics ("
           " kind TEXT,"
           " name TEXT,"
           " begin_src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_src INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER)");

#ifndef VALUES8
#define VALUES8() "?,?,?,?,?,?,?,?"
#endif // !VALUES8

  for (unsigned i = 0; i < all_semantics.i; ++i) {
    const char *kind = string_get(&all_semantics.data[i].kind->elem);
    const char *name = string_get(&all_semantics.data[i].name->elem);
    const Range *range = &all_semantics.data[i].range;

    INSERT_INTO(semantics, KIND, NAME, BEGIN_SRC, BEGIN_ROW, BEGIN_COL, END_SRC,
                END_ROW, END_COL);
    FILL_TEXT(KIND, kind);
    FILL_TEXT(NAME, name);
    FILL_INT(BEGIN_SRC, range->begin.file->hash);
    FILL_INT(BEGIN_ROW, range->begin.line);
    FILL_INT(BEGIN_COL, range->begin.col);
    FILL_INT(END_SRC, range->end.file->hash);
    FILL_INT(END_ROW, range->end.line);
    FILL_INT(END_COL, range->end.col);
    END_INSERT_INTO();
  }

  return 0;
}
