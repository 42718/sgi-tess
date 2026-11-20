/*
 * tess_mod_julia.c - Julia sets, the second module.
 *
 * Build 6's test of the module contract: this file plus two lines in
 * tess_modules.c, and nothing else. The scheduler, transport, panel and
 * shading do not know it exists.
 *
 * The same iteration as the Mandelbrot set with the roles swapped: z starts
 * at the pixel and c is fixed for the whole picture, so c is the module's own
 * parameter rather than part of the view. Every c gives a different set; the
 * default is the familiar dendrite-and-spiral one near the Mandelbrot set's
 * main cardioid edge.
 *
 * The escape loop and smooth fraction are written out again rather than
 * shared with tess_mandel.c, because sharing them would mean editing that file
 * and the point of this one is that nothing else is edited. Same escape
 * radius, same quantisation, same interior rule, so the shading treats both
 * modules identically. Built with the same -OPT:roundoff=0 flags as every
 * worker source, so tiles agree across hosts (DESIGN.md section 8).
 *
 * C89.
 */

#include <math.h>

#include "tess_module.h"

#define JULIA_ESCAPE 4.0

/*
 * Named values of c, each a well-known Julia set and all inside the
 * Mandelbrot set, so each is one connected shape. A c outside it gives dust:
 * -0.8 + 0.3i looks like a faint ghost of the default, which is correct and
 * is why these are offered. The menu text repeats the numbers; keep the two
 * in step.
 */
/*
 * Each preset also has its own home frame. Every Julia set is symmetric
 * through the origin (z and -z escape together), so the centre is always 0;
 * only the width differs. Each span is the set's measured extent at 4:3 plus
 * about 10%: the spiral reaches re +-1.53, so the old shared 3.2 clipped it,
 * and the swirl is taller than wide, so it needs more width than its own.
 */
typedef struct JuliaPreset {
    double re, im, span;
} JuliaPreset;

static const JuliaPreset presets[] = {
    { -0.8,     0.156,  3.35 },
    { -0.123,   0.745,  3.2  },
    { -0.391,  -0.587,  3.0  },
    { -0.75,    0.0,    3.3  },
    {  0.0,     1.0,    3.4  },
    {  0.285,   0.01,   3.2  },
    { -0.7269,  0.1889, 3.3  }
};

/* For Custom c, where nothing is measured: a little wider than the widest
   preset. A c far from all of them may still need zooming out. */
#define JULIA_SPAN 3.6

#define NPRESETS ((int)(sizeof presets / sizeof presets[0]))
#define CUSTOM   NPRESETS        /* the last menu entry */

static const TessParamDesc julia_params[] = {
    { "", TESS_P_ENUM, TESS_MSEL(0), 0.0, (double)CUSTOM, TESS_CLUSTER,
      { "Spiral: c re -0.8, c im 0.156",
        "Rabbit: c re -0.123, c im 0.745",
        "Siegel disk: c re -0.391, c im -0.587",
        "San Marco: c re -0.75, c im 0",
        "Dendrite: c re 0, c im 1",
        "Swirl: c re 0.285, c im 0.01",
        "Tendrils: c re -0.7269, c im 0.1889",
        "Custom" }, 0, 0 },
    { "c re", TESS_P_DOUBLE, TESS_MP(0), -2.0, 2.0, TESS_CLUSTER, { 0 },
      6, 0 },
    { "c im", TESS_P_DOUBLE, TESS_MP(1), -2.0, 2.0, TESS_CLUSTER, { 0 },
      6, 1 }
};

static void julia_defaults(TessModValues *v)
{
    int k;

    for (k = 0; k < TESS_MOD_NPARAM; k++) {
        v->mp[k] = 0.0;
        v->sel[k] = 0;
    }
    v->mp[0] = presets[0].re;
    v->mp[1] = presets[0].im;
}

static void julia_home(const TessModValues *v, double *cx, double *cy,
                       double *span)
{
    int s = v->sel[0];

    *cx = 0.0;
    *cy = 0.0;
    *span = (s >= 0 && s < NPRESETS) ? presets[s].span : JULIA_SPAN;
}

/* A preset sets c and frames it; typing c by hand makes it Custom and
   leaves the view alone, since you are probably exploring from it. */
static int julia_changed(const TessParamDesc *d, TessModValues *v)
{
    int s;

    if (d == &julia_params[0]) {
        s = v->sel[0];
        if (s >= 0 && s < NPRESETS) {
            v->mp[0] = presets[s].re;
            v->mp[1] = presets[s].im;
            return TESS_MOD_PANE | TESS_MOD_HOME;
        }
        return 0;
    }
    if (v->sel[0] != CUSTOM) {
        v->sel[0] = CUSTOM;
        return TESS_MOD_PANE;
    }
    return 0;
}

static void julia_tile(const TessAssign *a, tess_u8 *out)
{
    unsigned int px, py, step;
    int i;
    double cr, ci, zr, zi, zr2, zi2, mag2;
    double half_w, half_h;
    tess_u8 *p;
    unsigned int it;
    double frac;

    cr = a->mp[0];
    ci = a->mp[1];
    half_w = (double)a->width * 0.5;
    half_h = (double)a->height * 0.5;
    p = out;
    step = a->step > 0 ? a->step : 1;

    for (py = a->y; py < a->y + a->h; py += step) {
        for (px = a->x; px < a->x + a->w; px += step) {
            zr = a->cx + ((double)px - half_w) * a->scale;
            zi = a->cy + ((double)py - half_h) * a->scale;
            mag2 = zr * zr + zi * zi;
            i = 0;
            while (i < (int)a->max_iter && mag2 <= JULIA_ESCAPE) {
                zr2 = zr * zr - zi * zi + cr;
                zi2 = 2.0 * zr * zi + ci;
                zr = zr2;
                zi = zi2;
                mag2 = zr * zr + zi * zi;
                if (mag2 > JULIA_ESCAPE) {
                    break;
                }
                i++;
            }

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
                it = 65535u;
            }
            *p++ = (tess_u8)((it >> 8) & 0xff);
            *p++ = (tess_u8)(it & 0xff);
            *p++ = (tess_u8)(frac * 255.0);
        }
    }
}

const TessModule tess_mod_julia = {
    2,
    "Julia",
    julia_params,
    (int)(sizeof julia_params / sizeof julia_params[0]),
    julia_defaults,
    julia_home,
    julia_changed,
    julia_tile
};
