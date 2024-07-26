#include "html.h"
#include "util.h"

#include <stdio.h>
#include <assert.h>

int html(const char *db_file) { return 0; }

int rename_html(const char *db_file, const char *out_filename) {
  char from[PATH_MAX];
  char to[PATH_MAX];
  snprintf(from, sizeof(from), "%s.html", db_file);
  snprintf(to, sizeof(to), "%s.html", out_filename);
  int err = rename(from, to);
  assert(err == 0);
  return err;
}