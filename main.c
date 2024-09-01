#include "build.h"
#include "parse.h"
#include "test.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int debug_flag = 0;       // the option of -d
  int silent_flag = 0;      // the option of -s
  int c_flag = 0;           // the option of -c
  int input_kind = IK_TEXT; // the default input file kind
  int output_kind = OK_NIL; // the default output file kind

  char *output_file = NULL;
  char *tu_name = NULL;

  int c;
  while ((c = getopt(argc, argv, "ht::T::dsCcx::i:o:")) != -1)
    switch (c) {
    case 'h':
      printf("Usage: %s [OPTION]... [-- [CLANG OPTION]...] [FILE]\n", argv[0]);
      printf("The utility to handle Clang AST (from the stdin by default)\n");
      printf("\n");
      printf("  -h         display this help and exit\n");
      printf("  -t[help]   run test\n");
      printf("  -T[help]   operate toggle\n");
      printf("  -d         enable debug\n");
      printf("  -s         parse silently\n");
      printf("  -C         treat the default input file as C code\n");
      printf("  -c         the alias of -xs if no -xt given\n");
      printf("  -x         the alias of -xt\n");
      printf("  -xs[ql]    output AST in SQLite3 database\n");
      printf("  -xt[ext]   output AST in text format\n");
      printf("  -i NAME    set the TU name\n");
      printf("  -o OUTPUT  specify the output file\n");
      return 0;
    case 't':
      return optarg && strcmp(optarg, "help") == 0 ? test_help()
                                                   : RUN_TEST(optarg);
    case 'T':
      if (optarg && strcmp(optarg, "help") == 0)
        return toggle_help();
      if (toggle(optarg))
        return -1;
      break;
    case 'd':
      debug_flag = 1;
      break;
    case 's':
      silent_flag = 1;
      break;
    case 'C':
      input_kind = IK_C;
      break;
    case 'c':
      c_flag = 1;
      break;
    case 'x':
      if (!optarg)
        output_kind = OK_TEXT;
      else if (strcmp(optarg, "text") == 0 || strcmp(optarg, "t") == 0)
        output_kind = OK_TEXT;
      else if (strcmp(optarg, "sql") == 0 || strcmp(optarg, "s") == 0)
        output_kind = OK_SQL;
      else
        return fprintf(stderr, "invalid output format: %s\n", optarg);
      break;
    case 'i':
      tu_name = optarg;
      break;
    case 'o':
      output_file = strcmp(optarg, "-") ? optarg : "/dev/stdout";
      break;
    default:
      exit(1);
    }

  // Leave all options after `--` to final build stage
  char **opts = argv + optind;

  // handle Clang options roughly
  for (int i = optind; i < argc; ++i) {
    char *s = argv[i];
    const size_t n = strlen(s);
    const char *dot = NULL;

    if (n == 2 && s[0] == '-' && i + 1 < argc) {
      // handle -o and -c

      if (s[1] == 'o')
        output_file = argv[++i];
      else if (s[1] == 'c')
        c_flag = 1;
    } else if (n > 2 && (dot = strrchr(s, '.')) && dot != s) {
      // handle input files

      const char *ext = dot + 1;
      int kind; // TODO: detect file kind by reading ELF info.
      if (strcmp(ext, "c") == 0) {
        kind = IK_C;
      } else if (strcmp(ext, "o") == 0) {
        kind = IK_SQL;
      } else {
        kind = input_kind;
      }

      argv[i] = ""; // clear the old positional argument
      add_input({kind, s, tu_name, opts});
    }
  }

  // Use the default file if no Clang options provided
  add_input_if_empty({input_kind, ALT(argv[optind], "/dev/stdin"), tu_name});

  if (debug_flag)
    yydebug = 1;

  // Handle the default case of output
  if (output_kind == OK_NIL) {
    if (c_flag) // for the similar usage of command `cc -o a.o -c a.c`
      output_kind = OK_SQL;
    else if (output_file) // for the similar usage of command `cc -o a.out a.o`
      output_kind = OK_HTML;
  }

  return build_output((struct output){output_kind, output_file, silent_flag});
}