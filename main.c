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

static int debug_flag;
static int text_flag;
static int cc_flag;
static const char *in_filename;
static const char *db_filename;
static FILE *text_file;
static int max_level;

struct parser_context {
  YYLTYPE lloc;
  user_context uctx;
  int errs;
};

static int parse_line(char *line, size_t n, size_t cap, void *data) {
  assert(n + 1 < cap);
  line[n] = line[n+1] = 0;

  if (text_file) fwrite(line, n, 1, text_file);
  YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);

  struct parser_context *ctx = (struct parser_context *)data;
  ctx->uctx.line = line;

  int err = parse(&ctx->lloc, &ctx->uctx);
  if (err) ctx->errs++;
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
  if (in_filename) {
    fp = fopen(in_filename, "r");
    if (!fp) {
      fprintf(stderr, "%s: open('%s') error: %s\n",
              __func__, in_filename, strerror(errno));
      return 1;
    }
  }

  int err = parse_file(fp);
  if (in_filename) fclose(fp);
  return err;
}

static int cc_mode(int argc, char **argv) {
#ifdef USE_CLANG_TOOL
  struct parser_context ctx = { (YYLTYPE){1, 1, 1, 1} };
  int err = remark(in_filename,
                   argv + optind,
                   argc - optind,
                   in_filename ? parse_line : NULL,
                   &ctx);
  return err ? err : ctx.errs;
#else
  fprintf(stderr, "CC mode is not compiled in\n");
  return -1;
#endif // USE_CLANG_TOOL
}

int main(int argc, char **argv) {
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "htdco:xl:")) != -1)
    switch (c) {
    case 'h':
      fprintf(stderr, "Usage: %s [OPTION]... [-c [-- [CLANG OPTION]...]] [FILE]\n", argv[0]);
      fprintf(stderr, "The utility to handle Clang AST from FILE (the stdin by default)\n\n"
                      "If -c is specified, the input FILE must be named with a .c extension,"
                      " or in the case of stdin, the AST generated by Clang Tool will be dumped"
                      " instead of being handled by the scanner.\n\n"
                      "Additionally, -o in CLANG OPTION will export the SQLite3 database"
                      " rather than the ordinary object file.\n\n");
      fprintf(stderr, "\t-t\t\trun test\n");
      fprintf(stderr, "\t-d\t\tenable yydebug\n");
      fprintf(stderr, "\t-c\t\tread from .c source\n");
      fprintf(stderr, "\t-o DB\t\texport SQLite3 database\n");
      fprintf(stderr, "\t-x\t\tdump the AST text into DB.text or stdout\n");
      fprintf(stderr, "\t-l LEVEL\tthe maximum level to export\n");
      fprintf(stderr, "\t-h\t\tdisplay this help and exit\n");
      return 0;
    case 't':
      return RUN_TEST();
      break;
    case 'd':
      debug_flag = 1;
      break;
    case 'c':
      cc_flag = 1;
      break;
    case 'o':
      db_filename = optarg;
      break;
    case 'x':
      text_flag = 1;
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
        fprintf(stderr, "%s: open('%s') error: %s\n",
                __func__, filename, strerror(errno));
        return 1;
      }
    } else {
      text_file = stdout;
    }
  }

  int err = cc_flag ? cc_mode(argc, argv) : text_mode(argc, argv);
  if (!err && db_filename) err = dump(max_level, db_filename);
  if (db_filename && text_file) fclose(text_file);

  destroy();
  yylex_destroy();

  return err;
}
