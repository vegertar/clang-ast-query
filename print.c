#include "print.h"
#include "parse.h"

void print_type(FILE *fp, const struct type *type) {
  if (type->qualified) {
    fprintf(fp, "'%s'", type->qualified);
    if (type->desugared) {
      fprintf(fp, ":'%s'", type->desugared);
    }
  }
}

void print_array(FILE *fp, const struct array *array) {
  for (unsigned i = 0; i < array->i; ++i) {
    fprintf(fp, "%s", (const char *)array->data[i]);
    if (i < array->i - 1) {
      fprintf(fp, " ");
    }
  }
}

void print_sloc(FILE *fp, const struct sloc *sloc) {
  if (sloc->file || sloc->line || sloc->col) {
    fprintf(fp, "%s:%u:%u", sloc->file, sloc->line, sloc->col);
  }
}

void print_srange(FILE *fp, const struct srange *srange) {
  print_sloc(fp, &srange->begin);
  if (srange->end.file || srange->end.line || srange->end.col) {
    fprintf(fp, ", ");
    print_sloc(fp, &srange->end);
  }
}

void print_ref(FILE *fp, const struct ref *ref) {
  if (ref->pointer) {
    fprintf(fp, "%s %s", ref->name, ref->pointer);
    if (ref->type.qualified) {
      fprintf(fp, " ");
      print_type(fp, &ref->type);
    }
  }
}

void print_op(FILE *fp, const struct op *op) {
  if (op->operator) {
    fprintf(fp, "'%s'", op->operator);
    for (unsigned i = 0; i < op->tags.i; ++i) {
      fprintf(fp, " %s=", (const char *)op->tags.data[i].name);
      print_type(fp, &op->tags.data[i].type);
    }
  }
}

void print_mem(FILE *fp, const struct mem *mem) {
  if (mem->kind) {
    fprintf(fp, mem->kind == MEM_KIND_ARROW ? "->" : ".");
    fprintf(fp, "%s %s", mem->name, mem->pointer);
  }
}

void print_def(FILE *fp, const struct def *def) {
  fprintf(fp, "type(");
  print_type(fp, &def->type);
  fprintf(fp, ")");

  fprintf(fp, " specs(");
  print_array(fp, &def->specs);
  fprintf(fp, ")");

  fprintf(fp, " value(%s)", def->value ? def->value : "");

  fprintf(fp, " op(");
  print_op(fp, &def->op);
  fprintf(fp, ")");

  fprintf(fp, " mem(");
  print_mem(fp, &def->mem);
  fprintf(fp, ")");

  if (def->field) {
    fprintf(fp, " field");
  }

  fprintf(fp, " ref(");
  print_ref(fp, &def->ref);
  fprintf(fp, ")");

  fprintf(fp, " cast(%s)", def->cast ? def->cast : "");
}

void print_decl(FILE *fp, const struct decl *decl) {
  fprintf(fp, "<%d>", decl->kind);
  switch (decl->kind) {
  case DECL_KIND_NIL:
    break;
  case DECL_KIND_V1:
    fprintf(fp, " class(%s)", decl->variants.v1.class);
    break;
  case DECL_KIND_V2:
    fprintf(fp, " name(%s)", decl->variants.v2.name);
    break;
  case DECL_KIND_V3:
    fprintf(fp, " class(%s) name(%s)", decl->variants.v3.class,
            decl->variants.v3.name);
    break;
  case DECL_KIND_V4:
    fprintf(fp, " sqname(%s) pointer(%s)", decl->variants.v4.sqname,
            decl->variants.v4.pointer);
    break;
  case DECL_KIND_V5:
    fprintf(fp, " sqname(%s) trait(%s)", decl->variants.v5.sqname,
            decl->variants.v5.trait);
    break;
  case DECL_KIND_V6:
    fprintf(fp, " sqname(%s) trait(%s) def(", decl->variants.v6.sqname,
            decl->variants.v6.trait);
    print_def(fp, &decl->variants.v6.def);
    fprintf(fp, ")");
    break;
  case DECL_KIND_V7:
    fprintf(fp, " sqname(%s) def(", decl->variants.v7.sqname);
    print_def(fp, &decl->variants.v7.def);
    fprintf(fp, ")");
    break;
  case DECL_KIND_V8:
    fprintf(fp, " def(");
    print_def(fp, &decl->variants.v8.def);
    fprintf(fp, ")");
    break;
  case DECL_KIND_V9:
    fprintf(fp, " name(%s) def(", decl->variants.v9.name);
    print_def(fp, &decl->variants.v9.def);
    fprintf(fp, ")");
    break;
  case DECL_KIND_V10:
    fprintf(fp, " seq(");
    print_array(fp, &decl->variants.v10.seq);
    fprintf(fp, ")");
    break;
  case DECL_KIND_V11:
    fprintf(fp, " name(%s) seq(", decl->variants.v11.name);
    print_array(fp, &decl->variants.v11.seq);
    fprintf(fp, ")");
    break;
  case DECL_KIND_V12:
    fprintf(fp, " comment(%s)", decl->variants.v12.comment);
    break;
  }
}

void print_node(FILE *fp, const struct node *node) {
  fprintf(fp, "<%d>", node->kind);
  switch (node->kind) {
  case NODE_KIND_HEAD:
    fprintf(fp, "name(%s) pointer(%s) parent(%s) prev(%s)", node->name,
            node->pointer, node->parent, node->prev);
    fprintf(fp, " range(");
    print_srange(fp, &node->range);
    fprintf(fp, ") loc(");
    print_sloc(fp, &node->loc);
    fprintf(fp, ") attrs(");
    print_array(fp, &node->attrs);
    fprintf(fp, ") labels(");
    print_array(fp, &node->labels);
    fprintf(fp, ") decl(");
    print_decl(fp, &node->decl);
    fprintf(fp, ") opts(");
    print_array(fp, &node->opts);
    fprintf(fp, ")");
    break;
  case NODE_KIND_ENUM:
    fprintf(fp, " name(%s)", node->name);
    break;
  case NODE_KIND_NULL:
    break;
  case NODE_KIND_TOKEN:
    fprintf(fp, " name(%s)", node->name);
    fprintf(fp, " range(");
    print_srange(fp, &node->range);
    fprintf(fp, ") loc(");
    print_sloc(fp, &node->loc);
    fprintf(fp, ") attrs(");
    print_array(fp, &node->attrs);
    fprintf(fp, ")");
    break;
  }
}
