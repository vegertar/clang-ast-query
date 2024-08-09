/* Disable Flex features we don't need, to avoid warnings. */
%option nodefault noinput nounput noyywrap

%{
#include "parse.h"
#include <assert.h>

// Each time a rule is matched, advance the end cursor/position.
#define YY_USER_ACTION                                                        \
  yylloc->last_column += (int)yyleng;

// Move the first position onto the last.
#define LOCATION_STEP()                                                       \
  do {                                                                        \
    yylloc->first_line = yylloc->last_line;                                   \
    yylloc->first_column = yylloc->last_column;                               \
  } while (0)

#define LINE_STEP()                                                           \
  do {                                                                        \
    yylloc->last_line++;                                                      \
    yylloc->last_column = 1;                                                  \
    return TOK_EOL;                                                           \
  } while (0)

#define REMARK_KV(tok)                                                        \
  do {                                                                        \
    char k[32];                                                               \
    char v[32];                                                               \
    int n = sscanf(yytext,                                                    \
                   tok == TOK_REMARK_VAR_TYPE                                 \
                       ? "#VAR-TYPE:%s %s"                                    \
                       : tok == TOK_REMARK_DECL_DEF ? "#DECL-DEF:%s %s" : "", \
                   k, v);                                                     \
    if (n != 2)                                                               \
      return TOK_YYUNDEF;                                                     \
    struct string_map *map =                                                  \
        tok == TOK_REMARK_VAR_TYPE                                            \
            ? &var_type_map                                                   \
            : tok == TOK_REMARK_DECL_DEF ? &decl_def_map : NULL;              \
    assert(map);                                                              \
    yylval->tok = add_string_map(map, strdup(k), strdup(v));                  \
    return tok;                                                               \
  } while (0)

#define OPT(X, ...) VAL(OPT_##X)
#define VAL(X) SET(X, TOK_##X)

#define SET(X, value)                                                         \
  do {                                                                        \
    yylval->TOK_##X = value;                                                  \
    return TOK_##X;                                                           \
  } while (0)

#define ATOI(X, base)                                                         \
  do {                                                                        \
    char *end;                                                                \
    errno = 0;                                                                \
    _Bool is_negative = yytext[0] == '-';                                     \
    yylval->TOK_##X = is_negative                                             \
          ? strtoll(yytext, &end, base)                                       \
          : strtoul(yytext, &end, base);                                      \
    if (errno || *end) return TOK_YYUNDEF;                                    \
    return TOK_##X;                                                           \
  } while (0)

static inline char * qname() {
  return strndup(yytext + 1, yyleng - 2);
}

%}

