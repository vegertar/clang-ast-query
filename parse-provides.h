#include "util.h"

// Note that a string could have multiple properties due to the same hash.
enum string_property : STRING_PROPERTY_TYPE {
  SP_FILE = 1U,
  SP_IDENTIFIER = 1U << 1,
  SP_BUILTIN = 1U << 2,
  SP_TYPE = 1U << 3,
  SP_VAR = 1U << 4,
  SP_TU = 1U << 5,
};

extern long ts;
extern char tu[PATH_MAX];
extern char cwd[PATH_MAX];

typedef DECL_ARRAY(NodeList, Node) NodeList;
static inline IMPL_ARRAY_PUSH(NodeList, Node);

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

String *add_string(struct string s);

static inline void add_string_property(String *s, uint8_t property) {
  if (s)
    s->property |= property;
}

struct error parse_init();
struct error parse_halt();

struct error parse(YYLTYPE *lloc, const UserContext *uctx);
struct error parse_line(char *line, size_t n, size_t cap, YYLTYPE *lloc,
                        const UserContext *uctx,
                        struct error (*parse_hook)(YYLTYPE *lloc,
                                                   const UserContext *uctx));