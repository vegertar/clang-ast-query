#pragma once

#ifdef __cplusplus

#include <cstddef>
using std::size_t;

extern "C" {
#endif

int remark(const char *filename, char **opts, int n,
           int (*parse_line)(char *line, size_t n, size_t cap, void *data),
           void *data);

#ifdef __cplusplus
}
#endif
