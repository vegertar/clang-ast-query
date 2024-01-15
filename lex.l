%x TYPE

%{
#include "parse.tab.h"

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

NAME ([a-zA-Z_][a-zA-Z_0-9]*)

NUMBER (-?([0-9]*\.[0-9]+|[0-9]+)([Ee][+-]?[0-9]+)?)

POINTER (0x[0-9a-f]+)

ATTR (sugar|imported|implicit|used|referenced|prefix|postfix|Inherited|Implicit|warn_unused_result|"<undeserialized declarations>")

SPEC ({ATTR}|extern|static|inline|lvalue|bitfield|cinit|const|volatile|cdecl)

CLASS (struct|union|enum)

TRAIT (__alignof|alignof|sizeof)

OPTION ("cannot overflow"|part_of_explicit_cast|definition|IsLiteralLabel|non_odr_use_unevaluated)

%%

"=" |
":" |
"," |
"<" |
">" {
  return yytext[0];
}

"','" |
"'%'" |
"'/'" |
"'*'" |
"'-'" |
"'+'" |
"'&'" |
"'|'" |
"'^'" |
"'!'" |
"'>'" |
"'<'" |
"'~'" |
"'='" |
"'>>'" |
"'<<'" |
"'>='" |
"'<='" |
"'=='" |
"'!='" |
"'+='" |
"'-='" |
"'*='" |
"'/='" |
"'%='" |
"'^='" |
"'|='" |
"'&='" |
"'>>='" |
"'<<='" |
"'--'" |
"'++'" |
"'&&'" |
"'||'" |
"'__extension__'" {
  yylval.s = qname();
  return OPERATOR;
}

"<<<NULL>>>" {
  return NULLIFY;
}

"<invalid sloc>" {
  return INVALID_SLOC;
}

"value: Int" {
  return ENUM;
}

{ATTR} {
  yylval.s = strdup(yytext);
  return ATTR;
}

{CLASS} {
  yylval.s = strdup(yytext);
  return CLASS;
}

{TRAIT} {
  yylval.s = strdup(yytext);
  return TRAIT;
}

{OPTION} {
  return OPTION;
}

{NAME}/= {
  yylval.s = strdup(yytext);
  return TAG;
}

^[|` ] {
  yylval.i = 2 + discards('-');
  return INDENT;
}

[A-Z][a-zA-Z]+" "{POINTER} {
  yylval.s = strdup(yytext);
  return HEAD;
}

parent" "{POINTER} {
  yylval.s = strdup(yytext);
  return PARENT;
}

prev" "{POINTER} {
  yylval.s = strdup(yytext);
  return PREV;
}

("->"|"."){NAME}?" "{POINTER} {
  yylval.s = strdup(yytext);
  return MEMBER;
}

{POINTER} {
  yylval.s = strdup(yytext);
  return POINTER;
}

{NAME} {
  yylval.s = strdup(yytext);
  return NAME;
}

[^ <:]+/:[0-9]+:[0-9]+ |
"<scratch space>" |
"<built-in>" {
  yylval.s = strdup(yytext);
  return FILENAME;
}

line/:[0-9]+:[0-9]+ {
  return LINE;
}

col/:[0-9]+ {
  return COL;
}

field/" "Field {
  return FIELD;
}

-?[0-9]+ {
  yylval.i = atoi(yytext);
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

[ \t\n] { }
. { fprintf(stderr, "mystery character '%c'\n", *yytext); }

<TYPE>{SPEC}/[ \n] {
  yylval.s = strdup(yytext);
  return SPEC;
}

<TYPE>{NUMBER}/[ \n] {
  yylval.d = atof(yytext);
  return NUMBER;
}

<TYPE>\".*\"/[ \n] {
  yylval.s = qname();
  return STRING;
}

<TYPE>[ \t] { }
<TYPE>.|\n {
  unput(yytext[0]);
  BEGIN INITIAL;
}

%%
