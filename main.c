#include "html.h"
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

struct parser_context {
  YYLTYPE lloc;
  user_context uctx;
  int errs;
};

enum if_kind {
  IF_TEXT,
  IF_C,
  IF_SQL,
  IF_ELF, // TODO:
};

enum of_kind {
  OF_SQL,
  OF_HTML,
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
static IMPL_ARRAY_PUSH(input_file_list, struct input_file);
static IMPL_ARRAY_CLEAR(input_file_list, free_input_file);

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

static int compile(const char *code, size_t size, const char *filename,
                   int argc, char **argv) {
#ifdef USE_CLANG_TOOL
  struct parser_context ctx = {(YYLTYPE){1, 1, 1, 1}};
  int err = remark(code, size, filename, argv + optind, argc - optind,
                   parse_line, &ctx);
  return err ? err : ctx.errs;
#else
  fprintf(stderr, "Clang tool is not compiled in_filename\n");
  return -1;
#endif // USE_CLANG_TOOL
}

static int bundle(const char *sql) {
  int err = 0;

  char buffer[BUFSIZ];
  getcwd(buffer, sizeof(buffer));

  char path[PATH_MAX];
  expand_path(buffer, strlen(buffer), sql, path);

  snprintf(buffer, sizeof(buffer), "ATTACH '%s' AS sql", path);
  EXEC_SQL(buffer);
  EXEC_SQL("BEGIN TRANSACTION");

#ifndef VALUES2
#define VALUES2() "?,?"
#endif // !VALUES2

  // the index of sql
  static int i = 0;

  ++i;

  INSERT_INTO(sql, NUMBER, FILENAME);
  FILL_INT(NUMBER, i);
  FILL_TEXT(FILENAME, path);
  END_INSERT_INTO();

  snprintf(buffer, sizeof(buffer),
           "INSERT INTO sym SELECT name, decl, %d from sql.sym", i);
  EXEC_SQL(buffer);

  EXEC_SQL("END TRANSACTION");
  EXEC_SQL("DETACH sql");

  return err;
}

int main(int argc, char **argv) {
  srand(time(NULL));

  int debug_flag = 0;
  int c_flag = 0;
  int cc_flag = 0;
  int ld_flag = 1;
  int if_kind = IF_TEXT;
  int of_kind = OF_SQL;

  const char *out_filename = NULL;
  const char *tu_filename = NULL;

  int c, n, err = 0;
  char tmp[PATH_MAX];

  while ((c = getopt(argc, argv, "ht::T::dCcx::i:o:")) != -1)
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
      fprintf(stderr, "\t-c         the alias of -xt if no -xh given\n");
      fprintf(stderr, "\t-x         the alias of -xh\n");
      fprintf(stderr, "\t-xt[ext]   output as TEXT\n");
      fprintf(stderr, "\t-xh[tml]   output as HTML\n");
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
      c_flag = 1;
      break;
    case 'x':
      if (!optarg)
        of_kind = OF_HTML;
      else if (strcmp(optarg, "text") == 0 || strcmp(optarg, "t") == 0)
        of_kind = OF_TEXT;
      else if (strcmp(optarg, "html") == 0 || strcmp(optarg, "h") == 0)
        of_kind = OF_HTML;
      else
        return fprintf(stderr, "invalid output format: %s\n", optarg);
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

  if (c_flag) {
    if (of_kind == OF_HTML)
      cc_flag = 1;
    else
      of_kind = OF_TEXT;
  }

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
      const int kind = ext == 'c' ? IF_C : ext == 'o' ? IF_SQL : if_kind;
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

    n = snprintf(tmp, sizeof(tmp), "%s.caq-%s", out_filename,
                 rands(str, sizeof(str)));
    assert(n == strlen(tmp));

    if (ld_flag) {
      OPEN_DB(tmp);
      EXEC_SQL("PRAGMA synchronous = OFF");
      EXEC_SQL("PRAGMA journal_mode = MEMORY");
      EXEC_SQL("BEGIN TRANSACTION");
      EXEC_SQL("CREATE TABLE sql (number INTEGER, filename TEXT)");
      EXEC_SQL("CREATE TABLE sym (name TEXT, decl INTEGER, sql INTEGER)");
      EXEC_SQL("END TRANSACTION");
    }
  } else {
    *tmp = 0;
  }

  for (unsigned i = 0; !err && i < in_files.i; ++i) {
    const char *in_filename = ALT(in_files.data[i].filename, "/dev/stdin");
    const int in_kind = in_files.data[i].kind;

    if (in_kind != IF_SQL) {
      FILE *in_fp;
      if (!(in_fp = open_file(in_filename, "r"))) {
        err = errno;
        break;
      }

      if (*tmp)
        snprintf(tmp + n, sizeof(tmp) - n, ".%u", i + 1);

      if (out_filename && of_kind == OF_TEXT &&
          !(out_text = open_file(*tmp ? tmp : out_filename, "w"))) {
        err = errno;
        fclose(in_fp);
        break;
      }

      switch (in_kind) {
      case IF_TEXT:
        err = parse_file(in_fp);
        break;
      case IF_C:
        string_clear(&in_content, 0);
        if (!(err = reads(in_fp, &in_content, NULL)))
          err = compile(in_content.data, in_content.i,
                        ends_with(in_filename, ".c")
                            ? in_filename
                            : ALT(tu_filename, in_filename),
                        argc, argv);
        break;
      default:
        break;
      }

      if (!err && *tmp && of_kind <= OF_HTML) {
        err = sql(tmp);
        if (!err && of_kind == OF_HTML)
          err = html(tmp);
        if (!err && ld_flag)
          err = bundle(tmp);
      }

      if (!err && in_files.i == 1 && *tmp && !ld_flag) {
        rename(tmp, out_filename);
        if (of_kind == OF_HTML)
          html_rename(tmp, out_filename);
      }

      destroy();
      yylex_destroy();

      if (out_text) {
        fclose(out_text);
        out_text = NULL;
      }

      fclose(in_fp);
    } else if (ld_flag) {
      err = bundle(in_filename);
    }
  }

  if (*tmp && ld_flag) {
    CLOSE_DB();
    tmp[n] = 0;
    rename(tmp, out_filename);
  }

  sql_halt();
  html_halt();
  input_file_list_clear(&in_files, 1);
  string_clear(&in_content, 1);

  return err;
}
