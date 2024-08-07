#pragma once

#include "pp.h"
#include <stdint.h>

typedef struct {
  const char *file;
  unsigned line;
  unsigned col;
} Loc;

typedef struct {
  Loc begin;
  Loc end;
} Range;

typedef struct {
  const char *qualified;
  const char *desugared;
} BareType;

typedef unsigned ArgIndices;
typedef Range AngledRange;

#define ARG_INDICES_MAX (sizeof(ArgIndices) * 8)

#define Options_I1(n) Options_I2(n)
#define Options_I2(n) Options##n
#define Options(...) Options_I1(PP_NARG(__VA_ARGS__))(__VA_ARGS__)

#define Inclusive(...) Options(__VA_ARGS__)
#define Exclusive(...) Options(__VA_ARGS__)

#define Options1(a, ...) uint32_t is_##a : 1

#define Options2(a, ...)                                                       \
  Options1(a);                                                                 \
  Options1(__VA_ARGS__)

#define Options3(a, ...)                                                       \
  Options1(a);                                                                 \
  Options2(__VA_ARGS__)

#define Options4(a, ...)                                                       \
  Options1(a);                                                                 \
  Options3(__VA_ARGS__)

#define IS_WITHOUT_BODY(base)                                                  \
  { IS_##base }
#define IS_WITH_BODY(base, body)                                               \
  { IS_##base struct body; }

#define IS_CALL(arg1, arg2, arg3, ...) arg3
#define IS_DISPATCH(...) IS_CALL(__VA_ARGS__, IS_WITH_BODY, IS_WITHOUT_BODY)
#define IS(...) IS_DISPATCH(__VA_ARGS__)(__VA_ARGS__)

#define IS_BASE                                                                \
  uint32_t level : 8;                                                          \
  uint32_t group : 8;                                                          \
  uint32_t kind : 16;                                                          \
  intptr_t pointer;

#define IS_ATTR                                                                \
  IS_BASE                                                                      \
                                                                               \
  AngledRange range;                                                           \
  Inclusive(inherited, implicit);

#define IS_COMMENT                                                             \
  IS_BASE                                                                      \
                                                                               \
  AngledRange range;

#define IS_DECL                                                                \
  IS_BASE                                                                      \
                                                                               \
  const char *parent;                                                          \
  const char *prev;                                                            \
  AngledRange range;                                                           \
  Loc loc;                                                                     \
  Inclusive(imported, implicit, is_undeserialized_declarations);               \
  Exclusive(used, referenced);

#define Attr(X, ...) struct X##Attr IS(ATTR, ##__VA_ARGS__) X##Attr
#define Comment(X, ...) struct X##Comment IS(COMMENT, ##__VA_ARGS__) X##Comment
#define Decl(X, ...) struct X##Decl IS(DECL, ##__VA_ARGS__) X##Decl

struct Node {
  union {
    struct {
      IS_BASE
    };

    Attr(Mode, { const char *name; });
    Attr(NoThrow);
    Attr(NonNull, { ArgIndices arg_indices; });
    Attr(AsmLabel, {
      Inclusive(literal_label);
      const char *name;
    });
    Attr(Deprecated, {
      const char *message;
      const char *replacement;
    });
    Attr(Builtin, { unsigned id; });
    Attr(ReturnsTwice);
    Attr(Const);
    Attr(Aligned, { const char *name; });
    Attr(Restrict, { const char *name; });
    Attr(Format, {
      uint32_t string_index : 5;
      uint32_t first_to_check : 5;
      const char *archetype;
    });
    Attr(GNUInline);
    Attr(AllocSize, {
      uint32_t position1 : 5;
      uint32_t position2 : 5;
    });
    Attr(WarnUnusedResult, {
      const char *name;
      const char *message;
    });
    Attr(AllocAlign, { uint32_t position : 5; });
    Attr(TransparentUnion);
    Attr(Packed);
    Attr(Pure);

    Comment(Full);
    Comment(Paragraph);
    Comment(Text, { const char *text; });

    Decl(TranslationUnit);
    Decl(Typedef, {
      const char *name;
      BareType type;
    });
    Decl(Record, {
      Exclusive(struct, union, enum);
      Inclusive(definition);
      const char *name;
    });
    Decl(Field, {
      const char *name;
      BareType type;
    });
    Decl(Function, {
      Exclusive(extern, static);
      Inclusive(inline);
      const char *name;
      BareType type;
    });
    Decl(ParmVar, {
      const char *name;
      BareType type;
    });
    Decl(IndirectField, {
      const char *name;
      BareType type;
    });
    Decl(Enum, { const char *name; });
    Decl(EnumConstant, {
      const char *name;
      BareType type;
    });
    Decl(VarDecl, {
      Exclusive(extern, static);
      Exclusive(cinit, callinit, listinit, parenlistinit);
      const char *name;
      BareType type;
    });
  };
};
