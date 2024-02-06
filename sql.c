#include "test.h"
#include "parse.h"
#include <sqlite3.h>

#ifdef USE_TREE_SITTER
# include <tree_sitter/api.h>
#endif // USE_TREE_SITTER

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifndef MAX_AST_LEVEL
# define MAX_AST_LEVEL 255
#endif // !MAX_AST_LEVEL

#ifndef MAX_STMT_SIZE
# define MAX_STMT_SIZE 16
#endif // !MAX_STMT_SIZE

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
#define VALUES19() \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?"

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
      sqlite3_clear_bindings(stmt);                                            \
      code = sqlite3_reset(stmt);                                              \
    }                                                                          \
  }                                                                            \
  while (0)

#define INSERT_INTO(table, ...) \
  if_prepared_stmt("INSERT INTO " #table " (" #__VA_ARGS__ ") VALUES (" \
                    VALUES_I1(PP_NARG(__VA_ARGS__)) ")",                \
                   enum { _, __VA_ARGS__ })

#define END_INSERT_INTO() end_if_prepared_stmt()

#define FILL_TEXT(k, v) sqlite3_bind_text(stmt, k, v, -1, SQLITE_STATIC)
#define FILL_INT(k, v) sqlite3_bind_int(stmt, k, v)

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
static void dump_cst();
static void dump_src();

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

static inline _Bool is_internal_file(const char *filename) {
  return strcmp(filename, "<built-in>") == 0 ||
      strcmp(filename, "<scratch space>") == 0;
}

