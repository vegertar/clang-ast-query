#include "options.h"
#include "string_set.h"
#include <stdint.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Little-endian platform is required"
#endif

#define grp_used_or_referenced _(used_or_referenced, used, referenced)
#define grp_value_kind _(value_kind, lvalue)
#define grp_object_kind _(object_kind, bitfield)
#define grp_class _(class, struct, union, enum)
#define grp_ifx _(ifx, if, ifdef, ifndef, elif, elifdef, elifndef)
#define grp_storage _(storage, extern, static)
#define grp_init_style _(init_style, cinit, callinit, listinit, parenlistinit)
#define grp_trait _(trait, alignof, sizeof)
#define grp_prefix_or_postfix _(prefix_or_postfix, prefix, postfix)
#define grp_stringified_or_paste _(stringified_or_paste, stringified, paste)

#define grp_operator                                                           \
  _(operator, Comma, Remainder, Division, Multiplication, Subtraction,         \
    Addition, BitwiseAND, BitwiseOR, BitwiseXOR, BitwiseNOT, LogicalAND,       \
    LogicalOR, LogicalNOT, GreaterThan, GreaterThanOrEqual, LessThan,          \
    LessThanOrEqual, Equality, Inequality, Assignment, AdditionAssignment,     \
    SubtractionAssignment, MultiplicationAssignment, DivisionAssignment,       \
    RemainderAssignment, BitwiseXORAssignment, BitwiseORAssignment,            \
    BitwiseANDAssignment, RightShift, RightShiftAssignment, LeftShift,         \
    LeftShiftAssignment, Decrement, Increment, Extension)

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

#define IS_NODE(...)                                                           \
  union {                                                                      \
    uint32_t node;                                                             \
    struct {                                                                   \
      uint32_t kind : KIND_WIDTH;                                              \
      uint32_t group : GROUP_WIDTH;                                            \
      uint32_t level : LEVEL_WIDTH;                                            \
    };                                                                         \
  }

#define OF_RAW(...) WITH_OPTIONS(__VA_ARGS__)

#define IS_RAW(...)                                                            \
  IS_NODE();                                                                   \
  union {                                                                      \
    RawSelf self;                                                              \
    struct {                                                                   \
      OF_RAW(__VA_ARGS__);                                                     \
    };                                                                         \
  }

#define OF_ATTR(...)                                                           \
  WITH_OPTIONS(Inherited, Implicit __VA_OPT__(, ) __VA_ARGS__);                \
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
               grp_used_or_referenced __VA_OPT__(, ) __VA_ARGS__);             \
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
  WITH_OPTIONS(sugar, imported __VA_OPT__(, ) __VA_ARGS__);                    \
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
      OF_STMT(grp_value_kind, grp_object_kind __VA_OPT__(, ) __VA_ARGS__);     \
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

#define IS_OPERATOR(...) IS_EXPR(grp_operator __VA_OPT__(, ) __VA_ARGS__)
#define IS_CAST_EXPR(...) IS_EXPR(grp_cast __VA_OPT__(, ) __VA_ARGS__)

#define OF_DIRECTIVE(...)                                                      \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer, prev;                                                     \
  AngledRange range;                                                           \
  Loc loc

#define IS_DIRECTIVE(...)                                                      \
  IS_NODE();                                                                   \
  union {                                                                      \
    DirectiveSelf self;                                                        \
    struct {                                                                   \
      OF_DIRECTIVE(__VA_ARGS__);                                               \
    };                                                                         \
  }

#define OF_PPDECL(...)                                                         \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_PPDECL(...)                                                         \
  IS_NODE();                                                                   \
  union {                                                                      \
    PPDeclSelf self;                                                           \
    struct {                                                                   \
      OF_PPDECL(__VA_ARGS__);                                                  \
    };                                                                         \
  }

#define OF_PPEXPR(...)                                                         \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_PPEXPR(...)                                                         \
  IS_NODE();                                                                   \
  union {                                                                      \
    PPExprSelf self;                                                           \
    struct {                                                                   \
      OF_PPEXPR(__VA_ARGS__);                                                  \
    };                                                                         \
  }

#define OF_PPOPERATOR(...)                                                     \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_PPOPERATOR(...)                                                     \
  IS_NODE();                                                                   \
  union {                                                                      \
    PPOperatorSelf self;                                                       \
    struct {                                                                   \
      OF_PPOPERATOR(__VA_ARGS__);                                              \
    };                                                                         \
  }

#define OF_PPSTMT(...)                                                         \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_PPSTMT(...)                                                         \
  IS_NODE();                                                                   \
  union {                                                                      \
    PPStmtSelf self;                                                           \
    struct {                                                                   \
      OF_PPSTMT(__VA_ARGS__);                                                  \
    };                                                                         \
  }

