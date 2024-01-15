#include "parse.tab.h"
#include "lex.yy.h"
#include <string.h>
#include <stdio.h>

char buffer[8192];

int main(int argc, const char *argv[]) {
  FILE *fp = stdin;
  if (argc > 1) {
    fp = fopen(argv[1], "r");
    if (!fp) {
      fprintf(stderr, "open('%s') error: %s\n", argv[1], strerror(errno));
      return 1;
    }
  }

  const char *s = fgets(buffer, sizeof(buffer), fp);
  if (!s) {
    fprintf(stderr, "missing root\n");
    return 1;
  }

  const size_t n = strlen(s);
  int status = 0;
  size_t lineno = 1;
  while (status == 0 && (s = fgets(buffer + n, sizeof(buffer) - n, fp))) {
    ++lineno;
    YY_BUFFER_STATE buffer_state = yy_scan_string(buffer);
    status = yyparse();
    yy_delete_buffer(buffer_state);
  }

  if (status) {
    fprintf(stderr, "\n%8zu  %s\n", lineno, s);
  }
  return status;
}