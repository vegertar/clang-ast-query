%require "3.7"

// Emitted on top of the implementation file.
%code top {
  #include "parse-top.h"
}

// Emitted in the header file, before the opt_definition of YYSTYPE.
%code requires {
  #include "array.h"
  #include "parse-requires.h"
 
  DECL_ARRAY(array, void *);

  struct loc {
    const char *file;
    unsigned line;
    unsigned col;
  };

  struct range {
    struct loc begin;
    struct loc end;
  };

  struct type {
    const char *qualified;
    const char *desugared;
  };

  struct tag {
    const char *name;
    struct type type;
  };

  DECL_ARRAY(tags, struct tag);

  struct op {
    const char *operator;
    struct tags tags;
  };

  enum mem_kind {
    MEM_KIND_NIL,
    MEM_KIND_ARROW,
    MEM_KIND_DOT,
  };

  struct mem {
    enum mem_kind kind;
    const char *name;
    const char *pointer;
  };

  struct ref {
    const char *name;
    const char *pointer;
    const char *sqname;
    struct type type;
  };

  struct def {
    struct type type;
    struct array specs;
    const char *value;
    struct op op;
    struct mem mem;
    _Bool field;
    struct ref ref;
    const char *cast;
  };

  struct comment {
    const char *text;
  };

  enum decl_kind {
    DECL_KIND_NIL,
    DECL_KIND_V1,
    DECL_KIND_V2,
    DECL_KIND_V3,
    DECL_KIND_V4,
    DECL_KIND_V5,
    DECL_KIND_V6,
    DECL_KIND_V7,
    DECL_KIND_V8,
    DECL_KIND_V9,
    DECL_KIND_V10,
    DECL_KIND_V11,
    DECL_KIND_V12,
  };

  struct decl {
    enum decl_kind kind;
    union {
      struct {
        const char *class;
      } v1;
      struct {
        const char *name;
      } v2;
      struct {
        const char *class;
        const char *name;
      } v3;
      struct {
        const char *sqname;
        const char *pointer;
      } v4;
      struct {
        const char *sqname;
        const char *trait;
      } v5;
      struct {
        const char *sqname;
        const char *trait;
        struct def def;
      } v6;
      struct {
        const char *sqname;
        struct def def;
      } v7;
      struct {
        struct def def;
      } v8;
      struct {
        const char *name;
        struct def def;
      } v9;
      struct {
        struct array seq;
      } v10;
      struct {
        const char *name;
        struct array seq;
      } v11;
      struct {
        struct comment comment;
      } v12;
    } variants;
  };

  enum node_kind {
    NODE_KIND_HEAD,
    NODE_KIND_ENUM,
    NODE_KIND_NULL,
    NODE_KIND_TOKEN,
  };

  struct node {
    int level;
    enum node_kind kind;
    const char *name;
    const char *pointer;
    const char *parent;
    const char *prev;
    const char *macro;
    struct range range;
    struct loc loc;
    struct array attrs;
    struct array labels;
    struct decl decl;
    struct array opts;
  };

  DECL_ARRAY(ast, struct node);

  struct string_pair {
    const char *k;
    const char *v;
  };

  DECL_ARRAY(string_map, struct string_pair);

  struct src {
    const char *filename;
    unsigned number;
  };

  DECL_ARRAY(src_set, struct src);

  DECL_ARRAY(loc_exp_set, struct range);

  DECL_ARRAY(inactive_set, struct range);

  struct tok {
    struct loc loc;

    /*
     * Offset is used to identify the situation of tokens.
     *
     * -2: Testing only macros, i.e., without expansion occurring. E.g., #ifdef MACRO
     * -1: Non-macro identifiers; this is the default case.
     *  0: Expandable macros. E.g., #if MACRO, int x = MACRO;
     * >0: Expanded tokens from macro expansion, always accompanied by a 0-offset token.
     */
    int offset;
  };

  struct tok_decl_pair {
    struct tok tok;
    const char *decl;
  };

  DECL_ARRAY(tok_decl_set, struct tok_decl_pair);

  struct tok_kind_pair {
    struct range tok;
    const char *kind;
  };

  DECL_ARRAY(tok_kind_set, struct tok_kind_pair);

  struct exp_expr_pair {
    struct loc exp;
    const char *expr;
  };

  DECL_ARRAY(exp_expr_set, struct exp_expr_pair);

  // Exchanging information with the parser.
  typedef struct {
    // Whether to not emit error messages.
    int silent;
    // The current input line.
    const char *line;
  } user_context;
}

