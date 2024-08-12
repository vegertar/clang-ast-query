#include "string_set.h"
#include "test.h"
#include <stdlib.h>

typedef DECL_ARRAY(ANON, Node) NodeList;

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
  const char *string_set_size = getenv("STRING_SET_SIZE");
  int n = string_set_size ? atoi(string_set_size) : 10099;
  StringSet_reserve(&all_strings, n);
}

void parse_halt() {
  TOGGLE(dump_all_strings, StringSet_dump(&all_strings, NULL));
  TOGGLE(log_string_set_load_factor,
         fprintf(stderr, "The load factor of <all_strings> is %.2f\n",
                 (float)all_strings.i / all_strings.n));
  StringSet_clear(&all_strings, 1);
}
