#include "build.h"
#include "parse.h"
#include "remark.h"
#include "render.h"
#include "store.h"
#include "util.h"

#include <time.h>

struct input_list input_list;
static struct string input_content;
static struct output output;

struct output_file {
  FILE *file;
  char tmp[PATH_MAX];
};

static inline bool needs_to_save_tmp(const char *filename) {
  assert(filename);
  return !starts_with(filename,
                      "/dev/")            // NOT /dev/stdout, /dev/fd/1, etc.
         && strcmp(filename, ":memory:"); // NOT sqlite3 in-memory database
}

static inline const char *get_tmp(struct output_file *of,
                                  const char *filename) {
  assert(of && !of->tmp[0]);
  if (needs_to_save_tmp(filename)) {
    char str[8];
    snprintf(of->tmp, sizeof(of->tmp), "%s.tmp-%s", output.file,
             rands(str, sizeof(str)));
    return of->tmp;
  }

  return NULL;
}

static struct error open_output(struct output_file *of) {
  assert(of);
  of->file = NULL;
  of->tmp[0] = 0;

  struct error err = {};
  if (output.file) {
    const char *tmp = get_tmp(of, output.file);
    const char *filename = ALT(tmp, output.file);

    switch (output.kind) {
    case OK_TEXT:
    case OK_HTML:
      err = open_file(filename, "w", &of->file);
      break;

    case OK_DATA:
      err = store_open(filename);
      break;

    default:
      err = (struct error){ES_UNKNOWN_OUTPUT};
      break;
    }
  }

  return err;
}

static struct error close_output(struct output_file *of) {
  assert(of);

  struct error err = {};
  if (output.file) {
    switch (output.kind) {
    case OK_TEXT:
    case OK_HTML:
      err = close_file(of->file);
      break;

    case OK_DATA:
      err = store_close();
      break;

    default:
      err = (struct error){ES_UNKNOWN_OUTPUT};
      break;
    }
  }

  return err;
}

static inline struct error cleanup_output(struct error err,
                                          struct output_file *of) {
  assert(of);

  if (of->tmp[0]) {
    if (!err.es && output.file)
      return rename_file(of->tmp, output.file);

    // Remove the file anyway if there was an error
    return next_error(err, unlink_file(of->tmp));
  }

  return err;
}

struct parse_context {
  YYLTYPE lloc;
  UserContext uctx;
  int errs;
};

#define PARSE_CONTEXT_INIT(type, data)                                         \
  {                                                                            \
    {1, 1, 1, 1}, { output.silent, type, NULL, data }                          \
  }

typedef struct error (*build_t)(struct input i);

static struct error parse_line_and_dump(char *line, size_t n, size_t cap,
                                        YYLTYPE *lloc, UserContext *uctx) {
  struct error err = {};

  if (!output.noparse) {
    if (lloc->last_column != 1) {
      // There was an error, we step the line manually
      lloc->last_line++;
      lloc->last_column = 1;
    }
    uctx->line = line;
    err = parse_line(line, n, cap, lloc, uctx, parse);
  }

  if (!err.es && uctx->data) {
    switch (uctx->type) {
    case 1:
      if (fwrite(line, 1, n, uctx->data) < n)
        err = (struct error){ES_FILE_WRITE};
      break;
    default:
      err = (struct error){ES_DUMP};
      break;
    }
  }

  return err;
}

static struct error parse_text(FILE *in, FILE *out) {
  struct error err = {};
  size_t n = 0;
  char line[BUFSIZ];
  struct parse_context ctx = PARSE_CONTEXT_INIT(1, out);

  while (!err.es && fgets(line, sizeof(line), in)) {
    n = strlen(line);
    err = parse_line_and_dump(line, n, sizeof(line), &ctx.lloc, &ctx.uctx);
  }

  return err;
}

static int remark_line(char *line, size_t n, size_t cap, void *data) {
  struct parse_context *ctx = data;

  // Parsing failure does not abort the remark process, so we count the errors
  ctx->errs += !!parse_line_and_dump(line, n, cap, &ctx->lloc, &ctx->uctx).es;
  return ctx->errs;
}

static struct error remark_c(const struct input *i, FILE *out) {
#ifdef USE_CLANG_TOOL
  struct error err;
  FILE *in;

  if ((err = open_file(i->file, "r", &in)).es)
    return err;

  if (!(err = reads(in, &input_content, NULL)).es) {
    struct parse_context ctx = PARSE_CONTEXT_INIT(1, out);
    err = remark(string_get(&input_content), string_len(&input_content),
                 ALT(i->tu, i->file), i->opts, remark_line, &ctx);
    err = next_error(err, ctx.errs ? (struct error){ES_PARSE, ctx.errs}
                                   : (struct error){});
  }

