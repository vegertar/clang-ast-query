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
static const char *out_filename;
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

  if (text_file) {
    fwrite(line, n, 1, text_file);
    return 0;
  }

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

  while ((c = getopt(argc, argv, "ht::T::dci:o:")) != -1)
    switch (c) {
    case 'h':
      fprintf(stderr, "Usage: %s [OPTION]... [-- [CLANG OPTION]...] [FILE]\n",
              argv[0]);
      fprintf(stderr, "The utility to handle Clang AST from FILE (the stdin by "
                      "default)\n\n");
      fprintf(stderr, "\t-t[help]   run test\n");
      fprintf(stderr, "\t-T[help]   operate toggle\n");
      fprintf(stderr, "\t-d         enable debug\n");
      fprintf(stderr, "\t-c         only dump text\n");
      fprintf(stderr, "\t-i NAME    name the input TU\n");
      fprintf(stderr, "\t-o OUTPUT  specify the output file\n");
      fprintf(stderr, "\t-h         display this help and exit\n");
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
      text_flag = 1;
      break;
    case 'i':
      tu_filename = optarg;
      break;
    case 'o':
      out_filename = optarg;
      break;
    default:
      exit(1);
    }

  if (debug_flag)
    yydebug = 1;

  in_filename = argv[optind];

  // examine the CLANG OPTION
  for (int i = optind; i < argc; ++i) {
    const char *s = argv[i];
    size_t n = strlen(s);
    if (s[n - 1] == 'c' && s[n - 2] == '.') {
      in_filename = s;
      argv[i] = "";
      cc_flag = 1;
    } else if (n == 2) {
      if (s[0] == '-' && s[1] == 'c')
        text_flag = 1;
      else if (i + 1 < argc && s[0] == '-' && s[1] == 'o')
        out_filename = argv[i + 1];
    }
  }

  if (text_flag && out_filename && !(text_file = fopen(out_filename, "w"))) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, out_filename,
            strerror(errno));
    return 1;
  }

  int err = 0;
  if (cc_flag) {
    prepare_tu(&in_content);
    err = cc_mode(argc, argv);
  } else {
    err = text_mode(argc, argv);
  }

  if (!err && !text_flag && out_filename)
    err = dump(out_filename);

  if (text_file)
    fclose(text_file);
  destroy();
  yylex_destroy();
  string_clear(&in_content, 2);

  return err;
}
