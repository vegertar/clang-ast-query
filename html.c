#include "html.h"
#include "sql.h"
#include "util.h"

#include <sqlite3.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef READER_JS
#define READER_JS "./reader.js"
#endif // READER_JS

#define DUMP(x) fprintf(fp, "Symbol('" #x "'),")

static char tmp[PATH_MAX];
static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int err;

struct source {
  unsigned __int128 key;
  struct string path;
  struct string code;
  int src;
};

static int compare_source(const void *a, const void *b, size_t n) {
  struct source *x = (struct source *)a;
  struct source *y = (struct source *)b;
  return x->key - y->key;
}

static void free_source(void *p) {
  struct source *source = (struct source *)p;
  string_clear(&source->path, 1);
  string_clear(&source->code, 1);
}

DECL_ARRAY(source_map, struct source);
static inline IMPL_ARRAY_PUSH(source_map, struct source);
static inline IMPL_ARRAY_BSEARCH(source_map, compare_source);
static inline IMPL_ARRAY_BADD(source_map, NULL);
static inline IMPL_ARRAY_CLEAR(source_map, free_source);

static struct source_map source_map;

static int add_source(const char *path) {
  struct source source = {.key = hash(path, strlen(path)), .src = -1};
  unsigned i = -1;
  source_map_badd(&source_map, &source, &i);
  assert(i != -1 && i < INT_MAX);

  if (!source_map.data[i].code.i) {
    FILE *fp = open_file(path, "r");
    if (!fp || reads(fp, &source_map.data[i].code, "$`"))
      i = -1;
    if (fp)
      fclose(fp);
    if (i != -1)
      string_appends(&source_map.data[i].path, path, strlen(path));
  }

  return i;
}

static void dump_link(FILE *fp, struct source *source);
static void dump_decl(FILE *fp, struct source *source);
static void dump_macro_decl(FILE *fp, struct source *source);
static void dump_semantics(FILE *fp, struct source *source);

int html(const char *db_file) {
  char title[PATH_MAX] = {0};
  struct source source = {0};

  OPEN_DB(db_file);
  QUERY("SELECT cwd, tu FROM dot");
  QUERY_END({
    char path[PATH_MAX];
    const char *tu = COL_TEXT(1);
    strncpy(title, tu, COL_SIZE(1));
    int i = add_source(expand_path(COL_TEXT(0), COL_SIZE(0), tu, path));
    if (i != -1)
      source = source_map.data[i];
  });

  if (!err) {
    snprintf(tmp, sizeof(tmp), "%s.html", db_file);
    FILE *fp = open_file(tmp, "w");
    if (!fp)
      return errno;

    fprintf(fp, "\
<!DOCTYPE html>\
<html lang='en'>\
  <head>\
    <title>%s</title>\
    <meta charset='utf-8'>\
    <style></style>\
    <script type='module'>\
      import {ReaderView} from '%s';\
\
      new ReaderView({\
        doc: String.raw`%s`,\
        data: [",
            title, READER_JS, source.code.data);

    dump_link(fp, &source);
    dump_decl(fp, &source);
    dump_macro_decl(fp, &source);

    // Specifically, we place the semantic data at the end to make it easier to
    // detect input errors. At that moment, semantic highlighting would not
    // work.
    dump_semantics(fp, &source);

    fprintf(fp, "\
        ],\
        parent: document.body\
      });\
    </script>\
  </head>\
  <body></body>\
</html>");
    fclose(fp);
  }

  CLOSE_DB();
  return err;
}

void html_halt() { source_map_clear(&source_map, 1); }

int html_rename(const char *db_file, const char *out_filename) {
  assert(starts_with(tmp, out_filename));

  char out[PATH_MAX];
  snprintf(out, sizeof(out), "%s.html", out_filename);
  err = rename(tmp, out);
  assert(err == 0);
  return err;
}

static void require_src(struct source *source) {
  if (err || source->src != -1)
    return;

  assert(source->path.i);

  QUERY("SELECT number FROM src WHERE filename = ?");
  FILL_TEXT(1, source->path.data);
  QUERY_END({ source->src = COL_INT(0); });

  assert(source->src != -1);
}

static void dump_link(FILE *fp, struct source *source) {
  require_src(source);
  DUMP(link);

  QUERY("SELECT begin_row, begin_col, end_row, end_col, desugared_type uri"
        " FROM ast"
        " WHERE kind = 'InclusionDirective'"
        " AND src = ?");
  FILL_INT(1, source->src);
  QUERY_END({
    fprintf(fp, "%d,%d,%d,%d,'%s',", COL_INT(0), COL_INT(1), COL_INT(2),
            COL_INT(3), COL_TEXT(4));
  });
}

static void dump_decl(FILE *fp, struct source *source) {
  require_src(source);
  DUMP(decl);

  QUERY("SELECT tok.begin_row, tok.begin_col, name, kind, specs,"
        "  class elaborated_type, qualified_type, desugared_type"
        " FROM ast"
        " JOIN tok"
        " ON ast.number = tok.decl"
        " WHERE tok.src = ?"
        " AND offset IS NULL");
  FILL_INT(1, source->src);
  QUERY_END({
    fprintf(fp, "%d,%d,'%s','%s',%d,%d,'%s','%s',", COL_INT(0), /* begin_row */
            COL_INT(1),                                         /* begin_col */
            COL_TEXT(2),                                        /* name */
            COL_TEXT(3),                                        /* kind */
            COL_INT(4),                                         /* specs */
            COL_INT(5),           /* elaboratetd_type */
            ALT(COL_TEXT(6), ""), /* qualified_type */
            ALT(COL_TEXT(7), "")  /* desugared_type */
    );
  });
}

static void dump_macro_decl(FILE *fp, struct source *source) {
  require_src(source);
  DUMP(macro_decl);

  QUERY("SELECT tok.begin_row, tok.begin_col, name,"
        "  qualified_type parameters, desugared_type body"
        " FROM ast"
        " JOIN ("
        "   SELECT tok.begin_row, tok.begin_col, ref_ptr ptr"
        "   FROM ast"
        "   JOIN tok"
        "   ON ast.number = tok.decl"
        "   WHERE tok.src = ?"
        "   AND offset = 0"
        " ) tok"
        " USING (ptr)"
        " ORDER BY tok.begin_row, tok.begin_col");
  FILL_INT(1, source->src);
  QUERY_END({
    fprintf(fp, "%d,%d,'%s','%s','%s',", COL_INT(0), /* begin_row */
            COL_INT(1),                              /* begin_col */
            COL_TEXT(2),                             /* name */
            COL_TEXT(3),                             /* parameters */
            COL_TEXT(4)                              /* body */
    );
  });
}

static void dump_semantics(FILE *fp, struct source *source) {
  require_src(source);
  DUMP(semantics);

  QUERY("SELECT begin_row, begin_col, end_row, end_col, kind"
        " FROM loc"
        " WHERE begin_src = ?"
        " ORDER BY begin_row, begin_col");
  FILL_INT(1, source->src);
  QUERY_END({
    fprintf(fp, "%d,%d,%d,%d,'%s',", COL_INT(0), COL_INT(1), COL_INT(2),
            COL_INT(3), COL_TEXT(4));
  });
}