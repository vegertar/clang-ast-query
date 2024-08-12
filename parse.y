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
  <enum yytokentype>
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

  <intptr_t>
    POINTER
  <long long>
    INTEGER
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
    BareType
  <Node>
    Node
  <ArgIndices>
    ArgIndices
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

Node: NULL {}
 | IntValue INTEGER {}
 | Enum POINTER SQNAME {} 
 | Typedef POINTER BareType {} 
 | Record POINTER BareType {}
 | Field POINTER SQNAME BareType {}
 | AttrNode {}
 | CommentNode {}
 | DeclNode {}
 | TypeNode {}
 | StmtNode {} 

AttrNode: ModeAttr Attr NAME {}
 | NoThrowAttr Attr {}
 | NonNullAttr Attr ArgIndices {}
 | AsmLabelAttr Attr DQNAME opt_IsLiteralLabel {}
 | DeprecatedAttr Attr DQNAME DQNAME {}
 | BuiltinAttr Attr INTEGER {}
 | ReturnsTwiceAttr Attr {}
 | ConstAttr Attr {}
 | AlignedAttr Attr NAME {}
 | RestrictAttr Attr NAME {}
 | FormatAttr Attr NAME INTEGER INTEGER {}
 | GNUInlineAttr Attr {}
 | AllocSizeAttr Attr INTEGER integer {}
 | WarnUnusedResultAttr Attr NAME DQNAME {}
 | AllocAlignAttr Attr INTEGER {}
 | TransparentUnionAttr Attr {}
 | PackedAttr Attr {}
 | PureAttr Attr {}

CommentNode: FullComment Comment {}
 | ParagraphComment Comment {}
 | TextComment Comment Text {}

DeclNode: TranslationUnitDecl Decl {}
 | TypedefDecl Decl NAME BareType {}
 | RecordDecl Decl Class name opt_definition {}
 | FieldDecl Decl name BareType {}
 | FunctionDecl Decl NAME BareType opt_storage opt_inline {}
 | ParmVarDecl Decl name BareType {}
 | IndirectFieldDecl Decl NAME BareType {}
 | EnumDecl Decl name {}
 | EnumConstantDecl Decl NAME BareType {}
 | VarDecl Decl NAME BareType opt_storage opt_init_style {}

TypeNode: BuiltinType Type {}
 | RecordType Type {} 
 | PointerType Type {}
 | ConstantArrayType Type INTEGER {}
 | ElaboratedType Type {}
 | TypedefType Type {}
 | QualType Type opt_const opt_volatile {}
 | EnumType Type {}
 | FunctionProtoType Type NAME {}
 | ParenType Type {}

StmtNode: ExprNode {}
 | CompoundStmt Stmt {}
 | ReturnStmt Stmt {}
 | DeclStmt Stmt {}
 | WhileStmt Stmt {}
 | IfStmt Stmt opt_has_else {}
 | ForStmt Stmt {}
 | NullStmt Stmt {}
 | GotoStmt Stmt Label {}
 | SwitchStmt Stmt {}
 | CaseStmt Stmt {}
 | DefaultStmt Stmt {}
 | LabelStmt Stmt SQNAME {}
 | ContinueStmt Stmt {}
 | BreakStmt Stmt {}

ExprNode: LiteralNode {}
 | OperatorNode {}
 | CastExprNode {}
 | ParenExpr Expr {}
 | DeclRefExpr Expr DeclRef opt_non_odr_use {}
 | ConstantExpr Expr {}
 | CallExpr Expr {}
 | MemberExpr Expr Member {}
 | ArraySubscriptExpr Expr {}
 | InitListExpr Expr {}
 | OffsetOfExpr Expr {}
 | UnaryExprOrTypeTraitExpr Expr Trait argument_type {}

LiteralNode: IntegerLiteral Expr INTEGER {}
 | CharacterLiteral Expr INTEGER {} 
 | StringLiteral Expr DQNAME {}

