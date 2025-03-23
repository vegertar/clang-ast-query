#include "util.h"

#define STRING_PROPERTY_FILE 1U
#define STRING_PROPERTY_IDENTIFIER 2U
#define STRING_PROPERTY_BUILTIN 4U
#define STRING_PROPERTY_TYPE 8U

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

String *add_string(struct string s, uint8_t property);

static inline void update_string_property(String *s, uint8_t property) {
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