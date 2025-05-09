#define ES(...)                                                                \
  PP_CAT(ES, PP_NARG(__VA_ARGS__))                                             \
  (PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

#define ES_CURR(order, n, type, mask, name, ...)                               \
  ES2(n, type##_##name, (mask) + (n - order + 1))

#define ES_NEXT(n, type, mask, name, ...)                                      \
  n, type, mask __VA_OPT__(, ) __VA_ARGS__

#define ES1(n, name) ES_ENUM(name)
#define ES2(n, type, value) ES_ENUM(type, value)
#define ES2_(...) ES2(__VA_ARGS__)
#define ES3(...) ES_CURR(3, __VA_ARGS__) ES2_(ES_NEXT(__VA_ARGS__))
#define ES4(...) ES_CURR(4, __VA_ARGS__) ES3(ES_NEXT(__VA_ARGS__))
#define ES5(...) ES_CURR(5, __VA_ARGS__) ES4(ES_NEXT(__VA_ARGS__))
#define ES6(...) ES_CURR(6, __VA_ARGS__) ES5(ES_NEXT(__VA_ARGS__))
#define ES7(...) ES_CURR(7, __VA_ARGS__) ES6(ES_NEXT(__VA_ARGS__))
#define ES8(...) ES_CURR(8, __VA_ARGS__) ES7(ES_NEXT(__VA_ARGS__))

#ifndef ES_ENUM
#define ES_ENUM(...) PP_OVERLOAD(ES_ENUM __VA_OPT__(, ) __VA_ARGS__)
#define ES_ENUM1(name) ES_##name,
#define ES_ENUM2(name, value) ES_##name = value,
#endif // ES_ENUM

ES(OK)
ES(REQUIRE)
ES(BUILDING_PROHIBITED)
ES(DUMP)
ES(UNKNOWN_OUTPUT)

ES(FILE, 0x0100U, OPEN, CLOSE, READ, WRITE, RENAME, UNLINK)
ES(REMARK, 0x0200U, NO_CLANG)
ES(PARSE, 0x0300U, INIT, HALT)
ES(STORE, 0x0400U, OPEN, CLOSE)
ES(RENDER, 0x0500U)
ES(QUERY, 0x0600U, TU, STRINGS, SEMANTICS, LINK, LINT)

#undef ES

#undef ES_CURR
#undef ES_NEXT

#undef ES1
#undef ES2
#undef ES2_
#undef ES3
#undef ES4
#undef ES5
#undef ES6
#undef ES7
#undef ES8

#undef ES_ENUM
#undef ES_ENUM1
#undef ES_ENUM2