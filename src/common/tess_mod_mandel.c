/*
 * tess_mod_mandel.c - the Mandelbrot set, registered as a module.
 *
 * The compute is tess_mandel_tile in tess_mandel.c, unchanged. This file only
 * describes it to the shell: an id, a name, the opening view, and no
 * parameters of its own, since everything the Mandelbrot set needs is the
 * view the View pane already edits.
 *
 * C89.
 */

#include "tess_module.h"
#include "tess_mandel.h"

static void mandel_defaults(TessModValues *v)
{
    int k;

    for (k = 0; k < TESS_MOD_NPARAM; k++) {
        v->mp[k] = 0.0;
        v->sel[k] = 0;
    }
}

static void mandel_home(const TessModValues *v, double *cx, double *cy,
                        double *span)
{
    *cx = -0.6;
    *cy = 0.0;
    *span = 3.2;
}

const TessModule tess_mod_mandel = {
    1,
    "Mandelbrot",
    (const TessParamDesc *)0,
    0,
    mandel_defaults,
    mandel_home,
    (int (*)(const TessParamDesc *, TessModValues *))0,
    tess_mandel_tile
};
