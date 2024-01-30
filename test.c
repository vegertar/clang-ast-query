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
  int status = 0;
  while (status == 0 && fgets(line, sizeof(line), fp)) {
    user_context uctx = {0, line};
    size_t n = strlen(line);
    assert(n + 1 < sizeof(line));
    line[n] = line[n+1] = 0;
    YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);
    status = parse(&lloc, &uctx);
    yy_delete_buffer(buffer);
  }
  return status;
}

int main(int argc, char **argv) {
  int debug_flag = 0;
  const char *db_file = NULL;
  int max_level = 0;
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "dhx:l:")) != -1)
    switch (c) {
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
      fprintf(stderr, "The utility to test clang-ast-query from FILE (the stdin by default)\n\n");
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

  int status = parse_file(fp);
  if (fp != stdin) {
    fclose(fp);
  }
  if (!status && db_file) {
    status = dump(max_level, db_file);
  }

  destroy();
  yylex_destroy();
  return status;
}