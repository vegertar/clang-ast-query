#include "parse.h"
#include "scan.h"
#include "sql.h"
#include "test.h"
#include "util.h"

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

enum of_kind {
  OF_OBJ,
  OF_TEXT,
};

struct input_file {
  enum if_kind kind;
  const char *filename;
  const char *db_filename;
};

static void free_input_file(void *p) {
  struct input_file *f = (struct input_file *)p;
  if (f->db_filename && f->db_filename != f->filename)
    free((void *)f->db_filename);
}

DECL_ARRAY(input_file_list, struct input_file);
IMPL_ARRAY_PUSH(input_file_list, struct input_file)
IMPL_ARRAY_CLEAR(input_file_list, free_input_file)

static struct string in_content;
static struct input_file_list in_files;
static FILE *out_text;
static sqlite3 *db;
static sqlite3_stmt *stmts[MAX_STMT_SIZE];
static char *errmsg;

static int parse_line(char *line, size_t n, size_t cap, void *data) {
  assert(n + 1 < cap);
  line[n] = line[n + 1] = 0;

  if (out_text) {
    fwrite(line, n, 1, out_text);
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

static int bundle(const char *obj, unsigned i) {
  int err = 0;

  char buffer[BUFSIZ];
  getcwd(buffer, sizeof(buffer));

  char path[PATH_MAX];
  expand_path(buffer, strlen(buffer), obj, path);

  snprintf(buffer, sizeof(buffer), "ATTACH '%s' AS obj", path);
  EXEC_SQL(buffer);
  EXEC_SQL("BEGIN TRANSACTION");

#ifndef VALUES2
#define VALUES2() "?,?"
#endif // !VALUES2

  INSERT_INTO(obj, NUMBER, FILENAME) {
    FILL_INT(NUMBER, i);
    FILL_TEXT(FILENAME, path);
  }
  END_INSERT_INTO();

  snprintf(buffer, sizeof(buffer),
           "INSERT INTO sym SELECT name, decl, %u from obj.sym", i);
  EXEC_SQL(buffer);

  EXEC_SQL("END TRANSACTION");
  EXEC_SQL("DETACH obj");

  return err;
}

int main(int argc, char **argv) {
  srand(time(NULL));

  int debug_flag = 0;
  int cc_flag = 0;
  int ld_flag = 1;
  int if_kind = IF_TEXT;
  int of_kind = OF_OBJ;

  const char *out_filename = NULL;
  const char *tu_filename = NULL;

  int c, n, err = 0;
  char tmp[PATH_MAX];

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
      fprintf(stderr, "\t-C         treat the default input file as C code\n");
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
      if_kind = IF_C;
      break;
    case 'c':
      of_kind = OF_TEXT;
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
    const size_t n = strlen(s);
    if (n == 2 && s[0] == '-' && i + 1 < argc) {
      if (s[1] == 'o')
        out_filename = argv[++i];
      else if (s[1] == 'c')
        cc_flag = 1;
    } else if (n > 2 && s[n - 2] == '.') {
      const int ext = s[n - 1];
      argv[i] = "";
      // TODO: identify magic
      const int kind = ext == 'c' ? IF_C : ext == 'o' ? IF_OBJ : if_kind;
      input_file_list_push(&in_files, (struct input_file){kind, s});
    }
  }

  if (!in_files.i)
    input_file_list_push(&in_files, (struct input_file){if_kind, argv[optind]});

  if (of_kind == OF_TEXT || cc_flag)
    ld_flag = 0;

  if (out_filename && !starts_with(out_filename, "/dev/fd/") &&
      !starts_with(out_filename, "/dev/std")) {
    char str[8];

    n = snprintf(tmp, sizeof(tmp), "%s.tmp-%s", out_filename,
                 rands(str, sizeof(str)));
    assert(n == strlen(tmp));

    if (ld_flag) {
      if ((err = sqlite3_open(tmp, &db)))
        fprintf(stderr, "sqlite3 error(%d): %s\n", err, sqlite3_errstr(err));

      EXEC_SQL("PRAGMA synchronous = OFF");
      EXEC_SQL("PRAGMA journal_mode = MEMORY");
      EXEC_SQL("BEGIN TRANSACTION");
      EXEC_SQL("CREATE TABLE obj (number INTEGER, filename TEXT)");
      EXEC_SQL("CREATE TABLE sym (name TEXT, decl INTEGER, obj INTEGER)");
      EXEC_SQL("END TRANSACTION");
    }
  } else {
    *tmp = 0;
  }

  for (unsigned i = 0; !err && i < in_files.i; ++i) {
    const char *in = ALT(in_files.data[i].filename, "/dev/stdin");
    const int in_kind = in_files.data[i].kind;

    if (in_kind != IF_OBJ) {
      FILE *in_fp;
      if (!(in_fp = open_file(in, "r"))) {
        err = errno;
        break;
      }

      if (*tmp)
        snprintf(tmp + n, sizeof(tmp) - n, ".%u", i + 1);

      const char *out = *tmp ? tmp : out_filename;

      if (out_filename && of_kind == OF_TEXT &&
          !(out_text = open_file(out, "w"))) {
        err = errno;
        fclose(in_fp);
        break;
      }

      if (in_kind == IF_TEXT) {
        err = parse_file(in_fp);
      } else {
        string_clear(&in_content, 0);
        if (!(err = read_content(in_fp, &in_content)))
          err = compile(in_content.data, in_content.i,
                        ends_with(in, ".c") ? in : ALT(tu_filename, in), argc,
                        argv);
      }

      if (!err && out_filename && of_kind == OF_OBJ) {
        err = dump(out);
        if (!err && *tmp && ld_flag)
          err = bundle(tmp, i + 1);
      }

      if (!err && in_files.i == 1 && *tmp && !ld_flag)
        rename(tmp, out_filename);

      destroy();
      yylex_destroy();

      if (out_text) {
        fclose(out_text);
        out_text = NULL;
      }

      fclose(in_fp);
    } else if (ld_flag) {
      err = bundle(in, i + 1);
    }
  }

  if (*tmp && ld_flag) {
    sqlite3_close(db);
    tmp[n] = 0;
    rename(tmp, out_filename);
  }

  input_file_list_clear(&in_files, 1);
  string_clear(&in_content, 1);

  return err;
}
