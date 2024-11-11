#include "build.h"
#include "parse.h"
#include "remark.h"
#include "sql.h"
#include "util.h"
#include <time.h>
#include <unistd.h>

struct input_list input_list;
static struct string input_content;
static struct output output;

struct output_file {
  FILE *file;
  char tmp[PATH_MAX];
};

#define FILL_TMP(tmp)                                                          \
  do {                                                                         \
    assert(output.file);                                                       \
    if (!starts_with(output.file, "/dev/")) {                                  \
      char str[8];                                                             \
      snprintf(tmp, sizeof(tmp), "%s.tmp-%s", output.file,                     \
               rands(str, sizeof(str)));                                       \
    } else {                                                                   \
      tmp[0] = 0;                                                              \
    }                                                                          \
  } while (0)

static int open_output(struct output_file *of) {
  if (output.file) {
    FILL_TMP(of->tmp);
    if (!(of->file = open_file(of->tmp[0] ? of->tmp : output.file, "w")))
      return -1;
  } else {
    of->file = NULL;
  }

  return 0;
}

static void close_output(struct output_file *of) {
  if (of->file) {
    fclose(of->file);
    if (of->tmp[0])
      rename(of->tmp, output.file);
  }
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

typedef int (*build_t)(struct input i);

static int parse_and_dump(char *line, size_t n, size_t cap, YYLTYPE *lloc,
                          UserContext *uctx) {
  int err = 0;

  if (!output.noparse) {
    if (lloc->last_column != 1) {
      // There was an error, we step the line manually
      lloc->last_line++;
      lloc->last_column = 1;
    }
    uctx->line = line;
    err = parse_line(line, n, cap, lloc, uctx, parse);
  }

  if (!err && uctx->data)
    err = fwrite(line, 1, n, uctx->data) < n;

  return err;
}

static int parse_text(FILE *in, FILE *out) {
  int err = 0;
  size_t n = 0;
  char line[BUFSIZ];
  struct parse_context ctx = PARSE_CONTEXT_INIT(out);

  while (!err && fgets(line, sizeof(line), in)) {
    n = strlen(line);
    err = parse_and_dump(line, n, sizeof(line), &ctx.lloc, &ctx.uctx);
  }

  return err;
}

static int remark_line(char *line, size_t n, size_t cap, void *data) {
  struct parse_context *ctx = data;

  // Parsing failure does not abort the remark process, so we count the errors
  ctx->errs += !!parse_and_dump(line, n, cap, &ctx->lloc, &ctx->uctx);
  return ctx->errs;
}

static int parse_c(const struct input *i, FILE *out) {
  FILE *in = open_file(i->file, "r");
  if (!in)
    return -1;

  int err;
  if (!(err = reads(in, &input_content, NULL))) {
    struct parse_context ctx = PARSE_CONTEXT_INIT(out);
    err = remark(string_get(&input_content), string_len(&input_content),
                 ALT(i->tu, i->file), i->opts, remark_line, &ctx);
    if (!err)
      err = ctx.errs;
  }

  fclose(in);
  return err;
}

static int parse_text_and_dump(struct input i) {
  FILE *in = open_file(i.file, "r");
  if (!in)
    return -1;

  struct output_file of;
  if (open_output(&of)) {
    fclose(in);
    return -1;
  }

  int err = parse_text(in, of.file);

  close_output(&of);
  fclose(in);

  return err;
}

static int parse_text_only(struct input i) {
  char *file = output.file;
  output.file = NULL;
  int err = parse_text_and_dump(i);
  output.file = file;
  return err;
}

static int parse_text_and_store(struct input i) {
  int err = parse_text_only(i);
  if (!err && output.file) {
    char tmp[PATH_MAX];
    FILL_TMP(tmp);

    if ((err = sql(tmp)) || (err = rename(tmp, output.file)))
      unlink(tmp);
  }

  return err;
}

static int parse_text_and_render(struct input i) { return 0; }

static int remark_c_and_dump(struct input i) {
#ifdef USE_CLANG_TOOL
  struct output_file of;
  if (open_output(&of))
    return -1;

  _Bool noparse = output.noparse;
  output.noparse = 1;
  int err = parse_c(&i, of.file);
  output.noparse = noparse;

  close_output(&of);
  return err;
#else
  fprintf(stderr, "Clang tool is not compiled in\n");
  return -1;
#endif // USE_CLANG_TOOL
}

static int remark_c_only(struct input i) {
  char *file = output.file;
  output.file = NULL;
  int err = remark_c_and_dump(i);
  output.file = file;
  return err;
}

_Static_assert(IK_NUMS < 16 && OK_NUMS < 16, "Too many input/output kinds");

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
    IOB(C, DATA, NULL),  // TODO:
    IOB(C, HTML, NULL), // TODO:

#undef IOB
};

int build_output(struct output o) {
  int err = 0;

  output = o;
  srand(time(NULL));
  parse_init();

  foreach_input(i, {
    unsigned k = IO(i.kind, o.kind);
    assert(k < sizeof(builders) / sizeof(*builders));
    if (!builders[k].build) {
      fprintf(stderr, "Not allowed building yet: %s\n", builders[k].info);
      err = 1;
    } else {
      err = builders[k].build(i);
    }
    if (err)
      break;
  });

  parse_halt();
  string_clear(&input_content, 1);
  input_list_clear(&input_list, 1);

  return err;
}