#include "parse.h"
#include "test.h"
#include <sqlite3.h>

#ifdef USE_TREE_SITTER
#include <tree_sitter/api.h>
#endif // USE_TREE_SITTER

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_AST_LEVEL
#define MAX_AST_LEVEL 255
#endif // !MAX_AST_LEVEL

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
#define VALUES7()                                                              \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?"
#define VALUES19()                                                             \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?"

#define if_prepared_stmt(sql, ...)                                             \
  do {                                                                         \
    static sqlite3_stmt *stmt = NULL;                                          \
    __VA_ARGS__;                                                               \
    if (!err && !stmt) {                                                       \
      int i = 0;                                                               \
      while (i < MAX_STMT_SIZE && stmts[i]) {                                  \
        ++i;                                                                   \
      }                                                                        \
      assert(i < MAX_STMT_SIZE);                                               \
      err = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmts[i], NULL);         \
      stmt = stmts[i];                                                         \
    }                                                                          \
    if (stmt)                                                                  \
      sqlite3_clear_bindings(stmt);                                            \
    if (!err && stmt)

#define end_if_prepared_stmt()                                                 \
  if (!err && stmt) {                                                          \
    sqlite3_step(stmt);                                                        \
    err = sqlite3_reset(stmt);                                                 \
  }                                                                            \
  }                                                                            \
  while (0)

#define INSERT_INTO(table, ...)                                                \
  if_prepared_stmt("INSERT INTO " #table " (" #__VA_ARGS__                     \
                   ") VALUES (" VALUES_I1(PP_NARG(__VA_ARGS__)) ")",           \
                   enum {_, __VA_ARGS__})

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
  return (a.row < b.row) ? -1 : (a.row == b.row) ? (a.col - b.col) : 1;
}

struct cst_node {
  int symbol;
  int kind;
  int decl;
  struct position begin;
  struct position end;
};

DECL_ARRAY(cst, struct cst_node);
static inline IMPL_ARRAY_PUSH(cst, struct cst_node);
static inline IMPL_ARRAY_CLEAR(cst, NULL);

struct cst_wrapper {
  struct cst cst;
  ARRAY_size_t i; // Zero-based index at which to start searching
};

static void destroy_cst(void *p) { cst_clear((struct cst *)p, 2); }

DECL_ARRAY(cst_set, struct cst_wrapper);
static inline IMPL_ARRAY_RESERVE(cst_set, struct cst_wrapper);
static inline IMPL_ARRAY_SET(cst_set, struct cst_wrapper);
static inline IMPL_ARRAY_CLEAR(cst_set, destroy_cst);

static void dump_src(struct cst_set *cst_set);
static void dump_ast(const struct cst_set *cst_set, const struct ast *ast);
static void dump_cst(const struct cst_set *cst_set);

#else

static void dump_src();
static void dump_ast(const struct ast *ast);

#endif // USE_TREE_SITTER

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
  unsigned i = -1;
  const char *found = search_src(filename, &i);
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

static int numeric_class(const char *s) {
  if (!s)
    return 0;
  if (strcmp(s, "struct") == 0)
    return 1;
  if (strcmp(s, "union") == 0)
    return 2;
  if (strcmp(s, "enum") == 0)
    return 3;
  return -1;
}

static inline _Bool is_internal_file(const char *filename) {
  return strcmp(filename, "<built-in>") == 0 ||
         strcmp(filename, "<scratch space>") == 0 ||
         strcmp(filename, "<command line>") == 0;
}

int dump(const char *db_file) {
  sqlite3_open(db_file, &db);

  exec_sql("PRAGMA synchronous = OFF");
  exec_sql("PRAGMA journal_mode = MEMORY");
  exec_sql("BEGIN TRANSACTION");

#ifdef USE_TREE_SITTER
  struct cst_set cst_set = {};
  dump_src(&cst_set);
  dump_ast(&cst_set, &ast);
  dump_cst(&cst_set);
#else
  dump_src();
  dump_ast(&ast);
#endif // USE_TREE_SITTER

  exec_sql("END TRANSACTION");

  for (int i = 0; i < MAX_STMT_SIZE; ++i) {
    if (stmts[i])
      sqlite3_finalize(stmts[i]);
  }

  if (err && !errmsg)
    fprintf(stderr, "sqlite3 error(%d): %s\n", err, sqlite3_errstr(err));

#ifdef USE_TREE_SITTER
  cst_set_clear(&cst_set, 1);
#endif // USE_TREE_SITTER

  sqlite3_close(db);
  return err;
}

