#pragma once

#ifndef MAX_STMT_SIZE
#define MAX_STMT_SIZE 16
#endif // !MAX_STMT_SIZE

// Credit: https://stackoverflow.com/a/2124385
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,  \
                 _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26,   \
                 _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38,   \
                 _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50,   \
                 _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62,   \
                 _63, N, ...)                                                  \
  N
#define PP_RSEQ_N()                                                            \
  63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,  \
      44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27,  \
      26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9,   \
      8, 7, 6, 5, 4, 3, 2, 1, 0

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
      err = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);         \
      if (err)                                                                 \
        fprintf(stderr, "sqlite3_prepare_v2 error: %s\n", sqlite3_errmsg(db)); \
      stmt = stmts[i];                                                         \
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
      fprintf(stderr, "sqlite3_step error: %s\n", sqlite3_errmsg(db));         \
    if ((err = sqlite3_reset(stmt)))                                           \
      fprintf(stderr, "sqlite3 error(%d): %s\n", err, sqlite3_errstr(err));    \
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
#define COL_INT(k) sqlite3_column_INT(stmt, k)
#define COL_SIZE(k) sqlite3_column_bytes(stmt, k)

#define OPEN_DB(file)                                                          \
  do {                                                                         \
    db = NULL;                                                                 \
    memset(stmts, 0, sizeof(stmts));                                           \
    errmsg = NULL;                                                             \
    if ((err = sqlite3_open(file, &db)))                                       \
      fprintf(stderr, "%s: sqlite3 error(%d): %s\n", file, err,                \
              sqlite3_errstr(err));                                            \
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
        fprintf(stderr, "sqlite3_exec(%s): %s\n", s, errmsg);                  \
        sqlite3_free(errmsg);                                                  \
      }                                                                        \
    } else {                                                                   \
      fprintf(stderr, "Ignored `%s' due to error\n", s);                       \
    }                                                                          \
  } while (0)

int sql(const char *db_file);
void sql_halt();