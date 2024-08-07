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

#define KW(X) RET(KW_##X)
#define OPT(X, ...) RET(OPT_##X)
#define RET(X) SET(X, TOK_##X)

#define SET(X, value)                                                         \
  do {                                                                        \
    yylval->TOK_##X = value;                                                  \
    return TOK_##X;                                                           \
  } while (0)

#define ATOI(X, base)                                                         \
  do {                                                                        \
    char *end;                                                                \
    errno = 0;                                                                \
    yylval->TOK_##X = yytext[0] == '-'                                        \
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

INTEGER (-?([0-9]+))

NUMBER (-?([0-9]*\.[0-9]+|[0-9]+)([Ee][+-]?[0-9]+)?)

POINTER (0x[0-9a-f]+)

CLASS (struct|union|enum)

INC_DEC ("'--'"|"'++'")

UNARY_PREFIX ({INC_DEC}|"'-'"|"'+'"|"'*'"|"'&'"|"'~'"|"'!'")

MEMBER (("->"|".")({NAME}|{CLASS}" "{NAME}::\("anonymous at ".+\))?)

SRC ([^ <:]+|"<scratch space>"|"<command line>"|"<built-in>")

SQNAME ('((?:[^'\\]|\\.)*)')

DQNAME (\"((?:[^"\\]|\\.)*)\")

%%

%{
  // Each time yylex is called, move the head position to the end one.
  LOCATION_STEP();
%}

TranslationUnitDecl             RET(TranslationUnitDecl);
IndirectFieldDecl               RET(IndirectFieldDecl);
EnumConstantDecl                RET(EnumConstantDecl);
FunctionDecl                    RET(FunctionDecl);
ParmVarDecl                     RET(ParmVarDecl);
TypedefDecl                     RET(TypedefDecl);
RecordDecl                      RET(RecordDecl);
FieldDecl                       RET(FieldDecl);
EnumDecl                        RET(EnumDecl);
VarDecl                         RET(VarDecl);

ConstantArrayType               RET(ConstantArrayType);
FunctionProtoType               RET(FunctionProtoType);
ElaboratedType                  RET(ElaboratedType);
BuiltinType                     RET(BuiltinType);
PointerType                     RET(PointerType);
TypedefType                     RET(TypedefType);
RecordType                      RET(RecordType);
ParenType                       RET(ParenType);
QualType                        RET(QualType);
EnumType                        RET(EnumType);

TransparentUnionAttr            RET(TransparentUnionAttr);
WarnUnusedResultAttr            RET(WarnUnusedResultAttr);
ReturnsTwiceAttr                RET(ReturnsTwiceAttr);
AllocAlignAttr                  RET(AllocAlignAttr);
DeprecatedAttr                  RET(DeprecatedAttr);
GNUInlineAttr                   RET(GNUInlineAttr);
AllocSizeAttr                   RET(AllocSizeAttr);
RestrictAttr                    RET(RestrictAttr);
AsmLabelAttr                    RET(AsmLabelAttr);
AlignedAttr                     RET(AlignedAttr);
NoThrowAttr                     RET(NoThrowAttr);
NonNullAttr                     RET(NonNullAttr);
BuiltinAttr                     RET(BuiltinAttr);
PackedAttr                      RET(PackedAttr);
FormatAttr                      RET(FormatAttr);
ConstAttr                       RET(ConstAttr);
PureAttr                        RET(PureAttr);
ModeAttr                        RET(ModeAttr);

CompoundStmt                    RET(CompoundStmt);
ContinueStmt                    RET(ContinueStmt);
DefaultStmt                     RET(DefaultStmt);
SwitchStmt                      RET(SwitchStmt);
ReturnStmt                      RET(ReturnStmt);
LabelStmt                       RET(LabelStmt);
BreakStmt                       RET(BreakStmt);
WhileStmt                       RET(WhileStmt);
DeclStmt                        RET(DeclStmt);
CaseStmt                        RET(CaseStmt);
NullStmt                        RET(NullStmt);
GotoStmt                        RET(GotoStmt);
ForStmt                         RET(ForStmt);
IfStmt                          RET(IfStmt);

UnaryExprOrTypeTraitExpr        RET(UnaryExprOrTypeTraitExpr);
ArraySubscriptExpr              RET(ArraySubscriptExpr);
ImplicitCastExpr                RET(ImplicitCastExpr);
CStyleCastExpr                  RET(CStyleCastExpr);
ConstantExpr                    RET(ConstantExpr);
InitListExpr                    RET(InitListExpr);
OffsetOfExpr                    RET(OffsetOfExpr);
DeclRefExpr                     RET(DeclRefExpr);
MemberExpr                      RET(MemberExpr);
ParenExpr                       RET(ParenExpr);
CallExpr                        RET(CallExpr);

CompoundAssignOperator          RET(CompoundAssignOperator);
ConditionalOperator             RET(ConditionalOperator);
BinaryOperator                  RET(BinaryOperator);
UnaryOperator                   RET(UnaryOperator);

CharacterLiteral                RET(CharacterLiteral);
IntegerLiteral                  RET(IntegerLiteral);
StringLiteral                   RET(StringLiteral);

ParagraphComment                RET(ParagraphComment);
FullComment                     RET(FullComment);
TextComment                     RET(TextComment);

Typedef                         RET(Typedef);
Record                          RET(Record);
Field                           RET(Field);
Enum                            RET(Enum);

"value: Int"                    RET(IntValue);