#define FILL_REF(expr)                                                         \
  do {                                                                         \
    const struct ref *ref = expr;                                              \
    if (ref->pointer) {                                                        \
      FILL_TEXT(REF_PTR, (ref_ptr = ref->pointer));                            \
      FILL_TEXT(NAME, ref->sqname);                                            \
    }                                                                          \
  } while (0)

#define FILL_DEF(expr)                                                         \
  do {                                                                         \
    const struct def *def = expr;                                              \
    FILL_TEXT(QUALIFIED_TYPE, def->type.qualified);                            \
    FILL_TEXT(DESUGARED_TYPE, def->type.desugared);                            \
    FILL_INT(SPECS, mark_specs(def));                                          \
    FILL_REF(&def->ref);                                                       \
  } while (0)

#ifdef USE_TREE_SITTER

struct decl_number_value {
  unsigned var;
  unsigned type;
  unsigned macro;
};

/// Represents a pair consisting of a pointer to a declaration node in
/// an AST and its corresponding index number for variable or type.
struct decl_number_pair {
  const char *decl;
  struct decl_number_value number;
};

static int compare_decl_number(const void *a, const void *b, size_t n) {
  struct decl_number_pair *x = (struct decl_number_pair *)a;
  struct decl_number_pair *y = (struct decl_number_pair *)b;
  return strcmp(x->decl, y->decl);
}

DECL_ARRAY(decl_number_map, struct decl_number_pair);
static inline IMPL_ARRAY_PUSH(decl_number_map, struct decl_number_pair);
static inline IMPL_ARRAY_BSEARCH(decl_number_map, compare_decl_number);
static inline IMPL_ARRAY_BADD(decl_number_map, NULL);
static inline IMPL_ARRAY_CLEAR(decl_number_map, NULL);

enum name_kind {
  NAME_KIND_UNKNOWN = 0U,
  NAME_KIND_DECL = 1U,          // b0001
  NAME_KIND_VALUABLE_MASK = 2U, // b0010
  NAME_KIND_VALUABLE_DECL = 3U, // b0011
  NAME_KIND_FUNCTION_DECL = 7U, // b0111
};

static enum name_kind name_kind(const char *name) {
  unsigned kind = NAME_KIND_UNKNOWN;
  if (!name)
    return kind;
  size_t n = strlen(name);
  if (n > 4 && strcmp(name + n - 4, "Decl") == 0) {
    kind = NAME_KIND_DECL;
    if (n == 12 && strncmp("Function", name, 8) == 0)
      kind = NAME_KIND_FUNCTION_DECL;
    else if (n == 9 && strncmp("Field", name, 5) == 0 ||
             n >= 7 && strncmp("Var", name + n - 7, 3) == 0)
      kind = NAME_KIND_VALUABLE_DECL;
  }
  return kind;
}

enum symbol_kind {
  SYMBOL_KIND_UNKNOWN = 0U,
  SYMBOL_KIND_IDENTIFIER = 1U,  // b0001
  SYMBOL_KIND_TYPE = 2U,        // b0010
  SYMBOL_KIND_CUSTOM_MASK = 4U, // b0100
};

static enum symbol_kind symbol_kind(int symbol) {
  // See also
  // https://raw.githubusercontent.com/vegertar/tree-sitter-c/master/src/parser.c
  switch (symbol) {
  case 155: // true
  case 156: // false
  case 158: // nullptr
#if __STDC_VERSION__ >= 202311L
    return SYMBOL_KIND_IDENTIFIER;
#endif      // c23
  case 157: // NULL
  case 1:   // identifier
  case 351: // field
    return SYMBOL_KIND_CUSTOM_MASK | SYMBOL_KIND_IDENTIFIER;
  case 91: // void type
  case 92: // boolean type
  case 93: // complex type
  case 94: // primitive type
    return SYMBOL_KIND_TYPE;
  case 353: // custom type
    return SYMBOL_KIND_CUSTOM_MASK | SYMBOL_KIND_TYPE;
  default:
    return SYMBOL_KIND_UNKNOWN;
  }
}

static void set_cst_decl(struct cst_node *cst_node,
                         struct decl_number_value decl) {
  if (cst_node) {
    if (cst_node->kind & SYMBOL_KIND_IDENTIFIER)
      cst_node->decl = decl.var;
    else if (cst_node->kind & SYMBOL_KIND_TYPE)
      cst_node->decl = decl.type;
  }
}

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
static inline IMPL_ARRAY_PUSH(ts_node_list, ts_node_wrapper);
static inline IMPL_ARRAY_CLEAR(ts_node_list, NULL);

