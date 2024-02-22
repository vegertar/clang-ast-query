/* Disable Flex features we don't need, to avoid warnings. */
%option nodefault noinput nounput noyywrap


%x TYPE

%{
#include "parse.h"
#include <assert.h>

// Each time a rule is matched, advance the end cursor/position.
#define YY_USER_ACTION                          \
  yylloc->last_column += (int)yyleng;

// Move the first position onto the last.
#define LOCATION_STEP()                         \
  do {                                          \
    yylloc->first_line = yylloc->last_line;     \
    yylloc->first_column = yylloc->last_column; \
  } while (0)

static inline char * qname() {
  return strndup(yytext + 1, yyleng - 2);
}

%}

INDENT ^[ \|`]+-

NAME ([a-zA-Z_][a-zA-Z_0-9]*)

INTEGER (-?([0-9]+))

NUMBER (-?([0-9]*\.[0-9]+|[0-9]+)([Ee][+-]?[0-9]+)?)

POINTER (0x[0-9a-f]+)

ATTR (sugar|imported|implicit|used|referenced|prefix|postfix|Inherited|Implicit|warn_unused_result|"<undeserialized declarations>")

SPEC ({ATTR}|extern|static|inline|lvalue|bitfield|cinit|const|volatile|cdecl)

CLASS (struct|union|enum)

TRAIT (__alignof|alignof|sizeof)

OPTION ("cannot overflow"|part_of_explicit_cast|definition|IsLiteralLabel|non_odr_use_unevaluated)

OPERATOR ("','"|"'%'"|"'/'"|"'*'"|"'-'"|"'+'"|"'&'"|"'|'"|"'^'"|"'!'"|"'>'"|"'<'"|"'~'"|"'='"|"'>>'"|"'<<'"|"'>='"|"'<='"|"'=='"|"'!='"|"'+='"|"'-='"|"'*='"|"'/='"|"'%='"|"'^='"|"'|='"|"'&='"|"'>>='"|"'<<='"|"'--'"|"'++'"|"'&&'"|"'||'"|"'__extension__'")

%%

%{
  // Each time yylex is called, move the head position to the end one.
  LOCATION_STEP();
%}

"=" |
":" |
"," |
"<" |
">" {
  return yytext[0];
}

{OPERATOR}/[ \n] {
  yylval->TOK_OPERATOR = qname();
  return TOK_OPERATOR;
}

"<<<NULL>>>" {
  return TOK_NULL;
}

"<invalid sloc>" {
  return TOK_INVALID_SLOC;
}

"value: Int"/" "{INTEGER} {
  return TOK_ENUM;
}

^"#TU:".+ {
  int n = sscanf(yytext, "#TU:%s", tu);
  if (n != 1) return TOK_YYUNDEF;
  yylval->TOK_REMARK_TU = tu;
  return TOK_REMARK_TU;
}

^"#CWD:".+ {
  int n = sscanf(yytext, "#CWD:%s", cwd);
  if (n != 1) return TOK_YYUNDEF;
  yylval->TOK_REMARK_CWD = cwd;
  return TOK_REMARK_CWD;
}

^"#VAR-TYPE:".+ {
  char var[32];
  char type[32];
  int n = sscanf(yytext, "#VAR-TYPE:%s %s", var, type);
  if (n != 2) return TOK_YYUNDEF;
  yylval->TOK_REMARK_VAR_TYPE = add_var_type_map(strdup(var), strdup(type));
  return TOK_REMARK_VAR_TYPE;
}

{ATTR}/[ \n] {
  yylval->TOK_ATTR = strdup(yytext);
  return TOK_ATTR;
}

{CLASS}/[ \n] {
  yylval->TOK_CLASS = strdup(yytext);
  return TOK_CLASS;
}

{TRAIT}/[ \n] {
  yylval->TOK_TRAIT = strdup(yytext);
  return TOK_TRAIT;
}

{OPTION}/[ \n] {
  yylval->TOK_OPTION = strdup(yytext);
  return TOK_OPTION;
}

{NAME}/= {
  yylval->TOK_TAG = strdup(yytext);
  return TOK_TAG;
}

{INDENT}/[^ \n] {
  yylval->TOK_INDENT = yyleng;
  return TOK_INDENT;
}

[A-Z][a-zA-Z]+" "{POINTER}/[ \n] {
  yylval->TOK_HEAD = strdup(yytext);
  return TOK_HEAD;
}

parent" "{POINTER}/[ \n] {
  yylval->TOK_PARENT = strdup(yytext + 7);
  return TOK_PARENT;
}

prev" "{POINTER}/[ \n] {
  yylval->TOK_PREV = strdup(yytext + 6);
  return TOK_PREV;
}

("->"|"."){NAME}?" "{POINTER}/[ \n] {
  yylval->TOK_MEMBER = strdup(yytext);
  return TOK_MEMBER;
}

{POINTER}/[ \n] {
  yylval->TOK_POINTER = strdup(yytext);
  return TOK_POINTER;
}

{NAME}/[ \n] {
  yylval->TOK_NAME = strdup(yytext);
  return TOK_NAME;
}

line/:[0-9]+:[0-9]+ {
  return TOK_LINE;
}

col/:[0-9]+ {
  return TOK_COL;
}

[^ <:]+/:[0-9]+:[0-9]+ |
"<scratch space>" |
"<command line>" |
"<built-in>" {
  yylval->TOK_SRC = (char *)add_src(yytext);
  return TOK_SRC;
}

field/" "Field[ \n] {
  return TOK_FIELD;
}

{INTEGER}/[:>, \n] {
  yylval->TOK_INTEGER = yytext;
  return TOK_INTEGER;
}

\"[^\"]*\"/[ \n] {
  yylval->TOK_DQNAME = qname();
  return TOK_DQNAME;
}

\<[A-Z][^\>]+\>/[ \n] {
  yylval->TOK_BQNAME = qname();
  return TOK_BQNAME;
}

'((?:[^'\\]|\\.)*)'/[: \n] {
  BEGIN TYPE;
  yylval->TOK_SQNAME = qname();
  return TOK_SQNAME;
}

[ \t] { }
\n { 
  yylloc->last_line++;
  yylloc->last_column = 1;
  return TOK_EOL;
}
. {
  return TOK_YYUNDEF;
}

<TYPE>{SPEC}/[ \n] {
  yylval->TOK_SPEC = strdup(yytext);
  return TOK_SPEC;
}

<TYPE>{NUMBER}/[ \n] {
  yylval->TOK_NUMBER = strdup(yytext);
  return TOK_NUMBER;
}

<TYPE>\".*\"/[ \n] {
  yylval->TOK_STRING = strdup(yytext);
  return TOK_STRING;
}

<TYPE>[ \t] { }
<TYPE>.|\n {
  BEGIN INITIAL;
  yyless(0);
}

%%
