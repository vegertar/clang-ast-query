#include "test.h"
#include "parse.h"
#include "scan.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

int parse_file(FILE *fp) {
  YYLTYPE lloc = {1, 1, 1, 1};
  char line[8192];
  int err = 0;
  while (err == 0 && fgets(line, sizeof(line), fp)) {
    user_context uctx = {0, line};
    size_t n = strlen(line);
    assert(n + 1 < sizeof(line));
    line[n] = line[n+1] = 0;
    YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);
    err = parse(&lloc, &uctx);
    yy_delete_buffer(buffer);
  }
  return err;
}

int main(int argc, char **argv) {
  int test_flag = 0;
  int debug_flag = 0;
  const char *db_file = NULL;
  int max_level = 0;
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "tdhx:l:")) != -1)
    switch (c) {
    case 't':
      test_flag = 1;
      break;
    case 'd':
      debug_flag = 1;
      break;
    case 'x':
      db_file = optarg;
      break;
    case 'l':
      max_level = atoi(optarg);
      break;
    case 'h':
      fprintf(stderr, "Usage: %s [OPTION]... [FILE]\n", argv[0]);
      fprintf(stderr, "The utility to parse/query Clang AST from FILE (the stdin by default)\n\n");
      fprintf(stderr, "\t-t\t\trun test\n");
      fprintf(stderr, "\t-d\t\tenable yydebug\n");
      fprintf(stderr, "\t-x DB\t\texport SQLite3 database\n");
      fprintf(stderr, "\t-l LEVEL\tthe maximum level to export\n");
      fprintf(stderr, "\t-h\t\tdisplay this help and exit\n");
      return 0;
    case '?':
      if (optopt == 'l') {
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      } else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort();
    }

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

  if (debug_flag) {
    yydebug = 1;
  }

  int err = parse_file(fp);
  if (!err && db_file) {
    err = dump(max_level, db_file);
  }

  if (!err && test_flag) {
    err = RUN_TEST();
  }

  destroy();
  yylex_destroy();

  if (fp != stdin) {
    fclose(fp);
  }
  return err;
}