#include "pp.h"

#define NO(...) NO_I1(PP_NARG(__VA_ARGS__))
#define NO_I1(n) NO_I2(n)
#define NO_I2(n) n

#define OPTIONS(...) OPTIONS_I1(PP_NARG(__VA_ARGS__))(__VA_ARGS__)
#define OPTIONS_I1(n) OPTIONS_I2(n)
#define OPTIONS_I2(n) OPTIONS##n
#define OPTIONS1(a, ...) uint64_t opt_##a : 1;

#define optn(n, ...) opt##n(__VA_ARGS__)
#define optn_(n, ...) optn(n, __VA_ARGS__)
#define opt__(name, ...) optn_(NO(__VA_ARGS__), __VA_ARGS__)

#define opt1(a) opt_##a
#define opt2(a, b) opt_##a : 1, opt_##b
#define opt3(a, ...) opt_##a : 1, opt2(__VA_ARGS__)
#define opt4(a, ...) opt_##a : 1, opt3(__VA_ARGS__)
#define opt5(a, ...) opt_##a : 1, opt4(__VA_ARGS__)
#define opt6(a, ...) opt_##a : 1, opt5(__VA_ARGS__)
#define opt7(a, ...) opt_##a : 1, opt6(__VA_ARGS__)
#define opt8(a, ...) opt_##a : 1, opt7(__VA_ARGS__)
#define opt9(a, ...) opt_##a : 1, opt8(__VA_ARGS__)
#define opt10(a, ...) opt_##a : 1, opt9(__VA_ARGS__)
#define opt11(a, ...) opt_##a : 1, opt10(__VA_ARGS__)
#define opt12(a, ...) opt_##a : 1, opt11(__VA_ARGS__)
#define opt13(a, ...) opt_##a : 1, opt12(__VA_ARGS__)
#define opt14(a, ...) opt_##a : 1, opt13(__VA_ARGS__)
#define opt15(a, ...) opt_##a : 1, opt14(__VA_ARGS__)
#define opt16(a, ...) opt_##a : 1, opt15(__VA_ARGS__)
#define opt17(a, ...) opt_##a : 1, opt16(__VA_ARGS__)
#define opt18(a, ...) opt_##a : 1, opt17(__VA_ARGS__)
#define opt19(a, ...) opt_##a : 1, opt18(__VA_ARGS__)
#define opt20(a, ...) opt_##a : 1, opt19(__VA_ARGS__)
#define opt21(a, ...) opt_##a : 1, opt20(__VA_ARGS__)
#define opt22(a, ...) opt_##a : 1, opt21(__VA_ARGS__)
#define opt23(a, ...) opt_##a : 1, opt22(__VA_ARGS__)
#define opt24(a, ...) opt_##a : 1, opt23(__VA_ARGS__)
#define opt25(a, ...) opt_##a : 1, opt24(__VA_ARGS__)
#define opt26(a, ...) opt_##a : 1, opt25(__VA_ARGS__)
#define opt27(a, ...) opt_##a : 1, opt26(__VA_ARGS__)
#define opt28(a, ...) opt_##a : 1, opt27(__VA_ARGS__)
#define opt29(a, ...) opt_##a : 1, opt28(__VA_ARGS__)
#define opt30(a, ...) opt_##a : 1, opt29(__VA_ARGS__)
#define opt31(a, ...) opt_##a : 1, opt30(__VA_ARGS__)
#define opt32(a, ...) opt_##a : 1, opt31(__VA_ARGS__)
#define opt33(a, ...) opt_##a : 1, opt32(__VA_ARGS__)
#define opt34(a, ...) opt_##a : 1, opt33(__VA_ARGS__)
#define opt35(a, ...) opt_##a : 1, opt34(__VA_ARGS__)
#define opt36(a, ...) opt_##a : 1, opt35(__VA_ARGS__)
#define opt37(a, ...) opt_##a : 1, opt36(__VA_ARGS__)
#define opt38(a, ...) opt_##a : 1, opt37(__VA_ARGS__)
#define opt39(a, ...) opt_##a : 1, opt38(__VA_ARGS__)
#define opt40(a, ...) opt_##a : 1, opt39(__VA_ARGS__)

#define GROUPS(...) GROUPS_I1(PP_NARG(__VA_ARGS__))(__VA_ARGS__)
#define GROUPS_I1(n) GROUPS_I2(n)
#define GROUPS_I2(n) GROUPS##n
#define GROUPS1(a) grp(grp_##a);

#define grpn(n, ...) grp##n(__VA_ARGS__)
#define grpn_(n, ...) grpn(n, __VA_ARGS__)
#define grp(...) grpn_(NO(__VA_ARGS__), __VA_ARGS__)
#define grp__(name, ...) grp_##name, NO(__VA_ARGS__)

// clang-format off
#define grp1(a) uint64_t : 1
// clang-format on
#define grp2(a, n) uint64_t a : n

#define OPTIONS2(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS1(__VA_ARGS__)

#define OPTIONS3(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS2(__VA_ARGS__)

#define OPTIONS4(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS3(__VA_ARGS__)

#define OPTIONS5(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS4(__VA_ARGS__)