INDENT ^[ \|`]+-

NAME ([a-zA-Z_][a-zA-Z_0-9]*)

ANAME ({CLASS}" "{NAME}::\("anonymous at ".+\))

INTEGER (-?([0-9]+))

POINTER (0x[0-9a-f]+)

CLASS (struct|union|enum)

INC_DEC ("'--'"|"'++'")

UNARY_PREFIX ({INC_DEC}|"'-'"|"'+'"|"'*'"|"'&'"|"'~'"|"'!'")

SRC ([^ <:]+|"<scratch space>"|"<command line>"|"<built-in>")

SQNAME ('((?:[^'\\]|\\.)*)')

DQNAME (\"((?:[^"\\]|\\.)*)\")

%%

%{
  // Each time yylex is called, move the head position to the end one.
  LOCATION_STEP();
%}

TranslationUnitDecl             VAL(TranslationUnitDecl);
IndirectFieldDecl               VAL(IndirectFieldDecl);
EnumConstantDecl                VAL(EnumConstantDecl);
FunctionDecl                    VAL(FunctionDecl);
ParmVarDecl                     VAL(ParmVarDecl);
TypedefDecl                     VAL(TypedefDecl);
RecordDecl                      VAL(RecordDecl);
FieldDecl                       VAL(FieldDecl);
EnumDecl                        VAL(EnumDecl);
VarDecl                         VAL(VarDecl);

ConstantArrayType               VAL(ConstantArrayType);
FunctionProtoType               VAL(FunctionProtoType);
ElaboratedType                  VAL(ElaboratedType);
BuiltinType                     VAL(BuiltinType);
PointerType                     VAL(PointerType);
TypedefType                     VAL(TypedefType);
RecordType                      VAL(RecordType);
ParenType                       VAL(ParenType);
QualType                        VAL(QualType);
EnumType                        VAL(EnumType);

TransparentUnionAttr            VAL(TransparentUnionAttr);
WarnUnusedResultAttr            VAL(WarnUnusedResultAttr);
ReturnsTwiceAttr                VAL(ReturnsTwiceAttr);
AllocAlignAttr                  VAL(AllocAlignAttr);
DeprecatedAttr                  VAL(DeprecatedAttr);
GNUInlineAttr                   VAL(GNUInlineAttr);
AllocSizeAttr                   VAL(AllocSizeAttr);
RestrictAttr                    VAL(RestrictAttr);
AsmLabelAttr                    VAL(AsmLabelAttr);
AlignedAttr                     VAL(AlignedAttr);
NoThrowAttr                     VAL(NoThrowAttr);
NonNullAttr                     VAL(NonNullAttr);
BuiltinAttr                     VAL(BuiltinAttr);
PackedAttr                      VAL(PackedAttr);
FormatAttr                      VAL(FormatAttr);
ConstAttr                       VAL(ConstAttr);
PureAttr                        VAL(PureAttr);
ModeAttr                        VAL(ModeAttr);

CompoundStmt                    VAL(CompoundStmt);
ContinueStmt                    VAL(ContinueStmt);
DefaultStmt                     VAL(DefaultStmt);
SwitchStmt                      VAL(SwitchStmt);
ReturnStmt                      VAL(ReturnStmt);
LabelStmt                       VAL(LabelStmt);
BreakStmt                       VAL(BreakStmt);
WhileStmt                       VAL(WhileStmt);
DeclStmt                        VAL(DeclStmt);
CaseStmt                        VAL(CaseStmt);
NullStmt                        VAL(NullStmt);
GotoStmt                        VAL(GotoStmt);
ForStmt                         VAL(ForStmt);
IfStmt                          VAL(IfStmt);

UnaryExprOrTypeTraitExpr        VAL(UnaryExprOrTypeTraitExpr);
ArraySubscriptExpr              VAL(ArraySubscriptExpr);
ImplicitCastExpr                VAL(ImplicitCastExpr);
CStyleCastExpr                  VAL(CStyleCastExpr);
ConstantExpr                    VAL(ConstantExpr);
InitListExpr                    VAL(InitListExpr);
OffsetOfExpr                    VAL(OffsetOfExpr);
DeclRefExpr                     VAL(DeclRefExpr);
MemberExpr                      VAL(MemberExpr);
ParenExpr                       VAL(ParenExpr);
CallExpr                        VAL(CallExpr);

CompoundAssignOperator          VAL(CompoundAssignOperator);
ConditionalOperator             VAL(ConditionalOperator);
BinaryOperator                  VAL(BinaryOperator);
UnaryOperator                   VAL(UnaryOperator);

CharacterLiteral                VAL(CharacterLiteral);
IntegerLiteral                  VAL(IntegerLiteral);
StringLiteral                   VAL(StringLiteral);

ParagraphComment                VAL(ParagraphComment);
FullComment                     VAL(FullComment);
TextComment                     VAL(TextComment);

Typedef                         VAL(Typedef);
Record                          VAL(Record);
Field                           VAL(Field);
Enum                            VAL(Enum);

"value: Int"                    VAL(IntValue);

"','"                           OPT(Comma, Operator);
"'%'"                           OPT(Remainder, Operator);
"'/'"                           OPT(Division, Operator);
"'*'"                           OPT(Multiplication, Operator);
"'-'"                           OPT(Subtraction, Operator);
"'+'"                           OPT(Addition, Operator);
"'&'"                           OPT(BitwiseAND, Operator);
"'|'"                           OPT(BitwiseOR, Operator);
"'^'"                           OPT(BitwiseXOR, Operator);
"'&&'"                          OPT(LogicalAND, Operator);
"'||'"                          OPT(LogicalOR, Operator);
"'!'"                           OPT(LogicalNOT, Operator);
"'>'"                           OPT(GreaterThan, Operator);
"'<'"                           OPT(LessThan, Operator);
"'~'"                           OPT(BitwiseNOT, Operator);
"'='"                           OPT(Assignment, Operator);
"'>>'"                          OPT(RightShift, Operator);
"'<<'"                          OPT(LeftShift, Operator);
"'>='"                          OPT(GreaterThanOrEqual, Operator);
"'<='"                          OPT(LessThanOrEqual, Operator);
"'=='"                          OPT(Equality, Operator);
"'!='"                          OPT(Inequality, Operator);
"'+='"                          OPT(AdditionAssignment, Operator);
"'-='"                          OPT(SubtractionAssignment, Operator);
"'*='"                          OPT(MultiplicationAssignment, Operator);
"'/='"                          OPT(DivisionAssignment, Operator);
"'%='"                          OPT(RemainderAssignment, Operator);
"'^='"                          OPT(BitwiseXORAssignment, Operator);
"'|='"                          OPT(BitwiseORAssignment, Operator);
"'&='"                          OPT(BitwiseANDAssignment, Operator);
"'>>='"                         OPT(RightShiftAssignment, Operator);
"'<<='"                         OPT(LeftShiftAssignment, Operator);
"'--'"                          OPT(Decrement, Operator);
"'++'"                          OPT(Increment, Operator);

"->"                            OPT(arrow, MemberExpr);
"."                             OPT(dot, MemberExpr);

"<IntegralCast>"                OPT(IntegralCast, Cast);
"<LValueToRValue>"              OPT(LValueToRValue, Cast);
"<FunctionToPointerDecay>"      OPT(FunctionToPointerDecay, Cast);
"<BuiltinFnToFnPtr>"            OPT(BuiltinFnToFnPtr, Cast);
"<BitCast>"                     OPT(BitCast, Cast);
"<NullToPointer>"               OPT(NullToPointer, Cast);
"<NoOp>"                        OPT(NoOp, Cast);
"<ToVoid>"                      OPT(ToVoid, Cast);
"<ArrayToPointerDecay>"         OPT(ArrayToPointerDecay, Cast);
"<IntegralToFloating>"          OPT(IntegralToFloating, Cast);
"<IntegralToPointer>"           OPT(IntegralToPointer, Cast);

__alignof|alignof               OPT(alignof, UnaryExprOrTypeTraitExpr);
sizeof                          OPT(sizeof, UnaryExprOrTypeTraitExpr);

volatile                        OPT(volatile, QualType);
const                           OPT(const, QualType);

imported                        OPT(imported, Type);
sugar                           OPT(sugar, Type);

inline                          OPT(inline, FunctionDecl);
extern                          OPT(extern, FunctionDecl);
static                          OPT(static, FunctionDecl);

definition                      OPT(definition, RecordDecl);
struct                          OPT(struct, RecordDecl);
union                           OPT(union, RecordDecl);
enum                            OPT(enum, RecordDecl);

"<undeserialized declarations>" OPT(undeserialized_declarations, Decl);
referenced                      OPT(referenced, Decl);
implicit                        OPT(implicit, Decl);
used                            OPT(used, Decl);

parenlistinit                   OPT(parenlistinit, VarDecl);
callinit                        OPT(callinit, VarDecl);
listinit                        OPT(listinit, VarDecl);
cinit                           OPT(cinit, VarDecl);

non_odr_use_unevaluated         OPT(non_odr_use_unevaluated, DeclRefExpr);
non_odr_use_discarded           OPT(non_odr_use_discarded, DeclRefExpr);
non_odr_use_constant            OPT(non_odr_use_constant, DeclRefExpr);

has_else                        OPT(has_else, IfStmt);

bitfield                        OPT(bitfield, Stmt);
lvalue                          OPT(lvalue, Stmt);

part_of_explicit_cast           OPT(part_of_explicit_cast, ImplicitCastExpr);

IsLiteralLabel                  OPT(IsLiteralLabel, AsmLabelAttr);

Inherited                       OPT(Inherited, Attr);
Implicit                        OPT(Implicit, Attr);

prefix/" "{UNARY_PREFIX}        OPT(prefix, UnaryOperator);
postfix/" "{INC_DEC}            OPT(postfix, UnaryOperator);
"cannot overflow"               OPT(cannot_overflow, UnaryOperator);

"Text="                         OPT(Text, FullComment);

"ComputeResultTy="              OPT(ComputeResultTy, CompoundAssignOperator);
"ComputeLHSTy="                 OPT(ComputeLHSTy, CompoundAssignOperator);

"<invalid sloc>"                return(TOK_INVALID_SLOC);
"<<<NULL>>>"                    return(TOK_NULL);
prev/" "{POINTER}               return(TOK_PREV);
parent/" "{POINTER}             return(TOK_PARENT);
line/:[0-9]+:[0-9]+             return(TOK_LINE);
col/:[0-9]+                     return(TOK_COL);

{POINTER}                       ATOI(POINTER, 16);
{INTEGER}                       ATOI(INTEGER, 10);

{SRC}/:[0-9]+:[0-9]+            SET(SRC, (char *)add_src(yytext));
{INDENT}/[^ \n]                 SET(INDENT, yyleng);
{SQNAME}                        SET(SQNAME, qname());
{DQNAME}                        SET(DQNAME, qname());
"\xE2\x80\x8B"{ANAME}           SET(ANAME, strdup(yytext + 3));
"\xE2\x80\x8B"{NAME}            SET(NAME, strdup(yytext + 3));
{NAME}                          SET(NAME, strdup(yytext));

":"|","|"<"|">"                 return(yytext[0]);
" "
\n                              LINE_STEP();
.                               return(TOK_YYUNDEF);


%%
