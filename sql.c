#include "parse.h"
#include <stdio.h>
#include <assert.h>

#ifndef MAX_AST_LEVEL
#define MAX_AST_LEVEL 255
#endif

static void dump_node(const struct node *node) {
  switch (node->kind) {
  case NODE_KIND_HEAD:
    if (node->loc.file) {
      printf("INSERT OR IGNORE INTO ast "
             "(kind, id, file, line, col)\n"
             "\tVALUES ('%s', '%s', '%s', %u, %u);\n",
             node->name, node->pointer, node->loc.file, node->loc.line,
             node->loc.col);
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
           "\tSET name = '%s',\n"
           "\t    type = '%s'\n"
           "\tWHERE id = '%s';\n",
           decl->variants.v9.name, decl->variants.v9.def.type.qualified,
           pointer);
    break;
  default:
    break;
  }
}

static void dump_ast(const struct ast *ast, int max_level) {
  printf("CREATE TABLE ast (\n"
         "\tkind TEXT,\n"
         "\tid TEXT PRIMARY KEY,\n"
         "\tfile TEXT,\n"
         "\tline INTEGER,\n"
         "\tcol INTEGER,\n"
         "\tname TEXT,\n"
         "\ttype TEXT\n"
         ");\n");
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    if (max_level == 0 || node->level <= max_level) {
      dump_node(node);
      dump_decl(&node->decl, node->pointer);
    }
  }
}

static void dump_hierarchy(const struct ast *ast, int max_level) {
  printf("CREATE TABLE hierarchy (\n"
         "\tid TEXT PRIMARY KEY,\n"
         "\tparent TEXT\n"
         ");\n");
  const char *parents[MAX_AST_LEVEL + 1] = {};
  for (unsigned i = 0; i < ast->i; ++i) {
    const struct node *node = &ast->data[i];
    if (max_level > 0 && node->level > max_level) {
      continue;
    }

    switch (node->kind) {
    case NODE_KIND_HEAD:
      assert(node->level < MAX_AST_LEVEL);

      printf("INSERT OR IGNORE INTO hierarchy (id, parent)\n"
             "\tVALUES ('%s', '%s');\n",
             node->pointer, parents[node->level]);

      if (node->level + 1 < MAX_AST_LEVEL) {
        parents[node->level + 1] = node->pointer;
      }
      break;
    case NODE_KIND_ENUM:
      break;
    case NODE_KIND_NULL:
      break;
    }
  }
}

void dump(int max_level) {
  printf("BEGIN;\n");
  dump_ast(&ast, max_level);
  dump_hierarchy(&ast, max_level);
  printf("COMMIT;\n");
}