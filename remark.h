#pragma once

#ifdef __cplusplus

#include <cstddef>
using std::size_t;

extern "C" {
#else
#include <stddef.h>
#endif

int remark(const char *code, size_t size, const char *filename, char **opts,
           int (*parse_line)(char *line, size_t n, size_t cap, void *data),
           void *data);

#ifdef __cplusplus
}
#endif