// Emitted in the header file, after the opt_definition of YYSTYPE.
%code provides {
  // Tell Flex the expected prototype of yylex.
  #define YY_DECL \
    yytoken_kind_t yylex(YYSTYPE *yylval, YYLTYPE *yylloc, const user_context *uctx)

  YY_DECL;
  void yyerror(const YYLTYPE *loc, const user_context *uctx,
               char const *format, ...)
    __attribute__ ((__format__ (__printf__, 3, 4)));

  extern struct ast ast;
  extern struct src_set src_set;
  extern struct loc_exp_set loc_exp_set;
  extern struct inactive_set inactive_set;
  extern struct tok_decl_set tok_decl_set;
  extern struct tok_kind_set tok_kind_set;
  extern struct exp_expr_set exp_expr_set;
  extern struct string_map var_type_map;
  extern struct string_map decl_def_map;
  extern struct array exported_symbols;

  #ifndef PATH_MAX
  # define PATH_MAX 4096
  #endif // !PATH_MAX

  extern long ts;
  extern char tu[PATH_MAX];
  extern char cwd[PATH_MAX];

  int parse(YYLTYPE *lloc, const user_context *uctx);
  void destroy();

  /// If the filename is not found, a copy will be added and returned.
  const char *add_src(const char *filename);
  /// The memory of the found filename is owned by the underlying array.
  const char *search_src(const char *filename, unsigned *number);

  /// Memory ownership of the parameters will be transferred to the underlying map.
  struct string_pair add_string_map(struct string_map *map, const char *k, const char *v);
  /// The memory of the found type is owned by the underlying map.
  const char *find_string_map(struct string_map *map, const char *k);

  #include "parse-provides.h"
}

