#include "token.tab.h"
#include "lex.yy.h"
#include <string.h>
#include <stdio.h>

char buffer[8192];

int main(void) {
  const char *s = fgets(buffer, sizeof(buffer), stdin);
  if (!s) {
    fprintf(stderr, "missing root\n");
    return 1;
  }

  const size_t n = strlen(s);
  int status = 0;
  size_t lineno = 1;
  while (status == 0 && (s = fgets(buffer + n, sizeof(buffer) - n, stdin))) {
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