/*
 * tess_modules.c - the list of compute modules.
 *
 * Adding a module is a new src/common/tess_mod_<name>.c, which the Makefile
 * picks up by wildcard, plus its two lines here. Nothing else.
 *
 * The first entry is the default: what the GUI opens with, and what a job
 * from an older GUI that names no module gets.
 *
 * C89.
 */

#include <stddef.h>

#include "tess_module.h"

extern const TessModule tess_mod_mandel;
extern const TessModule tess_mod_julia;

static const TessModule *const modules[] = {
    &tess_mod_mandel,
    &tess_mod_julia
};

#define NMODULES ((int)(sizeof modules / sizeof modules[0]))

int tess_module_count(void)
{
    return NMODULES;
}

const TessModule *tess_module_at(int i)
{
    if (i < 0 || i >= NMODULES) {
        return modules[0];
    }
    return modules[i];
}

const TessModule *tess_module_find(tess_u32 id)
{
    int i;

    for (i = 0; i < NMODULES; i++) {
        if (modules[i]->id == id) {
            return modules[i];
        }
    }
    return (const TessModule *)0;
}

int tess_module_index(tess_u32 id)
{
    int i;

    for (i = 0; i < NMODULES; i++) {
        if (modules[i]->id == id) {
            return i;
        }
    }
    return 0;
}
