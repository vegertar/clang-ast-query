%require "3.7"

// Emitted on top of the implementation file.
%code top {
  #include "parse-top.h"
}

// Emitted in the header file, before the opt_definition of YYSTYPE.
%code requires {
  #include "parse-requires.h"
}

// Emitted in the header file, after the opt_definition of YYSTYPE.
%code provides {
  #include "parse-provides.h"
}

// Emitted in the implementation file.
%code {
  #include "parse-impl.c"
}

// Include the header in the implementation rather than duplicating it.
%define api.header.include {"parse.h"}

// Don't share global variables between the scanner and the parser.
%define api.pure full

// Generate a push parser.
%define api.push-pull push

// To avoid name clashes (e.g., with C's EOF) prefix token definitions
// with TOK_ (e.g., TOK_EOF).
%define api.token.prefix {TOK_}

// %token and %type use genuine types (e.g., "%token <int>").  Let
// %bison define YYSTYPE as a union of all these types.
%define api.value.type union

// Customized syntax error messages (see yyreport_syntax_error)...
%define parse.error custom

// with locations.
%locations

// ... and accurate list of expected tokens.
%define parse.lac full

// Enable debug traces.
%define parse.trace

// Generate the parser description file (parse.output).
%verbose

// User context, exchanged between main, yyparse and yylex.
%param {const UserContext *uctx}

// Formatting semantic values in debug traces.
%printer { fprintf(yyo, "%s", "\\n"); } EOL;

%token
    EOL
    INVALID_SLOC
    LINE
    COL
    NULL
    PREV
    PARENT
  <unsigned>
    INDENT

    IntValue
    Enum
    Typedef
    Record
    Field
    Preprocessor

    ModeAttr
    NoThrowAttr
    NonNullAttr
    AsmLabelAttr
    DeprecatedAttr
    BuiltinAttr
    ReturnsTwiceAttr
    ConstAttr
    AlignedAttr
    RestrictAttr
    FormatAttr
    GNUInlineAttr
    AllocSizeAttr
    WarnUnusedResultAttr
    AllocAlignAttr
    TransparentUnionAttr
    PackedAttr
    PureAttr

    FullComment
    ParagraphComment
    TextComment

    TranslationUnitDecl
    TypedefDecl
    RecordDecl
    FieldDecl
    FunctionDecl
    ParmVarDecl
    IndirectFieldDecl
    EnumDecl
    EnumConstantDecl
    VarDecl

    BuiltinType
    RecordType
    PointerType
    ConstantArrayType
    ElaboratedType
    TypedefType
    QualType
    EnumType
    FunctionProtoType
    ParenType

    CompoundStmt
    ReturnStmt
    DeclStmt
    WhileStmt
    IfStmt
    ForStmt
    NullStmt
    GotoStmt
    SwitchStmt
    CaseStmt
    DefaultStmt
    LabelStmt
    ContinueStmt
    BreakStmt
    DoStmt

    ParenExpr
    DeclRefExpr
    ConstantExpr
    CallExpr
    MemberExpr
    ArraySubscriptExpr
    InitListExpr
    OffsetOfExpr
    UnaryExprOrTypeTraitExpr
    StmtExpr

    IntegerLiteral
    CharacterLiteral
    StringLiteral

    UnaryOperator
    BinaryOperator
    ConditionalOperator
    CompoundAssignOperator

    CStyleCastExpr
    ImplicitCastExpr

  <enum yytokentype>
    /* Operator */
    OPT_Comma
    OPT_Remainder
    OPT_Division
    OPT_Multiplication
    OPT_Subtraction
    OPT_Addition
    OPT_BitwiseAND
    OPT_BitwiseOR
    OPT_BitwiseXOR
    OPT_BitwiseNOT
    OPT_LogicalAND
    OPT_LogicalOR
    OPT_LogicalNOT
    OPT_GreaterThan
    OPT_GreaterThanOrEqual
    OPT_LessThan
    OPT_LessThanOrEqual
    OPT_Equality
    OPT_Inequality
    OPT_Assignment
    OPT_AdditionAssignment
    OPT_SubtractionAssignment
    OPT_MultiplicationAssignment
    OPT_DivisionAssignment
    OPT_RemainderAssignment
    OPT_BitwiseXORAssignment
    OPT_BitwiseORAssignment
    OPT_BitwiseANDAssignment
    OPT_RightShift
    OPT_RightShiftAssignment
    OPT_LeftShift
    OPT_LeftShiftAssignment
    OPT_Decrement
    OPT_Increment
    OPT_Extension

    /* MemberExpr */
    OPT_arrow
    OPT_dot

    /* Cast */
    OPT_IntegralCast
    OPT_LValueToRValue
    OPT_FunctionToPointerDecay
    OPT_BuiltinFnToFnPtr
    OPT_BitCast
    OPT_NullToPointer
    OPT_NoOp
    OPT_ToVoid
    OPT_ArrayToPointerDecay
    OPT_IntegralToFloating
    OPT_IntegralToPointer

    /* UnaryExprOrTypeTraitExpr */
    OPT_alignof
    OPT_sizeof
    
    /* QualType */
    OPT_volatile
    OPT_const

    /* Type */
    OPT_imported
    OPT_sugar

    /* FunctionDecl */
    OPT_inline
    OPT_extern
    OPT_static

    /* RecordDecl */
    OPT_definition
    OPT_struct
    OPT_union
    OPT_enum

    /* Decl */
    OPT_undeserialized_declarations
    OPT_referenced
    OPT_implicit
    OPT_used

    /* VarDecl */
    OPT_parenlistinit
    OPT_callinit
    OPT_listinit
    OPT_cinit

    /* DeclRefExpr */
    OPT_non_odr_use_unevaluated
    OPT_non_odr_use_discarded
    OPT_non_odr_use_constant

    /* IfStmt */
    OPT_has_else

    /* Stmt */
    OPT_bitfield
    OPT_lvalue

    /* ImplicitCastExpr */
    OPT_part_of_explicit_cast

    /* AsmLabelAttr */
    OPT_IsLiteralLabel

    /* Attr */
    OPT_Inherited
    OPT_Implicit

    /* UnaryOperator */
    OPT_prefix
    OPT_postfix
    OPT_cannot_overflow

    /* FullComment */
    OPT_Text

    /* CompoundAssignOperator */
    OPT_ComputeResultTy
    OPT_ComputeLHSTy

  <Integer>
    INTEGER
    POINTER
  <const char *>
    NAME
    ANAME
    SQNAME
    DQNAME
    TEXT
    SRC
