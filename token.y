%{
#include "lex.yy.h"
#include <stdio.h>

#define yyerror(...) (fprintf(stderr, __VA_ARGS__))

%}

%union {
  long i;
  double d;
  const char *s;
}

%token <i> INDENT
%token <i> INTEGER
%token <d> NUMBER
%token <s> POINTER
%token <s> HEAD
%token <s> NAME
%token <s> MEMBER
%token <s> SQNAME
%token <s> DQNAME
%token <s> BQNAME
%token <s> FILENAME
%token <s> STRING
%token <s> OPERATOR
%token <s> OPTION
%token <s> ATTR
%token <s> SPEC
%token <s> CLASS
%token <s> TRAIT
%token <s> PARENT
%token <s> PREV
%token <s> TAG
%token NULLIFY
%token INVALID_SLOC
%token UNDESERIALIZED_DECLARATIONS
%token LINE
%token COL
%token ENUM
%token FIELD

%%

tree: root nodes

root: node

nodes:
 | INDENT node nodes

node: HEAD parent prev srange sloc attrs labels undeserialized decl opts
 | ENUM INTEGER
 | NULLIFY

parent:
 | PARENT

prev:
 | PREV

srange:
 | '<' loc_list '>'

sloc:
 | loc

attrs:
 | attrs ATTR

labels:
 | labels DQNAME

undeserialized:
 | UNDESERIALIZED_DECLARATIONS

decl:
 | CLASS
 | NAME
 | CLASS NAME
 | SQNAME POINTER
 | SQNAME TRAIT
 | SQNAME TRAIT def
 | SQNAME def
 | def
 | NAME def
 | seq
 | NAME seq
 | comment 

loc_list: loc
 | loc_list ',' loc

loc: INVALID_SLOC
 | file_sloc
 | line_sloc
 | col_sloc

file_sloc: FILENAME ':' INTEGER ':' INTEGER

line_sloc: LINE ':' INTEGER ':' INTEGER

col_sloc: COL ':' INTEGER

def: type specs value op mem field ref cast

comment: TAG '=' DQNAME

seq: INTEGER
 | seq INTEGER

type: SQNAME
 | type ':' SQNAME

specs:
 | specs SPEC

value:
 | NUMBER
 | STRING

op:
 | OPERATOR tags

tags:
 | tags TAG '=' type

opts:
 | opts OPTION

field:
 | FIELD

mem:
 | MEMBER

ref:
 | HEAD SQNAME type

cast:
 | BQNAME

%%
