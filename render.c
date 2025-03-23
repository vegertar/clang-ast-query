#include "render.h"
#include "parse.h"
#include "store.h"
#include "util.h"

#ifndef READER_JS
#include "readerjs-inc.h"
#endif // !READER_JS

typedef struct {
  String file;
  struct string content;
} Source;

typedef DECL_ARRAY(SourceList, Source) SourceList;

SourceList all_sources;

struct {
  char cwd[PATH_MAX];
  char tu[PATH_MAX];
} current;

static inline int compare_source(const void *a, const void *b, size_t n) {
  return ((const String *)a)->hash - ((const String *)b)->hash;
}

static void destroy_source(void *p) {
  Source *src = p;
  string_clear(&src->file.elem, 1);
  string_clear(&src->content, 1);
}

static inline IMPL_ARRAY_BSEARCH(SourceList, compare_source);
static inline IMPL_ARRAY_BADD(SourceList, NULL);
static inline IMPL_ARRAY_CLEAR(SourceList, destroy_source);

static inline void add_source(struct string src, HASH_size_t hash) {
  String s = {hash, 0, src};
  SourceList_badd(&all_sources, &s, NULL);
  // TODO: open and read file
}

static inline bool dot_row(const char *cwd, int cwd_len, const char *tu,
                           int tu_len, void *obj) {
  assert(cwd_len < sizeof(current.cwd));
  strcpy(current.cwd, cwd);

  assert(tu_len < sizeof(current.tu));
  strcpy(current.tu, tu);

  return true;
}

static inline bool strings_row(const char *key, int key_len, uint8_t property,
                               uint32_t hash, void *obj) {
  // Add non-builtin files only
  if (!(property & SP_BUILTIN))
    add_source(string_from(key, key_len), hash);
  return false;
}

struct error render_init() {
  return (struct error){};
}

struct error render_halt() {
  for (unsigned i = 0; i < all_sources.i; ++i)
    fprintf(stderr, "%s\n", string_get(&all_sources.data[i].file.elem));
  SourceList_clear(&all_sources, 1);
  return (struct error){};
}

static struct error load_sources() { return (struct error){}; }

struct error render(FILE *fp) {
  memset(&current, 0, sizeof(current));

  struct error err = next_error(query_dot(dot_row, NULL),
                                query_strings(SP_FILE, strings_row, NULL));
  if (err.es)
    return err;

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
#define reader_js READER_JS
#define reader_js_len (sizeof(READER_JS) - 1)
          R"code(
      import {ReaderView} from '%.*s'
)code"
#else
          R"code(%.*s)code"
#endif // READER_JS
          R"code(
      window.ReaderView = ReaderView;
    </script>
    <script type='module'>)code",
          current.tu, reader_js_len, reader_js);

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