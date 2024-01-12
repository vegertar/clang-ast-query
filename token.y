%{
#include "parse.h"
#include <stdio.h>
%}

%union {
  long i;
  double d;
  const char *s;
}

%token <i> INDENT
%token <s> KIND
%token <s> NAME
%token <s> SQNAME
%token <s> DQNAME
%token <s> BQNAME
%token <s> ID
%token <s> FILENAME
%token <i> INTEGER
%token <d> NUMBER
%token <s> STRING
%token <s> OPERATOR
%token <s> ATTR
%token <s> SPEC
%token <s> CLASS
%token INVALID_SLOC
%token UNDESERIALIZED_DECLARATIONS
%token PREV
%token LINE
%token COL
%token TEXT
%token ENUM

%%

tree: root nodes

root: node

nodes:
 | INDENT node nodes

node: KIND ID prev srange sloc label attr decl tag
 | ENUM INTEGER

prev:
 | PREV ID

comment: TEXT '=' DQNAME

def: type spec value op ref cast

type: SQNAME
 | type ':' SQNAME

label:
 | label DQNAME

cast:
 | BQNAME

ref:
 | KIND ID SQNAME type

attr:
 | attr ATTR

decl:
 | UNDESERIALIZED_DECLARATIONS name
 | NAME
 | NAME def
 | SQNAME def 
 | def
 | comment
 | seq

name:
 | NAME

tag:
 | CLASS name attr

spec:
 | spec SPEC

value:
 | NUMBER
 | STRING

seq: INTEGER
 | seq INTEGER

op:
 | OPERATOR

srange:
 | '<' loc_list '>'

sloc:
 | loc

loc_list: loc
 | loc_list ',' loc

loc: INVALID_SLOC
 | file_sloc
 | line_sloc
 | col_sloc

file_sloc: FILENAME ':' INTEGER ':' INTEGER

line_sloc: LINE ':' INTEGER ':' INTEGER

col_sloc: COL ':' INTEGER

%%
