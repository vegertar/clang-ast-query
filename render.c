#include "render.h"
#include "store.h"
#include "util.h"

#ifndef READER_JS
#include "readerjs-inc.h"
#endif // !READER_JS

struct error render(FILE *fp) {
  char path[PATH_MAX];
  struct error err = query_tu(path, sizeof(path));

  if (err.es)
    return err;

  char *title = strrchr(path, '/');
  assert(title);
  ++title;

  fprintf(fp,
          R"code(
<!DOCTYPE html>
<html lang='en'>
  <head>
    <title>%s</title>
    <meta charset='utf-8'>
    <style></style>
    <script type='module'>
)code"
#ifdef READER_JS
          R"code(
      import {ReaderView} from '%s'
)code"
#else
#define READER_JS reader_js
          R"code(%s)code"
#endif // READER_JS
          R"code(
      window.ReaderView = ReaderView;
    </script>
    <script type='module'>)code",
          title, READER_JS);

  fprintf(fp, R"code(
      new window.ReaderView({
        doc: [
        ],
        data: [)code");

  // dump_link(fp, &source);
  // dump_decl(fp, &source);
  // dump_macro_decl(fp, &source);
  // dump_test_macro_decl(fp, &source);

  // Specifically, we place the semantic data at the end to make it easier to
  // detect input errors. At that moment, semantic highlighting would not work.
  // dump_semantics(fp, &source);

  fprintf(fp, R"code(
        ],
        parent: document.body
      });
    </script>
  </head>
  <body></body>
</html>)code");

  return err;
}