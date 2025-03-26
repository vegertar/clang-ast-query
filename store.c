#include "store.h"
#include "parse.h"
#include "test.h"

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
    assert(state % 2 == 1 && "Should be in open");                             \
    static sqlite3_stmt *stmt;                                                 \
    static unsigned last_state;                                                \
    __VA_ARGS__;                                                               \
    if (last_state != state) {                                                 \
      last_state = state;                                                      \
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

static_assert(sizeof(long) <= 8, "Should be filled in sqlite3 integer");

// clang-format off
#define FILL_INT(k, v)                                                         \
  _Generic((v), signed            : sqlite3_bind_int,                          \
                unsigned          : sqlite3_bind_int64,                        \
                long              : sqlite3_bind_int64,                        \
                char              : sqlite3_bind_int,                          \
                unsigned char     : sqlite3_bind_int)(stmt, k, v)

#define PICK_INT(k, v)                                                         \
  v = _Generic((v),                                                            \
                signed            : sqlite3_column_int,                        \
                unsigned          : sqlite3_column_int64,                      \
                long              : sqlite3_column_int64,                      \
                char              : sqlite3_column_int,                        \
                unsigned char     : sqlite3_column_int)(stmt, k)

// clang-format on

#define COL_TEXT(k) (const char *)sqlite3_column_text(stmt, k)
#define COL_SIZE(k) sqlite3_column_bytes(stmt, k)

#define OPEN_DB(file)                                                          \
  do {                                                                         \
    assert(state % 2 == 0 && "Should be closed");                              \
    db = NULL;                                                                 \
    memset(stmts, 0, sizeof(stmts));                                           \
    errmsg = NULL;                                                             \
    if ((errcode = sqlite3_open(file, &db)))                                   \
      fprintf(stderr, "%s:%d: sqlite3_open(%s) error(%d): %s\n", __func__,     \
              __LINE__, file, errcode, sqlite3_errstr(errcode));               \
    else                                                                       \
      ++state;                                                                 \
  } while (0)

#define CLOSE_DB()                                                             \
  do {                                                                         \
    assert(state % 2 == 1 && "Should be in open");                             \
    for (int i = 0; !errcode && i < MAX_STMT_SIZE; ++i) {                      \
      if (stmts[i]) {                                                          \
        if ((errcode = sqlite3_finalize(stmts[i]))) {                          \
          fprintf(stderr, "%s:%d: sqlite3_finalize error(%d): %s\n", __func__, \
                  __LINE__, errcode, sqlite3_errstr(errcode));                 \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    if (!errcode) {                                                            \
      if ((errcode = sqlite3_close(db)))                                       \
        fprintf(stderr, "%s:%d: sqlite3_close error(%d): %s\n", __func__,      \
                __LINE__, errcode, sqlite3_errstr(errcode));                   \
      else                                                                     \
        ++state;                                                               \
    }                                                                          \
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
static unsigned state; // increasing, even for closed, odd for being open

static void store_meta();
static void store_strings();
static void store_semantics();
static void store_nodes();

struct error store_open(const char *db_file) {
  OPEN_DB(db_file);
  EXEC_SQL("PRAGMA synchronous = OFF");
  EXEC_SQL("PRAGMA journal_mode = MEMORY");
  return ERROR_OF(ES_STORE_OPEN);
}

struct error store() {
  EXEC_SQL("BEGIN TRANSACTION");
  store_meta();
  store_strings();
  store_semantics();
  store_nodes();
  EXEC_SQL("END TRANSACTION");
  return ERROR_OF(ES_STORE);
}

struct error store_close() {
  CLOSE_DB();
  return ERROR_OF(ES_STORE_CLOSE);
}

static void store_meta() {
  EXEC_SQL("CREATE TABLE meta ("
           " cwd TEXT,"
           " tu TEXT,"
           " ts INTEGER)");

  INSERT_INTO(meta, CWD, TU, TS);
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
    if (errcode)
      break;

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

static void store_nodes() {
  EXEC_SQL("CREATE TABLE nodes ("
           " node INTEGER,"
           " ptr INTEGER,"
           " prev_ptr INTEGER,"
           " begin_src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_src INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER,"
           " src INTEGER,"
           " row INTEGER,"
           " col INTEGER,"
           "link INTEGER)");

  for (unsigned i = 0; i < all_nodes.i && !errcode; ++i) {
    INSERT_INTO(nodes, NODE, PTR, PREV_PTR, BEGIN_SRC, BEGIN_ROW, BEGIN_COL,
                END_SRC, END_ROW, END_COL, SRC, ROW, COL, LINK);
    FILL_INT(NODE, all_nodes.data[i].node);

#define FILL_PTR(ptr) FILL_INT(PTR, (int64_t)ptr)
#define FILL_PREV_PTR(ptr) FILL_INT(PREV_PTR, (int64_t)ptr)
#define FILL_RANGE(range)                                                      \
  do {                                                                         \
    FILL_INT(BEGIN_SRC, range.begin.file->hash);                               \
    FILL_INT(BEGIN_ROW, range.begin.line);                                     \
    FILL_INT(BEGIN_COL, range.begin.col);                                      \
    FILL_INT(END_SRC, range.end.file->hash);                                   \
    FILL_INT(END_ROW, range.end.line);                                         \
    FILL_INT(END_COL, range.end.col);                                          \
  } while (0)
#define FILL_LOC(loc)                                                          \
  do {                                                                         \
    FILL_INT(SRC, loc.file->hash);                                             \
    FILL_INT(ROW, loc.line);                                                   \
    FILL_INT(COL, loc.col);                                                    \
  } while (0)

    switch (all_nodes.data[i].kind) {
    case TOK_InclusionDirective: {
      assert(all_nodes.data[i].group == NG_Directive);
      auto *p = &all_nodes.data[i].InclusionDirective;
      FILL_PTR(p->pointer);
      FILL_PREV_PTR(p->prev);
      FILL_RANGE(p->range);
      FILL_LOC(p->loc);
      FILL_INT(LINK, p->path->hash);
      break;
    }

    default:
      break;
    }

    END_INSERT_INTO();
  }
}

struct error query_meta(query_meta_row_t row, void *obj) {
  assert(row);
  QUERY("SELECT cwd, tu FROM meta");
  END_QUERY({
    if (row(COL_TEXT(0), COL_SIZE(0), COL_TEXT(1), COL_SIZE(1), obj))
      break;
  });
  return ERROR_OF(ES_QUERY_TU);
}

struct error query_strings(uint8_t property, query_strings_row_t row,
                           void *obj) {
  assert(row);
  QUERY("SELECT key, property, hash FROM strings WHERE (property & ?)");
  FILL_INT(1, property);
  END_QUERY({
    uint8_t property;
    uint32_t hash;

    PICK_INT(1, property);
    PICK_INT(2, hash);

    if (row(COL_TEXT(0), COL_SIZE(0), property, hash, obj))
      break;
  });
  return ERROR_OF(ES_QUERY_STRINGS);
}

struct error query_semantics(unsigned src, query_semantics_row_t row,
                             void *obj) {
  assert(row);
  QUERY("SELECT begin_row, begin_col, end_row, end_col, kind, name"
        " FROM semantics"
        " WHERE begin_src = ?"
        " ORDER BY begin_row, begin_col");
  FILL_INT(1, src);
  END_QUERY({
    unsigned begin_row, begin_col, end_row, end_col;

    PICK_INT(0, begin_row);
    PICK_INT(1, begin_col);
    PICK_INT(2, end_row);
    PICK_INT(3, end_col);

    const char *kind = COL_TEXT(4);
    const char *name = COL_TEXT(5);

    if (row(begin_row, begin_col, end_row, end_col, kind, name, obj))
      break;
  });
  return ERROR_OF(ES_QUERY_SEMANTICS);
}

struct error query_link(unsigned src, query_link_row_t row, void *obj) {
  assert(row);
  QUERY("SELECT begin_row, begin_col, end_row, end_col, link"
        " FROM nodes"
        " WHERE begin_src = ?"
        " ORDER BY begin_row, begin_col");
  FILL_INT(1, src);
  END_QUERY({
    unsigned begin_row, begin_col, end_row, end_col, link;

    PICK_INT(0, begin_row);
    PICK_INT(1, begin_col);
    PICK_INT(2, end_row);
    PICK_INT(3, end_col);
    PICK_INT(4, link);

    if (row(begin_row, begin_col, end_row, end_col, link, obj))
      break;
  });
  return ERROR_OF(ES_QUERY_LINK);
}