static const char *ts_file_input_read(void *payload, uint32_t byte_index,
                                      TSPoint position, uint32_t *bytes_read) {
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

static struct cst create_cst(const char *filename, const char *code,
                             size_t size, struct TSParser *parser) {
  struct ts_file_input input = {};
  if (!code && !(input.fp = fopen(filename, "ro"))) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, filename,
            strerror(errno));
    return (struct cst){};
  }

  struct cst cst = {};
  TSTree *tree = code ? ts_parser_parse_string(parser, NULL, code, size)
                      : ts_parser_parse(parser, NULL,
                                        (TSInput){
                                            &input,
                                            ts_file_input_read,
                                        });
  struct ts_node_list stack = {};
  ts_node_list_push(&stack, (ts_node_wrapper){0, ts_tree_root_node(tree)});

  TOGGLE(print_cst, fprintf(stderr, "=== %s ===\n", filename));

  // A preorder traversal
  while (stack.i) {
    ts_node_wrapper top = stack.data[--stack.i];
    uint32_t n = ts_node_child_count(top.node);
    int symbol = ts_node_symbol(top.node);
    unsigned kind = symbol_kind(symbol);

    // Note that TSPoint uses a zero-based index.
    const TSPoint start = ts_node_start_point(top.node);
    const TSPoint end = ts_node_end_point(top.node);

    TOGGLE(print_cst, fprintf(stderr, "%*s<%d:%u>[%u:%u - %u:%u]\n",
                              top.indent * 2, "", symbol, kind, start.row + 1,
                              start.column + 1, end.row + 1, end.column + 1));

    // Only insert leaf nodes so that all ranges are disjoint.
    // Also ignore zero-length tokens.
    if (n == 0 && kind && memcmp(&start, &end, sizeof(TSPoint))) {
      cst_push(&cst,
               (struct cst_node){
                   .symbol = symbol,
                   .kind = kind,
                   .begin = (struct position){start.row + 1, start.column + 1},
                   .end = (struct position){end.row + 1, end.column + 1},
               });
    }

    for (uint32_t i = 0; i < n; ++i) {
      // Push children in reverse order so that the leftmost child is on top of
      // the stack
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

static struct cst_node *find_cst(const struct cst_set *cst_set, int src,
                                 struct position pos) {
  if (src < 0 || src >= cst_set->i)
    return NULL;

  struct cst_wrapper *cst_wrapper = &cst_set->data[src];

  // Find the next closest cst token.
  while (cst_wrapper->i < cst_wrapper->cst.i) {
    struct cst_node *v = &cst_wrapper->cst.data[cst_wrapper->i];
    if (compare_position(pos, v->begin) <= 0)
      return v;

    cst_wrapper->i += 1;
  }
  return NULL;
}

static void dump_cst(const struct cst_set *cst_set) {
  exec_sql("CREATE TABLE cst ("
           " src INTEGER,"
           " kind INTEGER,"
           " decl INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER)");

  for (unsigned i = 0; i < src_set.i; ++i) {
    unsigned src = src_set.data[i].number;
    struct cst cst = cst_set->data[src].cst;

    TOGGLE(print_cst_node_missing_decl,
           fprintf(stderr, "=== %s ===\n", src_set.data[i].filename));

    for (unsigned j = 0; j < cst.i; ++j) {
      struct cst_node *cst_node = &cst.data[j];
      if (!cst_node->decl) {
        TOGGLE(print_cst_node_missing_decl, {
          if (cst_node->kind & SYMBOL_KIND_CUSTOM_MASK)
            fprintf(stderr, "<%d:%u>[%d:%d - %d:%d]\n", cst_node->symbol,
                    cst_node->kind, cst_node->begin.row, cst_node->begin.col,
                    cst_node->end.row, cst_node->end.col);
        });
        continue;
      }

      INSERT_INTO(cst, SRC, KIND, DECL, BEGIN_ROW, BEGIN_COL, END_ROW,
                  END_COL) {
        FILL_INT(SRC, src);
        FILL_INT(KIND, cst_node->kind);
        FILL_INT(DECL, cst_node->decl);
        FILL_INT(BEGIN_ROW, cst_node->begin.row);
        FILL_INT(BEGIN_COL, cst_node->begin.col);
        FILL_INT(END_ROW, cst_node->end.row);
        FILL_INT(END_COL, cst_node->end.col);
      }
      END_INSERT_INTO();
    }
  }
}

struct positional_node {
  int src;
  struct position pos;
  unsigned i;       // The index of current node.
  const char *decl; // The pointer of the declaration node.
};

static int compare_positional_node(const void *a, const void *b) {
  const struct positional_node *x = (const struct positional_node *)a;
  const struct positional_node *y = (const struct positional_node *)b;
  const int d = x->src - y->src;
  return d == 0 ? compare_position(x->pos, y->pos) : d;
}

DECL_ARRAY(positional_node_set, struct positional_node);
static inline IMPL_ARRAY_PUSH(positional_node_set, struct positional_node);
static inline IMPL_ARRAY_CLEAR(positional_node_set, NULL);

static void dump_ast(const struct cst_set *cst_set, const struct ast *ast)
#else
static void dump_ast(const struct ast *ast)
#endif // USE_TREE_SITTER
{
  exec_sql("CREATE TABLE ast ("
           " number INTEGER PRIMARY KEY,"
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
           " class INTEGER,"
           " qualified_type TEXT,"
           " desugared_type TEXT,"
           " specs INTEGER,"
           " ref_ptr TEXT)");

#ifdef USE_TREE_SITTER
  struct positional_node_set positional_node_set = {};
  struct decl_number_map decl_number_map = {};
  _Bool found = 0, added = 0;
  unsigned j = -1;
  unsigned kind = 0;
  const char *decl_ptr = NULL;
  struct cst_node *last_cst_node = NULL;
#endif

  unsigned parents[MAX_AST_LEVEL + 1] = {-1};
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    const struct decl *decl = &node->decl;
    const char *ref_ptr = NULL;
    int parent_number = -1;
    int begin_src = -1, begin_row = 0, begin_col = 0;
    int src = -1, row = 0, col = 0;

    INSERT_INTO(ast, NUMBER, PARENT_NUMBER, KIND, PTR, BEGIN_SRC, BEGIN_ROW,
                BEGIN_COL, END_SRC, END_ROW, END_COL, SRC, ROW, COL, CLASS,
                NAME, QUALIFIED_TYPE, DESUGARED_TYPE, SPECS, REF_PTR) {

      switch (decl->kind) {
      case DECL_KIND_V2:
        FILL_TEXT(NAME, decl->variants.v2.name);
        break;
      case DECL_KIND_V3:
        FILL_INT(CLASS, numeric_class(decl->variants.v3.class));
        FILL_TEXT(NAME, decl->variants.v3.name);
        break;
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

      switch (node->kind) {
      case NODE_KIND_HEAD:
        assert(node->level < MAX_AST_LEVEL);
        parent_number = parents[node->level];
        if (node->level + 1 < MAX_AST_LEVEL)
          parents[node->level + 1] = i;

        begin_src = src_number(node->range.begin.file);
        begin_row = node->range.begin.line;
        begin_col = node->range.begin.col;

        src = src_number(node->loc.file);
        row = node->loc.line;
        col = node->loc.col;

#ifdef USE_TREE_SITTER

        kind = name_kind(node->name);
        if (kind & NAME_KIND_DECL) {
          struct decl_number_pair entry = {node->pointer};
          if (kind & NAME_KIND_VALUABLE_MASK)
            entry.number.var = i;
          else
            entry.number.type = i;

          added = decl_number_map_badd(&decl_number_map, &entry, NULL);
          assert(added);
          decl_ptr = node->pointer;
        } else {
          decl_ptr = ref_ptr;
        }

        if (decl_ptr) {
          if (begin_src != -1 && begin_row && begin_col)
            positional_node_set_push(
                &positional_node_set,
                (struct positional_node){
                    .src = begin_src,
                    .pos = (struct position){begin_row, begin_col},
                    .i = i,
                    .decl = decl_ptr,
                });

          if (src != -1 && row && col)
            positional_node_set_push(&positional_node_set,
                                     (struct positional_node){
                                         .src = src,
                                         .pos = (struct position){row, col},
                                         .i = i,
                                         .decl = decl_ptr,
                                     });
        }

#endif // USE_TREE_SITTER

        FILL_INT(NUMBER, i);
        FILL_INT(PARENT_NUMBER, parent_number);
        FILL_TEXT(KIND, node->name);
        FILL_TEXT(PTR, node->pointer);
        FILL_INT(BEGIN_SRC, begin_src);
        FILL_INT(BEGIN_ROW, begin_row);
        FILL_INT(BEGIN_COL, begin_col);
        FILL_INT(END_SRC, src_number(node->range.end.file));
        FILL_INT(END_ROW, node->range.end.line);
        FILL_INT(END_COL, node->range.end.col);
        FILL_INT(SRC, src);
        FILL_INT(ROW, row);
        FILL_INT(COL, col);
        break;
      case NODE_KIND_ENUM:
        break;
      case NODE_KIND_NULL:
        break;
      case NODE_KIND_TOKEN:
        break;
      }
    }
    END_INSERT_INTO();
  }

#ifdef USE_TREE_SITTER
  for (unsigned i = 0; i < decl_number_map.i; ++i) {
    struct decl_number_pair *entry = &decl_number_map.data[i];
    if (entry->number.var) {
      const char *type_ptr = find_var_type_map(entry->decl);
      if (type_ptr) {
        found = decl_number_map_bsearch(&decl_number_map, &type_ptr, &j);
        assert(found);
        entry->number.type = decl_number_map.data[j].number.type;
        assert(entry->number.type);
      }
    }
  }

  qsort(positional_node_set.data, positional_node_set.i,
        sizeof(struct positional_node), compare_positional_node);

  for (unsigned i = 0; i < positional_node_set.i; ++i) {
    struct positional_node pos_node = positional_node_set.data[i];
    struct cst_node *cst_node = find_cst(cst_set, pos_node.src, pos_node.pos);

    found = decl_number_map_bsearch(&decl_number_map, &pos_node.decl, &j);
    assert(found);

    set_cst_decl(cst_node, decl_number_map.data[j].number);

    // Revoke the last setting
    if (last_cst_node && cst_node &&
        last_cst_node->symbol != cst_node->symbol &&
        last_cst_node->decl == cst_node->decl)
      set_cst_decl(last_cst_node, (struct decl_number_value){});

    last_cst_node = cst_node;

    TOGGLE(print_ast_cst_match, {
      struct node *node = &ast->data[pos_node.i];
      fprintf(stderr, "%-16s %s | %*s%s:%d:%d -> ", node->name, node->pointer,
              node->level * 2, "", node->range.begin.file, pos_node.pos.row,
              pos_node.pos.col);
      if (!cst_node)
        fprintf(stderr, "NULL\n");
      else
        fprintf(stderr, "%s <%d>[%d:%d - %d:%d]\n",
                (cst_node->decl ? ast->data[cst_node->decl].pointer : "NULL"),
                cst_node->symbol, cst_node->begin.row, cst_node->begin.col,
                cst_node->end.row, cst_node->end.col);
    });
  }

  decl_number_map_clear(&decl_number_map, 2);
  positional_node_set_clear(&positional_node_set, 2);
#endif // USE_TREE_SITTER
}

static const char *expand_path(const char *cwd, size_t n, const char *in,
                               char *out) {
  assert(n < PATH_MAX);
  if (!in || *in == '/')
    return in;
  while (n > 0 && cwd[n - 1] == '/')
    --n;

  size_t i = 0;
  while (in[i] && in[i] != '/')
    ++i;
  if (!i)
    return out;

  if (in[0] == '.' && i <= 2) {
    if (in[1] == '.') {
      while (n > 0 && cwd[n - 1] != '/')
        --n;
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
  ASSERT(strcmp(expand_path("/tmp/xx/yy", 10, "./../a", path), "/tmp/xx/a") ==
         0);
})

#ifdef USE_TREE_SITTER
static void dump_src(struct cst_set *cst_set)
#else
static void dump_src()
#endif
{
  exec_sql("CREATE TABLE src ("
           " filename TEXT PRIMARY KEY,"
           " number INTEGER)");

#ifdef USE_TREE_SITTER
  TSLanguage *tree_sitter_c();
  TSLanguage *lang = tree_sitter_c();
  TSParser *parser = ts_parser_new();
  ts_parser_set_language(parser, lang);
  cst_set_reserve(cst_set, src_set.i);
#endif

  char buffer[PATH_MAX];
  size_t cwd_len = strlen(cwd);
  for (unsigned i = 0; i < src_set.i; ++i) {
    const char *filename = src_set.data[i].filename;
    if (is_internal_file(filename)) {
      continue;
    }

    const int number = src_set.data[i].number;
    const char *fullpath = expand_path(cwd, cwd_len, filename, buffer);
    if_prepared_stmt("INSERT INTO src (filename, number)"
                     " VALUES (?, ?)") {
      BIND_TEXT(1, fullpath,
                fullpath == buffer ? SQLITE_TRANSIENT : SQLITE_STATIC);
      FILL_INT(2, number);
    }
    end_if_prepared_stmt();

#ifdef USE_TREE_SITTER
    extern const char *tu_code;
    extern size_t tu_size;

    const char *code = strcmp(filename, tu) == 0 ? tu_code : NULL;
    size_t size = code ? tu_size : 0;
    cst_set_set(cst_set, number,
                (struct cst_wrapper){create_cst(fullpath, code, size, parser)});
#endif // USE_TREE_SITTER
  }

#ifdef USE_TREE_SITTER
  ts_parser_delete(parser);
#endif // USE_TREE_SITTER
}
