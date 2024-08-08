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

#define Inclusive(...) OPTIONS(__VA_ARGS__)
#define Exclusive(...) OPTIONS(__VA_ARGS__)

#define OPTIONS_I1(n) OPTIONS_I2(n)
#define OPTIONS_I2(n) OPTIONS##n
#define OPTIONS(...) OPTIONS_I1(PP_NARG(__VA_ARGS__))(__VA_ARGS__)

#define OPTIONS1(a, ...) uint32_t opt_##a : 1
#include "options.h"

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

#define IS_TYPE                                                                \
  IS_BASE                                                                      \
                                                                               \
  BareType type;                                                               \
  Inclusive(sugar, imported);

#define IS_STMT                                                                \
  IS_BASE                                                                      \
                                                                               \
  AngledRange range;

#define IS_EXPR                                                                \
  IS_STMT                                                                      \
                                                                               \
  BareType type;                                                               \
  Inclusive(lvalue, bitfield);

#define IS_OPERATOR                                                            \
  IS_EXPR                                                                      \
                                                                               \
  Exclusive(Comma, Remainder, Division, Multiplication, Subtraction, Addition, \
            BitwiseAND, BitwiseOR, BitwiseXOR, BitwiseNOT, LogicalAND,         \
            LogicalOR, LogicalNOT, GreaterThan, GreaterThanOrEqual, LessThan,  \
            LessThanOrEqual, Equality, Inequality, Assignment,                 \
            AdditionAssignment, SubtractionAssignment,                         \
            MultiplicationAssignment, DivisionAssignment, RemainderAssignment, \
            BitwiseXORAssignment, BitwiseORAssignment, BitwiseANDAssignment,   \
            RightShift, RightShiftAssignment, LeftShift, LeftShiftAssignment,  \
            Decrement, Increment);

#define IS_CAST_EXPR                                                           \
  IS_EXPR                                                                      \
                                                                               \
  Inclusive(IntegralCast, LValueToRValue, FunctionToPointerDecay,              \
            BuiltinFnToFnPtr, BitCast, NullToPointer, NoOp, ToVoid,            \
            ArrayToPointerDecay, IntegralToFloating, IntegralToPointer);

#define Attr(X, ...) struct IS(ATTR, ##__VA_ARGS__) X##Attr
#define Comment(X, ...) struct IS(COMMENT, ##__VA_ARGS__) X##Comment
#define Decl(X, ...) struct IS(DECL, ##__VA_ARGS__) X##Decl
#define Type(X, ...) struct IS(TYPE, ##__VA_ARGS__) X##Type
#define Stmt(X, ...) struct IS(STMT, ##__VA_ARGS__) X##Stmt
#define Expr(X, ...) struct IS(EXPR, ##__VA_ARGS__) X##Expr
#define Literal(X, ...) struct IS(EXPR, ##__VA_ARGS__) X##Literal
#define Operator(X, ...) struct IS(OPERATOR, ##__VA_ARGS__) X##Operator
#define CastExpr(X, ...) struct IS(CAST_EXPR, ##__VA_ARGS__) X##CastExpr

struct Node {
  union {
    struct {
      IS_BASE
    };

    union {
      long long i;
      unsigned long long u;
    } IntValue;

    struct {
      intptr_t pointer;
      BareType type;
    } Record, Typedef;

    struct {
      intptr_t pointer;
      const char *name;
      BareType type;
    } Field;

    struct {
      intptr_t pointer;
      const char *name;
    } Enum;

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

    Type(Builtin);
    Type(Record);
    Type(Pointer);
    Type(ConstantArray, { uint64_t size; });
    Type(Elaborated);
    Type(Typedef);
    Type(Qual, { Inclusive(const, volatile); });
    Type(Enum);
    Type(FunctionProto, { const char *name; });
    Type(Paren);

    Stmt(Compound);
    Stmt(Return);
    Stmt(Decl);
    Stmt(While);
    Stmt(If, { Inclusive(has_else); });
    Stmt(For);
    Stmt(Null);
    Stmt(Goto);
    Stmt(Switch);
    Stmt(Case);
    Stmt(Default);
    Stmt(Label);
    Stmt(Continue);
    Stmt(Break);

    Expr(Paren);
    Expr(DeclRef, {
      Exclusive(non_odr_use_unevaluated, non_odr_use_constant,
                non_odr_use_discarded);
      DeclRef ref;
    });
    Expr(ConstantExpr);
    Expr(Call);
    Expr(Member, { Member member; });
    Expr(ArraySubscript);
    Expr(InitList);
    Expr(OffsetOf);
    Expr(UnaryExprOrTypeTrait, {
      Exclusive(alignof, sizeof);
      BareType argument_type;
    });

    Literal(Integer, {
      union {
        long long i;
        unsigned long long u;
      };
    });
    Literal(Character, { char c; });
    Literal(String, { const char *s; });

    Operator(Unary, {
      Exclusive(prefix, postfix);
      Inclusive(cannot_overflow);
    });
    Operator(Binary);
    Operator(Conditional);
    Operator(CompoundAssign, {
      BareType computation_lhs_type;
      BareType computation_result_type;
    });

    CastExpr(CStyle);
    CastExpr(Implicit, { Inclusive(part_of_explicit_cast); });
  };
};

#undef Attr
#undef Comment
#undef Decl
#undef Type
#undef Stmt
#undef Expr
#undef Literal
#undef Operator
#undef CastExpr