#define OF_EXPANSION(...)                                                      \
  WITH_OPTIONS(__VA_ARGS__);                                                   \
  uintptr_t pointer;                                                           \
  AngledRange range

#define IS_EXPANSION(...)                                                      \
  IS_NODE();                                                                   \
  union {                                                                      \
    ExpansionSelf self;                                                        \
    struct {                                                                   \
      OF_EXPANSION(__VA_ARGS__);                                               \
    };                                                                         \
  }

#define IS(X, Y, ...)                                                          \
  {                                                                            \
    IS_##X(__VA_ARGS__);                                                       \
    struct Y;                                                                  \
  }

#define Raw(X, Y, ...) struct IS(RAW, Y __VA_OPT__(, ) __VA_ARGS__) X
#define Attr(X, Y, ...) struct IS(ATTR, Y __VA_OPT__(, ) __VA_ARGS__) X##Attr
#define Comment(X, Y, ...)                                                     \
  struct IS(COMMENT, Y __VA_OPT__(, ) __VA_ARGS__) X##Comment
#define Decl(X, Y, ...) struct IS(DECL, Y __VA_OPT__(, ) __VA_ARGS__) X##Decl
#define Type(X, Y, ...) struct IS(TYPE, Y __VA_OPT__(, ) __VA_ARGS__) X##Type
#define Stmt(X, Y, ...) struct IS(STMT, Y __VA_OPT__(, ) __VA_ARGS__) X##Stmt
#define Expr(X, Y, ...) struct IS(EXPR, Y __VA_OPT__(, ) __VA_ARGS__) X##Expr
#define Literal(X, Y, ...)                                                     \
  struct IS(EXPR, Y __VA_OPT__(, ) __VA_ARGS__) X##Literal
#define Operator(X, Y, ...)                                                    \
  struct IS(OPERATOR, Y __VA_OPT__(, ) __VA_ARGS__) X##Operator
#define CastExpr(X, Y, ...)                                                    \
  struct IS(CAST_EXPR, Y __VA_OPT__(, ) __VA_ARGS__) X##CastExpr
#define Directive(X, Y, ...)                                                   \
  struct IS(DIRECTIVE, Y __VA_OPT__(, ) __VA_ARGS__) X##Directive
#define PPDecl(X, Y, ...)                                                      \
  struct IS(PPDECL, Y __VA_OPT__(, ) __VA_ARGS__) X##PPDecl
#define PPExpr(X, Y, ...)                                                      \
  struct IS(PPEXPR, Y __VA_OPT__(, ) __VA_ARGS__) X##PPExpr
#define PPOperator(X, Y, ...)                                                  \
  struct IS(PPOPERATOR, Y __VA_OPT__(, ) __VA_ARGS__) X##PPOperator
#define PPStmt(X, Y, ...)                                                      \
  struct IS(PPSTMT, Y __VA_OPT__(, ) __VA_ARGS__) X##PPStmt
#define Expansion(X, Y, ...)                                                   \
  struct IS(EXPANSION, Y __VA_OPT__(, ) __VA_ARGS__) X##Expansion

typedef struct {
  union {
    long long i;
    unsigned long long u;
  };
  bool negative;
} Integer;

typedef enum {
  NG_NULL,
  NG_IntValue,
  NG_Enum,
  NG_Typedef,
  NG_Record,
  NG_Field,
  NG_Attr,
  NG_Comment,
  NG_Decl,
  NG_Type,
  NG_Stmt,
  NG_Expr,
  NG_Literal,
  NG_Operator,
  NG_CastExpr,
  NG_Preprocessor,
  NG_Token,
  NG_Directive,
  NG_PPDecl,
  NG_PPExpr,
  NG_PPOperator,
  NG_PPStmt,
  NG_Expansion,
} NodeGroup;

typedef struct {
  const String *file;
  unsigned line;
  unsigned col;
} Loc;

typedef struct {
  Loc begin;
  Loc end;
} Range;

typedef struct {
  const String *qualified;
  const String *desugared;
} BareType;

typedef struct {
  const String *name;
  uintptr_t pointer;
} Ref;

typedef struct {
  const String *decl;
  Ref ref;
  BareType type;
} DeclRef;

typedef struct {
  bool anonymous;
  const String *name;
} MemberFace;

typedef struct {
  uint8_t dot : 1;
  uint8_t anonymous : 1;
  Ref ref;
} Member;

typedef unsigned ArgIndices;
typedef Range AngledRange;
typedef Ref Label;
typedef Ref Macro;

typedef struct {
  Macro macro;
  Loc loc;
} MacroRef;

typedef struct {
#pragma push_macro("OPTIONS_TYPE")
#define OPTIONS_TYPE uint32_t
  OF_RAW();
#pragma pop_macro("OPTIONS_TYPE")
} RawSelf;

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
  OF_DIRECTIVE();
} DirectiveSelf;

