#include "store.h"
#include "parse.h"
#include "test.h"
#include "util.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_STMT_SIZE
#define MAX_STMT_SIZE 16
#endif // !MAX_STMT_SIZE

#define PLACEHOLDERS(n) PP_JOIN(",", PP_DUP("?", n))
#define VALUES(...) PLACEHOLDERS(PP_NARG(__VA_ARGS__))

#define if_prepared_stmt(sql, ...)                                             \
  do {                                                                         \
    static sqlite3_stmt *stmt;                                                 \
    static sqlite3 *last_db;                                                   \
    __VA_ARGS__;                                                               \
    if (last_db != db) {                                                       \
      last_db = db;                                                            \
      stmt = NULL;                                                             \
    }                                                                          \
    if (!errcode && !stmt) {                                                   \
      int i = 0;                                                               \
      while (i < MAX_STMT_SIZE && stmts[i]) {                                  \
        ++i;                                                                   \
      }                                                                        \
      assert(i < MAX_STMT_SIZE);                                               \
      assert(db);                                                              \
      errcode = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);     \
      if (errcode)                                                             \
        fprintf(stderr, "%s:%d: sqlite3_prepare_v2 error: %s\n", __func__,     \
                __LINE__, sqlite3_errmsg(db));                                 \
      else                                                                     \
        stmt = stmts[i];                                                       \
    }                                                                          \
    if (stmt)                                                                  \
      sqlite3_clear_bindings(stmt);                                            \
    if (!errcode && stmt)

#define end_if_prepared_stmt(...)                                              \
  if (!errcode && stmt) {                                                      \
    int rc = 0;                                                                \
    do {                                                                       \
      rc = sqlite3_step(stmt);                                                 \
      if (rc == SQLITE_ROW) {                                                  \
        __VA_ARGS__                                                            \
      }                                                                        \
    } while (rc == SQLITE_ROW);                                                \
    if (rc == SQLITE_ERROR)                                                    \
      fprintf(stderr, "%s:%d: sqlite3_step error: %s\n", __func__, __LINE__,   \
              sqlite3_errmsg(db));                                             \
    if ((errcode = sqlite3_reset(stmt)))                                       \
      fprintf(stderr, "%s:%d: sqlite3_reset error(%d): %s\n", __func__,        \
              __LINE__, errcode, sqlite3_errstr(errcode));                     \
  }                                                                            \
  }                                                                            \
  while (0)

#define INSERT_INTO(table, ...)                                                \
  if_prepared_stmt("INSERT INTO " #table " (" #__VA_ARGS__                     \
                   ") VALUES (" VALUES(__VA_ARGS__) ")",                       \
                   enum {_, __VA_ARGS__}) {
#define END_INSERT_INTO()                                                      \
  }                                                                            \
  end_if_prepared_stmt()

#define QUERY(sql, ...) if_prepared_stmt(sql, __VA_ARGS__) {
#define END_QUERY(...)                                                         \
  }                                                                            \
  end_if_prepared_stmt(__VA_ARGS__)

#define BIND_TEXT(k, v, opt) sqlite3_bind_text(stmt, k, v, -1, opt)
#define FILL_TEXT(k, v) sqlite3_bind_text(stmt, k, v, -1, SQLITE_STATIC)
#define FILL_INT(k, v) sqlite3_bind_int(stmt, k, v)
#define COL_TEXT(k) (const char *)sqlite3_column_text(stmt, k)
#define COL_INT(k) sqlite3_column_int(stmt, k)
#define COL_SIZE(k) sqlite3_column_bytes(stmt, k)

#define OPEN_DB(file)                                                          \
  do {                                                                         \
    db = NULL;                                                                 \
    memset(stmts, 0, sizeof(stmts));                                           \
    errmsg = NULL;                                                             \
    if ((errcode = sqlite3_open(file, &db)))                                   \
      fprintf(stderr, "%s:%d: sqlite3_open(%s) error(%d): %s\n", __func__,     \
              __LINE__, file, errcode, sqlite3_errstr(errcode));               \
  } while (0)

#define CLOSE_DB()                                                             \
  do {                                                                         \
    for (int i = 0; !errcode && i < MAX_STMT_SIZE; ++i) {                      \
      if (stmts[i]) {                                                          \
        if ((errcode = sqlite3_finalize(stmts[i]))) {                          \
          fprintf(stderr, "%s:%d: sqlite3_finalize error(%d): %s\n", __func__, \
                  __LINE__, errcode, sqlite3_errstr(errcode));                 \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    if (!errcode && (errcode = sqlite3_close(db)))                             \
      fprintf(stderr, "%s:%d: sqlite3_close error(%d): %s\n", __func__,        \
              __LINE__, errcode, sqlite3_errstr(errcode));                     \
  } while (0)

#define EXEC_SQL(s)                                                            \
  do {                                                                         \
    if (db && !errcode) {                                                      \
      errcode = sqlite3_exec(db, s, NULL, NULL, &errmsg);                      \
      if (errmsg) {                                                            \
        fprintf(stderr, "%s:%d: sqlite3_exec(%s) error(%d): %s\n", __func__,   \
                __LINE__, s, errcode, errmsg);                                 \
        sqlite3_free(errmsg);                                                  \
      }                                                                        \
    } else {                                                                   \
      fprintf(stderr, "%s:%d: ignored `%s' due to error\n", __func__,          \
              __LINE__, s);                                                    \
    }                                                                          \
  } while (0)

#ifndef MAX_AST_LEVEL
#define MAX_AST_LEVEL 255
#endif // !MAX_AST_LEVEL

#define ERROR_OF(x) (errcode ? (struct error){x, errcode} : (struct error){})

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int errcode;

static void store_dot();
static void store_strings();
static void store_semantics();

struct error store_open(const char *db_file) {
  OPEN_DB(db_file);
  EXEC_SQL("PRAGMA synchronous = OFF");
  EXEC_SQL("PRAGMA journal_mode = MEMORY");
  return ERROR_OF(ES_STORE_OPEN);
}

struct error store() {
  EXEC_SQL("BEGIN TRANSACTION");
  store_dot();
  store_strings();
  store_semantics();
  EXEC_SQL("END TRANSACTION");
  return ERROR_OF(ES_STORE);
}

struct error store_close() {
  CLOSE_DB();
  return ERROR_OF(ES_STORE_CLOSE);
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

static void store_strings() {
  EXEC_SQL("CREATE TABLE strings ("
           " key TEXT PRIMARY KEY,"
           " property INTEGER,"
           " hash INTEGER)");

  StringSet_for(all_strings, i) {
    INSERT_INTO(strings, KEY, PROPERTY, HASH);
    FILL_TEXT(KEY, string_get(&all_strings.data[i].elem));
    FILL_INT(PROPERTY, all_strings.data[i].property);
    FILL_INT(HASH, all_strings.data[i].hash);
    END_INSERT_INTO();
  }
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
    const char *cwd = COL_TEXT(0);
    int cwd_len = COL_SIZE(0);
    const char *tu = COL_TEXT(1);
    const char *abs_path = expand_path(cwd, cwd_len, tu, path, n);
    if (abs_path != path) {
      require(strlen(abs_path) < n, "prepare to strcpy");
      strcpy(path, abs_path);
    }
  });
  return ERROR_OF(ES_QUERY_TU);
}

struct error query_strings(uint8_t property) {
  QUERY("SELECT key, hash FROM strings WHERE (property & ?)");
  FILL_INT(1, property);
  END_QUERY({
    fprintf(stderr, "%s\n", COL_TEXT(0));
  });
  return ERROR_OF(ES_QUERY_STRINGS);
}