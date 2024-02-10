#include "test.h"
#include "parse.h"
#include <sqlite3.h>

#ifdef USE_TREE_SITTER
# include <tree_sitter/api.h>
#endif // USE_TREE_SITTER

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
#define VALUES18() \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?," \
  "?,?,?"

#define if_prepared_stmt(sql, ...)                                             \
  do {                                                                         \
    static sqlite3_stmt *stmt = NULL;                                          \
    __VA_ARGS__;                                                               \
    if (!err && !stmt) {                                                      \
      int i = 0;                                                               \
      while (i < MAX_STMT_SIZE && stmts[i]) {                                  \
        ++i;                                                                   \
      }                                                                        \
      assert(i < MAX_STMT_SIZE);                                               \
      err = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);        \
      stmt = stmts[i];                                                         \
    }                                                                          \
    if (!err && stmt)

#define end_if_prepared_stmt()                                                 \
    if (!err && stmt) {                                                       \
      sqlite3_step(stmt);                                                      \
      sqlite3_clear_bindings(stmt);                                            \
      err = sqlite3_reset(stmt);                                              \
    }                                                                          \
  }                                                                            \
  while (0)

#define INSERT_INTO(table, ...)                                                \
  if_prepared_stmt("INSERT INTO " #table " (" #__VA_ARGS__ ") VALUES ("        \
                   VALUES_I1(PP_NARG(__VA_ARGS__)) ")",                        \
                   enum { _, __VA_ARGS__ })

#define END_INSERT_INTO() end_if_prepared_stmt()

#define BIND_TEXT(k, v, opt) sqlite3_bind_text(stmt, k, v, -1, opt)
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
static char *errmsg;
static int err;

#ifdef USE_TREE_SITTER

struct position {
  int row;
  int col;
};

static int compare_position(struct position a, struct position b) {
  if (a.row < b.row) return -1;
  if (a.row == b.row) return a.col - b.col;
  return 1;
}

struct cst_node {
  int symbol;
  struct position begin;
  struct position end;
  int decl;
};

static int compare_cst_node(const void *a, struct cst_node *b) {
  if (compare_position(*(struct position *)a, b->begin) < 0) return -1;
  if (compare_position(*(struct position *)a, b->end) >= 0) return 1;
  return 0;
}

DECL_ARRAY(cst, struct cst_node);
IMPL_ARRAY_PUSH(cst, struct cst_node)
IMPL_ARRAY_BSEARCH(cst, compare_cst_node);
IMPL_ARRAY_CLEAR(cst, (void))

static void destroy_cst(void *p) {
  cst_clear((struct cst *)p, 2);
}

DECL_ARRAY(cst_set, struct cst);
IMPL_ARRAY_RESERVE(cst_set, struct cst)
IMPL_ARRAY_SET(cst_set, struct cst)
IMPL_ARRAY_CLEAR(cst_set, destroy_cst)

static struct cst_node * find_cst(const struct cst_set *cst_set,
                                  int src,
                                  int line,
                                  int col);

#else

struct cst_set {};

#endif // USE_TREE_SITTER

static void dump_src(struct cst_set *cst_set);
static void dump_ast(const struct cst_set *cst_set, const struct ast *ast);