%nterm
  <Node>
    Node

    IntValueNode
    EnumNode
    TypedefNode
    RecordNode
    FieldNode
    PreprocessorNode

    ModeAttrNode
    NoThrowAttrNode
    NonNullAttrNode
    AsmLabelAttrNode
    DeprecatedAttrNode
    BuiltinAttrNode
    ReturnsTwiceAttrNode
    ConstAttrNode
    AlignedAttrNode
    RestrictAttrNode
    FormatAttrNode
    GNUInlineAttrNode
    AllocSizeAttrNode
    WarnUnusedResultAttrNode
    AllocAlignAttrNode
    TransparentUnionAttrNode
    PackedAttrNode
    PureAttrNode

    FullCommentNode
    ParagraphCommentNode
    TextCommentNode

    TranslationUnitDeclNode
    TypedefDeclNode
    RecordDeclNode
    FieldDeclNode
    FunctionDeclNode
    ParmVarDeclNode
    IndirectFieldDeclNode
    EnumDeclNode
    EnumConstantDeclNode
    VarDeclNode

    BuiltinTypeNode
    RecordTypeNode
    PointerTypeNode
    ConstantArrayTypeNode
    ElaboratedTypeNode
    TypedefTypeNode
    QualTypeNode
    EnumTypeNode
    FunctionProtoTypeNode
    ParenTypeNode

    CompoundStmtNode
    ReturnStmtNode
    DeclStmtNode
    WhileStmtNode
    IfStmtNode
    ForStmtNode
    NullStmtNode
    GotoStmtNode
    SwitchStmtNode
    CaseStmtNode
    DefaultStmtNode
    LabelStmtNode
    ContinueStmtNode
    BreakStmtNode
    DoStmtNode

    ParenExprNode
    DeclRefExprNode
    ConstantExprNode
    CallExprNode
    MemberExprNode
    ArraySubscriptExprNode
    InitListExprNode
    OffsetOfExprNode
    UnaryExprOrTypeTraitExprNode
    StmtExprNode

    IntegerLiteralNode
    CharacterLiteralNode
    StringLiteralNode

    UnaryOperatorNode
    BinaryOperatorNode
    ConditionalOperatorNode
    CompoundAssignOperatorNode

    CStyleCastExprNode
    ImplicitCastExprNode
  <AttrSelf>
    Attr
  <CommentSelf>
    Comment
  <DeclSelf>
    Decl
  <TypeSelf>
    Type
  <StmtSelf>
    Stmt
  <ExprSelf>
    Expr
  <DeclRef>
    DeclRef
  <ArgIndices>
    ArgIndices
  <Member>
    Member
  <MemberDecl>
    MemberDecl
  <Integer>
    integer
  <Label>
    Label
  <Loc>
    Loc
    FileLoc
    LineLoc
    ColLoc
  <Range>
    Range
    AngledRange
  <BareType>
    BareType
    argument_type
    TagComputeLHSTy
    TagComputeResultTy
  <intptr_t>
    parent
    prev
  <_Bool>
    opt_inline
    opt_const
    opt_volatile
    opt_cannot_overflow
    opt_part_of_explicit_cast
    opt_sugar
    opt_imported
    opt_implicit
    opt_has_else
    opt_definition
    opt_IsLiteralLabel
    opt_Inherited
    opt_Implicit
    opt_undeserialized_declarations
  <enum yytokentype>
    MemberAccess
    Operator
    Cast
    Trait
    Class
    PrefixOrPostfix
    storage
    init_style
    used_or_referenced
    non_odr_use
    value_kind
    object_kind
  <const char *>
    name
    TagText
