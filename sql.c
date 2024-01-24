#include "parse.h"
#include <sqlite3.h>
#include <stdio.h>
#include <assert.h>

#ifndef MAX_AST_LEVEL
#define MAX_AST_LEVEL 255
#endif

#ifndef MAX_STMT_SIZE
#define MAX_STMT_SIZE 16
#endif

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *err;
static int code;

static void exec_sql(const char *s) {
  if (!db || code) {
    return;
  }

  code = sqlite3_exec(db, s, NULL, NULL, &err);
  if (code && err) {
    fprintf(stderr, "sqlite3_exec(%s): %s\n", s, err);
    sqlite3_free(err);
  }
}

#define if_prepared_stmt(sql)                                                  \
  do {                                                                         \
    static sqlite3_stmt *stmt = NULL;                                          \
    if (!code && !stmt) {                                                      \
      int i = 0;                                                               \
      while (i < MAX_STMT_SIZE && stmts[i]) {                                  \
        ++i;                                                                   \
      }                                                                        \
      assert(i < MAX_STMT_SIZE);                                               \
      code = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);        \
      stmt = stmts[i];                                                         \
    }                                                                          \
    if (stmt)

#define end_if_prepared_stmt()                                                 \
  sqlite3_step(stmt);                                                          \
  sqlite3_reset(stmt);                                                         \
  }                                                                            \
  while (0)

static void dump_node(const struct node *node) {
  switch (node->kind) {
  case NODE_KIND_HEAD:
    if (node->loc.file) {
      if_prepared_stmt("INSERT OR IGNORE INTO ast (kind, id, file, line, col)"
                       " VALUES (?, ?, ?, ?, ?)") {
        sqlite3_bind_text(stmt, 1, node->name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, node->pointer, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, node->loc.file, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, node->loc.line);
        sqlite3_bind_int(stmt, 5, node->loc.col);
      }
      end_if_prepared_stmt();
    }
    break;
  case NODE_KIND_ENUM:
    break;
  case NODE_KIND_NULL:
    break;
  }
}

static void dump_decl(const struct decl *decl, const char *pointer) {
  switch (decl->kind) {
  case DECL_KIND_V9:
    if_prepared_stmt("UPDATE ast"
                     " SET name = ?, type = ?"
                     " WHERE id = ?") {
      sqlite3_bind_text(stmt, 1, decl->variants.v9.name, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, decl->variants.v9.def.type.qualified, -1,
                        SQLITE_STATIC);
      sqlite3_bind_text(stmt, 3, pointer, -1, SQLITE_STATIC);
    }
    end_if_prepared_stmt();

    break;
  default:
    break;
  }
}

static void dump_ast(const struct ast *ast, int max_level) {
  exec_sql("CREATE TABLE ast ("
           " kind TEXT,"
           " id TEXT PRIMARY KEY,"
           " file TEXT,"
           " line INTEGER,"
           " col INTEGER,"
           " name TEXT,"
           " type TEXT)");

  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    if (max_level == 0 || node->level <= max_level) {
      dump_node(node);
      dump_decl(&node->decl, node->pointer);
    }
  }
}

static void dump_hierarchy(const struct ast *ast, int max_level) {
  exec_sql("CREATE TABLE hierarchy ("
           " id TEXT PRIMARY KEY,"
           " parent TEXT)");

  const char *parents[MAX_AST_LEVEL + 1] = {};
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    if (max_level > 0 && node->level > max_level) {
      continue;
    }

    switch (node->kind) {
    case NODE_KIND_HEAD:
      assert(node->level < MAX_AST_LEVEL);

      if_prepared_stmt("INSERT OR IGNORE INTO hierarchy (id, parent)"
                       " VALUES (?, ?)") {
        sqlite3_bind_text(stmt, 1, node->pointer, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, parents[node->level], -1, SQLITE_STATIC);
      }
      end_if_prepared_stmt();

      if (node->level + 1 < MAX_AST_LEVEL) {
        parents[node->level + 1] = node->pointer;
      }
      break;
    case NODE_KIND_ENUM:
      break;
    case NODE_KIND_NULL:
      break;
    }
  }
}

void dump(int max_level, const char *db_file) {
  sqlite3_open(db_file, &db);

  exec_sql("PRAGMA synchronous = OFF");
  exec_sql("PRAGMA journal_mode = MEMORY");
  exec_sql("BEGIN TRANSACTION");
  dump_ast(&ast, max_level);
  dump_hierarchy(&ast, max_level);
  exec_sql("END TRANSACTION");

  for (int i = 0; i < MAX_STMT_SIZE; ++i) {
    if (stmts[i]) {
      sqlite3_finalize(stmts[i]);
    }
  }

  sqlite3_close(db);
}