#ifndef STAIRSTEP_H__
#define STAIRSTEP_H__

#include <stdio.h>

#define SS_OK   0
#define SS_ERR -1

typedef struct ss_surface_s ss_surface_t;

ss_surface_t *ss_surface_new(size_t width, size_t height);
void ss_surface_free(ss_surface_t *ss_surface);
int ss_surface_write_bmp(ss_surface_t *ss_surface, FILE *out);
int ss_surface_read_bmp(ss_surface_t *ss_surface, FILE *in);

void ss_line(ss_surface_t *surface, double from_x, double from_y,
    double to_x, double to_y);

#endif
