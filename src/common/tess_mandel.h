/*
 * tess_mandel.h - the first module: escape time, and the colour for it.
 *
 * Split exactly as the module contract in DESIGN.md section 5 requires:
 * compute() never touches X and runs on the workers; shade() runs only in the
 * GUI. That is why the cluster ships iteration counts and the GUI owns the
 * palette, and why recolouring costs no network traffic.
 */

#ifndef TESS_MANDEL_H
#define TESS_MANDEL_H

#include "tess_types.h"
#include "tess_proto.h"

/*
 * Escape time for one tile into out, w*h*3 bytes: u16 iteration count
 * big-endian-free (native, it never crosses the GUI wire unconverted) plus a
 * u8 smooth fraction. Pixel (px,py) is in whole-image coordinates.
 */
void tess_mandel_tile(const TessAssign *a, tess_u8 *out);

/*
 * Colouring, entirely in the GUI. DESIGN.md section 5 gives shade() a
 * TessPalette; this is it. None of these fields reach the cluster, which is
 * the whole point: changing any of them re-shades tile data already in hand.
 */
typedef struct TessPalette {
    int ramp;        /* 0 blue-gold, 1 grey */
    int cycles;      /* how many times the ramp repeats across the range */
    int rotate;      /* 0..255, shifts the ramp */
    int interior;    /* 0 black, 1 white */
} TessPalette;

void tess_palette_default(TessPalette *p);
void tess_shade(const tess_u8 *in, int npix, int max_iter,
                const TessPalette *pal, tess_u32 *rgb_out);

#endif /* TESS_MANDEL_H */
