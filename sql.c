#include "parse.h"
#include <stdio.h>

void dump_node(const struct node *node) {
  switch (node->kind) {
  case NODE_KIND_HEAD:
    if (node->loc.file) {
      printf("INSERT INTO ast "
             "(indent, head$name, head$pointer, loc$file, loc$line, loc$col)\n"
             "\tVALUES (%d, '%s', '%s', '%s', %u, %u);\n",
             node->indent, node->name, node->pointer, node->loc.file,
             node->loc.line, node->loc.col);
    }
    break;
  case NODE_KIND_ENUM:
    break;
  case NODE_KIND_NULL:
    break;
  }
}

static void dump_decl(const struct decl *decl, const char *pointer) {
  switch (decl->kind) {
  case DECL_KIND_V9:
    printf("UPDATE ast\n"
           "\tSET decl$name = '%s'\n"
           "\tWHERE head$pointer = '%s';\n",
           decl->variants.v9.name, pointer);
    break;
  default:
    break;
  }
}

static void dump_ast(const struct ast *ast) {
  printf("CREATE TABLE ast (\n"
         "\tindent INTEGER,\n"
         "\thead$name TEXT,\n"
         "\thead$pointer TEXT PRIMARY KEY,\n"
         "\tloc$file TEXT,\n"
         "\tloc$line INTEGER,\n"
         "\tloc$col INTEGER,\n"
         "\tdecl$name TEXT\n"
         ");\n");
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    dump_node(node);
    dump_decl(&node->decl, node->pointer);
  }
}

void dump() { dump_ast(&ast); }