#ifndef UTIL_MACROS_H__
#define UTIL_MACROS_H__

/* common macros are placed here, so as to avoid littering the code with
 * slightly different variations of them */

#ifndef MIN
#define MIN(x__,y__) \
    ((x__) < (y__) ? (x__) : (y__))
#endif

#ifndef MAX
#define MAX(x__,y__) \
    ((x__) > (y__) ? (x__) : (y__))
#endif

#define CLAMP(val__, low__, high__) \
    ((val__) < (low__) ? (low__) : ( (val__) > (high__) ? (high__) : (val__)))

#define ARRAY_SIZE(x__) (sizeof((x__))/sizeof((x__)[0]))

#define STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)

/* NULL sort order in *cmp funcs */
#define NULLCMP(l,r)                       \
  if ((l) == NULL && (r) == NULL) {        \
    return 0;                              \
  } else if ((l) == NULL && (r) != NULL) { \
    return 1;                              \
  } else if ((l) != NULL && (r) == NULL) { \
    return -1;                             \
  }

#endif
