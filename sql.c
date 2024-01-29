#include "parse.h"
#include <sqlite3.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifndef MAX_AST_LEVEL
#define MAX_AST_LEVEL 255
#endif

#ifndef MAX_STMT_SIZE
#define MAX_STMT_SIZE 16
#endif

// Credit: https://stackoverflow.com/a/2124385
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(                          \
  _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
  _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
  _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
  _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
  _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
  _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
  _61,_62,_63,N,...) N
#define PP_RSEQ_N()              \
  63,62,61,60,                   \
  59,58,57,56,55,54,53,52,51,50, \
  49,48,47,46,45,44,43,42,41,40, \
  39,38,37,36,35,34,33,32,31,30, \
  29,28,27,26,25,24,23,22,21,20, \
  19,18,17,16,15,14,13,12,11,10, \
  9,8,7,6,5,4,3,2,1,0

#define VALUES_I1(n) VALUES_I2(n)
#define VALUES_I2(n) VALUES##n()
#define VALUES17() \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?"

#define if_prepared_stmt(sql, ...)                                             \
  do {                                                                         \
    static sqlite3_stmt *stmt = NULL;                                          \
    __VA_ARGS__;                                                               \
    if (!code && !stmt) {                                                      \
      int i = 0;                                                               \
      while (i < MAX_STMT_SIZE && stmts[i]) {                                  \
        ++i;                                                                   \
      }                                                                        \
      assert(i < MAX_STMT_SIZE);                                               \
      code = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);        \
      stmt = stmts[i];                                                         \
    }                                                                          \
    if (!code && stmt)

#define end_if_prepared_stmt()                                                 \
    if (!code && stmt) {                                                       \
      sqlite3_step(stmt);                                                      \
      code = sqlite3_reset(stmt);                                              \
    }                                                                          \
  }                                                                            \
  while (0)

#define INSERT_INTO(table, ...) \
  if_prepared_stmt("INSERT INTO " #table " (" #__VA_ARGS__ ") VALUES (" \
                    VALUES_I1(PP_NARG(__VA_ARGS__)) ")",                \
                   enum { _, __VA_ARGS__ })

#define END_INSERT_INTO() end_if_prepared_stmt()

enum spec {
  SPEC_EXTERN = 1U,
  SPEC_STATIC = 1U << 1,
  SPEC_INLINE = 1U << 2,
  SPEC_CONST = 1U << 3,
  SPEC_VOLATILE = 1U << 4,
};

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *err;
static int code;

static void dump_ast(const struct ast *ast, int max_level);

static void exec_sql(const char *s) {
  if (!db || code) {
    return;
  }

  code = sqlite3_exec(db, s, NULL, NULL, &err);
  if (err) {
    fprintf(stderr, "sqlite3_exec(%s): %s\n", s, err);
    sqlite3_free(err);
  }
}

static int src_number(const char *filename) {
  unsigned i = UINT_MAX;
  char *found = search_filename(filename, &i);
  assert(filename == NULL || found);
  assert(i == -1 || i < INT_MAX);
  return i;
}

static unsigned mark_specs(const struct def *def) {
  unsigned specs = 0;
  for (unsigned i = 0; i < def->specs.i; ++i) {
    const char *spec = (const char *)(def->specs.data[i]);
    if (strcmp(spec, "extern") == 0) {
      specs |= SPEC_EXTERN;
    } else if (strcmp(spec, "static") == 0) {
      specs |= SPEC_STATIC;
    } else if (strcmp(spec, "inline") == 0) {
      specs |= SPEC_INLINE;
    } else if (strcmp(spec, "const") == 0) {
      specs |= SPEC_CONST;
    } else if (strcmp(spec, "volatile") == 0) {
      specs |= SPEC_VOLATILE;
    }
  }
  assert(specs <= INT_MAX);
  return specs;
}

static void dump_source() {
  exec_sql("CREATE TABLE source ("
           " filename TEXT PRIMARY KEY,"
           " number INTEGER)");

  for (unsigned i = 0; i < filenames.i; ++i) {
    if_prepared_stmt("INSERT INTO source (filename, number)"
                     " VALUES (?, ?)") {
      sqlite3_bind_text(stmt, 1, (char *)filenames.data[i], -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 2, i);
    }
    end_if_prepared_stmt();
  }
}