%%

// Naming conventions:
// - All lowercase leading names are optional non-terminal tokens.
// - All "opt_" leading names are both optional and represent options.
// - All "OPT_" leading names are necessary options.

Start: Node EOL
  {
    // ast_push(&ast, $1);
  }
 | INDENT Node EOL
  {
    $2.level = $1 / 2;
    // ast_push(&ast, $2);
  }

Node: NULL { $$.node = 0;  }
 | IntValueNode
 | EnumNode
 | TypedefNode
 | RecordNode
 | FieldNode
 | PreprocessorNode

 | ModeAttrNode
 | NoThrowAttrNode
 | NonNullAttrNode
 | AsmLabelAttrNode
 | DeprecatedAttrNode
 | BuiltinAttrNode
 | ReturnsTwiceAttrNode
 | ConstAttrNode
 | AlignedAttrNode
 | RestrictAttrNode
 | FormatAttrNode
 | GNUInlineAttrNode
 | AllocSizeAttrNode
 | WarnUnusedResultAttrNode
 | AllocAlignAttrNode
 | TransparentUnionAttrNode
 | PackedAttrNode
 | PureAttrNode

 | FullCommentNode
 | ParagraphCommentNode
 | TextCommentNode

 | TranslationUnitDeclNode
 | TypedefDeclNode
 | RecordDeclNode
 | FieldDeclNode
 | FunctionDeclNode
 | ParmVarDeclNode
 | IndirectFieldDeclNode
 | EnumDeclNode
 | EnumConstantDeclNode
 | VarDeclNode

 | BuiltinTypeNode
 | RecordTypeNode
 | PointerTypeNode
 | ConstantArrayTypeNode
 | ElaboratedTypeNode
 | TypedefTypeNode
 | QualTypeNode
 | EnumTypeNode
 | FunctionProtoTypeNode
 | ParenTypeNode

 | CompoundStmtNode
 | ReturnStmtNode
 | DeclStmtNode
 | WhileStmtNode
 | IfStmtNode
 | ForStmtNode
 | NullStmtNode
 | GotoStmtNode
 | SwitchStmtNode
 | CaseStmtNode
 | DefaultStmtNode
 | LabelStmtNode
 | ContinueStmtNode
 | BreakStmtNode
 | DoStmtNode

 | ParenExprNode
 | DeclRefExprNode
 | ConstantExprNode
 | CallExprNode
 | MemberExprNode
 | ArraySubscriptExprNode
 | InitListExprNode
 | OffsetOfExprNode
 | UnaryExprOrTypeTraitExprNode
 | StmtExprNode

 | IntegerLiteralNode
 | CharacterLiteralNode
 | StringLiteralNode

 | UnaryOperatorNode
 | BinaryOperatorNode
 | ConditionalOperatorNode
 | CompoundAssignOperatorNode

 | CStyleCastExprNode
 | ImplicitCastExprNode

IntValueNode: IntValue INTEGER
  {
    $$.IntValue.node = $1;
    $$.IntValue.value = $2;
  }

EnumNode: Enum POINTER SQNAME
  {
    $$.Enum.node = $1;
    $$.Enum.pointer = $2.u;
    $$.Enum.name = $3;
  }

TypedefNode: Typedef POINTER BareType
  {
    $$.Typedef.node = $1;
    $$.Typedef.pointer = $2.u;
    $$.Typedef.type = $3;
  }

RecordNode: Record POINTER BareType
  {
    $$.Record.node = $1;
    $$.Record.pointer = $2.u;
    $$.Record.type = $3;
  }

FieldNode: Field POINTER SQNAME BareType
  {
    $$.Field.node = $1;
    $$.Field.pointer = $2.u;
    $$.Field.name = $3;
    $$.Field.type = $4;
  }

PreprocessorNode: Preprocessor POINTER
  {
    $$.Preprocessor.node = $1;
    $$.Preprocessor.pointer = $2.u;
  }

ModeAttrNode: ModeAttr Attr NAME
  {
    $$.ModeAttr.node = $1;
    $$.ModeAttr.self = $2;
    $$.ModeAttr.name = $3;
  }