OperatorNode: UnaryOperator Expr PrefixPostfix Operator opt_cannot_overflow {}
 | BinaryOperator Expr Operator {}
 | ConditionalOperator Expr {}
 | CompoundAssignOperator Expr Operator ComputeLHSTy ComputeResultTy {}

CastExprNode: CStyleCastExpr CastExpr {}
 | ImplicitCastExpr CastExpr opt_part_of_explicit_cast {}

Attr: POINTER AngledRange opt_Inherited opt_Implicit

Comment: POINTER AngledRange

Decl: POINTER parent prev AngledRange Loc opt_imported opt_implicit opt_used_or_referenced opt_undeserialized_declarations

Type: POINTER BareType opt_sugar opt_imported

Stmt: POINTER AngledRange

Expr: Stmt BareType opt_value_kind opt_object_kind

CastExpr: Expr Cast

Member: MemberAccess MemberDecl POINTER

MemberAccess: OPT_arrow
 | OPT_dot

MemberDecl: NAME
 | ANAME

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

Label: SQNAME POINTER

DeclRef: NAME POINTER SQNAME BareType

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
    last_loc_line = $3;
    $$ = (Loc){last_loc_src, last_loc_line, $5};
  }

LineLoc: LINE ':' INTEGER ':' INTEGER
  {
    last_loc_line = $3;
    $$ = (Loc){last_loc_src, last_loc_line, $5};
  }

ColLoc: COL ':' INTEGER
  {
    $$ = (Loc){last_loc_src, last_loc_line, $3};
  }

BareType: SQNAME { $$ = (BareType){$1}; }
 | SQNAME ':' SQNAME { $$ = (BareType){$1, $3}; }

ArgIndices: INTEGER
  {
    if ($1 < 1 || $1 > ARG_INDICES_MAX) {
      yyerror(&@$, uctx, "require a [1, %lu] index: %lld", ARG_INDICES_MAX, $1);
      YYERROR;
    }
    $$ = 0U << $1;
  }
 | ArgIndices INTEGER
  {
    if ($2 < 1 || $2 > ARG_INDICES_MAX) {
      yyerror(&@$, uctx, "require a [1, %lu] index: %lld", ARG_INDICES_MAX, $2);
      YYERROR;
    }
    $$ = $1 | (0U << $2);
  }

Text: OPT_Text DQNAME

ComputeLHSTy: OPT_ComputeLHSTy BareType

ComputeResultTy: OPT_ComputeResultTy BareType

PrefixPostfix: OPT_prefix
 | OPT_postfix

opt_storage:
 | OPT_extern
 | OPT_static

opt_inline:
 | OPT_inline

opt_const:
 | OPT_const

opt_volatile:
 | OPT_volatile

opt_init_style:
 | OPT_cinit
 | OPT_callinit
 | OPT_listinit
 | OPT_parenlistinit

opt_used_or_referenced:
 | OPT_used
 | OPT_referenced

opt_cannot_overflow:
 | OPT_cannot_overflow

opt_part_of_explicit_cast:
 | OPT_part_of_explicit_cast

opt_sugar:
 | OPT_sugar

opt_imported:
 | OPT_imported

opt_implicit:
 | OPT_implicit

opt_has_else:
 | OPT_has_else

opt_non_odr_use:
 | OPT_non_odr_use_unevaluated
 | OPT_non_odr_use_constant
 | OPT_non_odr_use_discarded

opt_value_kind:
 | OPT_lvalue

opt_object_kind:
 | OPT_bitfield

opt_definition:
 | OPT_definition

opt_IsLiteralLabel:
 | OPT_IsLiteralLabel

opt_Inherited:
 | OPT_Inherited

opt_Implicit:
 | OPT_Implicit

opt_undeserialized_declarations:
 | OPT_undeserialized_declarations

name:
 | NAME

integer:
 | INTEGER

argument_type:
 | BareType

prev:
 | PREV POINTER

parent:
 | PARENT POINTER

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
