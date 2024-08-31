#include "scan.h"
#include "string_set.h"
#include "test.h"
#include <stdlib.h>

typedef DECL_ARRAY(ANON, Node) NodeList;

const char *last_loc_src;
unsigned last_loc_line;

NodeList all_nodes;
StringSet all_strings;

const char *add_string(struct string s) {
  String x = {string_hash(&s), s};

  const String *y = StringSet_add(&all_strings, &x);
  if (!y) {
    fprintf(stderr,
            "The string set is full (STRING_SET_SIZE=%u), "
            "re-run with a bigger size\n",
            all_strings.n);

    exit(-1);
  }

  return string_get(&y->elem);
}

void parse_init() {
  assert(all_strings.n == 0 && "Has initialized");
  const char *string_set_size = getenv("STRING_SET_SIZE");
  int n = string_set_size ? atoi(string_set_size) : 10099;
  TOGGLE(log_string_set_size, fprintf(stderr, "string set size is %d\n", n));
  StringSet_reserve(&all_strings, n);
}

void parse_halt() {
  TOGGLE(dump_all_strings, StringSet_dump(&all_strings, stderr));
  TOGGLE(log_string_set_load_factor,
         fprintf(stderr, "The load factor of string set is %.2f\n",
                 (float)all_strings.i / all_strings.n));
  StringSet_clear(&all_strings, 1);
}

int parse(YYLTYPE *lloc, const UserContext *uctx) {
  yypstate *ps = yypstate_new();
  int status = 0;

  do {
    YYSTYPE lval;
    YY_DECL;
    yytoken_kind_t token = yylex(&lval, lloc, uctx);
    status = yypush_parse(ps, token, &lval, lloc, uctx);
  } while (status == YYPUSH_MORE);

  yypstate_delete(ps);
  return status;
}

int parse_line(char *line, size_t n, size_t cap, YYLTYPE *lloc,
               const UserContext *uctx,
               int (*parse_hook)(YYLTYPE *lloc, const UserContext *uctx)) {
  assert(n + 1 < cap);
  line[n + 1] = 0;

#ifdef NDEBUG
  YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);
#else
  YY_BUFFER_STATE buffer = yy_scan_bytes(line, n);
#endif // NDEBUG

  int err = parse_hook(lloc, uctx);
  yy_delete_buffer(buffer);

  return err;
}