"','"                           RET(Comma);
"'%'"                           RET(Remainder);
"'/'"                           RET(Division);
"'*'"                           RET(Multiplication);
"'-'"                           RET(Subtraction);
"'+'"                           RET(Addition);
"'&'"                           RET(BitwiseAND);
"'|'"                           RET(BitwiseOR);
"'^'"                           RET(BitwiseXOR);
"'!'"                           RET(LogicalNOT);
"'>'"                           RET(GreaterThan);
"'<'"                           RET(LessThan);
"'~'"                           RET(BitwiseNOT);
"'='"                           RET(Assignment);
"'>>'"                          RET(RightShift);
"'<<'"                          RET(LeftShift);
"'>='"                          RET(GreaterThanOrEqual);
"'<='"                          RET(LessThanOrEqual);
"'=='"                          RET(Equality);
"'!='"                          RET(Inequality);
"'+='"                          RET(AdditionAssignment);
"'-='"                          RET(SubtractionAssignment);
"'*='"                          RET(MultiplicationAssignment);
"'/='"                          RET(DivisionAssignment);
"'%='"                          RET(RemainderAssignment);
"'^='"                          RET(BitwiseXORAssignment);
"'|='"                          RET(BitwiseORAssignment);
"'&='"                          RET(BitwiseANDAssignment);
"'>>='"                         RET(RightShiftAssignment);
"'<<='"                         RET(LeftShiftAssignment);
"'--'"                          RET(Decrement);
"'++'"                          RET(Increment);
"'&&'"                          RET(LogicalAND);
"'||'"                          RET(LogicalOR);

"<IntegralCast>"                RET(IntegralCast);
"<LValueToRValue>"              RET(LValueToRValue);
"<FunctionToPointerDecay>"      RET(FunctionToPointerDecay);
"<BuiltinFnToFnPtr>"            RET(BuiltinFnToFnPtr);
"<BitCast>"                     RET(BitCast);
"<NullToPointer>"               RET(NullToPointer);
"<NoOp>"                        RET(NoOp);
"<ToVoid>"                      RET(ToVoid);
"<ArrayToPointerDecay>"         RET(ArrayToPointerDecay);
"<IntegralToFloating>"          RET(IntegralToFloating);
"<IntegralToPointer>"           RET(IntegralToPointer);

__alignof|alignof               KW(alignof);
sizeof                          KW(sizeof);

volatile                        KW(volatile);
const                           KW(const);

inline                          KW(inline);

extern                          KW(extern);
static                          KW(static);

struct                          KW(struct);
union                           KW(union);
enum                            KW(enum);

"<undeserialized declarations>" OPT(undeserialized_declarations, Decl);

implicit                        OPT(implicit, Decl);

referenced                      OPT(referenced, Decl);
used                            OPT(used, Decl);

parenlistinit                   OPT(parenlistinit, VarDecl);
callinit                        OPT(callinit, VarDecl);
listinit                        OPT(listinit, VarDecl);
cinit                           OPT(cinit, VarDecl);

definition                      OPT(definition, RecordDecl);

non_odr_use_unevaluated         OPT(non_odr_use_unevaluated, DeclRefExpr);
non_odr_use_discarded           OPT(non_odr_use_discarded, DeclRefExpr);
non_odr_use_constant            OPT(non_odr_use_constant, DeclRefExpr);

has_else                        OPT(has_else, IfStmt);

bitfield                        OPT(bitfield, Stmt);
lvalue                          OPT(lvalue, Stmt);

part_of_explicit_cast           OPT(part_of_explicit_cast, Cast);

sugar                           OPT(sugar, Type);
imported                        OPT(imported, Type);

IsLiteralLabel                  OPT(IsLiteralLabel, AsmLabelAttr);

Inherited                       OPT(Inherited, Attr);
Implicit                        OPT(Implicit, Attr);

"cannot overflow"               OPT(cannot_overflow, UnaryOperator);

"Text="                         OPT(Text, FullComment);

"ComputeResultTy="              OPT(ComputeResultTy, CompoundAssignOperator);
"ComputeLHSTy="                 OPT(ComputeLHSTy, CompoundAssignOperator);

"<invalid sloc>"                return(TOK_INVALID_SLOC);
"<<<NULL>>>"                    return(TOK_NULL);
prev/" "{POINTER}               return(TOK_PREV);
parent/" "{POINTER}             return(TOK_PARENT);
prefix/" "{UNARY_PREFIX}        return(TOK_PREFIX);
postfix/" "{INC_DEC}            return(TOK_POSTFIX);
line/:[0-9]+:[0-9]+             return(TOK_LINE);
col/:[0-9]+                     return(TOK_COL);

{POINTER}                       ATOI(POINTER, 16);
{INTEGER}                       ATOI(INTEGER, 10);

{SRC}/:[0-9]+:[0-9]+            SET(SRC, (char *)add_src(yytext));
{INDENT}/[^ \n]                 SET(INDENT, yyleng);
{MEMBER}                        SET(MEMBER, strdup(yytext));
{SQNAME}                        SET(SQNAME, qname());
{DQNAME}                        SET(DQNAME, qname());
"\xC2\xA0"{NAME}                SET(NAME, strdup(yytext + 2));
{NAME}                          SET(NAME, strdup(yytext));

":"|","|"<"|">"                 return(yytext[0]);
" "
\n                              LINE_STEP();
.                               return(TOK_YYUNDEF);


%%
