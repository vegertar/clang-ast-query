#include "sql.h"
#include "parse.h"
#include "util.h"

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

enum specs {
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
  SPEC_CINIT = 1U << 10,
};

enum semantics {
  SEMANTIC_EXPANSION,
  SEMANTIC_INACTIVE,
  SEMANTIC_LITERAL,
  SEMANTIC_IDENTIFIER,
  SEMANTIC_TYPE,
  SEMANTIC_KEYWORD,
  SEMANTIC_COMMENT,
  SEMANTIC_PUNCTUATOR,
};

static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int err;

static int dump_src();
static int dump_ast();
static int dump_loc();
static int dump_tok();
static int dump_sym();
static int dump_dot();

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
    } else if (strcmp(spec, "cinit") == 0) {
      specs |= SPEC_CINIT;
    }
  }
  assert(specs <= INT_MAX);
  return specs;
}

static int examine_semantics(const char *s) {
#define EXAMINE_SEMANTICS(s, x)                                                \
  do {                                                                         \
    if (strncmp(s, #x, sizeof(#x) - 1) == 0)                                   \
      return SEMANTIC_##x;                                                     \
  } while (0)

  EXAMINE_SEMANTICS(s, KEYWORD);
  EXAMINE_SEMANTICS(s, PUNCTUATOR);
  return -1;
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

static int compare_exp_expr_pair(const void *a, const void *b) {
  return strcmp(((const struct exp_expr_pair *)a)->expr,
                ((const struct exp_expr_pair *)b)->expr);
}

static void sort_exp_expr_set() {
  qsort(exp_expr_set.data, exp_expr_set.i, sizeof(struct exp_expr_pair),
        compare_exp_expr_pair);
}

static const struct exp_expr_pair *find_exp_expr_set(const char *expr) {
  struct exp_expr_pair key = {.expr = expr};
  return (const struct exp_expr_pair *)bsearch(&key, exp_expr_set.data,
                                               exp_expr_set.i, sizeof(key),
                                               compare_exp_expr_pair);
}

int sql(const char *db_file) {
  hierarchies_reserve(&hierarchies, ast.i);

  OPEN_DB(db_file);
  EXEC_SQL("PRAGMA synchronous = OFF");
  EXEC_SQL("PRAGMA journal_mode = MEMORY");
  EXEC_SQL("BEGIN TRANSACTION");

  sort_exp_expr_set();

  err += dump_src();
  err += dump_ast();
  err += dump_loc();
  err += dump_tok();
  err += dump_sym();
  err += dump_dot();

  EXEC_SQL("END TRANSACTION");
  CLOSE_DB();

  hierarchies_clear(&hierarchies, 0);
  decl_number_map_clear(&decl_number_map, 0);

  return err;
}

void sql_halt() {
  hierarchies_clear(&hierarchies, 2);
  decl_number_map_clear(&decl_number_map, 2);
}

#define FILL_REF(expr)                                                         \
  do {                                                                         \
    const struct ref *ref = expr;                                              \
    if (ref->pointer) {                                                        \
      FILL_TEXT(REF_KIND, ref->name);                                          \
      FILL_TEXT(REF_PTR, ref->pointer);                                        \
      FILL_TEXT(NAME, ref->sqname);                                            \
    }                                                                          \
  } while (0)

// mem shares the fields with ref
#define FILL_MEM(expr)                                                         \
  do {                                                                         \
    const struct mem *mem = expr;                                              \
    if (mem->pointer) {                                                        \
      assert(!def->ref.pointer);                                               \
      assert(mem->kind);                                                       \
      FILL_TEXT(REF_KIND, mem->kind == MEM_KIND_ARROW ? "arrow" : "dot");      \
      FILL_TEXT(REF_PTR, mem->pointer);                                        \
      FILL_TEXT(NAME, mem->name);                                              \
    }                                                                          \
  } while (0)

#define FILL_DEF(expr)                                                         \
  do {                                                                         \
    const struct def *def = expr;                                              \
    FILL_TEXT(QUALIFIED_TYPE, def->type.qualified);                            \
    FILL_TEXT(DESUGARED_TYPE, def->type.desugared);                            \
    FILL_REF(&def->ref);                                                       \
    FILL_MEM(&def->mem);                                                       \
    specs |= mark_specs(def->specs);                                           \
  } while (0)

static int dump_ast() {
  EXEC_SQL("CREATE TABLE ast ("
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
           " exp_src INTEGER,"
           " exp_row INTEGER,"
           " exp_col INTEGER,"
           " src INTEGER,"
           " row INTEGER,"
           " col INTEGER,"
           " name TEXT,"
           " class INTEGER,"
           " qualified_type TEXT,"
           " desugared_type TEXT,"
           " specs INTEGER,"
           " ref_kind TEXT,"
           " ref_ptr TEXT,"
           " def_ptr TEXT,"
           " type_ptr TEXT,"
           " ancestors TEXT)");

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
    const struct exp_expr_pair *exp_expr = NULL;
    const char *type_ptr = NULL;
    const char *def_ptr = NULL;
    unsigned specs = 0;
    unsigned j = 0;
    char ancestors[BUFSIZ] = {'['};
    unsigned length = 1;
    for (int parent_number = hierarchies.data[i].parent_number;
         parent_number != -1 && j < MAX_AST_LEVEL;
         parent_number = hierarchies.data[parent_number].parent_number) {
      const char *ancestor = ast.data[parent_number].name;
      const unsigned len = strlen(ancestor);
      assert(length + len + 3 < sizeof(ancestors));
      ancestors[length] = '"';
      memcpy(ancestors + length + 1, ancestor, len);
      ancestors[length + 1 + len] = '"';
      ancestors[length + 1 + len + 1] = ',';
      length += len + 3;
    }
    if (length > 1) {
      --length;
      assert(ancestors[length] == ',');
    }
    ancestors[length] = ']';

#ifndef VALUES29
#define VALUES29()                                                             \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?,?,"                                                                     \
  "?,?"
#endif // !VALUES29

    INSERT_INTO(ast, NUMBER, PARENT_NUMBER, FINAL_NUMBER, KIND, PTR, PREV,
                MACRO, BEGIN_SRC, BEGIN_ROW, BEGIN_COL, END_SRC, END_ROW,
                END_COL, EXP_SRC, EXP_ROW, EXP_COL, SRC, ROW, COL, CLASS, NAME,
                QUALIFIED_TYPE, DESUGARED_TYPE, SPECS, REF_KIND, REF_PTR,
                DEF_PTR, TYPE_PTR, ANCESTORS);

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

      if ((exp_expr = find_exp_expr_set(node->pointer))) {
        FILL_INT(EXP_SRC, src_number(exp_expr->exp.file));
        FILL_INT(EXP_ROW, exp_expr->exp.line);
        FILL_INT(EXP_COL, exp_expr->exp.col);
      }

      if ((def_ptr = find_string_map(&decl_def_map, node->pointer)))
        FILL_TEXT(DEF_PTR, def_ptr);

      if ((type_ptr = find_string_map(&var_type_map, node->pointer)))
        FILL_TEXT(TYPE_PTR, type_ptr);
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
    FILL_TEXT(ANCESTORS, ancestors);

    END_INSERT_INTO();
  }

  return 0;
}

static int dump_src() {
  EXEC_SQL("CREATE TABLE src ("
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

  return 0;
}

static int dump_loc() {
  EXEC_SQL("CREATE TABLE loc ("
           " begin_src INTEGER,"
           " begin_row INTEGER,"
           " begin_col INTEGER,"
           " end_src INTEGER,"
           " end_row INTEGER,"
           " end_col INTEGER,"
           " semantics INTEGER,"
           " kind TEXT)");

  struct {
    struct srange *data;
    unsigned i;
    enum semantics semantics;
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
                  END_COL, SEMANTICS);
      FILL_INT(BEGIN_SRC, begin_src);
      FILL_INT(BEGIN_ROW, srange->begin.line);
      FILL_INT(BEGIN_COL, srange->begin.col);
      FILL_INT(END_SRC, end_src);
      FILL_INT(END_ROW, srange->end.line);
      FILL_INT(END_COL, srange->end.col);
      FILL_INT(SEMANTICS, inputs[k].semantics);
      END_INSERT_INTO();
    }
  }

  for (unsigned i = 0; i < tok_kind_set.i; ++i) {
    struct srange tok = tok_kind_set.data[i].tok;
    int begin_src = src_number(tok.begin.file);
    int end_src = src_number(tok.end.file);

    const char *kind = tok_kind_set.data[i].kind;
    assert(strchr(kind, ' '));
    int semantics = examine_semantics(kind);

#ifndef VALUES8
#define VALUES8() "?,?,?,?,?,?,?,?"
#endif // !VALUES8

    INSERT_INTO(loc, BEGIN_SRC, BEGIN_ROW, BEGIN_COL, END_SRC, END_ROW, END_COL,
                SEMANTICS, KIND);
    FILL_INT(BEGIN_SRC, begin_src);
    FILL_INT(BEGIN_ROW, tok.begin.line);
    FILL_INT(BEGIN_COL, tok.begin.col);
    FILL_INT(END_SRC, end_src);
    FILL_INT(END_ROW, tok.end.line);
    FILL_INT(END_COL, tok.end.col);
    FILL_INT(SEMANTICS, semantics);
    FILL_TEXT(KIND, kind);
    END_INSERT_INTO();
  }

  return 0;
}

static int dump_tok() {
  EXEC_SQL("CREATE TABLE tok ("
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

    INSERT_INTO(tok, SRC, BEGIN_ROW, BEGIN_COL, OFFSET, DECL);
    FILL_INT(SRC, src);
    FILL_INT(BEGIN_ROW, tok->loc.line);
    FILL_INT(BEGIN_COL, tok->loc.col);
    FILL_INT(OFFSET, tok->offset);
    FILL_INT(DECL, decl);
    END_INSERT_INTO();
  }

  return 0;
}

static int dump_sym() {
  EXEC_SQL("CREATE TABLE sym ("
           " name TEXT,"
           " decl INTEGER)");

  for (unsigned i = 0; i < exported_symbols.i; ++i) {
    int decl = decl_number((const char *)exported_symbols.data[i]);
    assert(decl != -1);
    assert(strcmp(ast.data[decl].pointer,
                  (const char *)exported_symbols.data[i]) == 0);
    const char *name = NULL;
    const struct decl *v = &ast.data[decl].decl;
    switch (v->kind) {
    case DECL_KIND_V2:
      name = v->variants.v2.name;
      break;
    case DECL_KIND_V3:
      name = v->variants.v3.name;
      break;
    case DECL_KIND_V9:
      name = v->variants.v9.name;
      break;
    default:
      break;
    }
    assert(name);

    if_prepared_stmt("INSERT INTO sym (name, decl)"
                     " VALUES (?, ?)") {
      FILL_TEXT(1, name);
      FILL_INT(2, decl);
    }
    end_if_prepared_stmt();
  }

  return 0;
}

static int dump_dot() {
  EXEC_SQL("CREATE TABLE dot ("
           " ts INTEGER,"
           " tu TEXT,"
           " cwd TEXT)");

#ifndef VALUES3
#define VALUES3() "?,?,?"
#endif // !VALUES3

  INSERT_INTO(dot, TS, TU, CWD);
  FILL_INT(TS, ts);
  FILL_TEXT(TU, tu);
  FILL_TEXT(CWD, cwd);
  END_INSERT_INTO();

  return 0;
}