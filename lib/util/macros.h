#ifndef UTIL_MACROS_H__
#define UTIL_MACROS_H__

/* common macros are placed here, so as to avoid littering the code with
 * slightly different variations of them */

#define MIN(x__,y__) \
    ((x__) < (y__) ? (x__) : (y__))

#define MAX(x__,y__) \
    ((x__) > (y__) ? (x__) : (y__))

#define CLAMP(val__, low__, high__) \
    ((val__) < (low__) ? (low__) : ( (val__) > (high__) ? (high__) : (val__)))

#endif