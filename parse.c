#include "token.tab.h"
#include <stdio.h>

void yyerror(const char *s) { fprintf(stderr, "%s\n", s); }

int yywrap(void) { return 1; }

int main(void) {
  yyparse();
}