  return next_error(err, close_file(in));
#else
  fprintf(stderr, "Clang tool is not compiled in\n");
  return (struct error){ES_REMARK_NO_CLANG};
#endif // USE_CLANG_TOOL
}

#define PUSH_XXXoutput
#define PUSH_XXX(x)                                                            \
  struct output saved_output = output;                                         \
  output = x

#define POP_XXXoutput
#define POP_XXX(x) output = saved_output

#define PUSH_OUTPUT(x) PP_REPLACE(x, PUSH_XXX)
#define POP_OUTPUT(x) PP_REPLACE(x, POP_XXX)

static_assert(true PUSH_OUTPUT(output) POP_OUTPUT(output),
              "The macro expansions should append nothing");

#define DO(tmp_output, expr, ...)                                              \
  do {                                                                         \
    struct output_file of;                                                     \
    if (!err.es) {                                                             \
      PUSH_OUTPUT(tmp_output);                                                 \
      if (!(err = open_output(&of)).es) {                                      \
        err = expr;                                                            \
        __VA_ARGS__                                                            \
        err = next_error(err, close_output(&of));                              \
        err = cleanup_output(err, &of);                                        \
      }                                                                        \
      POP_OUTPUT(tmp_output);                                                  \
    }                                                                          \
  } while (0)

static struct error parse_text_and_dump(struct input i) {
  FILE *in;
  struct error err = open_file(i.file, "r", &in);
  DO(output, parse_text(in, of.file));
  return next_error(err, close_file(in));
}

static struct error parse_text_only(struct input i) {
  char *file = output.file;
  output.file = NULL;
  struct error err = parse_text_and_dump(i);
  output.file = file;
  return err;
}

static struct error parse_text_and_store(struct input i) {
  struct error err = parse_text_only(i);
  DO(output, store());
  return err;
}

static struct error remark_c_and_dump(struct input i) {
  bool noparse = output.noparse;
  output.noparse = 1;
  struct error err = {};
  DO(output, remark_c(&i, of.file));
  output.noparse = noparse;
  return err;
}

static struct error remark_c_only(struct input i) {
  char *file = output.file;
  output.file = NULL;
  struct error err = remark_c_and_dump(i);
  output.file = file;
  return err;
}

static struct error remark_c_and_store(struct input i) {
  struct error err = remark_c(&i, NULL);
  DO(output, store());
  return err;
}

static struct error render_html_only(struct input i) {
  struct error err = store_open(i.file);
  DO(output, render(of.file));
  return next_error(err, store_close());
}

static struct error inmemory_store_and_render(struct error err) {
  struct output origin = output;
  struct output inmemory = output;
  inmemory.kind = OK_DATA;
  inmemory.file = ":memory:";
  DO(inmemory, store(), { DO(origin, render(of.file)); });
  return err;
}

static struct error parse_text_and_render(struct input i) {
  return inmemory_store_and_render(parse_text_only(i));
}

static struct error remark_c_and_render(struct input i) {
  return inmemory_store_and_render(remark_c(&i, NULL));
}

static_assert(IK_NUMS < 16 && OK_NUMS < 16, "Too many input/output kinds");

#define IO(a, b) (a << 4) | b

struct builder {
  build_t build;
  const char *info;
};

static struct builder builders[128] = {
#define IOB(a, b, c) [IO(IK_##a, OK_##b)] = {c, #a " -> " #b}

    IOB(TEXT, NIL, parse_text_only),
    IOB(TEXT, TEXT, parse_text_and_dump),
    IOB(TEXT, DATA, parse_text_and_store),
    IOB(TEXT, HTML, parse_text_and_render),

    IOB(C, NIL, remark_c_only),
    IOB(C, TEXT, remark_c_and_dump),
    IOB(C, DATA, remark_c_and_store),
    IOB(C, HTML, remark_c_and_render),

    IOB(DATA, HTML, render_html_only),

#undef IOB
};

struct error build_output(struct output o) {
  struct error err = next_error(parse_init(), render_init());
  if (err.es)
    return err;

  output = o;
  srand(time(NULL));

  foreach_input(i, {
    unsigned k = IO(i.kind, o.kind);
    assert(k < sizeof(builders) / sizeof(*builders));
    if (!builders[k].build) {
      fprintf(stderr, "Building is not allowed: %s\n", builders[k].info);
      err = (struct error){ES_BUILDING_PROHIBITED};
    } else {
      err = builders[k].build(i);
    }
    if (err.es)
      break;
  });

  string_clear(&input_content, 1);
  return next_error(err, next_error(parse_halt(), render_halt()));
}