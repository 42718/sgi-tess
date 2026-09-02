/*
 * mandel_ui_c89.c — IRIX Motif Mandelbrot with panes, progress, dual CPU
 *
 * Build:
 *   cc -o mandel_ui mandel_ui_c89.c -lXm -lXt -lX11 -lm -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include <X11/Xlib.h>

#include <Xm/Xm.h>
#include <Xm/PanedW.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/DrawingA.h>
#include <Xm/Scale.h>
#include <Xm/ScrolledW.h>
#include <Xm/Text.h>

typedef unsigned int uint32_t;

/* ---------------- Rendering job ---------------- */

typedef struct RenderJob {
    int active;
    int cancel;

    int w, h;
    double cx, cy, scale;
    int max_iter;
    uint32_t *fb;

    int rows_done;
    pthread_mutex_t lock;

    pthread_t t0, t1;
} RenderJob;

/* ---------------- App state ---------------- */

typedef struct AppState {
    XtAppContext app;
    Widget toplevel;

    Widget paned;
    Widget top_form;
    Widget bottom_form;

    Widget canvas;
    Widget stats_text;
    Widget status_label;
    Widget progress_scale;

    Display *dpy;
    Visual  *visual;
    int depth;
    GC gc;
    XImage *ximg;

    int width, height;
    uint32_t *fb;

    double cx, cy, scale;
    int max_iter;

    RenderJob job;
} AppState;

/* ---------------- Utilities ---------------- */

