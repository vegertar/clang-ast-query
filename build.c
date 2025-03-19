#include "build.h"
#include "parse.h"
#include "remark.h"
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

static inline _Bool needs_to_save_tmp(const char *filename) {
  assert(filename);
  return !starts_with(filename, "/dev/");
}

static inline const char *get_tmp(struct output_file *of,
                                  const char *filename) {
  assert(of);
  if (needs_to_save_tmp(filename)) {
    char str[8];
    snprintf(of->tmp, sizeof(of->tmp), "%s.tmp-%s", output.file,
             rands(str, sizeof(str)));
    return of->tmp;
  }

  of->tmp[0] = 0;
  return NULL;
}

static struct error open_output(struct output_file *of) {
  assert(of);
  of->file = NULL;

  struct error err = {};
  if (output.file) {
    const char *tmp = get_tmp(of, output.file);
    const char *filename = ALT(tmp, output.file);

    switch (output.kind) {
    case OK_TEXT:
      err = open_file(filename, "w", &of->file);
      break;

    case OK_DATA:
      err = store_init(filename);
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
      err = close_file(of->file);
      break;

    case OK_DATA:
      err = store_halt();
      break;
    }
  }

  if (!err.es && of->tmp[0]) {
    if (output.file)
      err = rename_file(of->tmp, output.file);
    else
      err = unlink_file(of->tmp);
  }

  return err;
}

struct parse_context {
  YYLTYPE lloc;
  UserContext uctx;
  int errs;
};

#define PARSE_CONTEXT_INIT(data)                                               \
  {                                                                            \
    {1, 1, 1, 1}, { output.silent, NULL, data }                                \
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

  if (!err.es && uctx->data && fwrite(line, 1, n, uctx->data) < n)
    err = (struct error){ES_FILE_WRITE};

  return err;
}

static struct error parse_text(FILE *in, FILE *out) {
  struct error err = {};
  size_t n = 0;
  char line[BUFSIZ];
  struct parse_context ctx = PARSE_CONTEXT_INIT(out);

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

static struct error parse_c(const struct input *i, FILE *out) {
  struct error err;
  FILE *in;

  if ((err = open_file(i->file, "r", &in)).es)
    return err;

  if (!(err = reads(in, &input_content, NULL)).es) {
    struct parse_context ctx = PARSE_CONTEXT_INIT(out);
    err = remark(string_get(&input_content), string_len(&input_content),
                 ALT(i->tu, i->file), i->opts, remark_line, &ctx);
    err = next_error(err, ctx.errs ? (struct error){ES_PARSE, ctx.errs}
                                   : (struct error){});
  }

  return next_error(err, close_file(in));
}

static struct error parse_text_and_dump(struct input i) {
  struct error err;
  FILE *in;

  if ((err = open_file(i.file, "r", &in)).es)
    return err;

  struct output_file of;
  if (!(err = open_output(&of)).es) {
    err = parse_text(in, of.file);
    err = next_error(err, close_output(&of));
  }

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
  if (err.es)
    return err;

  struct output_file of;
  if (!(err = open_output(&of)).es) {
    err = store();
    err = next_error(err, close_output(&of));
  }

  return err;
}

static struct error parse_text_and_render(struct input i) {
  return (struct error){};
}

static struct error remark_c_and_dump(struct input i) {
#ifdef USE_CLANG_TOOL
  struct error err;
  struct output_file of;

  if ((err = open_output(&of)).es)
    return err;

  _Bool noparse = output.noparse;
  output.noparse = 1;
  err = parse_c(&i, of.file);
  output.noparse = noparse;

  return next_error(err, close_output(&of));
#else
  fprintf(stderr, "Clang tool is not compiled in\n");
  return (struct error){ES_REMARK_NO_CLANG};
#endif // USE_CLANG_TOOL
}

static struct error remark_c_only(struct input i) {
  char *file = output.file;
  output.file = NULL;
  struct error err = remark_c_and_dump(i);
  output.file = file;
  return err;
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
    IOB(C, DATA, NULL), // TODO:
    IOB(C, HTML, NULL), // TODO:

#undef IOB
};

struct error build_output(struct output o) {
  struct error err = parse_init();
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
  return next_error(err, parse_halt());
}