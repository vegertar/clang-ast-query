#include "parse.h"
#include "test.h"
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
  SPEC_HAS_LEADING_SPACE = 1U << 5,
  SPEC_STRINGIFIED = 1U << 6,
  SPEC_PASTE = 1U << 7,
  SPEC_ARG = 1U << 8,
  SPEC_FAST = 1U << 9,
};

enum semantic {
  SEMANTIC_EXPANSION,
  SEMANTIC_INACTIVE,
};

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int err;

static void dump_src();
static void dump_ast();
static void dump_loc();
static void dump_tok();

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

static unsigned mark_specs(const struct array array) {
  unsigned specs = 0;
  for (unsigned i = 0; i < array.i; ++i) {
    const char *spec = (const char *)(array.data[i]);
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
    } else if (strcmp(spec, "hasLeadingSpace") == 0) {
      specs |= SPEC_HAS_LEADING_SPACE;
    } else if (strcmp(spec, "stringified") == 0) {
      specs |= SPEC_STRINGIFIED;
    } else if (strcmp(spec, "paste") == 0) {
      specs |= SPEC_PASTE;
    } else if (strcmp(spec, "arg") == 0) {
      specs |= SPEC_ARG;
    } else if (strcmp(spec, "fast") == 0) {
      specs |= SPEC_FAST;
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

static inline _Bool is_decl(const struct node *node) {
  size_t n = strlen(node->name);
  return n > 4 && strcmp(node->name + n - 4, "Decl") == 0;
}

/// Represents a pair consisting of a pointer to a declaration node in
/// an AST and its corresponding index number.
struct decl_number_pair {
  const char *decl;
  unsigned number;
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

static struct decl_number_map decl_number_map;

static int decl_number(const char *decl) {
  if (!decl)
    return -1;

  unsigned i;
  _Bool found = decl_number_map_bsearch(&decl_number_map, &decl, &i);
  assert(found);
  unsigned number = decl_number_map.data[i].number;
  assert(number < INT_MAX);
  return number;
}

struct hierarchy {
  int parent_number;
  unsigned final_number; // the furthest descendants
};

DECL_ARRAY(hierarchies, struct hierarchy);
static inline IMPL_ARRAY_RESERVE(hierarchies, struct hierarchy);
static inline IMPL_ARRAY_CLEAR(hierarchies, NULL);

static struct hierarchies hierarchies;

int dump(const char *db_file) {
  _Bool is_stdout = strcmp(db_file, "/dev/stdout") == 0;

  sqlite3_open(is_stdout ? ":memory:" : db_file, &db);
  hierarchies_reserve(&hierarchies, ast.i);

  exec_sql("PRAGMA synchronous = OFF");
  exec_sql("PRAGMA journal_mode = MEMORY");
  exec_sql("BEGIN TRANSACTION");

  dump_src();
  dump_ast();
  dump_loc();
  dump_tok();

  exec_sql("END TRANSACTION");

  for (int i = 0; i < MAX_STMT_SIZE; ++i) {
    if (stmts[i])
      sqlite3_finalize(stmts[i]);
  }

  if (err && !errmsg)
    fprintf(stderr, "sqlite3 error(%d): %s\n", err, sqlite3_errstr(err));

  decl_number_map_clear(&decl_number_map, 2);
  hierarchies_clear(&hierarchies, 2);
  sqlite3_close(db);
  return err;
}

#define FILL_REF(expr)                                                         \
  do {                                                                         \
    const struct ref *ref = expr;                                              \
    if (ref->pointer) {                                                        \
      FILL_TEXT(REF_PTR, ref->pointer);                                        \
      FILL_TEXT(NAME, ref->sqname);                                            \
    }                                                                          \
  } while (0)

#define FILL_DEF(expr)                                                         \
  do {                                                                         \
    const struct def *def = expr;                                              \
    FILL_TEXT(QUALIFIED_TYPE, def->type.qualified);                            \
    FILL_TEXT(DESUGARED_TYPE, def->type.desugared);                            \
    FILL_REF(&def->ref);                                                       \
    specs |= mark_specs(def->specs);                                           \
  } while (0)

static void dump_ast() {
  exec_sql("CREATE TABLE ast ("
           " number INTEGER PRIMARY KEY,"
           " parent_number INTEGER,"
           " final_number INTEGER,"
           " kind TEXT,"
           " ptr TEXT,"
           " prev TEXT,"
           " macro TEXT,"
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

  unsigned parents[MAX_AST_LEVEL + 1];
  for (unsigned i = 0; i < sizeof(parents) / sizeof(*parents); ++i)
    parents[i] = -1;

  for (unsigned i = 0; i < ast.i; ++i) {
    const struct node *node = &ast.data[i];
    assert(node->level < MAX_AST_LEVEL);
    int parent_number = parents[node->level];
    if (node->level + 1 < MAX_AST_LEVEL)
      parents[node->level + 1] = i;

    hierarchies.data[i].parent_number = parent_number;
    hierarchies.data[i].final_number = i;
    while (parent_number != -1) {
      hierarchies.data[parent_number].final_number = i;
      parent_number = hierarchies.data[parent_number].parent_number;
    }
  }

  for (unsigned i = 0; i < ast.i; ++i) {
    const struct node *node = &ast.data[i];
    const struct decl *decl = &node->decl;
    int parent_number = -1;
    unsigned specs = 0;

#ifndef VALUES22
#define VALUES22()                                                             \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?"
#endif // !VALUES21

    INSERT_INTO(ast, NUMBER, PARENT_NUMBER, FINAL_NUMBER, KIND, PTR, PREV,
                MACRO, BEGIN_SRC, BEGIN_ROW, BEGIN_COL, END_SRC, END_ROW,
                END_COL, SRC, ROW, COL, CLASS, NAME, QUALIFIED_TYPE,
                DESUGARED_TYPE, SPECS, REF_PTR) {

      switch (decl->kind) {
      case DECL_KIND_V2:
        FILL_TEXT(NAME, decl->variants.v2.name);
        break;
      case DECL_KIND_V3:
        FILL_INT(CLASS, numeric_class(decl->variants.v3.class));
        FILL_TEXT(NAME, decl->variants.v3.name);
        break;
      case DECL_KIND_V7:
        FILL_TEXT(NAME, decl->variants.v7.sqname);
        FILL_DEF(&decl->variants.v7.def);
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
        if (is_decl(node)) {
          struct decl_number_pair entry = {node->pointer, i};
          _Bool added = decl_number_map_badd(&decl_number_map, &entry, NULL);
          assert(added);
        }

        FILL_TEXT(KIND, node->name);
        FILL_TEXT(PTR, node->pointer);
        FILL_TEXT(PREV, node->prev);
        break;
      case NODE_KIND_ENUM:
        break;
      case NODE_KIND_NULL:
        break;
      case NODE_KIND_TOKEN:
        FILL_TEXT(KIND, "Token");
        FILL_TEXT(NAME, node->name);
        FILL_TEXT(MACRO, node->macro);
        break;
      }

      specs |= mark_specs(node->attrs);

      FILL_INT(NUMBER, i);
      FILL_INT(PARENT_NUMBER, hierarchies.data[i].parent_number);
      FILL_INT(FINAL_NUMBER, hierarchies.data[i].final_number);
      FILL_INT(BEGIN_SRC, src_number(node->range.begin.file));
      FILL_INT(BEGIN_ROW, node->range.begin.line);
      FILL_INT(BEGIN_COL, node->range.begin.col);
      FILL_INT(END_SRC, src_number(node->range.end.file));
      FILL_INT(END_ROW, node->range.end.line);
      FILL_INT(END_COL, node->range.end.col);
      FILL_INT(SRC, src_number(node->loc.file));
      FILL_INT(ROW, node->loc.line);
      FILL_INT(COL, node->loc.col);
      FILL_INT(SPECS, specs);
    }
    END_INSERT_INTO();
  }
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

static void dump_src() {
  exec_sql("CREATE TABLE src ("
           " filename TEXT PRIMARY KEY,"
           " number INTEGER)");

  char buffer[PATH_MAX];
  size_t cwd_len = strlen(cwd);
  for (unsigned i = 0; i < src_set.i; ++i) {
    const char *filename = src_set.data[i].filename;
    const int number = src_set.data[i].number;
    const char *fullpath = is_internal_file(filename)
                               ? filename
                               : expand_path(cwd, cwd_len, filename, buffer);
    if_prepared_stmt("INSERT INTO src (filename, number)"
                     " VALUES (?, ?)") {
      BIND_TEXT(1, fullpath,
                fullpath == buffer ? SQLITE_TRANSIENT : SQLITE_STATIC);
      FILL_INT(2, number);
    }
    end_if_prepared_stmt();
  }
}

static void dump_loc() {
  exec_sql("CREATE TABLE loc ("
           " begin_src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_src INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER,"
           " semantics INTEGER)");

  struct {
    struct srange *data;
    unsigned i;
    enum semantic semantic;
  } inputs[] = {
      {
          loc_exp_set.data,
          loc_exp_set.i,
          SEMANTIC_EXPANSION,
      },
      {
          inactive_set.data,
          inactive_set.i,
          SEMANTIC_INACTIVE,
      },
  };

  for (unsigned k = 0; k < sizeof(inputs) / sizeof(*inputs); ++k) {
    for (unsigned i = 0; i < inputs[k].i; ++i) {
      struct srange *srange = &inputs[k].data[i];
      int begin_src = src_number(srange->begin.file);
      int end_src = src_number(srange->end.file);

#ifndef VALUES7
#define VALUES7() "?,?,?,?,?,?,?"
#endif // !VALUES7

      INSERT_INTO(loc, BEGIN_SRC, BEGIN_ROW, BEGIN_COL, END_SRC, END_ROW,
                  END_COL, SEMANTICS) {
        FILL_INT(BEGIN_SRC, begin_src);
        FILL_INT(BEGIN_ROW, srange->begin.line);
        FILL_INT(BEGIN_COL, srange->begin.col);
        FILL_INT(END_SRC, end_src);
        FILL_INT(END_ROW, srange->end.line);
        FILL_INT(END_COL, srange->end.col);
        FILL_INT(SEMANTICS, inputs[k].semantic);
      }
      END_INSERT_INTO();
    }
  }
}

static void dump_tok() {
  exec_sql("CREATE TABLE tok ("
           " src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " offset INTEGER,"
           " decl INTEGER)");

  for (unsigned i = 0; i < tok_decl_set.i; ++i) {
    struct tok *tok = &tok_decl_set.data[i].tok;
    int src = src_number(tok->loc.file);
    int decl = decl_number(tok_decl_set.data[i].decl);

#ifndef VALUES5
#define VALUES5() "?,?,?,?,?"
#endif // !VALUES5

    INSERT_INTO(tok, SRC, BEGIN_ROW, BEGIN_COL, OFFSET, DECL) {
      FILL_INT(SRC, src);
      FILL_INT(BEGIN_ROW, tok->loc.line);
      FILL_INT(BEGIN_COL, tok->loc.col);
      FILL_INT(OFFSET, tok->offset);
      FILL_INT(DECL, decl);
    }
    END_INSERT_INTO();
  }
}