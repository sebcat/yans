#include <stddef.h>
#include <stdint.h>

struct prng_ctx {
  uint32_t t[624];
  uint32_t mag[2];
  uint16_t i;
};

void prng_init(struct prng_ctx *ctx, uint32_t seed);
uint32_t prng_uint32(struct prng_ctx *ctx);
void prng_hex(struct prng_ctx *ctx, char *data, size_t len);
