/* Disable Flex features we don't need, to avoid warnings. */
%option nodefault noinput nounput noyywrap
%x comment

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
  } while (0)

#define EOL()                                                                 \
  do {                                                                        \
    LINE_STEP();                                                              \
    return TOK_EOL;                                                           \
  } while (0)

#define UND() return(TOK_YYUNDEF)

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

#define OPT(X, ctx...) VAL(OPT_##X)
#define VAL(X, ...) SET(X, TOK_##X __VA_OPT__(,) __VA_ARGS__)
#define TOK(X) return(TOK_##X)
#define CHR() return(yytext[0])

#define RAW(X) VAL(X, | (NG_##X << KIND_WIDTH))
#define ATTR(X) VAL(X##Attr, | (NG_Attr << KIND_WIDTH))
#define COMMENT(X) VAL(X##Comment, | (NG_Comment << KIND_WIDTH))
#define DECL(X) VAL(X##Decl, | (NG_Decl << KIND_WIDTH))
#define TYPE(X) VAL(X##Type, | (NG_Type << KIND_WIDTH))
#define STMT(X) VAL(X##Stmt, | (NG_Stmt << KIND_WIDTH))
#define EXPR(X) VAL(X##Expr, | (NG_Expr << KIND_WIDTH))
#define LITERAL(X) VAL(X##Literal, | (NG_Literal << KIND_WIDTH))
#define OPERATOR(X) VAL(X##Operator, | (NG_Operator << KIND_WIDTH))
#define CAST_EXPR(X) VAL(X##CastExpr, | (NG_CastExpr << KIND_WIDTH))
#define DIRECTIVE(X) VAL(X##Directive, | (NG_Directive << KIND_WIDTH))
#define PPDECL(X) VAL(X##PPDecl, | (NG_PPDecl << KIND_WIDTH))
#define PPEXPR(X) VAL(X##PPExpr, | (NG_PPExpr << KIND_WIDTH))
#define PPOPERATOR(X) VAL(X##PPOperator, | (NG_PPOperator << KIND_WIDTH))
#define PPSTMT(X) VAL(X##PPStmt, | (NG_PPStmt << KIND_WIDTH))
#define EXPANSION(X) VAL(X##Expansion, | (NG_Expansion << KIND_WIDTH))

#define SET(X, value, ...)                                                    \
  do {                                                                        \
    yylval->TOK_##X = value __VA_ARGS__;                                      \
    return TOK_##X;                                                           \
  } while (0)

#define ATOI(X, base)                                                         \
  do {                                                                        \
    char *end;                                                                \
    errno = 0;                                                                \
    bool is_negative = yytext[0] == '-';                                     \
    yylval->TOK_##X.negative = is_negative;                                   \
    if (is_negative)                                                          \
      yylval->TOK_##X.i = strtoll(yytext, &end, base);                        \
    else                                                                      \
      yylval->TOK_##X.u = strtoul(yytext, &end, base);                        \
                                                                              \
    return (errno || *end) ? TOK_YYUNDEF : TOK_##X;                           \
  } while (0)

#define add_static(s, n) add_string(string_static(s, n))

%}