#define OPTIONS6(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS5(__VA_ARGS__)

#define OPTIONS7(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS6(__VA_ARGS__)

#define OPTIONS8(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS7(__VA_ARGS__)

#define OPTIONS9(a, ...)                                                       \
  OPTIONS1(a)                                                                  \
  OPTIONS8(__VA_ARGS__)

#define OPTIONS10(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS9(__VA_ARGS__)

#define OPTIONS11(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS10(__VA_ARGS__)

#define OPTIONS12(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS11(__VA_ARGS__)

#define OPTIONS13(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS12(__VA_ARGS__)

#define OPTIONS14(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS13(__VA_ARGS__)

#define OPTIONS15(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS14(__VA_ARGS__)

#define OPTIONS16(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS15(__VA_ARGS__)

#define OPTIONS17(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS16(__VA_ARGS__)

#define OPTIONS18(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS17(__VA_ARGS__)

#define OPTIONS19(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS18(__VA_ARGS__)

#define OPTIONS20(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS19(__VA_ARGS__)

#define OPTIONS21(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS20(__VA_ARGS__)

#define OPTIONS22(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS21(__VA_ARGS__)

#define OPTIONS23(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS22(__VA_ARGS__)

#define OPTIONS24(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS23(__VA_ARGS__)

#define OPTIONS25(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS24(__VA_ARGS__)

#define OPTIONS26(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS25(__VA_ARGS__)

#define OPTIONS27(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS26(__VA_ARGS__)

#define OPTIONS28(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS17(__VA_ARGS__)

#define OPTIONS29(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS28(__VA_ARGS__)

#define OPTIONS30(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS29(__VA_ARGS__)

#define OPTIONS31(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS30(__VA_ARGS__)
#define OPTIONS32(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS31(__VA_ARGS__)

#define OPTIONS33(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS32(__VA_ARGS__)

#define OPTIONS34(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS33(__VA_ARGS__)

#define OPTIONS35(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS34(__VA_ARGS__)

#define OPTIONS36(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS35(__VA_ARGS__)

#define OPTIONS37(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS36(__VA_ARGS__)

#define OPTIONS38(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS37(__VA_ARGS__)

#define OPTIONS39(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS38(__VA_ARGS__)

#define OPTIONS40(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS39(__VA_ARGS__)

#define OPTIONS41(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS40(__VA_ARGS__)

#define OPTIONS42(a, ...)                                                      \
  OPTIONS1(a)                                                                  \
  OPTIONS41(__VA_ARGS__)

#define GROUPS2(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS1(__VA_ARGS__)

#define GROUPS3(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS2(__VA_ARGS__)

#define GROUPS4(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS3(__VA_ARGS__)

#define GROUPS5(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS4(__VA_ARGS__)

#define GROUPS6(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS5(__VA_ARGS__)

#define GROUPS7(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS6(__VA_ARGS__)

#define GROUPS8(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS7(__VA_ARGS__)

#define GROUPS9(a, ...)                                                        \
  GROUPS1(a)                                                                   \
  GROUPS8(__VA_ARGS__)

#define GROUPS10(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS9(__VA_ARGS__)

#define GROUPS11(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS10(__VA_ARGS__)

#define GROUPS12(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS11(__VA_ARGS__)

#define GROUPS13(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS12(__VA_ARGS__)

#define GROUPS14(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS13(__VA_ARGS__)

#define GROUPS15(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS14(__VA_ARGS__)

#define GROUPS16(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS15(__VA_ARGS__)

#define GROUPS17(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS16(__VA_ARGS__)

#define GROUPS18(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS17(__VA_ARGS__)

#define GROUPS19(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS18(__VA_ARGS__)

#define GROUPS20(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS19(__VA_ARGS__)

#define GROUPS21(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS20(__VA_ARGS__)

#define GROUPS22(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS21(__VA_ARGS__)

#define GROUPS23(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS22(__VA_ARGS__)

#define GROUPS24(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS23(__VA_ARGS__)

#define GROUPS25(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS24(__VA_ARGS__)

#define GROUPS26(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS25(__VA_ARGS__)

#define GROUPS27(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS26(__VA_ARGS__)

#define GROUPS28(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS17(__VA_ARGS__)

#define GROUPS29(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS28(__VA_ARGS__)

#define GROUPS30(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS29(__VA_ARGS__)

#define GROUPS31(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS30(__VA_ARGS__)
#define GROUPS32(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS31(__VA_ARGS__)

#define GROUPS33(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS32(__VA_ARGS__)

#define GROUPS34(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS33(__VA_ARGS__)

#define GROUPS35(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS34(__VA_ARGS__)

#define GROUPS36(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS35(__VA_ARGS__)

#define GROUPS37(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS36(__VA_ARGS__)

#define GROUPS38(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS37(__VA_ARGS__)

#define GROUPS39(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS38(__VA_ARGS__)

#define GROUPS40(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS39(__VA_ARGS__)

#define GROUPS41(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS40(__VA_ARGS__)

#define GROUPS42(a, ...)                                                       \
  GROUPS1(a)                                                                   \
  GROUPS41(__VA_ARGS__)
