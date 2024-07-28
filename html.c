#include "html.h"
#include "sql.h"
#include "util.h"

#include <sqlite3.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static char tmp[PATH_MAX];
static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;
static int err;

struct source {
  unsigned __int128 key;
  struct string content;
};

static int compare_source(const void *a, const void *b, size_t n) {
  struct source *x = (struct source *)a;
  struct source *y = (struct source *)b;
  return x->key - y->key;
}

static void free_source(void *p) {
  string_clear(&((struct source *)p)->content, 1);
}

DECL_ARRAY(source_map, struct source);
static inline IMPL_ARRAY_PUSH(source_map, struct source);
static inline IMPL_ARRAY_BSEARCH(source_map, compare_source);
static inline IMPL_ARRAY_BADD(source_map, NULL);
static inline IMPL_ARRAY_CLEAR(source_map, free_source);

static struct source_map source_map;

static int add_source(const char *path) {
  struct source source = {hash(path, strlen(path))};
  unsigned i = -1;
  source_map_badd(&source_map, &source, &i);
  assert(i != -1 && i < INT_MAX);

  if (!source_map.data[i].content.i) {
    FILE *fp = open_file(path, "r");
    if (!fp || reads(fp, &source_map.data[i].content, "$`"))
      i = -1;
    if (fp)
      fclose(fp);
  }

  return i;
}

int html(const char *db_file) {
  char title[PATH_MAX] = {0};
  const char *code = NULL;

  OPEN_DB(db_file);
  QUERY("SELECT cwd, tu FROM dot");
  QUERY_END({
    char path[PATH_MAX];
    const char *tu = COL_TEXT(1);
    strncpy(title, tu, COL_SIZE(1));
    int i = add_source(expand_path(COL_TEXT(0), COL_SIZE(0), tu, path));
    if (i != -1)
      code = source_map.data[i].content.data;
  });
  CLOSE_DB();

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
    import {EditorView} from 'https://cdn.jsdelivr.net/npm/@codemirror/view@6.29.0/+esm';\
\
    new EditorView({\
      doc: String.raw`%s`,\
      extensions: [],\
      parent: document.body\
    });\
  </script>\
  </head>\
  <body></body>\
</html>",
            title, code);
    fclose(fp);
  }
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