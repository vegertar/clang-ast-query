#include "parse.h"
#include "scan.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

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
  destroy();
  yylex_destroy();
  return status;
}

int main(int argc, const char *argv[]) {
  FILE *fp = stdin;
  if (argc > 1) {
    fp = fopen(argv[1], "r");
    if (!fp) {
      fprintf(stderr, "open('%s') error: %s\n", argv[1], strerror(errno));
      return 1;
    }
  }

  const char *yydebug_env = getenv("YYDEBUG");
  if (yydebug_env && atoi(yydebug_env)) {
    yydebug = 1;
  }

  int status = parse_file(fp);
  if (fp != stdin) {
    fclose(fp);
  }
  return status;
}