INDENT ^[ \|`]+-

NAME ([a-zA-Z_][a-zA-Z_0-9]*)

ANAME ({CLASS}" "{NAME}::\("anonymous at ".+\))

INTEGER (-?([0-9]+))

POINTER (0x[0-9a-f]+)

CLASS (struct|union|enum)

INC_DEC ("'--'"|"'++'")

UNARY_PREFIX ({INC_DEC}|"'-'"|"'+'"|"'*'"|"'&'"|"'~'"|"'!'"|"'__extension__'")

SRC ([^ <:]+|"<scratch space>"|"<command line>"|"<built-in>")

SQTEXT ('((?:[^'\\]|\\.)*)')

DQTEXT (\"((?:[^"\\]|\\.)*)\")

%%

%{
  // Each time yylex is called, move the head position to the end one.
  LOCATION_STEP();
%}

"value: Int"                    RAW(IntValue);
Enum                            RAW(Enum);
Typedef                         RAW(Typedef);
Record                          RAW(Record);
Field                           RAW(Field);
Preprocessor                    RAW(Preprocessor);
Token                           RAW(Token);

ModeAttr                        ATTR(Mode);
NoThrowAttr                     ATTR(NoThrow);
NonNullAttr                     ATTR(NonNull);
AsmLabelAttr                    ATTR(AsmLabel);
DeprecatedAttr                  ATTR(Deprecated);
BuiltinAttr                     ATTR(Builtin);
ReturnsTwiceAttr                ATTR(ReturnsTwice);
ConstAttr                       ATTR(Const);
AlignedAttr                     ATTR(Aligned);
RestrictAttr                    ATTR(Restrict);
FormatAttr                      ATTR(Format);
GNUInlineAttr                   ATTR(GNUInline);
AllocSizeAttr                   ATTR(AllocSize);
WarnUnusedResultAttr            ATTR(WarnUnusedResult);
AllocAlignAttr                  ATTR(AllocAlign);
TransparentUnionAttr            ATTR(TransparentUnion);
PackedAttr                      ATTR(Packed);
PureAttr                        ATTR(Pure);

FullComment                     COMMENT(Full);
ParagraphComment                COMMENT(Paragraph);
TextComment                     COMMENT(Text);

TranslationUnitDecl             DECL(TranslationUnit);
TypedefDecl                     DECL(Typedef);
RecordDecl                      DECL(Record);
FieldDecl                       DECL(Field);
FunctionDecl                    DECL(Function);
ParmVarDecl                     DECL(ParmVar);
IndirectFieldDecl               DECL(IndirectField);
EnumDecl                        DECL(Enum);
EnumConstantDecl                DECL(EnumConstant);
VarDecl                         DECL(Var);

BuiltinType                     TYPE(Builtin);
RecordType                      TYPE(Record);
PointerType                     TYPE(Pointer);
ConstantArrayType               TYPE(ConstantArray);
ElaboratedType                  TYPE(Elaborated);
TypedefType                     TYPE(Typedef);
QualType                        TYPE(Qual);
EnumType                        TYPE(Enum);
FunctionProtoType               TYPE(FunctionProto);
ParenType                       TYPE(Paren);

CompoundStmt                    STMT(Compound);
ReturnStmt                      STMT(Return);
DeclStmt                        STMT(Decl);
WhileStmt                       STMT(While);
IfStmt                          STMT(If);
ForStmt                         STMT(For);
NullStmt                        STMT(Null);
GotoStmt                        STMT(Goto);
SwitchStmt                      STMT(Switch);
CaseStmt                        STMT(Case);
DefaultStmt                     STMT(Default);
LabelStmt                       STMT(Label);
ContinueStmt                    STMT(Continue);
BreakStmt                       STMT(Break);
DoStmt                          STMT(Do);

ParenExpr                       EXPR(Paren);
DeclRefExpr                     EXPR(DeclRef);
ConstantExpr                    EXPR(Constant);
CallExpr                        EXPR(Call);
MemberExpr                      EXPR(Member);
ArraySubscriptExpr              EXPR(ArraySubscript);
InitListExpr                    EXPR(InitList);
OffsetOfExpr                    EXPR(OffsetOf);
UnaryExprOrTypeTraitExpr        EXPR(UnaryExprOrTypeTrait);
StmtExpr                        EXPR(Stmt);

IntegerLiteral                  LITERAL(Integer);
CharacterLiteral                LITERAL(Character);
StringLiteral                   LITERAL(String);

UnaryOperator                   OPERATOR(Unary);
BinaryOperator                  OPERATOR(Binary);
ConditionalOperator             OPERATOR(Conditional);
CompoundAssignOperator          OPERATOR(CompoundAssign);

CStyleCastExpr                  CAST_EXPR(CStyle);
ImplicitCastExpr                CAST_EXPR(Implicit);

DefineDirective                 DIRECTIVE(Define);
MacroPPDecl                     PPDECL(Macro);
InclusionDirective              DIRECTIVE(Inclusion);
IfDirective                     DIRECTIVE(If);
ConditionalPPExpr               PPEXPR(Conditional);
DefinedPPOperator               PPOPERATOR(Defined);
CompoundPPStmt                  PPSTMT(Compound);
MacroExpansion                  EXPANSION(Macro);

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
"'__extension__'"               OPT(Extension, Operator);

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
used                            OPT(used, Decl);
referenced                      OPT(referenced, Decl);
implicit                        OPT(implicit, Decl, PPExpr);

cinit                           OPT(cinit, VarDecl);
callinit                        OPT(callinit, VarDecl);
listinit                        OPT(listinit, VarDecl);
parenlistinit                   OPT(parenlistinit, VarDecl);

non_odr_use_unevaluated         OPT(non_odr_use_unevaluated, DeclRefExpr);
non_odr_use_constant            OPT(non_odr_use_constant, DeclRefExpr);
non_odr_use_discarded           OPT(non_odr_use_discarded, DeclRefExpr);

has_else                        OPT(has_else, IfStmt, IfDirective);

bitfield                        OPT(bitfield, Stmt);
lvalue                          OPT(lvalue, Stmt);

part_of_explicit_cast           OPT(part_of_explicit_cast, ImplicitCastExpr);

IsLiteralLabel                  OPT(IsLiteralLabel, AsmLabelAttr);

Inherited                       OPT(Inherited, Attr);
Implicit                        OPT(Implicit, Attr);

angled                          OPT(angled, InclusionDirective);

if                              OPT(if, IfDirective);
ifdef                           OPT(ifdef, IfDirective);
ifndef                          OPT(ifndef, IfDirective);
elif                            OPT(elif, IfDirective);
elifdef                         OPT(elifdef, IfDirective);
elifndef                        OPT(elifndef, IfDirective);

fast                            OPT(fast, MacroExpansion);

hasLeadingSpace                 OPT(hasLeadingSpace, Token);
is_arg                          OPT(is_arg, Token);
stringified                     OPT(stringified, Token);
paste                           OPT(paste, Token);

NotEvaluated                    OPT(NotEvaluated, ConditionalPPExpr);
False                           OPT(False, ConditionalPPExpr);
True                            OPT(True, ConditionalPPExpr);

prefix/" "{UNARY_PREFIX}        OPT(prefix, UnaryOperator);
postfix/" "{INC_DEC}            OPT(postfix, UnaryOperator);
"cannot overflow"               OPT(cannot_overflow, UnaryOperator);

"Text="                         OPT(Text, FullComment);
"ComputeResultTy="              OPT(ComputeResultTy, CompoundAssignOperator);
"ComputeLHSTy="                 OPT(ComputeLHSTy, CompoundAssignOperator);

"<invalid sloc>"                TOK(INVALID_SLOC);
"<<<NULL>>>"                    TOK(NULL);
Macro/" "{POINTER}              TOK(MACRO);
prev/" "{POINTER}               TOK(PREV);
parent/" "{POINTER}             TOK(PARENT);
line/:[0-9]+:[0-9]+             TOK(LINE);
col/:[0-9]+                     TOK(COL);

{POINTER}                       ATOI(POINTER, 16);
{INTEGER}                       ATOI(INTEGER, 10);

{INDENT}/[^ \n]                 SET(INDENT, yyleng);
{SRC}/:[0-9]+:[0-9]+            SET(SRC, add_static(yytext, yyleng));
{SQTEXT}                        SET(SQTEXT, add_static(yytext + 1, yyleng - 2));
{DQTEXT}                        SET(DQTEXT, add_static(yytext + 1, yyleng - 2));
"\xE2\x80\x8A".*                SET(TEXT, add_static(yytext + 3, yyleng - 3));
"\xE2\x80\x8B"{ANAME}           SET(ANAME, add_static(yytext + 3, yyleng - 3));
"\xE2\x80\x8B"{NAME}            SET(NAME, add_static(yytext + 3, yyleng - 3));
{NAME}                          SET(NAME, add_static(yytext, yyleng));

^"#TU "                         TOK(TU);
^"#TS "                         TOK(TS);
^"#CWD "                        TOK(CWD);
^"#"                            TOK(REMARK);

":"|","|"<"|">"                 CHR();
" "                             {}
\n                              EOL();
.                               UND();

%%
