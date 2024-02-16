#include "parse.h"
#include "scan.h"
#include "test.h"

#ifdef USE_CLANG_TOOL
#include "remark.h"
#endif // USE_CLANG_TOOL

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int debug_flag;
static int text_flag;
static int cc_flag;
static const char *in_filename;
static const char *db_filename;
static const char *tu_filename;
static FILE *text_file;

const char *tu_code;
size_t tu_size;

struct parser_context {
  YYLTYPE lloc;
  user_context uctx;
  int errs;
};

DECL_ARRAY(string, char);
IMPL_ARRAY_RESERVE(string, char)
IMPL_ARRAY_APPEND(string, char)
IMPL_ARRAY_CLEAR(string, NULL)

static struct string in_content;

static int parse_line(char *line, size_t n, size_t cap, void *data) {
  assert(n + 1 < cap);
  line[n] = line[n + 1] = 0;

  if (text_file)
    fwrite(line, n, 1, text_file);
  YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);

  struct parser_context *ctx = (struct parser_context *)data;
  ctx->uctx.line = line;

  int err = parse(&ctx->lloc, &ctx->uctx);
  if (err)
    ctx->errs++;
  yy_delete_buffer(buffer);
  return err;
}

static int parse_file(FILE *fp) {
  char line[BUFSIZ];
  struct parser_context ctx = {
      .lloc = (YYLTYPE){1, 1, 1, 1},
  };

  int err = 0;
  while (err == 0 && fgets(line, sizeof(line), fp)) {
    err = parse_line(line, strlen(line), sizeof(line), &ctx);
  }
  return err;
}

static inline FILE *open_file(const char *filename, const char *mode) {
  FILE *fp = fopen(filename, mode);
  if (!fp) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, in_filename,
            strerror(errno));
  }
  return fp;
}

static int text_mode(int argc, char **argv) {
  FILE *fp = in_filename ? open_file(in_filename, "r") : stdin;
  if (!fp)
    return 1;

  int err = parse_file(fp);
  if (in_filename)
    fclose(fp);
  return err;
}

static void read_content(struct string *s) {
  FILE *fp = in_filename ? open_file(in_filename, "r") : stdin;
  if (!fp)
    exit(1);

  char buffer[BUFSIZ];
  size_t n = 0;
  while ((n = fread(buffer, 1, sizeof(buffer), fp))) {
    string_append(s, buffer, n);
  }
  if (ferror(fp)) {
    fprintf(stderr, "fread error: %s\n", strerror(errno));
    exit(1);
  }

  if (in_filename)
    fclose(fp);
}

static void prepare_tu(struct string *s) {
  read_content(s);
  tu_code = s->data;
  tu_size = s->i;
  if (!tu_filename)
    tu_filename = in_filename;
}

static int cc_mode(int argc, char **argv) {
#ifdef USE_CLANG_TOOL
  struct parser_context ctx = {(YYLTYPE){1, 1, 1, 1}};
  int err = remark(tu_code, tu_size, tu_filename, argv + optind, argc - optind,
                   parse_line, &ctx);
  return err ? err : ctx.errs;
#else
  fprintf(stderr, "CC mode is not compiled in\n");
  return -1;
#endif // USE_CLANG_TOOL
}

int main(int argc, char **argv) {
  int c;

  while ((c = getopt(argc, argv, "ht::T::dcf:o:x")) != -1)
    switch (c) {
    case 'h':
      fprintf(stderr,
              "Usage: %s [OPTION]... [-c [-- [CLANG OPTION]...]] [FILE]\n",
              argv[0]);
      fprintf(
          stderr,
          "The utility to handle Clang AST from FILE (the stdin by default)\n\n"
          "If -c is specified, the input FILE must be named with a .c"
          " extension.\n\n"
          "Additionally, -o in CLANG OPTION will export the SQLite3 database"
          " rather than the ordinary object file.\n\n");
      fprintf(stderr, "\t-t[help]\trun test\n");
      fprintf(stderr, "\t-T[help]\toperate toggle\n");
      fprintf(stderr, "\t-d\t\tenable yydebug\n");
      fprintf(stderr,
              "\t-f\t\tspecify the filename if read code from stdin with -c\n");
      fprintf(stderr, "\t-c\t\tread from .c source\n");
      fprintf(stderr, "\t-o DB\t\texport SQLite3 database\n");
      fprintf(stderr, "\t-x\t\tdump the AST text into DB.text or stdout\n");
      fprintf(stderr, "\t-h\t\tdisplay this help and exit\n");
      return 0;
    case 't':
      return optarg && strcmp(optarg, "help") == 0 ? test_help()
                                                   : RUN_TEST(optarg);
    case 'T':
      if (optarg && strcmp(optarg, "help") == 0)
        return toggle_help();
      if (toggle(optarg))
        return -1;
      break;
    case 'd':
      debug_flag = 1;
      break;
    case 'c':
      cc_flag = 1;
      break;
    case 'f':
      tu_filename = optarg;
      break;
    case 'o':
      db_filename = optarg;
      break;
    case 'x':
      text_flag = 1;
      break;
    default:
      exit(1);
    }

  if (debug_flag)
    yydebug = 1;

  in_filename = argv[optind];

  // enable cc mode if provide a .c filename
  for (int i = optind; i < argc; ++i) {
    const char *s = argv[i];
    size_t n = strlen(s);
    if (s[n - 1] == 'c' && s[n - 2] == '.') {
      in_filename = s;
      argv[i] = "";
      cc_flag = 1;
    }
  }

  // allow the cc mode to specify the db filename by -o
  if (cc_flag) {
    for (int i = optind; i + 1 < argc; ++i) {
      if (strcmp(argv[i], "-o") == 0)
        db_filename = argv[i + 1];
    }
  }

  if (text_flag) {
    if (db_filename) {
      char filename[BUFSIZ];
      snprintf(filename, sizeof(filename), "%s.text", db_filename);
      if (!(text_file = fopen(filename, "w"))) {
        fprintf(stderr, "%s: open('%s') error: %s\n", __func__, filename,
                strerror(errno));
        return 1;
      }
    } else {
      text_file = stdout;
    }
  }

  if (cc_flag)
    prepare_tu(&in_content);

  int err = cc_flag ? cc_mode(argc, argv) : text_mode(argc, argv);
  if (!err && db_filename)
    err = dump(db_filename);
  if (db_filename && text_file)
    fclose(text_file);

  destroy();
  yylex_destroy();
  string_clear(&in_content, 2);

  return err;
}
