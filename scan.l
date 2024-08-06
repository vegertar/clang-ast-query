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
TypedefDecl                     RET(TypedefDecl);
BuiltinType                     RET(BuiltinType);
RecordType                      RET(RecordType);
Record                          RET(Record);
PointerType                     RET(PointerType);
ConstantArrayType               RET(ConstantArrayType);
ElaboratedType                  RET(ElaboratedType);
TypedefType                     RET(TypedefType);
Typedef                         RET(Typedef);
RecordDecl                      RET(RecordDecl);
FieldDecl                       RET(FieldDecl);
ModeAttr                        RET(ModeAttr);
FunctionDecl                    RET(FunctionDecl);
ParmVarDecl                     RET(ParmVarDecl);
CompoundStmt                    RET(CompoundStmt);
ReturnStmt                      RET(ReturnStmt);
ParenExpr                       RET(ParenExpr);
CStyleCastExpr                  RET(CStyleCastExpr);
BinaryOperator                  RET(BinaryOperator);
ImplicitCastExpr                RET(ImplicitCastExpr);
DeclRefExpr                     RET(DeclRefExpr);
IntegerLiteral                  RET(IntegerLiteral);
IndirectFieldDecl               RET(IndirectFieldDecl);
Field                           RET(Field);
QualType                        RET(QualType);
NoThrowAttr                     RET(NoThrowAttr);
NonNullAttr                     RET(NonNullAttr);
EnumDecl                        RET(EnumDecl);
EnumConstantDecl                RET(EnumConstantDecl);
ConstantExpr                    RET(ConstantExpr);
IntValue                        RET(IntValue);
EnumType                        RET(EnumType);
Enum                            RET(Enum);
AsmLabelAttr                    RET(AsmLabelAttr);
DeprecatedAttr                  RET(DeprecatedAttr);
VarDecl                         RET(VarDecl);
BuiltinAttr                     RET(BuiltinAttr);
ReturnsTwiceAttr                RET(ReturnsTwiceAttr);
ConstAttr                       RET(ConstAttr);
AlignedAttr                     RET(AlignedAttr);
UnaryExprOrTypeTraitExpr        RET(UnaryExprOrTypeTraitExpr);
FunctionProtoType               RET(FunctionProtoType);
RestrictAttr                    RET(RestrictAttr);
FormatAttr                      RET(FormatAttr);
CallExpr                        RET(CallExpr);
GNUInlineAttr                   RET(GNUInlineAttr);
ConditionalOperator             RET(ConditionalOperator);
MemberExpr                      RET(MemberExpr);
UnaryOperator                   RET(UnaryOperator);
CharacterLiteral                RET(CharacterLiteral);
PureAttr                        RET(PureAttr);
AllocSizeAttr                   RET(AllocSizeAttr);
WarnUnusedResultAttr            RET(WarnUnusedResultAttr);
AllocAlignAttr                  RET(AllocAlignAttr);
ParenType                       RET(ParenType);
DeclStmt                        RET(DeclStmt);
WhileStmt                       RET(WhileStmt);
IfStmt                          RET(IfStmt);
ArraySubscriptExpr              RET(ArraySubscriptExpr);
TransparentUnionAttr            RET(TransparentUnionAttr);
PackedAttr                      RET(PackedAttr);
FullComment                     RET(FullComment);
ParagraphComment                RET(ParagraphComment);
TextComment                     RET(TextComment);
ForStmt                         RET(ForStmt);
CompoundAssignOperator          RET(CompoundAssignOperator);
NullStmt                        RET(NullStmt);
InitListExpr                    RET(InitListExpr);
StringLiteral                   RET(StringLiteral);
GotoStmt                        RET(GotoStmt);
SwitchStmt                      RET(SwitchStmt);
CaseStmt                        RET(CaseStmt);
DefaultStmt                     RET(DefaultStmt);
LabelStmt                       RET(LabelStmt);
ContinueStmt                    RET(ContinueStmt);
BreakStmt                       RET(BreakStmt);
OffsetOfExpr                    RET(OffsetOfExpr);

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

__alignof|alignof               RET(KW_alignof);
sizeof                          RET(KW_sizeof);
struct                          RET(KW_struct);
union                           RET(KW_union);
enum                            RET(KW_enum);
extern                          RET(KW_extern);
static                          RET(KW_static);
inline                          RET(KW_inline);
const                           RET(KW_const);
volatile                        RET(KW_volatile);

cinit                           RET(OPT_cinit);
callinit                        RET(OPT_callinit);
listinit                        RET(OPT_listinit);
parenlistinit                   RET(OPT_parenlistinit);

non_odr_use_unevaluated         RET(OPT_non_odr_use_unevaluated);
non_odr_use_constant            RET(OPT_non_odr_use_constant);
non_odr_use_discarded           RET(OPT_non_odr_use_discarded);

lvalue                          RET(OPT_lvalue);

bitfield                        RET(OPT_bitfield);

used                            RET(OPT_used);
referenced                      RET(OPT_referenced);

":"|","|"<"|">"                 return(yytext[0]);
"<<<NULL>>>"                    return(TOK_NULL);
"<invalid sloc>"                return(TOK_INVALID_SLOC);
"value: Int"                    return(TOK_IntValue);
"Text="                         return(TOK_TEXT_TAG);
"ComputeLHSTy="                 return(TOK_COMPUTE_LHS_TY_TAG);
"ComputeResultTy="              return(TOK_COMPUTE_RESULT_TY_TAG);
"cannot overflow"               return(TOK_CANNOT_OVERFLOW);
"<undeserialized declarations>" return(TOK_UNDESERIALIZED_DECLARATIONS);
part_of_explicit_cast           return(TOK_PART_OF_EXPLICIT_CAST);
sugar                           return(TOK_SUGAR);
imported                        return(TOK_IMPORTED);
implicit                        return(TOK_IMPLICIT);
definition                      return(TOK_DEFINITION);
IsLiteralLabel                  return(TOK_IS_LITERAL_LABEL);
Inherited                       return(TOK_IS_INHERITED);
Implicit                        return(TOK_IS_IMPLICIT);
has_else                        return(TOK_HAS_ELSE);
prev/" "{POINTER}               return(TOK_PREV);
parent/" "{POINTER}             return(TOK_PARENT);
prefix/" "{UNARY_PREFIX}        return(TOK_PREFIX);
postfix/" "{INC_DEC}            return(TOK_POSTFIX);
line/:[0-9]+:[0-9]+             return(TOK_LINE);
col/:[0-9]+                     return(TOK_COL);

{INDENT}/[^ \n]                 SET(INDENT, yyleng);
{POINTER}                       ATOI(POINTER, 16);
{INTEGER}                       ATOI(INTEGER, 10);
{MEMBER}                        SET(MEMBER, strdup(yytext));
{SRC}/:[0-9]+:[0-9]+            SET(SRC, (char *)add_src(yytext));
{SQNAME}                        SET(SQNAME, qname());
{DQNAME}                        SET(DQNAME, qname());
"\xC2\xA0"{NAME}                SET(NAME, strdup(yytext + 2));
{NAME}                          SET(NAME, strdup(yytext));

[ \t]
\n                              LINE_STEP();
.                               return(TOK_YYUNDEF);


%%