static uint32_t rgb(unsigned r, unsigned g, unsigned b)
{
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

/* ---------------- XImage ---------------- */

static void recreate_ximage(AppState *s)
{
    if (s->ximg) {
        s->ximg->data = NULL;
        XDestroyImage(s->ximg);
        s->ximg = NULL;
    }

    s->ximg = XCreateImage(
        s->dpy,
        s->visual,
        (unsigned)s->depth,
        ZPixmap,
        0,
        (char *)s->fb,
        (unsigned)s->width,
        (unsigned)s->height,
        32,
        0
    );

    if (!s->ximg) {
        fprintf(stderr, "XCreateImage failed\n");
        exit(1);
    }
}

static void draw(AppState *s)
{
    Window win;
    if (!XtIsRealized(s->canvas)) return;

    win = XtWindow(s->canvas);
    if (!win) return;

    if (!s->gc)
        s->gc = XCreateGC(s->dpy, win, 0, NULL);

    XPutImage(s->dpy, win, s->gc, s->ximg,
              0, 0, 0, 0, s->width, s->height);
}

/* ---------------- Workers ---------------- */

typedef struct WorkerArgs {
    AppState *s;
    int y0, y1;
} WorkerArgs;

static int job_cancelled(RenderJob *j)
{
    int c;
    pthread_mutex_lock(&j->lock);
    c = j->cancel;
    pthread_mutex_unlock(&j->lock);
    return c;
}

static void job_add_row(RenderJob *j)
{
    pthread_mutex_lock(&j->lock);
    j->rows_done++;
    pthread_mutex_unlock(&j->lock);
}

static void *render_worker(void *arg)
{
    WorkerArgs *wa = (WorkerArgs *)arg;
    AppState *s = wa->s;
    RenderJob *j = &s->job;

    int x, y;

    for (y = wa->y0; y < wa->y1; y++) {
        double im;

        if (job_cancelled(j))
            break;

        im = j->cy + (y - j->h / 2.0) * j->scale;

        for (x = 0; x < j->w; x++) {
            double re, zr, zi;
            int i;

            re = j->cx + (x - j->w / 2.0) * j->scale;
            zr = 0.0;
            zi = 0.0;
            i = 0;

            while (i < j->max_iter) {
                double zr2 = zr * zr - zi * zi + re;
                double zi2 = 2.0 * zr * zi + im;
                zr = zr2;
                zi = zi2;
                if (zr * zr + zi * zi > 4.0) break;
                i++;
            }

            if (i == j->max_iter)
                j->fb[y * j->w + x] = rgb(0, 0, 0);
            else {
                unsigned v = (unsigned)(255.0 * i / j->max_iter);
                j->fb[y * j->w + x] = rgb(v, v, v);
            }
        }
        job_add_row(j);
    }

    free(wa);
    return NULL;
}

static void start_render(AppState *s)
{
    RenderJob *j = &s->job;
    WorkerArgs *a0, *a1;
    int mid;

    if (j->active) {
        pthread_mutex_lock(&j->lock);
        j->cancel = 1;
        pthread_mutex_unlock(&j->lock);
        pthread_join(j->t0, NULL);
        pthread_join(j->t1, NULL);
        j->active = 0;
    }

    pthread_mutex_lock(&j->lock);
    j->cancel = 0;
    j->rows_done = 0;
    pthread_mutex_unlock(&j->lock);

    j->w = s->width;
    j->h = s->height;
    j->cx = s->cx;
    j->cy = s->cy;
    j->scale = s->scale;
    j->max_iter = s->max_iter;
    j->fb = s->fb;

    XmScaleSetValue(s->progress_scale, 0);

    mid = s->height / 2;

    a0 = (WorkerArgs *)malloc(sizeof(WorkerArgs));
    a1 = (WorkerArgs *)malloc(sizeof(WorkerArgs));

    a0->s = s; a0->y0 = 0;     a0->y1 = mid;
    a1->s = s; a1->y0 = mid;   a1->y1 = s->height;

    j->active = 1;

    pthread_create(&j->t0, NULL, render_worker, a0);
    pthread_create(&j->t1, NULL, render_worker, a1);
}

/* ---------------- Timer ---------------- */

static void timer_cb(XtPointer cd, XtIntervalId *id)
{
    AppState *s = (AppState *)cd;
    RenderJob *j = &s->job;
    int rows, pct;
    char buf[256];
    XmString xs;

    (void)id;

    pthread_mutex_lock(&j->lock);
    rows = j->rows_done;
    pthread_mutex_unlock(&j->lock);

    pct = (int)(100.0 * rows / s->height);
    if (pct > 100) pct = 100;

    XmScaleSetValue(s->progress_scale, pct);

    if (rows >= s->height && j->active) {
        pthread_join(j->t0, NULL);
        pthread_join(j->t1, NULL);
        j->active = 0;
        draw(s);

        sprintf(buf, "Done  iter=%d  center=(%.6f, %.6f)",
                s->max_iter, s->cx, s->cy);
    } else {
        sprintf(buf, "Rendering... %d%%  iter=%d",
                pct, s->max_iter);
    }

    xs = XmStringCreateLocalized(buf);
    XtVaSetValues(s->status_label, XmNlabelString, xs, NULL);
    XmStringFree(xs);

    XtAppAddTimeOut(s->app, 50, timer_cb, (XtPointer)s);
}

/* ---------------- Callbacks ---------------- */

static void expose_cb(Widget w, XtPointer cd, XtPointer cb)
{
    (void)w; (void)cb;
    draw((AppState *)cd);
}

static void resize_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s = (AppState *)cd;
    Dimension nw, nh;

    (void)cb;

    XtVaGetValues(w, XmNwidth, &nw, XmNheight, &nh, NULL);
    if ((int)nw <= 0 || (int)nh <= 0) return;

    s->width = (int)nw;
    s->height = (int)nh;

    s->fb = (uint32_t *)realloc(
        s->fb,
        s->width * s->height * sizeof(uint32_t)
    );

    s->scale = 3.0 / s->width;

    recreate_ximage(s);
    start_render(s);
}

static void zoom(AppState *s, int x, int y, int in)
{
    s->cx += (x - s->width  / 2.0) * s->scale;
    s->cy += (y - s->height / 2.0) * s->scale;

    if (in) {
        s->scale *= 0.5;
        s->max_iter += 32;
    } else {
        s->scale *= 2.0;
        if (s->max_iter > 64) s->max_iter -= 32;
    }

    start_render(s);
}

static void input_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s = (AppState *)cd;
    XmDrawingAreaCallbackStruct *cbs =
        (XmDrawingAreaCallbackStruct *)cb;

    (void)w;

    if (!cbs || !cbs->event) return;

    if (cbs->event->type == ButtonPress) {
        XButtonEvent *e = (XButtonEvent *)cbs->event;
        if (e->button == Button1)
            zoom(s, e->x, e->y, 1);
        else if (e->button == Button3)
            zoom(s, e->x, e->y, 0);
    }
}