NoThrowAttrNode: NoThrowAttr Attr
  {
    $$.NoThrowAttr.node = $1;
    $$.NoThrowAttr.self = $2;
  }

NonNullAttrNode: NonNullAttr Attr ArgIndices
  {
    $$.NonNullAttr.node = $1;
    $$.NonNullAttr.self = $2;
    $$.NonNullAttr.arg_indices = $3;
  }

AsmLabelAttrNode: AsmLabelAttr Attr DQNAME opt_IsLiteralLabel
  {
    $$.AsmLabelAttr.node = $1;
    $$.AsmLabelAttr.self = $2;
    $$.AsmLabelAttr.name = $3;
    $$.AsmLabelAttr.opt_IsLiteralLabel = $4;
  }

DeprecatedAttrNode: DeprecatedAttr Attr DQNAME DQNAME
  {
    $$.DeprecatedAttr.node = $1;
    $$.DeprecatedAttr.self = $2;
    $$.DeprecatedAttr.message = $3;
    $$.DeprecatedAttr.replacement = $4;
  }

BuiltinAttrNode: BuiltinAttr Attr INTEGER
  {
    $$.BuiltinAttr.node = $1;
    $$.BuiltinAttr.self = $2;
    $$.BuiltinAttr.id = $3.u;
  }

ReturnsTwiceAttrNode: ReturnsTwiceAttr Attr
  {
    $$.ReturnsTwiceAttr.node = $1;
    $$.ReturnsTwiceAttr.self = $2;
  }

ConstAttrNode: ConstAttr Attr
  {
    $$.ConstAttr.node = $1;
    $$.ConstAttr.self = $2;
  }

AlignedAttrNode: AlignedAttr Attr NAME
  {
    $$.AlignedAttr.node = $1;
    $$.AlignedAttr.self = $2;
    $$.AlignedAttr.name = $3;
  }

RestrictAttrNode: RestrictAttr Attr NAME
  {
    $$.RestrictAttr.node = $1;
    $$.RestrictAttr.self = $2;
    $$.RestrictAttr.name = $3;
  }

FormatAttrNode: FormatAttr Attr NAME INTEGER INTEGER
  {
    $$.FormatAttr.node = $1;
    $$.FormatAttr.self = $2;
    $$.FormatAttr.archetype = $3;
    $$.FormatAttr.string_index = $4.u;
    $$.FormatAttr.first_to_check = $5.u;
  }

GNUInlineAttrNode: GNUInlineAttr Attr
  {
    $$.GNUInlineAttr.node = $1;
    $$.GNUInlineAttr.self = $2;
  }

AllocSizeAttrNode: AllocSizeAttr Attr INTEGER integer {}
  {
    $$.AllocSizeAttr.node = $1;
    $$.AllocSizeAttr.self = $2;
    $$.AllocSizeAttr.position1 = $3.u;
    $$.AllocSizeAttr.position2 = $4.u;
  }

WarnUnusedResultAttrNode: WarnUnusedResultAttr Attr NAME DQNAME {}
  {
    $$.WarnUnusedResultAttr.node = $1;
    $$.WarnUnusedResultAttr.self = $2;
    $$.WarnUnusedResultAttr.name = $3;
    $$.WarnUnusedResultAttr.message = $4;
  }

AllocAlignAttrNode: AllocAlignAttr Attr INTEGER
  {
    $$.AllocAlignAttr.node = $1;
    $$.AllocAlignAttr.self = $2;
    $$.AllocAlignAttr.position = $3.u;
  }

TransparentUnionAttrNode: TransparentUnionAttr Attr
  {
    $$.TransparentUnionAttr.node = $1;
    $$.TransparentUnionAttr.self = $2;
  }

PackedAttrNode: PackedAttr Attr
  {
    $$.PackedAttr.node = $1;
    $$.PackedAttr.self = $2;
  }

PureAttrNode: PureAttr Attr
  {
    $$.PureAttr.node = $1;
    $$.PureAttr.self = $2;
  }

FullCommentNode: FullComment Comment
  {
    $$.FullComment.node = $1;
    $$.FullComment.self = $2;
  }

ParagraphCommentNode: ParagraphComment Comment
  {
    $$.ParagraphComment.node = $1;
    $$.ParagraphComment.self = $2;
  }

TextCommentNode: TextComment Comment TagText
  {
    $$.TextComment.node = $1;
    $$.TextComment.self = $2;
    $$.TextComment.text = $3;
  }

TranslationUnitDeclNode: TranslationUnitDecl Decl
  {
    $$.TranslationUnitDecl.node = $1;
    $$.TranslationUnitDecl.self = $2;
  }

