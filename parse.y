%{
#include "lex.yy.h"
#include "ast.h"
#include <stdio.h>

#define yyerror(...) (fprintf(stderr, __VA_ARGS__))

%}

%union {
  int i;
  char *s;
  double d;
  struct ast *ast;
  struct node *node;
  struct sloc *sloc;
  struct sloc_range *sloc_range;
}

%token <i> INDENT
%token <i> INTEGER
%token <s> HEAD
%token <s> PARENT
%token <s> PREV
%token <s> POINTER
%token <s> MEMBER
%token <s> NAME
%token <s> SQNAME
%token <s> DQNAME
%token <s> BQNAME
%token <s> FILENAME
%token <s> OPERATOR
%token <s> OPTION
%token <s> ATTR
%token <s> SPEC
%token <s> CLASS
%token <s> TRAIT
%token <s> TAG
%token <s> STRING
%token <d> NUMBER
%token NULLIFY
%token INVALID_SLOC
%token LINE
%token COL
%token ENUM
%token FIELD

%type <ast> ast tree
%type <node> root node
%type <s> parent prev
%type <sloc> loc sloc file_sloc line_sloc col_sloc
%type <sloc_range> range sloc_range

%%

ast: root tree { $$ = NULL; }

root: node

tree: { $$ = NULL; }
 | tree INDENT node { $$ = NULL; }

node: HEAD parent prev range loc attrs labels decl opts { $$ = new_node($1, $2, $3, $4, $5); }
 | ENUM INTEGER { $$ = NULL; }
 | NULLIFY { $$ = NULL; }

parent: { $$ = NULL; }
 | PARENT

prev: { $$ = NULL; }
 | PREV

range: { $$ = NULL; }
 | '<' sloc_range '>' { $$ = $2; }

loc: { $$ = NULL; }
 | sloc

attrs:
 | attrs ATTR

labels:
 | labels DQNAME

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

sloc_range: sloc { $$ = new_sloc_range($1, NULL); }
 | sloc ',' sloc { $$ = new_sloc_range($1, $3); }

sloc: INVALID_SLOC { $$ = invalid_sloc(); }
 | file_sloc
 | line_sloc
 | col_sloc

file_sloc: FILENAME ':' INTEGER ':' INTEGER { $$ = new_file_sloc($1, $3, $5); }

line_sloc: LINE ':' INTEGER ':' INTEGER { $$ = new_line_sloc($3, $5); }

col_sloc: COL ':' INTEGER { $$ = new_col_sloc($3); }

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

mem:
 | MEMBER

field:
 | FIELD

ref:
 | HEAD SQNAME type

cast:
 | BQNAME

%%
