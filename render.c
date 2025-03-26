#include "render.h"
#include "parse.h"
#include "store.h"
#include "util.h"

#ifndef READER_JS
static const char reader_js[] = {
#embed "reader.js"
};
#define reader_js_len sizeof(reader_js)
#else
#define reader_js READER_JS
#define reader_js_len (sizeof(READER_JS) - 1)
#endif // !READER_JS

#define DUMP(fp, ...)                                                          \
  do {                                                                         \
    if (!err.es && (ferror(fp) || fprintf(fp, __VA_ARGS__) < 0))               \
      return (struct error){ES_RENDER};                                        \
  } while (0)

#define EVAL(x)                                                                \
  do {                                                                         \
    if (!err.es)                                                               \
      err = x;                                                                 \
  } while (0)

// The translation unit specific state.
struct {
  char cwd[PATH_MAX];
  char tu[PATH_MAX];
} state;

typedef struct {
  String file;
  struct string content;
} Source;

typedef struct {
  struct error err;
} StringsRowContext;

typedef struct {
  struct error err;
  FILE *out;
} SemanticsRowContext;

typedef DECL_ARRAY(SourceList, Source) SourceList;

SourceList all_sources;

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

static inline struct error add_source(struct string file, uint8_t property,
                                      HASH_size_t hash) {
  struct error err = {};
  String s = {hash, property, file};

  ARRAY_size_t i;
  if (SourceList_badd(&all_sources, &s, &i)) {
    FILE *fp;
    err = open_file(string_get(&all_sources.data[i].file.elem), "r", &fp);
    if (!err.es) {
      // We're going to dump the src content into JS, so read with escapes
      err = reads(fp, &all_sources.data[i].content, "'\\");
      err = next_error(err, close_file(fp));
    }
  }

  return err;
}

struct error render_init() {
  return (struct error){};
}

struct error render_halt() {
  SourceList_clear(&all_sources, 1);
  return (struct error){};
}

static inline bool dot_row(const char *cwd, int cwd_len, const char *tu,
                           int tu_len, void *obj) {
  assert(cwd_len < sizeof(state.cwd));
  strcpy(state.cwd, cwd);

  assert(tu_len < sizeof(state.tu));
  strcpy(state.tu, tu);

  return true;
}

static struct error load_state() { return query_dot(dot_row, NULL); }

static inline bool strings_row(const char *key, int key_len, uint8_t property,
                               uint32_t hash, void *obj) {
  StringsRowContext *ctx = obj;
  assert(ctx);

  // Add non-builtin files only
  if (!(property & SP_BUILTIN))
    ctx->err = add_source(string_from(key, key_len), property, hash);

  return ctx->err.es;
}

static struct error load_sources() {
  StringsRowContext ctx = {};
  struct error err = query_strings(SP_FILE, strings_row, &ctx);
  return next_error(err, ctx.err);
}

static struct error render_sources(FILE *fp) {
  struct error err = {};
  for (unsigned i = 0; i < all_sources.i; ++i) {
    DUMP(fp, R"code(
    <script data-id='%u' data-type='source' %s>
      document.currentScript.data = [
)code",
         all_sources.data[i].file.hash,
         all_sources.data[i].file.property & SP_TU ? "data-main" : "");

    const char *code = string_get(&all_sources.data[i].content);
    unsigned n = string_len(&all_sources.data[i].content);

    // Instead of using String.raw, which requires escaping variable
    // substitutions, we pass the code line by line.
    for (unsigned j = 0, k = 0; j < n; ++j) {
      if (code[j] == '\n' || j == n - 1) {
        DUMP(fp, "'%.*s',\n", j - k, code + k);
        k = j + 1;
      }
    }

    DUMP(fp, R"code(];
    </script>)code");
  }
  return err;
}

bool semantics_row(unsigned begin_row, unsigned begin_col, unsigned end_row,
                   unsigned end_col, const char *kind, const char *name,
                   void *obj) {
  SemanticsRowContext *ctx = obj;
  assert(ctx && ctx->out);

  if (fprintf(ctx->out, "%u,%u,%u,%u,'%s','%s',\n", begin_row, begin_col,
              end_row, end_col, kind, name) < 0)
    ctx->err = (struct error){ES_RENDER};

  return ctx->err.es;
}

struct error render_semantics(FILE *fp) {
  struct error err = {};
  for (unsigned i = 0; i < all_sources.i; ++i) {
    unsigned src = all_sources.data[i].file.hash;
    DUMP(fp, R"code(
    <script data-id='%u' data-type='semantics'>
      document.currentScript.data = [
)code",
         src);

    SemanticsRowContext ctx = {.out = fp};
    EVAL(query_semantics(src, semantics_row, &ctx));
    EVAL(ctx.err);

    DUMP(fp, R"code(];
    </script>)code");
  }
  return err;
}

struct error render(FILE *fp) {
  memset(&state, 0, sizeof(state));

  struct error err = next_error(load_state(), load_sources());

  // We provide the reader as a module script which already implied 'defer'.
  DUMP(fp,
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
      import {ReaderView} from '%.*s';
)code"
#else
       R"code(%.*s)code"
#endif // READER_JS
       R"code(
      new ReaderView();
    </script>)code",
       state.tu, (int)reader_js_len, reader_js);

  // We provide data by classic scripts without 'defer' or 'async'.
  // For a big project involving hundreds of thousands of files, the generated
  // final HTML will be too huge to fetch and load, one can roughly split the
  // HTML into many small pieces of scripts before serving.
  EVAL(render_sources(fp));
  EVAL(render_semantics(fp));

  DUMP(fp, R"code(
  </head>
  <body>
    <div></div>
  </body>
</html>
)code");

  return err;
}