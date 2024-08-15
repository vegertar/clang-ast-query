#include "options.h"
#include <stdint.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Little-endian platform is required"
#endif

#define grp_used_or_referenced _(used_or_referenced, used, referenced)
#define grp_value_kind _(value_kind, lvalue)
#define grp_object_kind _(object_kind, bitfield)
#define grp_class _(class, struct, union, enum)
#define grp_storage _(storage, extern, static)
#define grp_init_style _(init_style, cinit, callinit, listinit, parenlistinit)
#define grp_trait _(trait, alignof, sizeof)
#define grp_prefix_or_postfix _(prefix_or_postfix, prefix, postfix)

#define grp_operator                                                           \
  _(operator, Comma, Remainder, Division, Multiplication, Subtraction,         \
    Addition, BitwiseAND, BitwiseOR, BitwiseXOR, BitwiseNOT, LogicalAND,       \
    LogicalOR, LogicalNOT, GreaterThan, GreaterThanOrEqual, LessThan,          \
    LessThanOrEqual, Equality, Inequality, Assignment, AdditionAssignment,     \
    SubtractionAssignment, MultiplicationAssignment, DivisionAssignment,       \
    RemainderAssignment, BitwiseXORAssignment, BitwiseORAssignment,            \
    BitwiseANDAssignment, RightShift, RightShiftAssignment, LeftShift,         \
    LeftShiftAssignment, Decrement, Increment)

#define grp_cast                                                               \
  _(cast, IntegralCast, LValueToRValue, FunctionToPointerDecay,                \
    BuiltinFnToFnPtr, BitCast, NullToPointer, NoOp, ToVoid,                    \
    ArrayToPointerDecay, IntegralToFloating, IntegralToPointer)

#define grp_non_odr_use                                                        \
  _(non_odr_use, non_odr_use_unevaluated, non_odr_use_constant,                \
    non_odr_use_discarded)

#define ARG_INDICES_MAX (sizeof(ArgIndices) * 8)

#define LEVEL_WIDTH 8
#define GROUP_WIDTH 8
#define KIND_WIDTH 16

#define IS_NODE()                                                              \
  union {                                                                      \
    uint32_t node;                                                             \
    struct {                                                                   \
      uint32_t kind : KIND_WIDTH;                                              \
      uint32_t group : GROUP_WIDTH;                                            \
      uint32_t level : LEVEL_WIDTH;                                            \
    };                                                                         \
  }

