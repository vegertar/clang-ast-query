#pragma once

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif // !PATH_MAX

#define ALT(x, y) (x ? x : y)

const char *expand_path(const char *cwd, unsigned n, const char *in, char *out);

_Bool starts_with(const char *s, const char *starting);

_Bool ends_with(const char *s, const char *ending);

const char *rands(char buf[], unsigned cap);