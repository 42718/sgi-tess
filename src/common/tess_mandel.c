/*
 * tess_mandel.c - escape time, and the colouring of it.
 *
 * The compute half runs on workers with no X and no palette. The shading half
 * runs in the GUI, which is what makes palette changes instant: the tile data
 * already in hand is re-shaded locally, and the cluster is not disturbed.
 *
 * -OPT:roundoff=0:IEEE_arithmetic=1 matters for this file specifically. The
 * escape test is a comparison against a reassociable expression, so a compiler
 * free to reorder it produces a different boundary on different hosts, which
 * shows up as visible seams between tiles computed by different machines.
 * DESIGN.md section 8.
 *
 * C89 throughout.
 */

#include <math.h>

#include "tess_mandel.h"

#define TESS_ESCAPE 4.0

void tess_mandel_tile(const TessAssign *a, tess_u8 *out)
{
    unsigned int px, py;
    int i;
    double re, im, zr, zi, zr2, zi2, mag2;
    double half_w, half_h;
    tess_u8 *p;
    unsigned int it;
    double frac;

    half_w = (double)a->width * 0.5;
    half_h = (double)a->height * 0.5;
    p = out;

    for (py = a->y; py < a->y + a->h; py++) {
        im = a->cy + ((double)py - half_h) * a->scale;
        for (px = a->x; px < a->x + a->w; px++) {
            re = a->cx + ((double)px - half_w) * a->scale;

            zr = 0.0;
            zi = 0.0;
            mag2 = 0.0;
            i = 0;
            while (i < (int)a->max_iter) {
                zr2 = zr * zr - zi * zi + re;
                zi2 = 2.0 * zr * zi + im;
                zr = zr2;
                zi = zi2;
                mag2 = zr * zr + zi * zi;
                if (mag2 > TESS_ESCAPE) {
                    break;
                }
                i++;
            }

            /* Smooth fraction: the usual log-log continuation, quantised to
               1/255 of an iteration, which is finer than any 256-entry palette
               can express. Interior pixels get iteration == max_iter and a
               zero fraction, so the GUI can test one value for "inside". */
            if (i >= (int)a->max_iter) {
                it = a->max_iter;
                frac = 0.0;
            } else {
                it = (unsigned int)i;
                if (mag2 > 1.0) {
                    frac = 1.0 - log(log(sqrt(mag2)) / log(2.0)) / log(2.0);
                    if (frac < 0.0) {
                        frac = 0.0;
                    }
                    if (frac > 0.999) {
                        frac = 0.999;
                    }
                } else {
                    frac = 0.0;
                }
            }

            if (it > 65535u) {
                it = 65535u;            /* v1 caps; promotion to u32 is later */
            }
            *p++ = (tess_u8)((it >> 8) & 0xff);
            *p++ = (tess_u8)(it & 0xff);
            *p++ = (tess_u8)(frac * 255.0);
        }
    }
}

static tess_u32 rgb(unsigned int r, unsigned int g, unsigned int b)
{
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

void tess_shade(const tess_u8 *in, int npix, int max_iter, int palette,
                tess_u32 *rgb_out)
{
    int k;
    unsigned int it;
    double t, f;
    unsigned int r, g, b;

    for (k = 0; k < npix; k++) {
        it = ((unsigned int)in[k * 3] << 8) | (unsigned int)in[k * 3 + 1];
        f  = (double)in[k * 3 + 2] / 255.0;

        if (it >= (unsigned int)max_iter) {
            rgb_out[k] = rgb(0, 0, 0);
            continue;
        }

        t = ((double)it + f) / (double)max_iter;

        if (palette == 1) {
            /* grey, for checking geometry without colour getting in the way */
            r = g = b = (unsigned int)(255.0 * t);
        } else {
            /* blue through gold to white: cheap, and it shows banding honestly */
            r = (unsigned int)(255.0 * (t < 0.5 ? t * 1.2 : 0.6 + (t - 0.5) * 0.8));
            g = (unsigned int)(255.0 * (t < 0.5 ? t * 0.7 : 0.35 + (t - 0.5) * 1.3));
            b = (unsigned int)(255.0 * (t < 0.5 ? 0.35 + t * 1.0 : 0.85 - (t - 0.5) * 0.6));
        }
        if (r > 255u) r = 255u;
        if (g > 255u) g = 255u;
        if (b > 255u) b = 255u;
        rgb_out[k] = rgb(r, g, b);
    }
}
