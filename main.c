#include "test.h"
#include "parse.h"
#include "scan.h"

#ifdef USE_CLANG_TOOL
# include "remark.h"
#endif // USE_CLANG_TOOL

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

struct parser_context {
  YYLTYPE lloc;
  user_context uctx;
};

static int parse_line(char *line, size_t n, size_t cap, void *data) {
  assert(n + 1 < cap);
  line[n] = line[n+1] = 0;
  YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);

  struct parser_context *ctx = (struct parser_context *)data;
  ctx->uctx.line = line;

  int err = parse(&ctx->lloc, &ctx->uctx);
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

static int text_mode(int argc, char **argv) {
  FILE *fp = stdin;
  if (optind < argc) {
    const char *filename = argv[optind];
    fp = fopen(filename, "r");
    if (!fp) {
      fprintf(stderr, "%s: open('%s') error: %s\n",
              __func__, filename, strerror(errno));
      return 1;
    }
  }

  int err = parse_file(fp);
  if (fp != stdin) fclose(fp);
  return err;
}

static int cc_mode(int argc, char **argv) {
#ifdef USE_CLANG_TOOL
  const char *filename = NULL;
  for (int i = optind; i < argc; ++i) {
    const char *s = argv[i];
    size_t n = strlen(s);
    if (s[n - 1] == 'c' && s[n - 2] == '.') {
      filename = s;
      argv[i] = "";
    }
  }

  struct parser_context ctx = { (YYLTYPE){1, 1, 1, 1} };
  return remark(filename, argv + optind, argc - optind, parse_line, &ctx);
#else
  fprintf(stderr, "CC mode is not compiled in\n");
  return -1;
#endif // USE_CLANG_TOOL
}

int main(int argc, char **argv) {
  int test_flag = 0;
  int debug_flag = 0;
  int cc_flag = 0;
  const char *db_file = NULL;
  int max_level = 0;
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "htdcx:l:")) != -1)
    switch (c) {
    case 'h':
      fprintf(stderr, "Usage: %s [OPTION]... [-- [CLANG OPTION]...] [FILE]\n", argv[0]);
      fprintf(stderr, "The utility to handle Clang AST from FILE (the stdin by default)\n\n");
      fprintf(stderr, "\t-t\t\trun test\n");
      fprintf(stderr, "\t-d\t\tenable yydebug\n");
      fprintf(stderr, "\t-c\t\tread from .c source\n");
      fprintf(stderr, "\t-x DB\t\texport SQLite3 database\n");
      fprintf(stderr, "\t-l LEVEL\tthe maximum level to export\n");
      fprintf(stderr, "\t-h\t\tdisplay this help and exit\n");
      return 0;
    case 't':
      test_flag = 1;
      break;
    case 'd':
      debug_flag = 1;
      break;
    case 'c':
      cc_flag = 1;
      break;
    case 'x':
      db_file = optarg;
      break;
    case 'l':
      max_level = atoi(optarg);
      break;
    case '?':
      if (optopt == 'l')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort();
    }

  if (debug_flag) yydebug = 1;
  if (test_flag) return RUN_TEST();

  int err = cc_flag ? cc_mode(argc, argv) : text_mode(argc, argv);
  if (!err && db_file) err = dump(max_level, db_file);

  destroy();
  yylex_destroy();

  return err;
}
