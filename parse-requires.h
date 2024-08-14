#include "parse-requires-inl.h"
#include <stdint.h>

enum group {
  GROUP_NULL,
  GROUP_DECL,
  GROUP_TYPE,
  GROUP_ATTR,
  GROUP_STMT,
  GROUP_EXPR,
  GROUP_LITERAL,
  GROUP_OPERATOR,
  GROUP_CAST_EXPR,
  GROUP_COMMENT,
};

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

typedef struct {
  const char *decl;
  intptr_t pointer;
  const char *name;
  BareType type;
} DeclRef;

typedef struct {
  intptr_t pointer;
} Member;

typedef unsigned ArgIndices;
typedef Range AngledRange;

#define ARG_INDICES_MAX (sizeof(ArgIndices) * 8)

#define LEVEL_WIDTH 8
#define GROUP_WIDTH 8
#define KIND_WIDTH 16

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Little-endian platform is required"
#endif

#define IS_NODE()                                                              \
  union {                                                                      \
    uint32_t node;                                                             \
    struct {                                                                   \
      uint32_t kind : KIND_WIDTH;                                              \
      uint32_t group : GROUP_WIDTH;                                            \
      uint32_t level : LEVEL_WIDTH;                                            \
    };                                                                         \
  }

#define WITH_OPTIONS(...)                                                      \
  union {                                                                      \
    uint64_t options;                                                          \
    struct {                                                                   \
      OPTIONS(__VA_ARGS__)                                                     \
    };                                                                         \
    struct {                                                                   \
      GROUPS(__VA_ARGS__)                                                      \
    };                                                                         \
  }