TypedefDeclNode: TypedefDecl Decl NAME BareType
  {
    $$.TypedefDecl.node = $1;
    $$.TypedefDecl.self = $2;
    $$.TypedefDecl.name = $3;
    $$.TypedefDecl.type = $4;
  }

RecordDeclNode: RecordDecl Decl Class name opt_definition
  {
#define obj $$.RecordDecl
    obj.node = $1;
    obj.self = $2;
    SET_OPTIONS(obj, $3, class);
    obj.name = $4;
    obj.opt_definition = $5;
#undef obj
  }

FieldDeclNode: FieldDecl Decl name BareType
  {
#define obj $$.FieldDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
    obj.type = $4;
#undef obj
  }

FunctionDeclNode: FunctionDecl Decl NAME BareType storage opt_inline
  {
#define obj $$.FunctionDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
    obj.type = $4;
    SET_OPTIONS(obj, $5, storage);
    obj.opt_inline = $6;
#undef obj
  }

ParmVarDeclNode: ParmVarDecl Decl name BareType
  {
#define obj $$.ParmVarDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
    obj.type = $4;
#undef obj
  }

IndirectFieldDeclNode: IndirectFieldDecl Decl NAME BareType
  {
#define obj $$.IndirectFieldDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
    obj.type = $4;
#undef obj
  }

EnumDeclNode: EnumDecl Decl name
  {
#define obj $$.EnumDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
#undef obj
  }

EnumConstantDeclNode: EnumConstantDecl Decl NAME BareType
  {
#define obj $$.EnumConstantDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
    obj.type = $4;
#undef obj
  }

VarDeclNode: VarDecl Decl NAME BareType storage init_style
  {
#define obj $$.VarDecl
    obj.node = $1;
    obj.self = $2;
    obj.name = $3;
    obj.type = $4;
    SET_OPTIONS(obj, $5, storage);
    SET_OPTIONS(obj, $6, init_style);
#undef obj
  }

BuiltinTypeNode: BuiltinType Type
  {
    $$.BuiltinType.node = $1;
    $$.BuiltinType.self = $2;
  }

RecordTypeNode: RecordType Type {} 
  {
    $$.RecordType.node = $1;
    $$.RecordType.self = $2;
  }

PointerTypeNode: PointerType Type {}
  {
    $$.PointerType.node = $1;
    $$.PointerType.self = $2;
  }

ConstantArrayTypeNode: ConstantArrayType Type INTEGER {}
  {
    $$.ConstantArrayType.node = $1;
    $$.ConstantArrayType.self = $2;
    $$.ConstantArrayType.size = $3.u;
  }

ElaboratedTypeNode: ElaboratedType Type {}
  {
    $$.ElaboratedType.node = $1;
    $$.ElaboratedType.self = $2;
  }

TypedefTypeNode: TypedefType Type {}
  {
    $$.TypedefType.node = $1;
    $$.TypedefType.self = $2;
  }

QualTypeNode: QualType Type opt_const opt_volatile {}
  {
    $$.QualType.node = $1;
    $$.QualType.self = $2;
    $$.QualType.opt_const = $3;
    $$.QualType.opt_volatile = $4;
  }

EnumTypeNode: EnumType Type {}
  {
    $$.EnumType.node = $1;
    $$.EnumType.self = $2;
  }

FunctionProtoTypeNode: FunctionProtoType Type NAME {}
  {
    $$.FunctionProtoType.node = $1;
    $$.FunctionProtoType.self = $2;
    $$.FunctionProtoType.name = $3;
  }

ParenTypeNode: ParenType Type {}
  {
    $$.ParenType.node = $1;
    $$.ParenType.self = $2;
  }

CompoundStmtNode: CompoundStmt Stmt
  {
    $$.CompoundStmt.node = $1;
    $$.CompoundStmt.self = $2;
  }

ReturnStmtNode: ReturnStmt Stmt
  {
    $$.ReturnStmt.node = $1;
    $$.ReturnStmt.self = $2;
  }

DeclStmtNode: DeclStmt Stmt
  {
    $$.DeclStmt.node = $1;
    $$.DeclStmt.self = $2;
  }

WhileStmtNode: WhileStmt Stmt
  {
    $$.WhileStmt.node = $1;
    $$.WhileStmt.self = $2;
  }

IfStmtNode: IfStmt Stmt opt_has_else
  {
    $$.IfStmt.node = $1;
    $$.IfStmt.self = $2;
    $$.IfStmt.opt_has_else = $3;
  }

ForStmtNode: ForStmt Stmt
  {
    $$.ForStmt.node = $1;
    $$.ForStmt.self = $2;
  }

NullStmtNode: NullStmt Stmt
  {
    $$.NullStmt.node = $1;
    $$.NullStmt.self = $2;
  }

GotoStmtNode: GotoStmt Stmt Label
  {
    $$.GotoStmt.node = $1;
    $$.GotoStmt.self = $2;
    $$.GotoStmt.label = $3;
  }

