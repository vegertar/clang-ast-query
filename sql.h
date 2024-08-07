#pragma once

#include "pp.h"

#ifndef MAX_STMT_SIZE
#define MAX_STMT_SIZE 16
#endif // !MAX_STMT_SIZE

#define VALUES_I1(n) VALUES_I2(n)
#define VALUES_I2(n) VALUES##n()

#define if_prepared_stmt(sql, ...)                                             \
  do {                                                                         \
    static sqlite3_stmt *stmt;                                                 \
    static sqlite3 *last_db;                                                   \
    __VA_ARGS__;                                                               \
    if (last_db != db) {                                                       \
      last_db = db;                                                            \
      stmt = NULL;                                                             \
    }                                                                          \
    if (!err && !stmt) {                                                       \
      int i = 0;                                                               \
      while (i < MAX_STMT_SIZE && stmts[i]) {                                  \
        ++i;                                                                   \
      }                                                                        \
      assert(i < MAX_STMT_SIZE);                                               \
      assert(db);                                                              \
      err = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);         \
      if (err)                                                                 \
        fprintf(stderr, "%s:%d: sqlite3_prepare_v2 error: %s\n", __func__,     \
                __LINE__, sqlite3_errmsg(db));                                 \
      else                                                                     \
        stmt = stmts[i];                                                       \
    }                                                                          \
    if (stmt)                                                                  \
      sqlite3_clear_bindings(stmt);                                            \
    if (!err && stmt)

#define end_if_prepared_stmt(...)                                              \
  if (!err && stmt) {                                                          \
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
    if ((err = sqlite3_reset(stmt)))                                           \
      fprintf(stderr, "%s:%d: sqlite3 error(%d): %s\n", __func__, __LINE__,    \
              err, sqlite3_errstr(err));                                       \
  }                                                                            \
  }                                                                            \
  while (0)

#define INSERT_INTO(table, ...)                                                \
  if_prepared_stmt("INSERT INTO " #table " (" #__VA_ARGS__                     \
                   ") VALUES (" VALUES_I1(PP_NARG(__VA_ARGS__)) ")",           \
                   enum {_, __VA_ARGS__}) {
#define END_INSERT_INTO()                                                      \
  }                                                                            \
  end_if_prepared_stmt()

#define QUERY(sql, ...) if_prepared_stmt(sql, __VA_ARGS__) {
#define QUERY_END(...)                                                         \
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
    if ((err = sqlite3_open(file, &db)))                                       \
      fprintf(stderr, "%s:%d: %s: sqlite3 error(%d): %s\n", __func__,          \
              __LINE__, file, err, sqlite3_errstr(err));                       \
  } while (0)

#define CLOSE_DB()                                                             \
  do {                                                                         \
    for (int i = 0; i < MAX_STMT_SIZE; ++i) {                                  \
      if (stmts[i])                                                            \
        sqlite3_finalize(stmts[i]);                                            \
    }                                                                          \
    sqlite3_close(db);                                                         \
  } while (0)

#define EXEC_SQL(s)                                                            \
  do {                                                                         \
    if (db && !err) {                                                          \
      err = sqlite3_exec(db, s, NULL, NULL, &errmsg);                          \
      if (errmsg) {                                                            \
        fprintf(stderr, "%s:%d: sqlite3_exec(%s): %s\n", __func__, __LINE__,   \
                s, errmsg);                                                    \
        sqlite3_free(errmsg);                                                  \
      }                                                                        \
    } else {                                                                   \
      fprintf(stderr, "%s:%d: ignored `%s' due to error\n", __func__,          \
              __LINE__, s);                                                    \
    }                                                                          \
  } while (0)

int sql(const char *db_file);
void sql_halt();