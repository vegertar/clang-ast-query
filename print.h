#pragma once

#include <stdio.h>

struct type;
struct array;
struct loc;
struct range;
struct ref;
struct op;
struct mem;
struct def;
struct comment;
struct decl;
struct node;

void print_type(FILE *fp, const struct type *type);
void print_array(FILE *fp, const struct array *array);
void print_loc(FILE *fp, const struct loc *loc);
void print_range(FILE *fp, const struct range *range);
void print_ref(FILE *fp, const struct ref *ref);
void print_op(FILE *fp, const struct op *op);
void print_mem(FILE *fp, const struct mem *mem);
void print_def(FILE *fp, const struct def *def);
void print_comment(FILE *fp, const struct comment *comment);
void print_decl(FILE *fp, const struct decl *decl);
void print_node(FILE *fp, const struct node *node);