SwitchStmtNode: SwitchStmt Stmt
  {
    $$.SwitchStmt.node = $1;
    $$.SwitchStmt.self = $2;
  }

CaseStmtNode: CaseStmt Stmt
  {
    $$.CaseStmt.node = $1;
    $$.CaseStmt.self = $2;
  }

DefaultStmtNode: DefaultStmt Stmt
  {
    $$.DefaultStmt.node = $1;
    $$.DefaultStmt.self = $2;
  }

LabelStmtNode: LabelStmt Stmt SQNAME
  {
    $$.LabelStmt.node = $1;
    $$.LabelStmt.self = $2;
    $$.LabelStmt.name = $3;
  }

ContinueStmtNode: ContinueStmt Stmt
  {
    $$.ContinueStmt.node = $1;
    $$.ContinueStmt.self = $2;
  }

BreakStmtNode: BreakStmt Stmt
  {
    $$.BreakStmt.node = $1;
    $$.BreakStmt.self = $2;
  }

DoStmtNode: DoStmt Stmt
  {
    $$.DoStmt.node = $1;
    $$.DoStmt.self = $2;
  }

ParenExprNode: ParenExpr Expr
  {
    $$.ParenExpr.node = $1;
    $$.ParenExpr.self = $2;
  }

DeclRefExprNode: DeclRefExpr Expr DeclRef non_odr_use {}
  {
    $$.DeclRefExpr.node = $1;
    $$.DeclRefExpr.self = $2;
    $$.DeclRefExpr.ref = $3;

#define obj $$.DeclRefExpr
    SET_OPTIONS(obj, $4, non_odr_use);
#undef obj
  }

ConstantExprNode: ConstantExpr Expr {}
  {
    $$.ConstantExpr.node = $1;
    $$.ConstantExpr.self = $2;
  }

CallExprNode: CallExpr Expr {}
  {
    $$.CallExpr.node = $1;
    $$.CallExpr.self = $2;
  }

MemberExprNode: MemberExpr Expr Member {}
  {
    $$.MemberExpr.node = $1;
    $$.MemberExpr.self = $2;
    $$.MemberExpr.member = $3;
  }

ArraySubscriptExprNode: ArraySubscriptExpr Expr {}
  {
    $$.ArraySubscriptExpr.node = $1;
    $$.ArraySubscriptExpr.self = $2;
  }

InitListExprNode: InitListExpr Expr {}
  {
    $$.InitListExpr.node = $1;
    $$.InitListExpr.self = $2;
  }

OffsetOfExprNode: OffsetOfExpr Expr {}
  {
    $$.OffsetOfExpr.node = $1;
    $$.OffsetOfExpr.self = $2;
  }

UnaryExprOrTypeTraitExprNode: UnaryExprOrTypeTraitExpr Expr Trait argument_type {}
  {
    $$.UnaryExprOrTypeTraitExpr.node = $1;
    $$.UnaryExprOrTypeTraitExpr.self = $2;

#define obj $$.UnaryExprOrTypeTraitExpr
    SET_OPTIONS(obj, $3, trait);
#undef obj

    $$.UnaryExprOrTypeTraitExpr.argument_type = $4;
  }

StmtExprNode: StmtExpr Expr {}
  {
    $$.StmtExpr.node = $1;
    $$.StmtExpr.self = $2;
  }

IntegerLiteralNode: IntegerLiteral Expr INTEGER
  {
    $$.IntegerLiteral.node = $1;
    $$.IntegerLiteral.self = $2;
    $$.IntegerLiteral.value = $3;
  }

CharacterLiteralNode: CharacterLiteral Expr INTEGER
  {
    $$.CharacterLiteral.node = $1;
    $$.CharacterLiteral.self = $2;
    $$.CharacterLiteral.value = $3.i;
  }

StringLiteralNode: StringLiteral Expr DQNAME
  {
    $$.StringLiteral.node = $1;
    $$.StringLiteral.self = $2;
    $$.StringLiteral.value = $3;
  }

UnaryOperatorNode: UnaryOperator Expr PrefixOrPostfix Operator opt_cannot_overflow
  {
    $$.UnaryOperator.node = $1;
    $$.UnaryOperator.self = $2;

#define obj $$.UnaryOperator
    SET_OPTIONS(obj, $3, prefix_or_postfix);
    SET_OPTIONS(obj, $4, operator);
#undef obj

    $$.UnaryOperator.opt_cannot_overflow = $5;
  }

BinaryOperatorNode: BinaryOperator Expr Operator
  {
    $$.BinaryOperator.node = $1;
    $$.BinaryOperator.self = $2;

#define obj $$.BinaryOperator
    SET_OPTIONS(obj, $3, operator);
#undef obj
  }