int dump(int max_level, const char *db_file) {
  sqlite3_open(db_file, &db);

  exec_sql("PRAGMA synchronous = OFF");
  exec_sql("PRAGMA journal_mode = MEMORY");
  exec_sql("BEGIN TRANSACTION");
  dump_ast(&ast, max_level);
  dump_cst();
  dump_src();
  exec_sql("END TRANSACTION");

  for (int i = 0; i < MAX_STMT_SIZE; ++i) {
    if (stmts[i]) {
      sqlite3_finalize(stmts[i]);
    }
  }

  if (code && !err) {
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
           " qualified_type TEXT,"
           " desugared_type TEXT,"
           " specs INTEGER,"
           " ref_ptr TEXT,"
           " type_ptr TEXT)");

  unsigned parents[MAX_AST_LEVEL + 1] = {-1};
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    const struct decl *decl = &node->decl;
    if (max_level == 0 || node->level <= max_level) {
      INSERT_INTO(ast,
                  NUMBER, PARENT_NUMBER,
                  KIND, PTR, TYPE_PTR,
                  BEGIN_SRC, BEGIN_ROW, BEGIN_COL,
                  END_SRC, END_ROW, END_COL,
                  SRC, ROW, COL,
                  NAME, QUALIFIED_TYPE, DESUGARED_TYPE, SPECS, REF_PTR) {
        switch (node->kind) {
        case NODE_KIND_HEAD:
          assert(node->level < MAX_AST_LEVEL);
          FILL_INT(NUMBER, i);
          FILL_INT(PARENT_NUMBER, parents[node->level]);
          if (node->level + 1 < MAX_AST_LEVEL) {
            parents[node->level + 1] = i;
          }

          FILL_TEXT(KIND, node->name);
          FILL_TEXT(PTR, node->pointer);
          FILL_TEXT(TYPE_PTR, find_var_type_map(node->pointer));
          FILL_INT(BEGIN_SRC, src_number(node->range.begin.file));
          FILL_INT(BEGIN_ROW, node->range.begin.line);
          FILL_INT(BEGIN_COL, node->range.begin.col);
          FILL_INT(END_SRC, src_number(node->range.end.file));
          FILL_INT(END_ROW, node->range.end.line);
          FILL_INT(END_COL, node->range.end.col);
          FILL_INT(SRC, src_number(node->loc.file));
          FILL_INT(ROW, node->loc.line);
          FILL_INT(COL, node->loc.col);
          break;
        case NODE_KIND_ENUM:
          break;
        case NODE_KIND_NULL:
          break;
        }

#define FILL_REF(expr) do {                                                \
    const struct ref *ref = expr;                                          \
    if (ref->pointer) {                                                    \
      FILL_TEXT(REF_PTR, ref->pointer);                                    \
      FILL_TEXT(NAME, ref->sqname);                                        \
    }                                                                      \
  } while (0)

#define FILL_DEF(expr) do {                                                \
    const struct def *def = expr;                                          \
    FILL_TEXT(QUALIFIED_TYPE, def->type.qualified);                        \
    FILL_TEXT(DESUGARED_TYPE, def->type.desugared);                        \
    FILL_INT(SPECS, mark_specs(def));                                      \
    FILL_REF(&def->ref);                                                   \
  } while (0)

        switch (decl->kind) {
        case DECL_KIND_V8:
          FILL_DEF(&decl->variants.v8.def);
          break;
        case DECL_KIND_V9:
          FILL_TEXT(NAME, decl->variants.v9.name);
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

#ifdef USE_TREE_SITTER

struct ts_file_input {
  FILE *fp;
  char buffer[BUFSIZ];
  size_t n;
  size_t total;
};

static const char * ts_file_input_read(void *payload,
                                       uint32_t byte_index,
                                       TSPoint position,
                                       uint32_t *bytes_read) {
  struct ts_file_input *self = (struct ts_file_input *)payload;
  if (byte_index < self->total) {
    size_t rest = self->total - byte_index;
    size_t offset = self->n - rest;
    *bytes_read = rest;
    return self->buffer + offset;
  }

  size_t n = fread(self->buffer, 1, sizeof(self->buffer), self->fp);
  *bytes_read = self->n = n;
  self->total += n;
  return self->buffer;
}

static void clear_ts_file_input(struct ts_file_input *self) {
  if (self->fp) {
    fclose(self->fp);
    self->fp = NULL;
    self->n = self->total = 0;
  }
}

typedef struct {
  unsigned indent;
  TSNode node;
} ts_node_wrapper;

DECL_ARRAY(ts_node_list, ts_node_wrapper);
IMPL_ARRAY_PUSH(ts_node_list, ts_node_wrapper)
IMPL_ARRAY_CLEAR(ts_node_list, (void))

#endif // USE_TREE_SITTER

static void dump_cst() {
#ifdef USE_TREE_SITTER
  exec_sql("CREATE TABLE cst ("
           " src INTEGER,"
           " symbol INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER)");

  TSLanguage *tree_sitter_c();
  TSLanguage *lang = tree_sitter_c();
  TSParser *parser = ts_parser_new();
  ts_parser_set_language(parser, lang);
  struct ts_file_input input = {};

  for (unsigned i = 0; i < filenames.i; ++i) {
    const char *filename = (char *)filenames.data[i];
    if (is_internal_file(filename)) {
      continue;
    }

    if (!(input.fp = fopen(filename, "ro"))) {
      fprintf(stderr, "%s: open('%s') error: %s\n",
              __func__, filename, strerror(errno));
      continue;
    }

    int src = src_number(filename);
    TSTree *tree = ts_parser_parse(parser, NULL, (TSInput){
        &input,
        ts_file_input_read,
    });
    struct ts_node_list stack = {};
    ts_node_list_push(&stack, (ts_node_wrapper){0, ts_tree_root_node(tree)});

    // A preorder traversal
    while (stack.i) {
      ts_node_wrapper top = stack.data[--stack.i];
      uint32_t n = ts_node_child_count(top.node);
      int symbol = ts_node_symbol(top.node);

      // Note that TSPoint uses a zero-based index.
      const TSPoint start = ts_node_start_point(top.node);
      const TSPoint end = ts_node_end_point(top.node);

      if (yydebug) {
        printf("%*s<%d:%s> [%u, %u] - [%u, %u]\n", top.indent, "",
              symbol,
              ts_node_is_named(top.node) ? ts_language_symbol_name(lang, symbol) : "",
              start.row + 1, start.column + 1, end.row + 1, end.column + 1);
      }

      // Only insert leaf nodes so that all ranges are disjoint
      if (n == 0) {
        if_prepared_stmt("INSERT INTO cst (src, symbol, begin_row, begin_col, end_row, end_col)"
                         " VALUES (?, ?, ?, ?, ?, ?)") {
          FILL_INT(1, src);
          FILL_INT(2, symbol);
          FILL_INT(3, start.row + 1);
          FILL_INT(4, start.column + 1);
          FILL_INT(5, end.row + 1);
          FILL_INT(6, end.column + 1);
        }
        end_if_prepared_stmt();
      }

      for (uint32_t i = 0; i < n; ++i) {
        // Push children in reverse order so that the leftmost child is on top of the stack
        ts_node_list_push(&stack, (ts_node_wrapper){
            top.indent + 1,
            ts_node_child(top.node, n - 1 - i),
        });
      }
    }

    ts_node_list_clear(&stack, 2);
    ts_tree_delete(tree);
    clear_ts_file_input(&input);
  }

  ts_parser_delete(parser);
#endif // USE_TREE_SITTER
}

static void dump_src() {
  exec_sql("CREATE TABLE src ("
           " filename TEXT PRIMARY KEY,"
           " number INTEGER)");

  for (unsigned i = 0; i < filenames.i; ++i) {
    if_prepared_stmt("INSERT INTO src (filename, number)"
                     " VALUES (?, ?)") {
      FILL_TEXT(1, (char *)filenames.data[i]);
      FILL_INT(2, i);
    }
    end_if_prepared_stmt();
  }
}
