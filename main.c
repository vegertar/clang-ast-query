#include "parse.h"
#include "scan.h"
#include "test.h"

#ifdef USE_CLANG_TOOL
#include "remark.h"
#endif // USE_CLANG_TOOL

#include <sqlite3.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ALT(x, y) (x ? x : y)

struct parser_context {
  YYLTYPE lloc;
  user_context uctx;
  int errs;
};

DECL_ARRAY(string, char);
IMPL_ARRAY_APPEND(string, char)
IMPL_ARRAY_CLEAR(string, NULL)

enum if_kind {
  IF_TEXT,
  IF_C,
  IF_OBJ,
};

struct input_file {
  enum if_kind kind;
  const char *filename;
  sqlite3 *db;
};

static void free_input_file(void *p) {
  struct input_file *f = (struct input_file *)p;
  if (f->db)
    sqlite3_close(f->db);
}

DECL_ARRAY(input_file_list, struct input_file);
IMPL_ARRAY_PUSH(input_file_list, struct input_file)
IMPL_ARRAY_CLEAR(input_file_list, free_input_file)

static struct input_file_list in_files;
static FILE *text_file;

static int parse_line(char *line, size_t n, size_t cap, void *data) {
  assert(n + 1 < cap);
  line[n] = line[n + 1] = 0;

  if (text_file) {
    fwrite(line, n, 1, text_file);
    return 0;
  }

#ifdef NDEBUG
  YY_BUFFER_STATE buffer = yy_scan_buffer(line, n + 2);
#else
  YY_BUFFER_STATE buffer = yy_scan_bytes(line, n);
#endif // NDEBUG

  struct parser_context *ctx = (struct parser_context *)data;
  ctx->uctx.line = line;

  int err = parse(&ctx->lloc, &ctx->uctx);
  if (err)
    ctx->errs++;
  yy_delete_buffer(buffer);
  return err;
}

static int parse_file(FILE *fp) {
  char line[BUFSIZ];
  struct parser_context ctx = {
      .lloc = (YYLTYPE){1, 1, 1, 1},
  };

  int err = 0;
  while (err == 0 && fgets(line, sizeof(line), fp)) {
    err = parse_line(line, strlen(line), sizeof(line), &ctx);
  }
  return err;
}

static inline FILE *open_file(const char *filename, const char *mode) {
  FILE *fp = fopen(filename, mode);
  if (!fp) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, filename,
            strerror(errno));
  }
  return fp;
}

static int read_content(FILE *fp, struct string *s) {
  char buffer[BUFSIZ];
  size_t n = 0;
  while ((n = fread(buffer, 1, sizeof(buffer), fp))) {
    string_append(s, buffer, n);
  }
  return ferror(fp);
}

static _Bool ends_with(const char *s, const char *ending) {
  int n = s ? strlen(s) : 0;
  int m = ending ? strlen(ending) : 0;
  return m == 0 || n >= m && strcmp(s + n - m, ending) == 0;
}

static char rand_char() {
  static const char s[] = "0123456789"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz";
  return s[rand() % (sizeof(s) - 1)];
}

static const char *rand7() {
  static char s[8];
  for (unsigned i = 0; i < sizeof(s) - 1; ++i) {
    s[i] = rand_char();
  }
  return s;
}

static int compile(const char *code, size_t size, const char *filename,
                   int argc, char **argv) {
#ifdef USE_CLANG_TOOL
  struct parser_context ctx = {(YYLTYPE){1, 1, 1, 1}};
  int err = remark(code, size, filename, argv + optind, argc - optind,
                   parse_line, &ctx);
  return err ? err : ctx.errs;
#else
  fprintf(stderr, "Clang tool is not compiled in\n");
  return -1;
#endif // USE_CLANG_TOOL
}

static int bundle(const char *filename) { return 0; }