typedef struct {
  OF_PPDECL();
} PPDeclSelf;

typedef struct {
  OF_PPEXPR();
} PPExprSelf;

typedef struct {
  OF_PPOPERATOR();
} PPOperatorSelf;

typedef struct {
  OF_PPSTMT();
} PPStmtSelf;

typedef struct {
  OF_EXPANSION();
} ExpansionSelf;

typedef struct {
  union {
    struct {
      IS_NODE();
    };

#pragma push_macro("OPTIONS_TYPE")
#define OPTIONS_TYPE uint32_t
    Raw(IntValue, { Integer value; });
    Raw(Enum, {
      uintptr_t pointer;
      const String *name;
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
      const String *name;
      BareType type;
    });
    Raw(Preprocessor, { uintptr_t pointer; });
    Raw(
        Token,
        {
          Loc loc;
          const String *text;
          MacroRef ref;
        },
        is_arg, hasLeadingSpace, grp_stringified_or_paste);
#pragma pop_macro("OPTIONS_TYPE")

    Attr(Mode, { const String *name; });
    Attr(NoThrow, {});
    Attr(NonNull, { ArgIndices arg_indices; });
    Attr(
        AsmLabel, { const String *name; }, IsLiteralLabel);
    Attr(Deprecated, {
      const String *message;
      const String *replacement;
    });
    Attr(Builtin, { unsigned id; });
    Attr(ReturnsTwice, {});
    Attr(Const, {});
    Attr(Aligned, { const String *name; });
    Attr(Restrict, { const String *name; });
    Attr(Format, {
      const String *archetype;
      uint8_t string_index;
      uint8_t first_to_check;
    });
    Attr(GNUInline, {});
    Attr(AllocSize, {
      uint8_t position1;
      uint8_t position2;
    });
    Attr(WarnUnusedResult, {
      const String *name;
      const String *message;
    });
    Attr(AllocAlign, { uint8_t position; });
    Attr(TransparentUnion, {});
    Attr(Packed, {});
    Attr(Pure, {});

    Comment(Full, {});
    Comment(Paragraph, {});
    Comment(Text, { const String *text; });

    Decl(TranslationUnit, {});
    Decl(Typedef, {
      const String *name;
      BareType type;
    });
    Decl(
        Record, { const String *name; }, grp_class, definition);
    Decl(Field, {
      const String *name;
      BareType type;
    });
    Decl(
        Function,
        {
          const String *name;
          BareType type;
        },
        grp_storage, inline);
    Decl(ParmVar, {
      const String *name;
      BareType type;
    });
    Decl(IndirectField, {
      const String *name;
      BareType type;
    });
    Decl(Enum, { const String *name; });
    Decl(EnumConstant, {
      const String *name;
      BareType type;
    });
    Decl(
        Var,
        {
          const String *name;
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
    Type(FunctionProto, { const String *name; });
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
    Stmt(Label, { const String *name; });
    Stmt(Continue, {});
    Stmt(Break, {});
    Stmt(Do, {});

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
    Expr(Stmt, {});

    Literal(Integer, { Integer value; });
    Literal(Character, { char value; });
    Literal(String, { const String *value; });

    Operator(Unary, {}, grp_prefix_or_postfix, cannot_overflow);
    Operator(Binary, {});
    Operator(Conditional, {});
    Operator(CompoundAssign, {
      BareType computation_lhs_type;
      BareType computation_result_type;
    });

    CastExpr(CStyle, {});
    CastExpr(Implicit, {}, part_of_explicit_cast);

    Directive(Define, {});
    Directive(
        Inclusion,
        {
          const String *name;
          const String *file;
          const String *path;
        },
        angled);
    Directive(If, {}, grp_ifx, has_else);

    PPDecl(Macro, {
      const String *name;
      const String *parameters;
      const String *replacement;
    });

    PPExpr(
        Conditional, { uint8_t value; }, implicit);

    PPOperator(Defined, { Macro macro; });

    PPStmt(Compound, {});

    Expansion(
        Macro, { Macro macro; }, fast);
  };
} Node;

typedef struct {
  const String *kind;
  const String *name;
  Range range;
} Semantics;

// Exchanging information with the parser.
typedef struct {
  // Whether or not to emit error messages.
  uint8_t silent : 1;
  /**
   * The type of custom data:
   *   0: any opaque data
   *   1: FILE
   */
  uint8_t type : 1;
  // The current input line.
  const char *line;
  // The custom data.
  void *data;
} UserContext;

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
#undef Directive
#undef PPDecl
#undef PPExpr
#undef PPOperator
#undef PPStmt
#undef EXPANSION