ConditionalOperatorNode: ConditionalOperator Expr {}
CompoundAssignOperatorNode: CompoundAssignOperator Expr Operator TagComputeLHSTy TagComputeResultTy {}
  {
    $$.CompoundAssignOperator.node = $1;
    $$.CompoundAssignOperator.self = $2;

#define obj $$.CompoundAssignOperator
    SET_OPTIONS(obj, $3, operator);
#undef obj

    $$.CompoundAssignOperator.computation_lhs_type = $4;
    $$.CompoundAssignOperator.computation_result_type = $5;
  }

CStyleCastExprNode: CStyleCastExpr Expr Cast
  {
    $$.CStyleCastExpr.node = $1;
    $$.CStyleCastExpr.self = $2;

#define obj $$.CStyleCastExpr
    SET_OPTIONS(obj, $3, cast);
#undef obj
  }

ImplicitCastExprNode: ImplicitCastExpr Expr Cast opt_part_of_explicit_cast
  {
    $$.ImplicitCastExpr.node = $1;
    $$.ImplicitCastExpr.self = $2;

#define obj $$.ImplicitCastExpr
    SET_OPTIONS(obj, $3, cast);
#undef obj

    $$.ImplicitCastExpr.opt_part_of_explicit_cast = $4;
  }

Attr: POINTER AngledRange opt_Inherited opt_Implicit
  {
    $$.pointer = $1.u;
    $$.range = $2;
    $$.opt_Inherited = $3;
    $$.opt_Implicit = $4;
  }

Comment: POINTER AngledRange
  {
    $$.pointer = $1.u;
    $$.range = $2;
  }

Decl: POINTER parent prev AngledRange Loc opt_imported opt_implicit used_or_referenced opt_undeserialized_declarations
  {
    $$.pointer = $1.u;
    $$.parent = $2;
    $$.prev = $3;
    $$.range = $4;
    $$.loc = $5;
    $$.opt_imported = $6;
    $$.opt_implicit = $7;

#define obj $$
    SET_OPTIONS($$, $8, used_or_referenced);
#undef obj

    $$.opt_undeserialized_declarations = $9;
  }

Type: POINTER BareType opt_sugar opt_imported
  {
    $$.pointer = $1.u;
    $$.type = $2;
    $$.opt_sugar = $3;
    $$.opt_imported = $4;
  }

Stmt: POINTER AngledRange
  {
    $$.pointer = $1.u;
    $$.range = $2;
  }

Expr: Stmt BareType value_kind object_kind
  {
    $$.stmt = $1;
    $$.type = $2;
    $$.value_kind = $3;
    $$.object_kind = $4;
  }

Member: MemberAccess MemberDecl POINTER
  {
    $$.dot = $1 == TOK_OPT_dot;
    $$.anonymous = $2.anonymous;
    $$.name = $2.name;
    $$.pointer = $3.u;
  }

MemberAccess: OPT_arrow
 | OPT_dot

MemberDecl: NAME { $$ = (MemberDecl){0, $1}; }
 | ANAME         { $$ = (MemberDecl){1, $1}; }

Operator: OPT_Comma
 | OPT_Remainder
 | OPT_Division
 | OPT_Multiplication
 | OPT_Subtraction
 | OPT_Addition
 | OPT_BitwiseAND
 | OPT_BitwiseOR
 | OPT_BitwiseXOR
 | OPT_BitwiseNOT
 | OPT_LogicalAND
 | OPT_LogicalOR
 | OPT_LogicalNOT
 | OPT_GreaterThan
 | OPT_GreaterThanOrEqual
 | OPT_LessThan
 | OPT_LessThanOrEqual
 | OPT_Equality
 | OPT_Inequality
 | OPT_Assignment
 | OPT_AdditionAssignment
 | OPT_SubtractionAssignment
 | OPT_MultiplicationAssignment
 | OPT_DivisionAssignment
 | OPT_RemainderAssignment
 | OPT_BitwiseXORAssignment
 | OPT_BitwiseORAssignment
 | OPT_BitwiseANDAssignment
 | OPT_RightShift
 | OPT_RightShiftAssignment
 | OPT_LeftShift
 | OPT_LeftShiftAssignment
 | OPT_Decrement
 | OPT_Increment
 | OPT_Extension

Cast: OPT_IntegralCast
 | OPT_LValueToRValue
 | OPT_FunctionToPointerDecay
 | OPT_BuiltinFnToFnPtr
 | OPT_BitCast
 | OPT_NullToPointer
 | OPT_NoOp
 | OPT_ToVoid
 | OPT_ArrayToPointerDecay
 | OPT_IntegralToFloating
 | OPT_IntegralToPointer

Trait: OPT_alignof
 | OPT_sizeof

