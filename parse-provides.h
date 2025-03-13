#include "util.h"

extern long ts;
extern char tu[PATH_MAX];
extern char cwd[PATH_MAX];

typedef DECL_ARRAY(ANON, Node) NodeList;
typedef DECL_ARRAY(SemanticsList, Semantics) SemanticsList;

static inline IMPL_ARRAY_PUSH(SemanticsList, Semantics);

extern NodeList all_nodes;
extern StringSet all_strings;
extern SemanticsList all_semantics;

#define YY_DECL                                                                \
  yytoken_kind_t yylex(YYSTYPE *yylval, YYLTYPE *yylloc,                       \
                       const UserContext *uctx)

void yyerror(const YYLTYPE *loc, const UserContext *uctx, char const *format,
             ...) __attribute__((__format__(__printf__, 3, 4)));

const String *add_string(struct string s, uint8_t property);

void parse_init();
void parse_halt();

int parse(YYLTYPE *lloc, const UserContext *uctx);
int parse_line(char *line, size_t n, size_t cap, YYLTYPE *lloc,
               const UserContext *uctx,
               int (*parse_hook)(YYLTYPE *lloc, const UserContext *uctx));