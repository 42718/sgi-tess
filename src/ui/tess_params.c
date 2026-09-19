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
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Scale.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "tess_params.h"

/* The colour an indicator fills with when set: the palette's ok green. */
static Pixel select_pixel(Widget w)
{
    Display *d = XtDisplay(w);
    Colormap cm = DefaultColormap(d, DefaultScreen(d));
    XColor want, exact;

    if (XAllocNamedColor(d, cm, "#5f9e4a", &want, &exact)) {
        return (Pixel)want.pixel;
    }
    return BlackPixel(d, DefaultScreen(d));
}

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

static void toggle_label(Widget w, int on)
{
    XmString s = XmStringCreateLocalized(on ? "on" : "off");

    XtVaSetValues(w, XmNlabelString, s, NULL);
    XmStringFree(s);
}

static void toggle_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Binding *b = (Binding *)cd;
    XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)cb;

    toggle_label(w, s->set ? 1 : 0);
    write_value(b->pane, b->index, s->set ? 1.0 : 0.0);
    if (b->pane->apply) {
        b->pane->apply(b->pane->ctx, &b->pane->descs[b->index]);
    }
}

static void scale_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Binding *b = (Binding *)cd;
    XmScaleCallbackStruct *sc = (XmScaleCallbackStruct *)cb;

    write_value(b->pane, b->index, (double)sc->value);
    if (b->pane->apply) {
        b->pane->apply(b->pane->ctx, &b->pane->descs[b->index]);
    }
}

/* An option menu entry was chosen. */
static void menu_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Binding *b = (Binding *)cd;

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
        Widget lab;

        /*
         * One form per row, all sharing a fraction base, so the labels line up
         * in a right-aligned column and the fields start at the same x. A
         * RowColumn of label/widget pairs cannot do that: each row sizes
         * itself, which is what made the panel look hand-assembled.
         */
        row = XtVaCreateManagedWidget("row", xmFormWidgetClass, rc,
                                      XmNfractionBase, 100,
                                      NULL);
        /* Left-aligned in a fixed column: right-aligned labels left a wide
           empty gutter down the left of the panel and read as misaligned. */
        lab = XtVaCreateManagedWidget(descs[i].label, xmLabelWidgetClass, row,
                                      XmNalignment, XmALIGNMENT_BEGINNING,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 44,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      NULL);
        (void)lab;

        b = (Binding *)calloc(1, sizeof(Binding));
        b->pane = p;
        b->index = i;

        if (descs[i].type == TESS_P_BOOL) {
            /*
             * A labelled indicator. An empty label left a box a few pixels
             * across that read as neither on nor off, which is worse than no
             * control at all: you could not tell what state it was in.
             */
            /*
             * Motif's indicator is a small square that shades slightly when
             * set, which is easy to miss. A strong fill colour and a label
             * that reads off or on make the state unmistakable without
             * fighting the widget set.
             */
            XmString on = XmStringCreateLocalized("off");

            p->w[i] = XtVaCreateManagedWidget("toggle",
                                              xmToggleButtonWidgetClass, row,
                                              XmNlabelString, on,
                                              XmNindicatorType, XmN_OF_MANY,
                                              XmNindicatorSize, 16,
                                              XmNselectColor,
                                              select_pixel(row),
                                              XmNspacing, 4,
                                              XmNleftAttachment,
                                              XmATTACH_POSITION,
                                              XmNleftPosition, 46,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment,
                                              XmATTACH_FORM,
                                              NULL);
            XmStringFree(on);
            XtAddCallback(p->w[i], XmNvalueChangedCallback, toggle_cb,
                          (XtPointer)b);
        } else if (descs[i].type == TESS_P_ENUM) {
            /*
             * A dropdown, not a row of radio buttons. Two choices fitted
             * across a narrow panel; six ramps do not, and a list also says
             * plainly that these are alternatives rather than independent
             * switches.
             */
            Widget pull, opt, item;
            XmString none;

            pull = XmCreatePulldownMenu(row, "pull", (ArgList)0, 0);
            for (j = 0; j < TESS_MAX_ENUM && descs[i].names[j]; j++) {
                Binding *mb = (Binding *)calloc(1, sizeof(Binding));

                mb->pane = p;
                mb->index = i;
                mb->choice = j;
                item = XtVaCreateManagedWidget(descs[i].names[j],
                                               xmPushButtonWidgetClass, pull,
                                               NULL);
                XtAddCallback(item, XmNactivateCallback, menu_cb,
                              (XtPointer)mb);
                p->enumkids[i][j] = item;
            }
            none = XmStringCreateLocalized("");
            opt = XtVaCreateManagedWidget("opt", xmRowColumnWidgetClass, row,
                                          XmNrowColumnType, XmMENU_OPTION,
                                          XmNsubMenuId, pull,
                                          XmNlabelString, none,
                                          XmNmarginHeight, 0,
                                          XmNmarginWidth, 0,
                                          XmNleftAttachment, XmATTACH_POSITION,
                                          XmNleftPosition, 45,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          NULL);
            XmStringFree(none);
            p->w[i] = opt;
        } else if (descs[i].type == TESS_P_INT && descs[i].lo < descs[i].hi &&
                   (descs[i].hi - descs[i].lo) <= 512.0) {
            /* A bounded integer is a thing to drag, not to type: cycles and
               rotate are both instant, so the slider shows its effect live. */
            p->w[i] = XtVaCreateManagedWidget("slider", xmScaleWidgetClass,
                                              row,
                                              XmNorientation, XmHORIZONTAL,
                                              XmNminimum, (int)descs[i].lo,
                                              XmNmaximum, (int)descs[i].hi,
                                              XmNshowValue, True,
                                              XmNleftAttachment,
                                              XmATTACH_POSITION,
                                              XmNleftPosition, 45,
                                              XmNrightAttachment,
                                              XmATTACH_FORM,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment,
                                              XmATTACH_FORM,
                                              NULL);
            XtAddCallback(p->w[i], XmNvalueChangedCallback, scale_cb,
                          (XtPointer)b);
            XtAddCallback(p->w[i], XmNdragCallback, scale_cb, (XtPointer)b);
        } else {
            p->w[i] = XtVaCreateManagedWidget("f", xmTextFieldWidgetClass, row,
                                              XmNcolumns, 18,
                                              XmNleftAttachment,
                                              XmATTACH_POSITION,
                                              XmNleftPosition, 45,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment,
                                              XmATTACH_FORM,
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
            toggle_label(p->w[i], v != 0.0);
        } else if (p->descs[i].type == TESS_P_ENUM) {
            j = (int)v;
            if (j >= 0 && j < TESS_MAX_ENUM && p->enumkids[i][j]) {
                XtVaSetValues(p->w[i], XmNmenuHistory, p->enumkids[i][j],
                              NULL);
            }
        } else if (XmIsScale(p->w[i])) {
            XmScaleSetValue(p->w[i], (int)v);
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
