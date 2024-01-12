%x TYPE

%{
#include "token.tab.h"

static int input();

static inline int discards(int until) {
  int n = 0;
  while (input() != until) {
    ++n;
  }
  return n;
}

static inline char * qname() {
  return strndup(yytext + 1, strlen(yytext) - 2);
}

%}

ATTR (sugar|imported|implicit|used|referenced|definition|IsLiteralLabel|Inherited|Implicit)

SPEC ({ATTR}|extern|static|inline|lvalue|cinit|volatile)

CLASS (struct|class|union)

%%

"=" |
":" |
"," |
"<" |
">" {
  return yytext[0];
}

"'-'" |
"'+'" |
"'&'" |
"'|'" |
"'>>'" |
"'<<'" {
  yylval.s = qname();
  return OPERATOR;
}

"<invalid sloc>" {
  return INVALID_SLOC;
}

"<undeserialized declarations>" {
  return UNDESERIALIZED_DECLARATIONS;
}

"value: Int" {
  return ENUM;
}

\/[^:]+ |
"<scratch space>" {
  yylval.s = strdup(yytext);
  return FILENAME;
}

prev {
  return PREV;
}

line {
  return LINE;
}

col {
  return COL;
}

{ATTR} {
  yylval.s = strdup(yytext);
  return ATTR;
}

{CLASS} {
  yylval.s = strdup(yytext);
  return CLASS;
}

Text {
  return TEXT;
}

^[|` ] {
  yylval.i = 2 + discards('-');
  return INDENT;
}

[A-Z][a-zA-Z]+ {
  yylval.s = strdup(yytext);
  return KIND;
}

[a-zA-Z_][a-zA-Z_0-9]* {
  yylval.s = strdup(yytext);
  return NAME;
}

0x[0-9a-f]+ {
  yylval.s = strdup(yytext);
  return ID;
}

[+-]?[0-9]+ {
  yylval.i = atol(yytext);
  return INTEGER;
}

\"[^\"]*\" {
  yylval.s = qname();
  return DQNAME;
}

\<[A-Z][^\>]+\> {
  yylval.s = qname();
  return BQNAME;
}

'[^']*' {
  BEGIN TYPE;
  yylval.s = qname();
  return SQNAME;
}

<TYPE>{SPEC} {
  yylval.s = strdup(yytext);
  return SPEC;
}

<TYPE>[+-]?[0-9e.+]+ {
  yylval.d = atof(yytext);
  return NUMBER;
}

<TYPE>\"[^\"]*\" {
  yylval.s = qname();
  return STRING;
}

<TYPE>[ \t]
<TYPE>.|\n {
  unput(yytext[0]);
  BEGIN INITIAL;
}

[ \t\n] /* white space */
. { fprintf(stderr, "mystery character '%c'\n", *yytext); }

%%
