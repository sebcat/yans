#ifndef U8_H_
#define U8_H_
#include <stdint.h>
#include <stddef.h>

int32_t u8_to_cp(const uint8_t *s, size_t len, size_t *width);
#endif
