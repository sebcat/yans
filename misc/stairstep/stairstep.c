#include <stdint.h>
#include "stairstep.h"

#define CLAMP(val__, low__, high__) \
  ((val__) < (low__) ?              \
    (low__) :                       \
    ((val__) > (high__) ?           \
      (high__) :                    \
      (val__)))

struct ss_surface_s {
  size_t width;
  size_t height;
  size_t stride;
};

/* these are capitalized to correspond to the 'BMP file format' wikipedia
 * page, which maps out several separate BMP headers. Some variables were
 * said to be signed, but are unsigned here for simplicity. Maybe clamp
 * them on write-out to avoid overflow in other readers. */
struct BITMAPINFOHEADER {
  uint32_t header_size;     /* size of this header */
  uint32_t width;
  uint32_t height;
  uint16_t nbr_planes;      /* # of color planes, must be 1 */
  uint16_t bpp;
  uint32_t compression;
  uint32_t image_size;
  uint32_t horiz_ppm;
  uint32_t vertical_ppm;
  uint32_t nbr_palette_colors;
  uint32_t nbr_important_colors;
};

struct BITMAPFILEHEADER {
  uint16_t magic; /* 'BM' little endian */
  uint32_t file_size;
  uint16_t res0;
  uint16_t res1;
  uint32_t image_data_offset;
};


ss_surface_t *ss_surface_new(size_t width, size_t height) {
  size_t stride;

  stride = (width + 3) & ~3; /* divisible by four */
  /* TODO: Implement*/
  return NULL;
}

void ss_surface_free(ss_surface_t *ss_surface) {
  /* TODO: Implement*/
}

int ss_surface_write_bmp(ss_surface_t *ss_surface, FILE *out) {
  /* TODO: Implement*/
  return SS_ERR;
}

int ss_surface_read_bmp(ss_surface_t *ss_surface, FILE *in) {
  /* TODO: Implement*/
  return SS_ERR;
}

void ss_line(ss_surface_t *surface, double from_x, double from_y,
    double to_x, double to_y) {
    from_x = CLAMP(from_x, 0.0, 1.0);
    from_y = CLAMP(from_y, 0.0, 1.0);
    to_x = CLAMP(to_y, 0.0, 1.0);
    to_y = CLAMP(to_y, 0.0, 1.0);
   
  /* TODO: Implement*/
}