static void exec_sql(const char *s) {
  if (!db || err) {
    return;
  }

  err = sqlite3_exec(db, s, NULL, NULL, &errmsg);
  if (errmsg) {
    fprintf(stderr, "sqlite3_exec(%s): %s\n", s, errmsg);
    sqlite3_free(errmsg);
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

int dump(const char *db_file) {
  struct cst_set cst_set = {};

  sqlite3_open(db_file, &db);

  exec_sql("PRAGMA synchronous = OFF");
  exec_sql("PRAGMA journal_mode = MEMORY");
  exec_sql("BEGIN TRANSACTION");
  dump_src(&cst_set);
  dump_ast(&cst_set, &ast);
  exec_sql("END TRANSACTION");

  for (int i = 0; i < MAX_STMT_SIZE; ++i) {
    if (stmts[i]) sqlite3_finalize(stmts[i]);
  }

  if (err && !errmsg)
    fprintf(stderr, "sqlite3 error(%d): %s\n",
            err, sqlite3_errstr(err));

  sqlite3_close(db);

#ifdef USE_TREE_SITTER
  cst_set_clear(&cst_set, 1);
#endif // USE_TREE_SITTER

  return err;
}

static void dump_ast(const struct cst_set *cst_set, const struct ast *ast) {
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
           " ref_ptr TEXT)");
/// We augment five type-relevant fields: type, type_ptr, type_src, type_row, type_col.
/// The type field represents the minimal tokenized type, such as 'uint8_t' in "const uint8_t **".
/// The type_ptr field indicates the original type declaration.
/// Specifically, in the case of a function, type and type_ptr represent the corresponding return type.
/// The set of positional type fields (type_src, type_row, type_col) simplifies query statements.
          //  " type TEXT,"
          //  " type_ptr TEXT,"
          //  " type_src INTEGER,"
          //  " type_row INTEGER,"
          //  " type_col INTEGER)");

  unsigned parents[MAX_AST_LEVEL + 1] = {-1};
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    const struct decl *decl = &node->decl;
    int parent_number = -1;
    int src = -1, line = 0, col = 0;

    INSERT_INTO(ast,
                NUMBER, PARENT_NUMBER,
                KIND, PTR,
                BEGIN_SRC, BEGIN_ROW, BEGIN_COL,
                END_SRC, END_ROW, END_COL,
                SRC, ROW, COL,
                NAME, QUALIFIED_TYPE, DESUGARED_TYPE, SPECS, REF_PTR) {
      switch (node->kind) {
      case NODE_KIND_HEAD:
        assert(node->level < MAX_AST_LEVEL);
        parent_number = parents[node->level];
        if (node->level + 1 < MAX_AST_LEVEL) parents[node->level + 1] = i;

        src = src_number(node->range.begin.file);
        line = node->range.begin.line;
        col = node->range.begin.col;

#ifdef USE_TREE_SITTER
        if (src != -1) {
          struct cst_node *cst_node = find_cst(cst_set, src, line, col);
# ifdef USE_LOG
          fprintf(stderr, "%*s%s:%d:%d -> ", node->level, "",
                  node->range.begin.file, line, col);
          if (!cst_node) fprintf(stderr, "NULL\n");
          else fprintf(stderr, "<%d>[%d:%d - %d:%d]\n", cst_node->symbol,
                       cst_node->begin.row, cst_node->begin.col,
                       cst_node->end.row, cst_node->end.col);
# endif // USE_LOG

          if (cst_node) {
            // TODO:
          }
        }
#endif // USE_TREE_SITTER

        FILL_INT(NUMBER, i);
        FILL_INT(PARENT_NUMBER, parent_number);
        FILL_TEXT(KIND, node->name);
        FILL_TEXT(PTR, node->pointer);
        FILL_INT(BEGIN_SRC, src);
        FILL_INT(BEGIN_ROW, line);
        FILL_INT(BEGIN_COL, col);
        FILL_INT(END_SRC, src_number(node->range.end.file));
        FILL_INT(END_ROW, node->range.end.line);
        FILL_INT(END_COL, node->range.end.col);
        FILL_INT(SRC, src_number(node->loc.file));
        FILL_INT(ROW, node->loc.line);
        FILL_INT(COL, node->loc.col);
        // FILL_TEXT(TYPE_PTR, find_var_type_map(node->pointer));
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

#ifdef USE_TREE_SITTER

struct ts_file_input {
  FILE *fp;
  char buffer[BUFSIZ];
  size_t n;
  size_t total;
};

typedef struct {
  unsigned indent;
  TSNode node;
} ts_node_wrapper;

DECL_ARRAY(ts_node_list, ts_node_wrapper);
IMPL_ARRAY_PUSH(ts_node_list, ts_node_wrapper)
IMPL_ARRAY_CLEAR(ts_node_list, (void))

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

static int symbol_kind(int symbol) {
  // See also https://raw.githubusercontent.com/tree-sitter/tree-sitter-c/master/src/parser.c
  switch (symbol) {
  case 1: // identifier
  case 150: // true
  case 151: // false
  case 152: // NULL
  case 153: // nullptr
  case 345: // field
    return 1;
  case 89: // primitive type
  case 347: // custom type
    return 2;
  default:
    return 0;
  }
}

static struct cst create_cst(const char *filename,
                             const char *code,
                             size_t size,
                             struct TSParser *parser) {
  struct ts_file_input input = {};
  if (!code && !(input.fp = fopen(filename, "ro"))) {
    fprintf(stderr, "%s: open('%s') error: %s\n",
            __func__, filename, strerror(errno));
    return (struct cst){};
  }

  struct cst cst = {};
  TSTree *tree = code ?
    ts_parser_parse_string(parser, NULL, code, size) :
    ts_parser_parse(parser, NULL, (TSInput){
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
    int kind = symbol_kind(symbol);

    // Only insert leaf nodes so that all ranges are disjoint
    if (n == 0) {
      // Note that TSPoint uses a zero-based index.
      const TSPoint start = ts_node_start_point(top.node);
      const TSPoint end = ts_node_end_point(top.node);

#ifdef USE_LOG
      fprintf(stderr, "%*s<%d> [%u, %u] - [%u, %u]\n", top.indent, "",
              symbol,
              start.row + 1, start.column + 1, end.row + 1, end.column + 1);
#endif // USE_LOG

      // Also ignore zero-length tokens.
      if (kind && memcmp(&start, &end, sizeof(TSPoint))) {
        cst_push(&cst, (struct cst_node){
            .symbol = symbol,
            .begin = (struct position){start.row + 1, start.column + 1},
            .end = (struct position){end.row + 1, end.column + 1},
          });
      }
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

  return cst;
}

static struct cst_node * find_cst(const struct cst_set *cst_set,
                                  int src,
                                  int line,
                                  int col) {
  if (src < 0 || src >= cst_set->i) return NULL;
  const struct cst *cst = &cst_set->data[src];
  if (cst->i == 0) return NULL;
  struct position pos = {line, col};
  unsigned i = -1;
  cst_bsearch(cst, &pos, &i);
  return i < cst->i ? &cst->data[i] : NULL;
}

#endif // USE_TREE_SITTER

static const char * expand_path(const char *cwd,
                                size_t n,
                                const char *in,
                                char *out) {
  assert(n < PATH_MAX);
  if (!in || *in == '/') return in;
  while (n > 0 && cwd[n - 1] == '/') --n;

  size_t i = 0;
  while (in[i] && in[i] != '/') ++i;
  if (!i) return out;

  if (in[0] == '.' && i <= 2) {
    if (in[1] == '.') {
      while (n > 0 && cwd[n - 1] != '/') --n;
    }
    memcpy(out, cwd, n);
  } else {
    assert(n + 1 + i < PATH_MAX);
    memcpy(out, cwd, n);
    out[n++] = '/';
    memcpy(out + n, in, i);
    n += i;
  }

  out[n] = 0;
  return expand_path(out, n, in + i + (in[i] == '/'), out);
}

TEST(expand_path, {
  char path[PATH_MAX];
  ASSERT(expand_path(NULL, 0, NULL, NULL) == NULL);
  ASSERT(strcmp(expand_path(NULL, 0, "a", path), "/a") == 0);
  ASSERT(strcmp(expand_path(NULL, 0, "/a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/", 1, "a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/", 1, "./a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/", 1, "../a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "/a", NULL), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "a", path), "/tmp/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, ".", path), "/tmp") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "./a", path), "/tmp/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "././a", path), "/tmp/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "../a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, ".././a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp", 4, "./../a", path), "/a") == 0);
  ASSERT(strcmp(expand_path("/tmp/xx/yy", 10, "./../a", path), "/tmp/xx/a") == 0);
})

static void dump_src(struct cst_set *cst_set) {
  exec_sql("CREATE TABLE src ("
           " filename TEXT PRIMARY KEY,"
           " number INTEGER)");

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
  cst_set_reserve(cst_set, filenames.i);
#endif

  char buffer[PATH_MAX];
  size_t cwd_len = strlen(cwd);
  for (unsigned i = 0; i < filenames.i; ++i) {
    const char *filename = (char *)filenames.data[i];
    if (is_internal_file(filename)) {
      continue;
    }

    const char *fullpath = expand_path(cwd, cwd_len, filename, buffer);
    if_prepared_stmt("INSERT INTO src (filename, number)"
                     " VALUES (?, ?)") {
      BIND_TEXT(1, fullpath,
                fullpath == buffer ? SQLITE_TRANSIENT : SQLITE_STATIC);
      FILL_INT(2, i);
    }
    end_if_prepared_stmt();

#ifdef USE_TREE_SITTER
    extern const char *tu_code;
    extern size_t tu_size;

    const char *code = strcmp(filename, tu) == 0 ? tu_code : NULL;
    size_t size = code ? tu_size : 0;
    struct cst cst = create_cst(fullpath, code, size, parser);
    cst_set_set(cst_set, i, cst);

    for (unsigned j = 0; j < cst.i; ++j) {
      struct cst_node *cst_node = &cst.data[j];
      if_prepared_stmt("INSERT INTO cst (src, symbol, begin_row, begin_col, end_row, end_col)"
                       " VALUES (?, ?, ?, ?, ?, ?)") {
        FILL_INT(1, i);
        FILL_INT(2, cst_node->symbol);
        FILL_INT(3, cst_node->begin.row);
        FILL_INT(4, cst_node->begin.col);
        FILL_INT(5, cst_node->end.row);
        FILL_INT(6, cst_node->end.col);
      }
      end_if_prepared_stmt();
    }
#endif // USE_TREE_SITTER
  }

#ifdef USE_TREE_SITTER
  ts_parser_delete(parser);
#endif // USE_TREE_SITTER
}
