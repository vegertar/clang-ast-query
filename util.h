#pragma once

const char *expand_path(const char *cwd, size_t n, const char *in, char *out);

_Bool starts_with(const char *s, const char *starting);

_Bool ends_with(const char *s, const char *ending);

const char *rands(char buf[], size_t cap);