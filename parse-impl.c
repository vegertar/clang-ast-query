#include "scan.h"
#include "test.h"

static const String *last_loc_src;
static unsigned last_loc_line;

long ts;
char tu[PATH_MAX];
char cwd[PATH_MAX];

NodeList all_nodes;
StringSet all_strings;
SemanticsList all_semantics;

static inline IMPL_ARRAY_CLEAR(SemanticsList, NULL);

const String *add_string(struct string s, uint8_t property) {
  String x = {string_hash(&s), property, s};

  const String *y = StringSet_add(&all_strings, &x);
  if (!y) {
    fprintf(stderr,
            "The string set is full (STRING_SET_SIZE=%u), "
            "re-run with a bigger size\n",
            all_strings.n);

    exit(-1);
  }

  return y;
}

struct error parse_init() {
  assert(all_strings.n == 0 && "Expect uninitialized");
  const char *string_set_size = getenv("STRING_SET_SIZE");
  int n = string_set_size ? atoi(string_set_size) : 20071;
  TOGGLE(log_string_set_size, fprintf(stderr, "string set size is %d\n", n));
  StringSet_reserve(&all_strings, n);
  return (struct error){};
}

struct error parse_halt() {
  TOGGLE(dump_all_strings, StringSet_dump(&all_strings, stderr));
  TOGGLE(log_string_set_load_factor,
         fprintf(stderr, "The load factor of string set is %.2f\n",
                 (float)all_strings.i / all_strings.n));
  StringSet_clear(&all_strings, 1);
  SemanticsList_clear(&all_semantics, 1);
  return (struct error){};
}

struct error parse(YYLTYPE *lloc, const UserContext *uctx) {
  yypstate *ps = yypstate_new();
  int status = 0;

  do {
    YYSTYPE lval;
    YY_DECL;
    yytoken_kind_t token = yylex(&lval, lloc, uctx);
    status = yypush_parse(ps, token, &lval, lloc, uctx);
  } while (status == YYPUSH_MORE);

  yypstate_delete(ps);
  return status ? (struct error){ES_PARSE, status} : (struct error){};
}

struct error parse_line(char *line, size_t n, size_t cap, YYLTYPE *lloc,
                        const UserContext *uctx,
                        struct error (*parse_hook)(YYLTYPE *lloc,
                                                   const UserContext *uctx)) {
  assert(n + 1 < cap);
  assert(parse_hook);

  line[n + 1] = 0;

#ifdef NDEBUG
  YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);
#else
  YY_BUFFER_STATE buffer = yy_scan_bytes(line, n);
#endif // NDEBUG

  struct error err = parse_hook(lloc, uctx);
  yy_delete_buffer(buffer);

  return err;
}
