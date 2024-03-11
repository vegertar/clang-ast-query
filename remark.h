#pragma once

#ifdef __cplusplus

#include <cstddef>
using std::size_t;

extern "C" {
#endif

enum token_kind {
  TOKEN_KIND_UNKNOW,
  TOKEN_KIND_LITERAL,
  TOKEN_KIND_IDENTIFIER,
  TOKEN_KIND_TYPE,
  TOKEN_KIND_KEYWORD,
  TOKEN_KIND_COMMENT,
  TOKEN_KIND_PUNCTUATION,
};

int remark(const char *code,
           size_t size,
           const char *filename,
           char **opts,
           int n,
           int (*parse_line)(char *line, size_t n, size_t cap, void *data),
           void *data);

#ifdef __cplusplus
}
#endif