int dump(int max_level, const char *db_file) {
  sqlite3_open(db_file, &db);

  exec_sql("PRAGMA synchronous = OFF");
  exec_sql("PRAGMA journal_mode = MEMORY");
  exec_sql("BEGIN TRANSACTION");
  dump_ast(&ast, max_level);
  dump_source();
  exec_sql("END TRANSACTION");

  for (int i = 0; i < MAX_STMT_SIZE; ++i) {
    if (stmts[i]) {
      sqlite3_finalize(stmts[i]);
    }
  }

  if (code) {
    fprintf(stderr, "sqlite3 error(%d): %s\n", code, sqlite3_errstr(code));
  }
  sqlite3_close(db);
  return code;
}

static void dump_ast(const struct ast *ast, int max_level) {
  exec_sql("CREATE TABLE ast ("
           " number INTEGER,"
           " parent_number INTEGER,"
           " kind TEXT,"
           " ptr TEXT,"
           " begin_src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_src INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER,"
           " src INTEGER,"
           " row INTEGER,"
           " col INTEGER,"
           " name TEXT,"
           " type TEXT,"
           " specs INTEGER,"
           " ref_ptr TEXT)");

  unsigned parents[MAX_AST_LEVEL + 1] = {-1};
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    const struct decl *decl = &node->decl;
    if (max_level == 0 || node->level <= max_level) {
      INSERT_INTO(ast,
                  NUMBER, PARENT_NUMBER,
                  KIND, PTR,
                  BEGIN_SRC, BEGIN_ROW, BEGIN_COL,
                  END_SRC, END_ROW, END_COL,
                  SRC, ROW, COL,
                  NAME, TYPE, SPECS, REF_PTR) {
        switch (node->kind) {
        case NODE_KIND_HEAD:
          assert(node->level < MAX_AST_LEVEL);
          sqlite3_bind_int(stmt, NUMBER, i);
          sqlite3_bind_int(stmt, PARENT_NUMBER, parents[node->level]);
          if (node->level + 1 < MAX_AST_LEVEL) {
            parents[node->level + 1] = i;
          }

          sqlite3_bind_text(stmt, KIND, node->name, -1, SQLITE_STATIC);
          sqlite3_bind_text(stmt, PTR, node->pointer, -1, SQLITE_STATIC);
          sqlite3_bind_int(stmt, BEGIN_SRC, src_number(node->range.begin.file));
          sqlite3_bind_int(stmt, BEGIN_ROW, node->range.begin.line);
          sqlite3_bind_int(stmt, BEGIN_COL, node->range.begin.col);
          sqlite3_bind_int(stmt, END_SRC, src_number(node->range.end.file));
          sqlite3_bind_int(stmt, END_ROW, node->range.end.line);
          sqlite3_bind_int(stmt, END_COL, node->range.end.col);
          sqlite3_bind_int(stmt, SRC, src_number(node->loc.file));
          sqlite3_bind_int(stmt, ROW, node->loc.line);
          sqlite3_bind_int(stmt, COL, node->loc.col);
          break;
        case NODE_KIND_ENUM:
          break;
        case NODE_KIND_NULL:
          break;
        }

#define FILL_REF(expr) do {                                                \
    const struct ref *ref = expr;                                          \
    if (ref->pointer) {                                                    \
      sqlite3_bind_text(stmt, REF_PTR, ref->pointer, -1, SQLITE_STATIC);   \
      sqlite3_bind_text(stmt, NAME, ref->sqname, -1, SQLITE_STATIC);       \
    }                                                                      \
  } while (0)

#define FILL_DEF(expr) do {                                                \
    const struct def *def = expr;                                          \
    sqlite3_bind_text(stmt, TYPE, def->type.qualified, -1, SQLITE_STATIC); \
    sqlite3_bind_int(stmt, SPECS, mark_specs(def));                        \
    FILL_REF(&def->ref);                                                   \
  } while (0)

        switch (decl->kind) {
        case DECL_KIND_V8:
          FILL_DEF(&decl->variants.v8.def);
          break;
        case DECL_KIND_V9:
          sqlite3_bind_text(stmt, NAME, decl->variants.v9.name, -1, SQLITE_STATIC);
          FILL_DEF(&decl->variants.v9.def);
          break;
        default:
          break;
        }
      }
      END_INSERT_INTO();
    }
  }
}
