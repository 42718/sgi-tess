/*
 * tess_params.c - build a settings pane from a descriptor table.
 *
 * Nothing here knows what a fractal is. It knows doubles, ints, booleans and
 * enumerations, where they live, and whether changing one needs the cluster.
 * A second module therefore ships a table and gets a pane.
 *
 * C89, Motif 2.1. Widgets are created with the varargs form throughout: an Arg
 * array converts a Widget pointer to XtArgVal, which MIPSpro warns about at -64.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "tess_params.h"

typedef struct Binding {
    TessParamPane *pane;
    int            index;
    int            choice;      /* ENUM only */
} Binding;

static void write_value(TessParamPane *p, int i, double v)
{
    const TessParamDesc *d = &p->descs[i];
    char *slot = p->values + d->offset;

    if (d->type == TESS_P_DOUBLE) {
        if (d->lo < d->hi) {
            if (v < d->lo) v = d->lo;
            if (v > d->hi) v = d->hi;
        }
        memcpy(slot, (char *)&v, sizeof(double));
    } else {
        int iv = (int)v;

        if (d->lo < d->hi) {
            if (iv < (int)d->lo) iv = (int)d->lo;
            if (iv > (int)d->hi) iv = (int)d->hi;
        }
        memcpy(slot, (char *)&iv, sizeof(int));
    }
}

static double read_value(TessParamPane *p, int i)
{
    const TessParamDesc *d = &p->descs[i];
    char *slot = p->values + d->offset;

    if (d->type == TESS_P_DOUBLE) {
        double v;

        memcpy((char *)&v, slot, sizeof(double));
        return v;
    } else {
        int iv;

        memcpy((char *)&iv, slot, sizeof(int));
        return (double)iv;
    }
}

static void text_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Binding *b = (Binding *)cd;
    char *text;
    double v;

    text = XmTextFieldGetString(w);
    if (!text) {
        return;
    }
    v = atof(text);
    XtFree(text);
    write_value(b->pane, b->index, v);
    tess_params_refresh(b->pane);
    if (b->pane->apply) {
        b->pane->apply(b->pane->ctx, &b->pane->descs[b->index]);
    }
}

static void toggle_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Binding *b = (Binding *)cd;
    XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)cb;

    write_value(b->pane, b->index, s->set ? 1.0 : 0.0);
    if (b->pane->apply) {
        b->pane->apply(b->pane->ctx, &b->pane->descs[b->index]);
    }
}

static void radio_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Binding *b = (Binding *)cd;
    XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)cb;

    if (!s->set) {
        return;                 /* only the newly selected one matters */
    }
    write_value(b->pane, b->index, (double)b->choice);
    if (b->pane->apply) {
        b->pane->apply(b->pane->ctx, &b->pane->descs[b->index]);
    }
}

TessParamPane *tess_params_build(Widget parent, const char *title,
                                 const TessParamDesc *descs, int n,
                                 void *values, TessParamApply apply,
                                 void *ctx)
{
    TessParamPane *p;
    Widget frame, rc, row;
    Binding *b;
    int i, j;

    p = (TessParamPane *)calloc(1, sizeof(TessParamPane));
    if (!p) {
        return (TessParamPane *)0;
    }
    p->descs = descs;
    p->n = n > 32 ? 32 : n;
    p->values = (char *)values;
    p->apply = apply;
    p->ctx = ctx;

    frame = XtVaCreateManagedWidget("frame", xmFrameWidgetClass, parent,
                                    XmNshadowType, XmSHADOW_ETCHED_IN,
                                    NULL);
    XtVaCreateManagedWidget(title, xmLabelWidgetClass, frame,
                            XmNchildType, XmFRAME_TITLE_CHILD,
                            NULL);
    rc = XtVaCreateManagedWidget("rc", xmRowColumnWidgetClass, frame,
                                 XmNorientation, XmVERTICAL,
                                 NULL);
    p->form = rc;

    for (i = 0; i < p->n; i++) {
        row = XtVaCreateManagedWidget("row", xmRowColumnWidgetClass, rc,
                                      XmNorientation, XmHORIZONTAL,
                                      NULL);
        XtVaCreateManagedWidget(descs[i].label, xmLabelWidgetClass, row,
                                NULL);

        b = (Binding *)calloc(1, sizeof(Binding));
        b->pane = p;
        b->index = i;

        if (descs[i].type == TESS_P_BOOL) {
            p->w[i] = XtVaCreateManagedWidget("t", xmToggleButtonWidgetClass,
                                              row, NULL);
            XtAddCallback(p->w[i], XmNvalueChangedCallback, toggle_cb,
                          (XtPointer)b);
        } else if (descs[i].type == TESS_P_ENUM) {
            Widget box;

            box = XtVaCreateManagedWidget("radio", xmRowColumnWidgetClass, row,
                                          XmNorientation, XmHORIZONTAL,
                                          XmNradioBehavior, True,
                                          XmNpacking, XmPACK_TIGHT,
                                          NULL);
            p->w[i] = box;
            for (j = 0; j < TESS_MAX_ENUM && descs[i].names[j]; j++) {
                Binding *rb = (Binding *)calloc(1, sizeof(Binding));

                rb->pane = p;
                rb->index = i;
                rb->choice = j;
                p->enumkids[i][j] =
                    XtVaCreateManagedWidget(descs[i].names[j],
                                            xmToggleButtonWidgetClass, box,
                                            NULL);
                XtAddCallback(p->enumkids[i][j], XmNvalueChangedCallback,
                              radio_cb, (XtPointer)rb);
            }
        } else {
            p->w[i] = XtVaCreateManagedWidget("f", xmTextFieldWidgetClass, row,
                                              XmNcolumns, 16,
                                              NULL);
            XtAddCallback(p->w[i], XmNactivateCallback, text_cb,
                          (XtPointer)b);
        }
    }

    tess_params_refresh(p);
    return p;
}

void tess_params_refresh(TessParamPane *p)
{
    char buf[64];
    double v;
    int i, j;

    for (i = 0; i < p->n; i++) {
        v = read_value(p, i);
        if (p->descs[i].type == TESS_P_BOOL) {
            XmToggleButtonSetState(p->w[i], v != 0.0 ? True : False, False);
        } else if (p->descs[i].type == TESS_P_ENUM) {
            for (j = 0; j < TESS_MAX_ENUM && p->descs[i].names[j]; j++) {
                XmToggleButtonSetState(p->enumkids[i][j],
                                       (j == (int)v) ? True : False, False);
            }
        } else {
            if (p->descs[i].type == TESS_P_DOUBLE) {
                sprintf(buf, "%.12g", v);
            } else {
                sprintf(buf, "%d", (int)v);
            }
            XmTextFieldSetString(p->w[i], buf);
        }
    }
}