// Emitted in the implementation file.
%code {
  #include "test.h"
  #include "print.h"
  #include <assert.h>
  #include <stdio.h>
  #include <errno.h>
  #include <limits.h>

  #include "parse-impl.c"

  struct ast ast;
  struct src_set src_set;
  struct loc_exp_set loc_exp_set;
  struct inactive_set inactive_set;
  struct tok_decl_set tok_decl_set;
  struct tok_kind_set tok_kind_set;
  struct exp_expr_set exp_expr_set;
  struct string_map var_type_map;
  struct string_map decl_def_map;
  struct array exported_symbols;

  long ts;
  char tu[PATH_MAX];
  char cwd[PATH_MAX];

  static const char *last_loc_src;
  static unsigned last_loc_line;

  static char *get_pointer(char *s);

  #define DECL_METHOD(name, method, ...)                   \
    struct name * name##_##method(struct name *p, ##__VA_ARGS__)

  static DECL_METHOD(array, push, void *);
  static DECL_METHOD(array, clear, int);

  static DECL_METHOD(tags, push, struct tag);
  static DECL_METHOD(tags, clear, int);

  static DECL_METHOD(ast, push, struct node);
  static DECL_METHOD(ast, clear, int);

  static DECL_METHOD(src_set, clear, int);

  static DECL_METHOD(string_map, clear, int);

  static DECL_METHOD(loc_exp_set, push, struct range);
  static DECL_METHOD(loc_exp_set, clear, int);

  static DECL_METHOD(inactive_set, push, struct range);
  static DECL_METHOD(inactive_set, clear, int);

  static DECL_METHOD(tok_decl_set, push, struct tok_decl_pair);
  static DECL_METHOD(tok_decl_set, clear, int);

  static DECL_METHOD(tok_kind_set, push, struct tok_kind_pair);
  static DECL_METHOD(tok_kind_set, clear, int);

  static DECL_METHOD(exp_expr_set, push, struct exp_expr_pair);
  static DECL_METHOD(exp_expr_set, clear, int);
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
%param {const user_context *uctx}

// Formatting semantic values in debug traces.
%printer { fprintf(yyo, "%s", "\\n"); } EOL;
%printer { fprintf(yyo, "%s", "<invalid loc>"); } INVALID_SLOC;
%printer { fprintf(yyo, "%s", "line"); } LINE;
%printer { fprintf(yyo, "%s", "col"); } COL;
%printer { fprintf(yyo, "%d", $$); } <int>;
// %printer { fprintf(yyo, "\"%s\"", $$); } <char *>;
// %printer { print_type(yyo, &$$); } <struct type>;
// %printer { print_array(yyo, &$$); } <struct array>;
// %printer { print_loc(yyo, &$$); } <Loc>;
// %printer { print_range(yyo, &$$); } <Range>;
// %printer { print_node(yyo, &$$); } <struct node>;

%token
    EOL
    INVALID_SLOC
    LINE
    COL
    NULL
    PREV
    PARENT
  <int>
    INDENT
  <unsigned>
    TranslationUnitDecl
    IndirectFieldDecl
    EnumConstantDecl
    FunctionDecl
    ParmVarDecl
    TypedefDecl
    RecordDecl
    FieldDecl
    EnumDecl
    VarDecl

    ConstantArrayType
    FunctionProtoType
    ElaboratedType
    BuiltinType
    PointerType
    TypedefType
    RecordType
    ParenType
    QualType
    EnumType

    TransparentUnionAttr
    WarnUnusedResultAttr
    ReturnsTwiceAttr
    AllocAlignAttr
    DeprecatedAttr
    GNUInlineAttr
    AllocSizeAttr
    RestrictAttr
    AsmLabelAttr
    AlignedAttr
    NoThrowAttr
    NonNullAttr
    BuiltinAttr
    PackedAttr
    FormatAttr
    ConstAttr
    PureAttr
    ModeAttr

    CompoundStmt
    ContinueStmt
    DefaultStmt
    SwitchStmt
    ReturnStmt
    LabelStmt
    BreakStmt
    WhileStmt
    DeclStmt
    CaseStmt
    NullStmt
    GotoStmt
    ForStmt
    IfStmt

    UnaryExprOrTypeTraitExpr
    ArraySubscriptExpr
    ImplicitCastExpr
    CStyleCastExpr
    ConstantExpr
    InitListExpr
    OffsetOfExpr
    DeclRefExpr
    MemberExpr
    ParenExpr
    CallExpr

    CompoundAssignOperator
    ConditionalOperator
    BinaryOperator
    UnaryOperator

    CharacterLiteral
    IntegerLiteral
    StringLiteral

    ParagraphComment
    FullComment
    TextComment

    Typedef
    Record
    Field
    Enum

    IntValue

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
    SRC
%nterm
  <Loc>
    Loc
    FileLoc
    LineLoc
    ColLoc
  <Range>
    Range
    AngledRange
  <BareType>
    argument_type
    BareType
  <Node>
    Node

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

    ParenExprNode
    DeclRefExprNode
    ConstantExprNode
    CallExprNode
    MemberExprNode
    ArraySubscriptExprNode
    InitListExprNode
    OffsetOfExprNode
    UnaryExprOrTypeTraitExprNode

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
    Text
%%

// Naming conventions:
// - All lowercase leading names are optional non-terminal tokens.
// - All "opt_" leading names are both optional and represent options.

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
 | IntValue INTEGER {}
 | Enum POINTER SQNAME {} 
 | Typedef POINTER BareType {} 
 | Record POINTER BareType {}
 | Field POINTER SQNAME BareType {}

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

 | ParenExprNode
 | DeclRefExprNode
 | ConstantExprNode
 | CallExprNode
 | MemberExprNode
 | ArraySubscriptExprNode
 | InitListExprNode
 | OffsetOfExprNode
 | UnaryExprOrTypeTraitExprNode

 | LiteralNode {}
 | OperatorNode {}
 | CastExprNode {}

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

TextCommentNode: TextComment Comment Text
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

LiteralNode: IntegerLiteral Expr INTEGER {}
 | CharacterLiteral Expr INTEGER {} 
 | StringLiteral Expr DQNAME {}

OperatorNode: UnaryOperator Expr PrefixOrPostfix Operator opt_cannot_overflow {}
 | BinaryOperator Expr Operator {}
 | ConditionalOperator Expr {}
 | CompoundAssignOperator Expr Operator ComputeLHSTy ComputeResultTy {}

CastExprNode: CStyleCastExpr CastExpr {}
 | ImplicitCastExpr CastExpr opt_part_of_explicit_cast {}

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

CastExpr: Expr Cast

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

Range: Loc { $$ = (Range){$1, $1}; }
 | Loc ',' Loc { $$ = (Range){$1, $3}; }

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

BareType: SQNAME     { $$ = (BareType){$1}; }
 | SQNAME ':' SQNAME { $$ = (BareType){$1, $3}; }

ArgIndices: INTEGER
  {
    if ($1.u < 1 || $1.u > ARG_INDICES_MAX) {
      yyerror(&@$, uctx, "require a [1, %lu] index: %lld", ARG_INDICES_MAX, $1.u);
      YYERROR;
    }
    $$ = 0U << $1.u;
  }
 | ArgIndices INTEGER
  {
    if ($2.u < 1 || $2.u > ARG_INDICES_MAX) {
      yyerror(&@$, uctx, "require a [1, %lu] index: %lld", ARG_INDICES_MAX, $2.u);
      YYERROR;
    }
    $$ = $1 | (0U << $2.u);
  }

Text: OPT_Text DQNAME { $$ = $2; }

ComputeLHSTy: OPT_ComputeLHSTy BareType

ComputeResultTy: OPT_ComputeResultTy BareType

opt_inline:   { $$ = 0; }
 | OPT_inline { $$ = 1; }

opt_const:   { $$ = 0; }
 | OPT_const { $$ = 1; }

opt_volatile:   { $$ = 0; }
 | OPT_volatile { $$ = 1; }

opt_cannot_overflow:   { $$ = 0; }
 | OPT_cannot_overflow { $$ = 1; }

opt_part_of_explicit_cast:   { $$ = 0; }
 | OPT_part_of_explicit_cast { $$ = 1; }

opt_sugar:   { $$ = 0; }
 | OPT_sugar { $$ = 1; }

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

opt_Inherited:   { $$ = 0; }
 | OPT_Inherited { $$ = 1; }

opt_Implicit:   { $$ = 0; }
 | OPT_Implicit { $$ = 1; }

opt_undeserialized_declarations:   { $$ = 0; }
 | OPT_undeserialized_declarations { $$ = 1; }

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

prev:             { $$ = 0; }
 | PREV POINTER   { $$ = $2.u; }

parent:           { $$ = 0; }
 | PARENT POINTER { $$ = $2.u; }

%%

static const char *error_format_string(int argc) {
  switch (argc) {
  default: // Avoid compiler warnings.
  case 0:
    return ("%@: syntax error");
  case 1:
    return ("%@: syntax error: unexpected %u");
    // TRANSLATORS: '%@' is a location in a file, '%u' is an
    // "unexpected token", and '%0e', '%1e'... are expected tokens
    // at this point.
    //
    // For instance on the expression "1 + * 2", you'd get
    //
    // 1.5: syntax error: expected - or ( or number or function or variable
    // before *
  case 2:
    return ("%@: syntax error: expected %0e before %u");
  case 3:
    return ("%@: syntax error: expected %0e or %1e before %u");
  case 4:
    return ("%@: syntax error: expected %0e or %1e or %2e before %u");
  case 5:
    return ("%@: syntax error: expected %0e or %1e or %2e or %3e before %u");
  case 6:
    return (
        "%@: syntax error: expected %0e or %1e or %2e or %3e or %4e before %u");
  case 7:
    return ("%@: syntax error: expected %0e or %1e or %2e or %3e or %4e or "
            "%5e before %u");
  case 8:
    return ("%@: syntax error: expected %0e or %1e or %2e or %3e or %4e or "
            "%5e etc., before %u");
  }
}

int yyreport_syntax_error(const yypcontext_t *ctx, const user_context *uctx) {
  if (uctx->silent)
    return 0;

  enum { ARGS_MAX = 6 };
  yysymbol_kind_t arg[ARGS_MAX];
  int argsize = yypcontext_expected_tokens(ctx, arg, ARGS_MAX);
  if (argsize < 0)
    return argsize;

  const int too_many_expected_tokens = argsize == 0 && arg[0] != YYSYMBOL_YYEMPTY;
  if (too_many_expected_tokens)
    argsize = ARGS_MAX;

  const char *format = error_format_string(1 + argsize + too_many_expected_tokens);
  const YYLTYPE *loc = yypcontext_location(ctx);
  while (*format) {
    // %@: location.
    if (format[0] == '%' && format[1] == '@') {
      YYLOCATION_PRINT(stderr, loc);
      format += 2;
    }
    // %u: unexpected token.
    else if (format[0] == '%' && format[1] == 'u') {
      fputs(yysymbol_name(yypcontext_token(ctx)), stderr);
      format += 2;
    }
    // %0e, %1e...: expected token.
    else if (format[0] == '%' && isdigit((unsigned char)format[1]) &&
             format[2] == 'e' && (format[1] - '0') < argsize) {
      int i = format[1] - '0';
      fputs(yysymbol_name(arg[i]), stderr);
      format += 3;
    } else {
      fputc(*format, stderr);
      ++format;
    }
  }

  fputc('\n', stderr);

  // Quote the source line.
  {
    fprintf(stderr, "%5d | ", loc->first_line);
    if (!uctx->line) {
      fputs("(null)\n", stderr);
    } else {
      size_t n = strlen(uctx->line);
      fwrite(uctx->line, 1, n, stderr);
      if (n == 0 || uctx->line[n-1] != '\n') {
        putc('\n', stderr);
      }
    }
    fprintf(stderr, "%5s | %*s", "", loc->first_column, "^");
    for (int i = loc->last_column - loc->first_column - 1; 0 < i; --i)
      putc('~', stderr);
    putc('\n', stderr);
  }
  return 0;
}

void yyerror (const YYLTYPE *loc, const user_context *uctx, const char *format, ...) {
  if (uctx->silent)
    return;

  YYLOCATION_PRINT(stderr, loc);

  fputs(": ", stderr);
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  putc('\n', stderr);
}

int parse(YYLTYPE *lloc, const user_context *uctx) {
  yypstate *ps = yypstate_new();
  int status = 0;
  do {
    YYSTYPE lval;
    yytoken_kind_t token = yylex(&lval, lloc, uctx);
    status = yypush_parse(ps, token, &lval, lloc, uctx);
  } while (status == YYPUSH_MORE);
  yypstate_delete(ps);
  return status;
}

static int compare_src(const void *a, const void *b, size_t n) {
  const char *x = ((const struct src *)a)->filename;
  const char *y = ((const struct src *)b)->filename;
  return strcmp(x, y);
}

static void *init_src(void *a, const void *b, size_t n) {
  struct src *x = (struct src *)a;
  const struct src *y = (const struct src *)b;
  x->filename = strdup(y->filename);
  x->number = y->number;
  return x;
}

static void free_src(void *p) {
  struct src *src = (struct src *)p;
  free((void *)src->filename);
}

static IMPL_ARRAY_BSEARCH(src_set, compare_src)
static IMPL_ARRAY_BADD(src_set, init_src)
static IMPL_ARRAY_CLEAR(src_set, free_src)

const char * search_src(const char *filename, unsigned *number) {
  const char *s = NULL;
  unsigned j = -1;
  if (filename && src_set_bsearch(&src_set, &filename, &j)) {
    s = src_set.data[j].filename;
    if (number) *number = src_set.data[j].number;
  }

  return s;
}

const char * add_src(const char *filename) {
  unsigned i;
  struct src src = {filename, src_set.i};
  src_set_badd(&src_set, &src, &i);
  return src_set.data[i].filename;
}

static int compare_string_pair(const void *a, const void *b, size_t n) {
  const char *x = ((const struct string_pair *)a)->k;
  const char *y = ((const struct string_pair *)b)->k;
  return strcmp(x, y);
}

static void free_string_pair(void *p) {
  struct string_pair *pair = (struct string_pair *)p;
  free((void *)pair->k);
  free((void *)pair->v);
}

static IMPL_ARRAY_PUSH(string_map, struct string_pair)
static IMPL_ARRAY_BSEARCH(string_map, compare_string_pair)
static IMPL_ARRAY_BADD(string_map, NULL)
static IMPL_ARRAY_CLEAR(string_map, free_string_pair)

struct string_pair add_string_map(struct string_map *map, const char *k, const char *v) {
  struct string_pair pair = {k, v};
  _Bool added = string_map_badd(map, &pair, NULL);
  assert(added);
  return pair;
}

const char *find_string_map(struct string_map *map, const char *k) {
  unsigned i;
  _Bool found = string_map_bsearch(map, &k, &i);
  return found ? map->data[i].v : NULL;
}

static void sloc_destroy(struct loc *loc) {
  // the loc->file is owned by the global src_set
}

static void srange_destroy(struct range *range) {
  sloc_destroy(&range->begin);
  sloc_destroy(&range->end);
}

static void free_loc_exp(void *p) {
  srange_destroy((struct range *)p);
}

static IMPL_ARRAY_PUSH(loc_exp_set, struct range);
static IMPL_ARRAY_CLEAR(loc_exp_set, free_loc_exp);

static void free_inactive(void *p) {
  srange_destroy((struct range *)p);
}

static IMPL_ARRAY_PUSH(inactive_set, struct range);
static IMPL_ARRAY_CLEAR(inactive_set, free_inactive);

static void free_tok_decl(void *p) {
  struct tok_decl_pair *pair = (struct tok_decl_pair *)p;
  free((void *)pair->decl);
}

static IMPL_ARRAY_PUSH(tok_decl_set, struct tok_decl_pair);
static IMPL_ARRAY_CLEAR(tok_decl_set, free_tok_decl);

static void free_tok_kind(void *p) {
  struct tok_kind_pair *pair = (struct tok_kind_pair *)p;
  free((void *)pair->kind);
}

static IMPL_ARRAY_PUSH(tok_kind_set, struct tok_kind_pair);
static IMPL_ARRAY_CLEAR(tok_kind_set, free_tok_kind);

static void free_exp_expr(void *p) {
  struct exp_expr_pair *pair = (struct exp_expr_pair *)p;
  free((void *)pair->expr);
}

static IMPL_ARRAY_PUSH(exp_expr_set, struct exp_expr_pair);
static IMPL_ARRAY_CLEAR(exp_expr_set, free_exp_expr);

void destroy() {
  ast_clear(&ast, 1);
  src_set_clear(&src_set, 1);
  loc_exp_set_clear(&loc_exp_set, 1);
  inactive_set_clear(&inactive_set, 1);
  tok_decl_set_clear(&tok_decl_set, 1);
  tok_kind_set_clear(&tok_kind_set, 1);
  exp_expr_set_clear(&exp_expr_set, 1);
  string_map_clear(&var_type_map, 1);
  string_map_clear(&decl_def_map, 1);
  array_clear(&exported_symbols, 1);
}

static char *get_pointer(char *s) {
  if (!s) {
    return s;
  }
  char *delim = strchr(s, ' ');
  assert(delim);
  *delim = 0;
  return delim + 1;
}

static void type_destroy(struct type *type) {
  free((void *)type->qualified);
  free((void *)type->desugared);
}

static void tag_destroy(void *p) {
  struct tag *tag = (struct tag *)p;
  free((void *)tag->name);
  type_destroy(&tag->type);
}

static void op_destroy(struct op *op) {
  free((void *)op->operator);
  tags_clear(&op->tags, 1);
}

static void mem_destroy(struct mem *mem) {
  // the pointer was extracted from the name hence freeing name is enough
  switch (mem->kind) {
  case MEM_KIND_NIL:
    break;
  case MEM_KIND_ARROW:
    free((void *)(mem->name - 2));
    break;
  case MEM_KIND_DOT:
    free((void *)(mem->name - 1));
    break;
  default:
    fprintf(stderr, "Invalid mem kind: %d\n", mem->kind);
    abort();
  }
}

static void ref_destroy(struct ref *ref) {
  // the pointer was extracted from the name hence freeing name is enough
  free((void *)ref->name);
  free((void *)ref->sqname);
  type_destroy(&ref->type);
}

static void def_destroy(struct def *def) {
  type_destroy(&def->type);
  array_clear(&def->specs, 1);
  free((void *)def->value);
  op_destroy(&def->op);
  mem_destroy(&def->mem);
  ref_destroy(&def->ref);
  free((void *)def->cast);
}

static void comment_destroy(struct comment *comment) {
  free((void *)comment->text);
}

static void decl_destroy(struct decl *decl) {
  switch (decl->kind) {
  case DECL_KIND_NIL:
    break;
  case DECL_KIND_V1:
    free((void *)decl->variants.v1.class);
    break;
  case DECL_KIND_V2:
    free((void *)decl->variants.v2.name);
    break;
  case DECL_KIND_V3:
    free((void *)decl->variants.v3.class);
    free((void *)decl->variants.v3.name);
    break;
  case DECL_KIND_V4:
    free((void *)decl->variants.v4.sqname);
    free((void *)decl->variants.v4.pointer);
    break;
  case DECL_KIND_V5:
    free((void *)decl->variants.v5.sqname);
    free((void *)decl->variants.v5.trait);
    break;
  case DECL_KIND_V6:
    free((void *)decl->variants.v6.sqname);
    free((void *)decl->variants.v6.trait);
    def_destroy(&decl->variants.v6.def);
    break;
  case DECL_KIND_V7:
    free((void *)decl->variants.v7.sqname);
    def_destroy(&decl->variants.v7.def);
    break;
  case DECL_KIND_V8:
    def_destroy(&decl->variants.v8.def);
    break;
  case DECL_KIND_V9:
    free((void *)decl->variants.v9.name);
    def_destroy(&decl->variants.v9.def);
    break;
  case DECL_KIND_V10:
    array_clear(&decl->variants.v10.seq, 1);
    break;
  case DECL_KIND_V11:
    free((void *)decl->variants.v11.name);
    array_clear(&decl->variants.v11.seq, 1);
    break;
  case DECL_KIND_V12:
    comment_destroy(&decl->variants.v12.comment);
    break;
  default:
    fprintf(stderr, "%s:%s:%d: Invalid decl kind: %d\n",
            __FILE__, __func__, __LINE__, decl->kind);
    abort();
  }
}

static void node_destroy(void *p) {
  struct node *node = (struct node *)p;
  switch (node->kind) {
  case NODE_KIND_HEAD:
    // the pointer was extracted from the name hence freeing name is enough
    free((void *)node->name);
    free((void *)node->parent);
    free((void *)node->prev);
    srange_destroy(&node->range);
    sloc_destroy(&node->loc);
    array_clear(&node->attrs, 1);
    array_clear(&node->labels, 1);
    decl_destroy(&node->decl);
    array_clear(&node->opts, 1);
    break;
  case NODE_KIND_ENUM:
    free((void *)node->name);
    break;
  case NODE_KIND_NULL:
    break;
  case NODE_KIND_TOKEN:
    free((void *)node->name);
    free((void *)node->macro);
    srange_destroy(&node->range);
    sloc_destroy(&node->loc);
    array_clear(&node->attrs, 1);
    break;
  default:
    fprintf(stderr, "%s:%s:%d: Invalid node kind: %d\n",
            __FILE__, __func__, __LINE__, node->kind);
    abort();
  }
}

static void void_destroy(void *p) {
  free(*(void **)p);
}

static IMPL_ARRAY_PUSH(array, void *)
static IMPL_ARRAY_CLEAR(array, void_destroy)

static IMPL_ARRAY_PUSH(tags, struct tag)
static IMPL_ARRAY_CLEAR(tags, tag_destroy)

static IMPL_ARRAY_PUSH(ast, struct node)
static IMPL_ARRAY_CLEAR(ast, node_destroy)