#define IS_ATTR(...)                                                           \
  IS_NODE();                                                                   \
  WITH_OPTIONS(inherited, implicit, ##__VA_ARGS__);                            \
  intptr_t pointer;                                                            \
  AngledRange range

#define IS_COMMENT(...)                                                        \
  IS_NODE();                                                                   \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  intptr_t pointer;                                                            \
  AngledRange range

#define IS_DECL(...)                                                           \
  IS_NODE();                                                                   \
  WITH_OPTIONS(imported, implicit, is_undeserialized_declarations,             \
               _(used_or_referenced, used, referenced), ##__VA_ARGS__);        \
  intptr_t pointer, parent, prev;                                              \
  AngledRange range;                                                           \
  Loc loc

#define IS_TYPE(...)                                                           \
  IS_NODE();                                                                   \
  WITH_OPTIONS(sugar, imported, ##__VA_ARGS__);                                \
  intptr_t pointer;                                                            \
  BareType type

#define IS_STMT(...)                                                           \
  IS_NODE();                                                                   \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  intptr_t pointer;                                                            \
  AngledRange range

#define IS_EXPR(...)                                                           \
  IS_STMT(_(value_kind, lvalue), _(object_kind, bitfield), ##__VA_ARGS__);     \
  BareType type

#define IS_OPERATOR(...)                                                       \
  IS_EXPR(_(operator, Comma, Remainder, Division, Multiplication, Subtraction, \
            Addition, BitwiseAND, BitwiseOR, BitwiseXOR, BitwiseNOT,           \
            LogicalAND, LogicalOR, LogicalNOT, GreaterThan,                    \
            GreaterThanOrEqual, LessThan, LessThanOrEqual, Equality,           \
            Inequality, Assignment, AdditionAssignment, SubtractionAssignment, \
            MultiplicationAssignment, DivisionAssignment, RemainderAssignment, \
            BitwiseXORAssignment, BitwiseORAssignment, BitwiseANDAssignment,   \
            RightShift, RightShiftAssignment, LeftShift, LeftShiftAssignment,  \
            Decrement, Increment),                                             \
          ##__VA_ARGS__)

#define IS_CAST_EXPR(...)                                                      \
  IS_EXPR(_(cast, IntegralCast, LValueToRValue, FunctionToPointerDecay,        \
            BuiltinFnToFnPtr, BitCast, NullToPointer, NoOp, ToVoid,            \
            ArrayToPointerDecay, IntegralToFloating, IntegralToPointer),       \
          ##__VA_ARGS__)

#define IS(X, Y, ...)                                                          \
  {                                                                            \
    IS_##X(__VA_ARGS__);                                                       \
    struct Y;                                                                  \
  }

#define Raw(X, Y, ...) struct IS(NODE, Y, ##__VA_ARGS__) X
#define Attr(X, Y, ...) struct IS(ATTR, Y, ##__VA_ARGS__) X##Attr
#define Comment(X, Y, ...) struct IS(COMMENT, Y, ##__VA_ARGS__) X##Comment
#define Decl(X, Y, ...) struct IS(DECL, Y, ##__VA_ARGS__) X##Decl
#define Type(X, Y, ...) struct IS(TYPE, Y, ##__VA_ARGS__) X##Type
#define Stmt(X, Y, ...) struct IS(STMT, Y, ##__VA_ARGS__) X##Stmt
#define Expr(X, Y, ...) struct IS(EXPR, Y, ##__VA_ARGS__) X##Expr
#define Literal(X, Y, ...) struct IS(EXPR, Y, ##__VA_ARGS__) X##Literal
#define Operator(X, Y, ...) struct IS(OPERATOR, Y, ##__VA_ARGS__) X##Operator
#define CastExpr(X, Y, ...) struct IS(CAST_EXPR, Y, ##__VA_ARGS__) X##CastExpr

#define INTEGER                                                                \
  {                                                                            \
    union {                                                                    \
      long long i;                                                             \
      unsigned long long u;                                                    \
    };                                                                         \
  }

typedef struct {
  IS_DECL();
} DeclNode;

typedef struct {
  union {
    struct {
      IS_NODE();
    };

    Raw(IntValue, INTEGER);
    Raw(Enum, {
      intptr_t pointer;
      const char *name;
    });
    Raw(Typedef, {
      intptr_t pointer;
      BareType type;
    });
    Raw(Record, {
      intptr_t pointer;
      BareType type;
    });
    Raw(Field, {
      intptr_t pointer;
      const char *name;
      BareType type;
    });

    Attr(Mode, { const char *name; });
    Attr(NoThrow, {});
    Attr(NonNull, { ArgIndices arg_indices; });
    Attr(
        AsmLabel, { const char *name; }, literal_label);
    Attr(Deprecated, {
      const char *message;
      const char *replacement;
    });
    Attr(Builtin, { unsigned id; });
    Attr(ReturnsTwice, {});
    Attr(Const, {});
    Attr(Aligned, { const char *name; });
    Attr(Restrict, { const char *name; });
    Attr(Format, {
      uint16_t string_index : 5;
      uint16_t first_to_check : 5;
      const char *archetype;
    });
    Attr(GNUInline, {});
    Attr(AllocSize, {
      uint16_t position1 : 5;
      uint16_t position2 : 5;
    });
    Attr(WarnUnusedResult, {
      const char *name;
      const char *message;
    });
    Attr(AllocAlign, { uint16_t position : 5; });
    Attr(TransparentUnion, {});
    Attr(Packed, {});
    Attr(Pure, {});

    Comment(Full, {});
    Comment(Paragraph, {});
    Comment(Text, { const char *text; });

    Decl(TranslationUnit, {});
    Decl(Typedef, {
      const char *name;
      BareType type;
    });
    Decl(
        Record, { const char *name; }, _(class, struct, union, enum),
        definition);
    Decl(Field, {
      const char *name;
      BareType type;
    });
    Decl(
        Function,
        {
          const char *name;
          BareType type;
        },
        _(storage, extern, static), inline);
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
    Decl(
        VarDecl,
        {
          const char *name;
          BareType type;
        },
        _(storage, extern, static),
        _(init_style, cinit, callinit, listinit, parenlistinit));

    Type(Builtin, {});
    Type(Record, {});
    Type(Pointer, {});
    Type(ConstantArray, { uint64_t size; });
    Type(Elaborated, {});
    Type(Typedef, {});
    Type(Qual, {}, const, volatile);
    Type(Enum, {});
    Type(FunctionProto, { const char *name; });
    Type(Paren, {});

    Stmt(Compound, {});
    Stmt(Return, {});
    Stmt(Decl, {});
    Stmt(While, {});
    Stmt(If, {}, has_else);
    Stmt(For, {});
    Stmt(Null, {});
    Stmt(Goto, {});
    Stmt(Switch, {});
    Stmt(Case, {});
    Stmt(Default, {});
    Stmt(Label, {});
    Stmt(Continue, {});
    Stmt(Break, {});

    Expr(Paren, {});
    Expr(
        DeclRef, { DeclRef ref; },
        _(non_odr_use, non_odr_use_unevaluated, non_odr_use_constant,
          non_odr_use_discarded));
    Expr(ConstantExpr, {});
    Expr(Call, {});
    Expr(Member, { Member member; });
    Expr(ArraySubscript, {});
    Expr(InitList, {});
    Expr(OffsetOf, {});
    Expr(
        UnaryExprOrTypeTrait, { BareType argument_type; },
        _(trait, alignof, sizeof));

    Literal(Integer, INTEGER);
    Literal(Character, { char c; });
    Literal(String, { const char *s; });

    Operator(Unary, {}, _(prefix_postfix, prefix, postfix), cannot_overflow);
    Operator(Binary, {});
    Operator(Conditional, {});
    Operator(CompoundAssign, {
      BareType computation_lhs_type;
      BareType computation_result_type;
    });

    CastExpr(CStyle, {});
    CastExpr(Implicit, {}, part_of_explicit_cast);
  };
} Node;

#undef Raw
#undef Attr
#undef Comment
#undef Decl
#undef Type
#undef Stmt
#undef Expr
#undef Literal
#undef Operator
#undef CastExpr