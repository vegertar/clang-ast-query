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

#define ERROR_OF(x) (errcode ? (struct error){x, errcode} : (struct error){})

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int errcode;

static void store_dot();
static void store_semantics();

struct error store_init(const char *db_file) {
  INIT_DB(db_file);
  EXEC_SQL("PRAGMA synchronous = OFF");
  EXEC_SQL("PRAGMA journal_mode = MEMORY");
  return ERROR_OF(ES_STORE_INIT);
}

struct error store() {
  EXEC_SQL("BEGIN TRANSACTION");
  store_dot();
  store_semantics();
  EXEC_SQL("END TRANSACTION");
  return ERROR_OF(ES_STORE);
}

struct error store_halt() {
  HALT_DB();
  return ERROR_OF(ES_STORE_HALT);
}

static void store_dot() {
  EXEC_SQL("CREATE TABLE dot ("
           " cwd TEXT,"
           " tu TEXT,"
           " ts INTEGER)");

  INSERT_INTO(dot, CWD, TU, TS);
  FILL_TEXT(CWD, cwd);
  FILL_TEXT(TU, tu);
  FILL_INT(TS, ts);
  END_INSERT_INTO();
}

static void store_semantics() {
  EXEC_SQL("CREATE TABLE semantics ("
           " kind TEXT,"
           " name TEXT,"
           " begin_src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_src INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER)");

  for (unsigned i = 0; i < all_semantics.i && !errcode; ++i) {
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
}

struct error query_tu(char *path, int n) {
  QUERY("SELECT cwd, tu FROM dot");
  END_QUERY({
    assert(COL_SIZE(0) && "Expect a valid CWD");
    expand_path(COL_TEXT(0), COL_SIZE(0), COL_TEXT(1), path, n);
  });
  return ERROR_OF(ES_QUERY_TU);
}