/* ---------------- UI helpers ---------------- */

static Widget make_stats_pane(Widget parent)
{
    Widget sw, text;

    sw = XtVaCreateManagedWidget(
        "statsSW",
        xmScrolledWindowWidgetClass,
        parent,
        XmNscrollingPolicy, XmAUTOMATIC,
        NULL
    );

    text = XtVaCreateManagedWidget(
        "statsText",
        xmTextWidgetClass,
        sw,
        XmNeditable, False,
        XmNcursorPositionVisible, False,
        XmNrows, 30,
        XmNcolumns, 55,
        XmNwidth, 360,
        XmNheight, 520,
        NULL
    );

    XmTextSetString(text,
        "Cluster / Stats\n"
        "rank host        tiles/s   ms/tile\n"
        "---- ----------- -------   -------\n"
    );

    return text;
}

/* ---------------- main ---------------- */

int main(int argc, char **argv)
{
    AppState s;
    Dimension cw, ch;

    memset(&s, 0, sizeof(s));
    pthread_mutex_init(&s.job.lock, NULL);

    s.toplevel = XtVaAppInitialize(
        &s.app, "MandelUI", NULL, 0, &argc, argv, NULL, NULL
    );

    s.paned = XtVaCreateManagedWidget(
        "paned",
        xmPanedWindowWidgetClass,
        s.toplevel,
        NULL
    );

    s.top_form = XtVaCreateManagedWidget(
        "topForm",
        xmFormWidgetClass,
        s.paned,
        NULL
    );

    s.bottom_form = XtVaCreateManagedWidget(
        "bottomForm",
        xmFormWidgetClass,
        s.paned,
        XmNmarginWidth, 8,
        XmNmarginHeight, 6,
        NULL
    );

    s.canvas = XtVaCreateManagedWidget(
        "canvas",
        xmDrawingAreaWidgetClass,
        s.top_form,
        XmNleftAttachment, XmATTACH_FORM,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 60,
        XmNwidth, 800,
        XmNheight, 600,
        NULL
    );

    s.stats_text = make_stats_pane(
        XtVaCreateManagedWidget(
            "statsForm",
            xmFormWidgetClass,
            s.top_form,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 60,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            NULL
        )
    );

    s.progress_scale = XtVaCreateManagedWidget(
        "progress",
        xmScaleWidgetClass,
        s.bottom_form,
        XmNorientation, XmHORIZONTAL,
        XmNminimum, 0,
        XmNmaximum, 100,
        XmNshowValue, True,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 55,
        NULL
    );

    s.status_label = XtVaCreateManagedWidget(
        "status",
        xmLabelWidgetClass,
        s.bottom_form,
        XmNalignment, XmALIGNMENT_END,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, s.progress_scale,
        XmNleftOffset, 10,
        NULL
    );

    XtAddCallback(s.canvas, XmNexposeCallback, expose_cb, &s);
    XtAddCallback(s.canvas, XmNresizeCallback, resize_cb, &s);
    XtAddCallback(s.canvas, XmNinputCallback,  input_cb,  &s);

    XtRealizeWidget(s.toplevel);

    s.dpy = XtDisplay(s.toplevel);
    s.visual = DefaultVisual(s.dpy, DefaultScreen(s.dpy));
    s.depth = DefaultDepth(s.dpy, DefaultScreen(s.dpy));

    XtVaGetValues(s.canvas, XmNwidth, &cw, XmNheight, &ch, NULL);
    s.width = (int)cw;
    s.height = (int)ch;

    s.fb = (uint32_t *)calloc(
        s.width * s.height, sizeof(uint32_t)
    );

    s.cx = -0.5;
    s.cy = 0.0;
    s.scale = 3.0 / s.width;
    s.max_iter = 256;

    recreate_ximage(&s);
    start_render(&s);

    XtAppAddTimeOut(s.app, 50, timer_cb, (XtPointer)&s);
    XtAppMainLoop(s.app);
    return 0;
}