#define OF_ATTR(...)                                                           \
  WITH_OPTIONS(Inherited, Implicit, ##__VA_ARGS__);                            \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_ATTR(...)                                                           \
  IS_NODE();                                                                   \
  union {                                                                      \
    AttrSelf self;                                                             \
    struct {                                                                   \
      OF_ATTR(__VA_ARGS__);                                                    \
    };                                                                         \
  }

#define OF_COMMENT(...)                                                        \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_COMMENT(...)                                                        \
  IS_NODE();                                                                   \
  union {                                                                      \
    CommentSelf self;                                                          \
    struct {                                                                   \
      OF_COMMENT(__VA_ARGS__);                                                 \
    };                                                                         \
  }

#define OF_DECL(...)                                                           \
  WITH_OPTIONS(imported, implicit, undeserialized_declarations,                \
               grp_used_or_referenced, ##__VA_ARGS__);                         \
  uintptr_t pointer, parent, prev;                                             \
  AngledRange range;                                                           \
  Loc loc

#define IS_DECL(...)                                                           \
  IS_NODE();                                                                   \
  union {                                                                      \
    DeclSelf self;                                                             \
    struct {                                                                   \
      OF_DECL(__VA_ARGS__);                                                    \
    };                                                                         \
  }

#define OF_TYPE(...)                                                           \
  WITH_OPTIONS(sugar, imported, ##__VA_ARGS__);                                \
  uintptr_t pointer;                                                           \
  BareType type

#define IS_TYPE(...)                                                           \
  IS_NODE();                                                                   \
  union {                                                                      \
    TypeSelf self;                                                             \
    struct {                                                                   \
      OF_TYPE(__VA_ARGS__);                                                    \
    };                                                                         \
  }

#define OF_STMT(...)                                                           \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_STMT(...)                                                           \
  IS_NODE();                                                                   \
  union {                                                                      \
    StmtSelf self;                                                             \
    struct {                                                                   \
      OF_STMT(__VA_ARGS__);                                                    \
    };                                                                         \
  }

#define OF_EXPR(...)                                                           \
  union {                                                                      \
    StmtSelf stmt;                                                             \
    struct {                                                                   \
      OF_STMT(grp_value_kind, grp_object_kind, ##__VA_ARGS__);                 \
    };                                                                         \
  };                                                                           \
  BareType type

#define IS_EXPR(...)                                                           \
  IS_NODE();                                                                   \
  union {                                                                      \
    ExprSelf self;                                                             \
    struct {                                                                   \
      OF_EXPR(__VA_ARGS__);                                                    \
    };                                                                         \
  }

#define IS_OPERATOR(...) IS_EXPR(grp_operator, ##__VA_ARGS__)
#define IS_CAST_EXPR(...) IS_EXPR(grp_cast, ##__VA_ARGS__)

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
    _Bool negative;                                                            \
  }

typedef struct INTEGER Integer;

typedef enum {
  NG_NULL,
  NG_DECL,
  NG_TYPE,
  NG_ATTR,
  NG_STMT,
  NG_EXPR,
  NG_LITERAL,
  NG_OPERATOR,
  NG_CAST_EXPR,
  NG_COMMENT,
} NodeGroup;

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
  const char *name;
  uintptr_t pointer;
} Label;

typedef struct {
  const char *decl;
  uintptr_t pointer;
  const char *name;
  BareType type;
} DeclRef;

typedef struct {
  _Bool anonymous;
  const char *name;
} MemberDecl;

typedef struct {
  uint8_t dot : 1;
  uint8_t anonymous : 1;
  const char *name;
  uintptr_t pointer;
} Member;

typedef unsigned ArgIndices;
typedef Range AngledRange;

typedef struct {
  OF_ATTR();
} AttrSelf;

typedef struct {
  OF_COMMENT();
} CommentSelf;

typedef struct {
  OF_DECL();
} DeclSelf;

typedef struct {
  OF_TYPE();
} TypeSelf;

typedef struct {
  OF_STMT();
} StmtSelf;

typedef struct {
  OF_EXPR();
} ExprSelf;

typedef struct {
  union {
    struct {
      IS_NODE();
    };

    Raw(IntValue, INTEGER);
    Raw(Enum, {
      uintptr_t pointer;
      const char *name;
    });
    Raw(Typedef, {
      uintptr_t pointer;
      BareType type;
    });
    Raw(Record, {
      uintptr_t pointer;
      BareType type;
    });
    Raw(Field, {
      uintptr_t pointer;
      const char *name;
      BareType type;
    });

    Attr(Mode, { const char *name; });
    Attr(NoThrow, {});
    Attr(NonNull, { ArgIndices arg_indices; });
    Attr(
        AsmLabel, { const char *name; }, IsLiteralLabel);
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
      const char *archetype;
      uint16_t string_index : 5;
      uint16_t first_to_check : 5;
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
        Record, { const char *name; }, grp_class, definition);
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
        grp_storage, inline);
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
        Var,
        {
          const char *name;
          BareType type;
        },
        grp_storage, grp_init_style);

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
    Stmt(Goto, { Label label; });
    Stmt(Switch, {});
    Stmt(Case, {});
    Stmt(Default, {});
    Stmt(Label, { const char *name; });
    Stmt(Continue, {});
    Stmt(Break, {});

    Expr(Paren, {});
    Expr(
        DeclRef, { DeclRef ref; }, grp_non_odr_use);
    Expr(Constant, {});
    Expr(Call, {});
    Expr(Member, { Member member; });
    Expr(ArraySubscript, {});
    Expr(InitList, {});
    Expr(OffsetOf, {});
    Expr(
        UnaryExprOrTypeTrait, { BareType argument_type; }, grp_trait);

    Literal(Integer, INTEGER);
    Literal(Character, { char c; });
    Literal(String, { const char *s; });

    Operator(Unary, {}, grp_prefix_or_postfix, cannot_overflow);
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