#pragma once

#include <stdio.h>

struct type;
struct array;
struct sloc;
struct srange;
struct ref;
struct op;
struct mem;
struct def;
struct comment;
struct decl;
struct node;

void print_type(FILE *fp, const struct type *type);
void print_array(FILE *fp, const struct array *array);
void print_sloc(FILE *fp, const struct sloc *sloc);
void print_srange(FILE *fp, const struct srange *srange);
void print_ref(FILE *fp, const struct ref *ref);
void print_op(FILE *fp, const struct op *op);
void print_mem(FILE *fp, const struct mem *mem);
void print_def(FILE *fp, const struct def *def);
void print_comment(FILE *fp, const struct comment *comment);
void print_decl(FILE *fp, const struct decl *decl);
void print_node(FILE *fp, const struct node *node);