int main(int argc, char **argv) {
  int debug_flag = 0;
  int text_flag = 0;
  int cc_flag = 0;
  const char *out_filename = NULL;
  const char *tu_filename = NULL;

  int c;

  while ((c = getopt(argc, argv, "ht::T::dCci:o:")) != -1)
    switch (c) {
    case 'h':
      fprintf(stderr, "Usage: %s [OPTION]... [-- [CLANG OPTION]...] [FILE]\n",
              argv[0]);
      fprintf(stderr, "The utility to handle Clang AST from FILE (the stdin by "
                      "default)\n\n");
      fprintf(stderr, "\t-h         display this help and exit\n");
      fprintf(stderr, "\t-t[help]   run test\n");
      fprintf(stderr, "\t-T[help]   operate toggle\n");
      fprintf(stderr, "\t-d         enable debug\n");
      fprintf(stderr, "\t-C         treat the input file as the C input\n");
      fprintf(stderr, "\t-c         only dump text\n");
      fprintf(stderr, "\t-i NAME    name the input TU if possible\n");
      fprintf(stderr, "\t-o OUTPUT  specify the output file\n");
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
    case 'C':
      cc_flag = 1;
      break;
    case 'c':
      text_flag = 1;
      break;
    case 'i':
      tu_filename = optarg;
      break;
    case 'o':
      out_filename = strcmp(optarg, "-") ? optarg : "/dev/stdout";
      break;
    default:
      exit(1);
    }

  if (debug_flag)
    yydebug = 1;

  for (int i = optind; i < argc; ++i) {
    const char *s = argv[i];
    size_t n = strlen(s);
    if (n > 2 && s[n - 2] == '.') {
      argv[i] = "";
      // TODO: identify magic
      int kind = s[n - 1] == 'c' ? IF_C : s[n - 1] == 'o' ? IF_OBJ : cc_flag;
      input_file_list_push(&in_files, (struct input_file){kind, s});
    } else if (n == 2 && s[0] == '-' && s[1] == 'o' && i + 1 < argc) {
      out_filename = argv[++i];
    }
  }

  if (!in_files.i)
    input_file_list_push(&in_files, (struct input_file){cc_flag, argv[optind]});

  if (text_flag && in_files.i != 1) {
    fprintf(stderr, "Cannot specify `-c' with multiple input files\n");
    input_file_list_clear(&in_files, 2);
    return 1;
  }

  if (text_flag && out_filename && !(text_file = fopen(out_filename, "w"))) {
    fprintf(stderr, "%s: open('%s') error: %s\n", __func__, out_filename,
            strerror(errno));
    return 1;
  }

  int err = 0;
  _Bool is_linking = in_files.i > 1 && out_filename;

  for (unsigned i = 0; !err && i < in_files.i; ++i) {
    const char *in_filename = ALT(in_files.data[i].filename, "/dev/stdin");
    const int kind = in_files.data[i].kind;

    if (kind != IF_OBJ) {
      FILE *fp = open_file(in_filename, "r");
      if (!fp)
        exit(1);

      // Try to create the obj(sqlite3) file

      if (kind == IF_TEXT) {
        err = parse_file(fp);
      } else {
        struct string s = {};
        if (!(err = read_content(fp, &s)))
          err = compile(s.data, s.i,
                        ends_with(in_filename, ".c")
                            ? in_filename
                            : ALT(tu_filename, in_filename),
                        argc, argv);
        string_clear(&s, 2);
      }

      if (!err && !text_flag && out_filename) {
        char buf[PATH_MAX];
        strcpy(buf, out_filename);

        char tmp[PATH_MAX];
        int n = snprintf(tmp, sizeof(tmp), "%s", dirname(buf));
        assert(n > 0);

        if (tmp[n - 1] == '.')
          tmp[--n] = 0;
        else if (tmp[n - 1] != '/')
          tmp[n++] = '/';

        // Trim the suffix
        strcpy(buf, tu);
        char *dot = strrchr(buf, '.');
        if (dot)
          *dot = 0;

        srand(time(NULL));
        int m = snprintf(tmp + n, sizeof(tmp) - n, "%s-%s.o.tmp", basename(buf),
                         rand7());
        assert(m > 0);

        err = dump(tmp);
        if (!err) {
          if (is_linking) {
            memcpy(buf, tmp, n + m - 4);
            buf[n + m - 4] = 0;
            rename(tmp, buf);
            sqlite3_open(buf, &in_files.data[i].db);
          } else {
            rename(tmp, out_filename);
          }
        } else {
          unlink(tmp);
        }
      }

      destroy();
      yylex_destroy();
      fclose(fp);
    } else if (is_linking) {
      sqlite3_open(in_filename, &in_files.data[i].db);
    }
  }

  if (is_linking)
    err = bundle(out_filename);

  if (text_file)
    fclose(text_file);
  input_file_list_clear(&in_files, 1);
  return err;
}
