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
    unsigned int px, py, step;
    int i;
    double re, im, zr, zi, zr2, zi2, mag2;
    double half_w, half_h;
    tess_u8 *p;
    unsigned int it;
    double frac;

    half_w = (double)a->width * 0.5;
    half_h = (double)a->height * 0.5;
    p = out;
    step = a->step > 0 ? a->step : 1;

    /*
     * With step > 1 this samples a coarse grid over the same area: the
     * eighth-scale first pass computes 1/64 of the pixels for a whole-image
     * preview that costs 1.6% of the work (DESIGN.md section 3).
     */
    for (py = a->y; py < a->y + a->h; py += step) {
        im = a->cy + ((double)py - half_h) * a->scale;
        for (px = a->x; px < a->x + a->w; px += step) {
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

/*
 * The ramps. Each maps a position t in [0,1) to a colour; none of them knows
 * anything about fractals, and adding one is a case here plus a name in the
 * GUI's table. They run in the GUI, so switching costs no network traffic.
 */
static void ramp_rgb(int ramp, double t, unsigned int *pr, unsigned int *pg,
                     unsigned int *pb)
{
    double r = 0.0, g = 0.0, b = 0.0;
    double u;

    switch (ramp) {
    case 1:                                     /* grey */
        r = g = b = t;
        break;

    case 2:                                     /* fire */
        if (t < 0.4) {
            r = t / 0.4;
        } else {
            r = 1.0;
        }
        if (t > 0.35) {
            g = (t - 0.35) / 0.45;
        }
        if (t > 0.75) {
            b = (t - 0.75) / 0.25;
        }
        break;

    case 3:                                     /* ice */
        if (t < 0.5) {
            b = 0.3 + t;
            g = t * 0.6;
        } else {
            b = 1.0;
            g = 0.3 + (t - 0.5) * 1.4;
            r = (t - 0.5) * 1.6;
        }
        break;

    case 4:                                     /* spectrum, hue around */
        u = t * 6.0;
        if (u < 1.0)      { r = 1.0; g = u; }
        else if (u < 2.0) { r = 2.0 - u; g = 1.0; }
        else if (u < 3.0) { g = 1.0; b = u - 2.0; }
        else if (u < 4.0) { g = 4.0 - u; b = 1.0; }
        else if (u < 5.0) { r = u - 4.0; b = 1.0; }
        else              { r = 1.0; b = 6.0 - u; }
        break;

    case 5:                                     /* copper */
        r = t * 1.3;
        g = t * 0.8;
        b = t * 0.5;
        break;

    default:                                    /* blue through gold */
        if (t < 0.5) {
            r = t * 1.2;
            g = t * 0.7;
            b = 0.35 + t;
        } else {
            r = 0.6 + (t - 0.5) * 0.8;
            g = 0.35 + (t - 0.5) * 1.3;
            b = 0.85 - (t - 0.5) * 0.6;
        }
        break;
    }

    if (r < 0.0) r = 0.0;
    if (g < 0.0) g = 0.0;
    if (b < 0.0) b = 0.0;
    *pr = (unsigned int)(255.0 * (r > 1.0 ? 1.0 : r));
    *pg = (unsigned int)(255.0 * (g > 1.0 ? 1.0 : g));
    *pb = (unsigned int)(255.0 * (b > 1.0 ? 1.0 : b));
}

static tess_u32 rgb(unsigned int r, unsigned int g, unsigned int b)
{
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

void tess_palette_default(TessPalette *p)
{
    p->ramp = 0;
    p->cycles = 1;
    p->rotate = 0;
    p->interior = 0;
}

void tess_shade(const tess_u8 *in, int npix, int max_iter,
                const TessPalette *pal, tess_u32 *rgb_out)
{
    int k;
    unsigned int it;
    double t, f;
    unsigned int r, g, b;
    int cycles;

    cycles = pal->cycles > 0 ? pal->cycles : 1;

    for (k = 0; k < npix; k++) {
        it = ((unsigned int)in[k * 3] << 8) | (unsigned int)in[k * 3 + 1];
        f  = (double)in[k * 3 + 2] / 255.0;

        if (it >= (unsigned int)max_iter) {
            rgb_out[k] = pal->interior ? rgb(255, 255, 255) : rgb(0, 0, 0);
            continue;
        }

        /* Position along the ramp: repeated `cycles` times and rotated, both
           of which are pure GUI arithmetic on data the cluster already sent. */
        t = ((double)it + f) / (double)max_iter;
        t = t * (double)cycles + (double)pal->rotate / 256.0;
        t = t - (double)(int)t;
        if (t < 0.0) {
            t += 1.0;
        }

        ramp_rgb(pal->ramp, t, &r, &g, &b);
        if (r > 255u) r = 255u;
        if (g > 255u) g = 255u;
        if (b > 255u) b = 255u;
        rgb_out[k] = rgb(r, g, b);
    }
}
