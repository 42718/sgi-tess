/*
 * tess_module.h - what a compute module is, and where they are listed.
 *
 * DESIGN.md section 5 is the contract: a module is a few functions and a
 * parameter table, and the shell, scheduler, transport and panel know nothing
 * else about it. Build 6 proves that by adding a second module as new files
 * plus one line in tess_modules.c. If adding one ever needs an edit anywhere
 * else, the abstraction is wrong and that edit is the bug.
 *
 * Two deliberate departures from the sketch in section 5, both because only
 * escape-time modules exist yet:
 *
 *   - No plan(). Every module is tiled the same way by the scheduler, so a
 *     per-module planner would be a hook with one implementation.
 *   - No shade() or state_size(). Every module produces the same 3-byte
 *     escape-time pixel, which tess_shade() colours. The first module with a
 *     different pixel format is where these get added, not before.
 *
 * Module parameters travel as TESS_MOD_NPARAM doubles in the job and in every
 * assignment, so the wire format does not change when a module is added. A
 * module's parameter table addresses them with TESS_MP(k).
 *
 * C89. No X, no MPI: workers and the GUI both link this.
 */

#ifndef TESS_MODULE_H
#define TESS_MODULE_H

#include "tess_types.h"
#include "tess_proto.h"
#include "tess_paramdesc.h"

/*
 * What a module's parameter pane edits. mp[] goes to the cluster in every job;
 * sel[] stays in the GUI, for choices that only exist to set mp[], such as
 * Julia's presets. A parameter table addresses them with TESS_MP(k) and
 * TESS_MSEL(k).
 */
typedef struct TessModValues {
    double mp[TESS_MOD_NPARAM];
    int    sel[TESS_MOD_NPARAM];
} TessModValues;

#define TESS_MP(k)   ((int)((k) * sizeof(double)))
#define TESS_MSEL(k) ((int)(TESS_MOD_NPARAM * sizeof(double) + \
                            (k) * sizeof(int)))

/* What changed() returns. */
#define TESS_MOD_PANE 1
#define TESS_MOD_HOME 2

typedef struct TessModule {
    tess_u32             id;        /* on the wire; never reuse one */
    const char          *name;      /* in the Fractal menu */
    const TessParamDesc *params;    /* offsets: TESS_MP(k), TESS_MSEL(k) */
    int                  nparams;   /* at most TESS_MOD_NPARAM */

    /* The pane's opening values, used when the module is chosen. */
    void (*defaults)(TessModValues *v);

    /* The view that frames the picture these values make: centre, and width
       of the frame in complex-plane units at 4:3. Home goes here and keeps
       the values, so Home on a Julia preset frames that preset. */
    void (*home)(const TessModValues *v, double *cx, double *cy,
                 double *span);

    /* One of this module's own parameters was just edited, in the GUI. Lets
       one parameter set others, as a preset sets c. Returns TESS_MOD_PANE if
       it changed any values, so the pane is redrawn, plus TESS_MOD_HOME if
       the view should go to home() for the new values. NULL when not
       needed. */
    int  (*changed)(const TessParamDesc *d, TessModValues *v);

    /* One tile, on a worker. Same output as tess_mandel_tile: w*h*3 bytes of
       u16 iteration count and u8 smooth fraction. Never touches X. */
    void (*compute)(const TessAssign *a, tess_u8 *out);
} TessModule;

int               tess_module_count(void);
const TessModule *tess_module_at(int i);

/* By wire id. NULL for an id this build does not know. */
const TessModule *tess_module_find(tess_u32 id);

/* Index of a wire id in the list, or 0 for one it does not know. */
int               tess_module_index(tess_u32 id);

#endif /* TESS_MODULE_H */