Class: OPT_struct
 | OPT_union
 | OPT_enum

PrefixOrPostfix: OPT_prefix
 | OPT_postfix

Label: SQNAME POINTER
  {
    $$.name = $1;
    $$.pointer = $2.u;
  }

DeclRef: NAME POINTER SQNAME BareType
  {
    $$.decl = $1;
    $$.pointer = $2.u;
    $$.name = $3;
    $$.type = $4;
  }

AngledRange: '<' Range '>' { $$ = $2; }

Range: Loc      { $$ = (Range){$1, $1}; }
 | Loc ',' Loc  { $$ = (Range){$1, $3}; }

Loc: INVALID_SLOC { $$ = (Loc){}; }
 | FileLoc
 | LineLoc
 | ColLoc

FileLoc: SRC ':' INTEGER ':' INTEGER
  {
    last_loc_src = $1;
    last_loc_line = $3.u;
    $$ = (Loc){last_loc_src, last_loc_line, $5.u};
  }

LineLoc: LINE ':' INTEGER ':' INTEGER
  {
    last_loc_line = $3.u;
    $$ = (Loc){last_loc_src, last_loc_line, $5.u};
  }

ColLoc: COL ':' INTEGER
  {
    $$ = (Loc){last_loc_src, last_loc_line, $3.u};
  }

BareType: SQNAME      { $$ = (BareType){$1}; }
 | SQNAME ':' SQNAME  { $$ = (BareType){$1, $3}; }

ArgIndices: INTEGER
  {
    if ($1.u < 1 || $1.u > ARG_INDICES_MAX) {
      yyerror(&@$, uctx, "require a [1, %lu] index: %lld", ARG_INDICES_MAX, $1.u);
      YYERROR;
    }
    $$ = 1U << ($1.u - 1);
  }
 | ArgIndices INTEGER
  {
    if ($2.u < 1 || $2.u > ARG_INDICES_MAX) {
      yyerror(&@$, uctx, "require a [1, %lu] index: %lld", ARG_INDICES_MAX, $2.u);
      YYERROR;
    }
    $$ = $1 | (1U << ($2.u - 1));
  }

TagText: OPT_Text TEXT { $$ = $2; }

TagComputeLHSTy: OPT_ComputeLHSTy BareType { $$ = $2; }

TagComputeResultTy: OPT_ComputeResultTy BareType { $$ = $2; }

opt_inline:   { $$ = 0; }
 | OPT_inline { $$ = 1; }

opt_const:    { $$ = 0; }
 | OPT_const  { $$ = 1; }

opt_volatile:   { $$ = 0; }
 | OPT_volatile { $$ = 1; }

opt_cannot_overflow:    { $$ = 0; }
 | OPT_cannot_overflow  { $$ = 1; }

opt_part_of_explicit_cast:    { $$ = 0; }
 | OPT_part_of_explicit_cast  { $$ = 1; }

opt_sugar:    { $$ = 0; }
 | OPT_sugar  { $$ = 1; }

opt_imported:   { $$ = 0; }
 | OPT_imported { $$ = 1; }

opt_implicit:   { $$ = 0; }
 | OPT_implicit { $$ = 1; }

opt_has_else:   { $$ = 0; }
 | OPT_has_else { $$ = 1; }

opt_definition:   { $$ = 0; }
 | OPT_definition { $$ = 1; }

opt_IsLiteralLabel:   { $$ = 0; }
 | OPT_IsLiteralLabel { $$ = 1; }

opt_Inherited:    { $$ = 0; }
 | OPT_Inherited  { $$ = 1; }

opt_Implicit:   { $$ = 0; }
 | OPT_Implicit { $$ = 1; }

opt_undeserialized_declarations:    { $$ = 0; }
 | OPT_undeserialized_declarations  { $$ = 1; }

storage: { $$ = 0; }
 | OPT_extern
 | OPT_static

init_style: { $$ = 0; }
 | OPT_cinit
 | OPT_callinit
 | OPT_listinit
 | OPT_parenlistinit

used_or_referenced: { $$ = 0; }
 | OPT_used
 | OPT_referenced

non_odr_use: { $$ = 0; }
 | OPT_non_odr_use_unevaluated
 | OPT_non_odr_use_constant
 | OPT_non_odr_use_discarded

value_kind: { $$ = 0; }
 | OPT_lvalue

object_kind: { $$ = 0; }
 | OPT_bitfield

name: { $$ = NULL; }
 | NAME

integer: { $$ = (Integer){0}; }
 | INTEGER

argument_type: { $$ = (BareType){0}; }
 | BareType

prev:           { $$ = 0; }
 | PREV POINTER { $$ = $2.u; }

parent:           { $$ = 0; }
 | PARENT POINTER { $$ = $2.u; }

%%

#include "parse-error.c"