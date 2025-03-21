#pragma once

// Credit: https://stackoverflow.com/a/2124385
#define PP_NARG(...) PP_NARG_(__VA_ARGS__ __VA_OPT__(, ) PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,  \
                 _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26,   \
                 _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38,   \
                 _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50,   \
                 _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62,   \
                 _63, N, ...)                                                  \
  N
#define PP_RSEQ_N()                                                            \
  63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,  \
      44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27,  \
      26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9,   \
      8, 7, 6, 5, 4, 3, 2, 1, 0

#define PP_CAT(...) PP_CAT_(PP_CAT, PP_NARG(__VA_ARGS__))(__VA_ARGS__)
#define PP_CAT_(...) PP_CAT2_(__VA_ARGS__)
#define PP_CAT0(...)
#define PP_CAT1(a) a
#define PP_CAT2(a, b) PP_CAT2_(a, b)
#define PP_CAT2_(a, b) a##b
#define PP_CAT3(a, b, c) a##b##c
#define PP_CAT4(a, b, c, d) a##b##c##d
#define PP_CAT5(a, b, c, d, e) a##b##c##d##e
#define PP_CAT6(a, b, c, d, e, f) a##b##c##d##e##f
#define PP_CAT7(a, b, c, d, e, f, g) a##b##c##d##e##f##g
#define PP_CAT8(a, b, c, d, e, f, g, h) a##b##c##d##e##f##g##h
#define PP_CAT9(a, b, c, d, e, f, g, h, i) a##b##c##d##e##f##g##h##i

#define PP_JOIN(sep, ...)                                                      \
  PP_CAT2(PP_JOIN, PP_NARG(__VA_ARGS__))                                       \
  (sep __VA_OPT__(, ) __VA_ARGS__)
#define PP_JOIN0(...)
#define PP_JOIN1(sep, a, ...) a
#define PP_JOIN2(sep, a, ...) a sep PP_JOIN1(sep, __VA_ARGS__)
#define PP_JOIN3(sep, a, ...) a sep PP_JOIN2(sep, __VA_ARGS__)
#define PP_JOIN4(sep, a, ...) a sep PP_JOIN3(sep, __VA_ARGS__)
#define PP_JOIN5(sep, a, ...) a sep PP_JOIN4(sep, __VA_ARGS__)
#define PP_JOIN6(sep, a, ...) a sep PP_JOIN5(sep, __VA_ARGS__)
#define PP_JOIN7(sep, a, ...) a sep PP_JOIN6(sep, __VA_ARGS__)
#define PP_JOIN8(sep, a, ...) a sep PP_JOIN7(sep, __VA_ARGS__)
#define PP_JOIN9(sep, a, ...) a sep PP_JOIN8(sep, __VA_ARGS__)
#define PP_JOIN10(sep, a, ...) a sep PP_JOIN9(sep, __VA_ARGS__)
#define PP_JOIN11(sep, a, ...) a sep PP_JOIN10(sep, __VA_ARGS__)
#define PP_JOIN12(sep, a, ...) a sep PP_JOIN11(sep, __VA_ARGS__)
#define PP_JOIN13(sep, a, ...) a sep PP_JOIN12(sep, __VA_ARGS__)
#define PP_JOIN14(sep, a, ...) a sep PP_JOIN13(sep, __VA_ARGS__)
#define PP_JOIN15(sep, a, ...) a sep PP_JOIN14(sep, __VA_ARGS__)
#define PP_JOIN16(sep, a, ...) a sep PP_JOIN15(sep, __VA_ARGS__)

#define PP_DUP(x, n) PP_DUP##n(x)
#define PP_DUP1(x) x
#define PP_DUP2(x) x, x
#define PP_DUP3(x) x, x, x
#define PP_DUP4(x) x, x, x, x
#define PP_DUP5(x) x, x, x, x, x
#define PP_DUP6(x) x, x, x, x, x, x
#define PP_DUP7(x) x, x, x, x, x, x, x
#define PP_DUP8(x) x, x, x, x, x, x, x, x
#define PP_DUP9(x) x, x, x, x, x, x, x, x, x
#define PP_DUP10(x) x, PP_DUP9(x)
#define PP_DUP11(x) x, x, PP_DUP9(x)
#define PP_DUP12(x) x, x, x, PP_DUP9(x)
#define PP_DUP13(x) x, x, x, x, PP_DUP9(x)
#define PP_DUP14(x) x, x, x, x, x, PP_DUP9(x)
#define PP_DUP15(x) x, x, x, x, x, PP_DUP9(x)
#define PP_DUP16(x) x, x, x, x, x, x, PP_DUP9(x)

#define PP_OVERLOAD(name, ...) PP_CAT(name, PP_NARG(__VA_ARGS__))(__VA_ARGS__)

#define PP_REPLACE(x, template)                                                \
  PP_CAT(PP_REPLACE, PP_NARG(template##x))(x, template)
#define PP_REPLACE0(...)
#define PP_REPLACE1(x, template) template(x)
