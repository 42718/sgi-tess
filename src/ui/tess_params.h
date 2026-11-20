/*
 * tess_params.h - a parameter table, and the pane it generates.
 *
 * DESIGN.md section 5: "params is a descriptor table, so the settings pane is
 * generated, not hand-built - that is the piece that makes a second module
 * cheap, and it is worth building properly even though it looks like
 * over-engineering for one application."
 *
 * A descriptor says where the value lives (an offset into a caller-owned
 * struct), how to edit it, and - the field that earns its keep here - whether
 * changing it needs the cluster. TESS_LOCAL parameters are shading parameters:
 * they re-colour tile data already in hand and send nothing. Everything else
 * starts a new epoch.
 *
 * C89, Motif 2.1.
 */

#ifndef TESS_PARAMS_H
#define TESS_PARAMS_H

#include <Xm/Xm.h>

#include "tess_paramdesc.h"

/* Called after a value has been written back into the values struct. */
typedef void (*TessParamApply)(void *ctx, const TessParamDesc *d);

typedef struct TessParamPane {
    Widget                form;
    const TessParamDesc  *descs;
    int                   n;
    char                 *values;    /* the caller's struct */
    TessParamApply        apply;
    void                 *ctx;
    Widget                w[32];     /* the editing widget per parameter */
    Widget                enumkids[32][TESS_MAX_ENUM];
} TessParamPane;

/*
 * Build the pane into parent. Numeric fields commit on Return, toggles and
 * radio groups commit immediately - which is why the colour section has no
 * Apply button.
 */
TessParamPane *tess_params_build(Widget parent, const char *title,
                                 const TessParamDesc *descs, int n,
                                 void *values, TessParamApply apply,
                                 void *ctx);

/* Push the struct's current contents back into the widgets. */
void tess_params_refresh(TessParamPane *p);

#endif /* TESS_PARAMS_H */
