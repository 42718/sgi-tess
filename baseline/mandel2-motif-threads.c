/*
 * mandel_ui.c — IRIX Motif Mandelbrot UI skeleton (C89) with panes + progress + dual-CPU rendering
 *
 * Build:
 *   cc -o mandel_ui mandel_ui.c -lXm -lXt -lX11 -lm -lpthread
 *
 * UI:
 *   - Left: DrawingArea (canvas)
 *   - Right: ScrolledText (cluster stats placeholder)
 *   - Bottom: status label + progress (XmScale)
 *
 * Controls:
 *   Left click  : zoom in at cursor (restarts render)
 *   Right click : zoom out at cursor (restarts render)
 *
 * Notes: 
 * - Rendering is done in 2 pthreads (dual CPU) into a framebuffer.
 * - UI remains responsive; progress is updated by XtAppAddTimeOut().
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
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/DrawingA.h>
#include <Xm/Scale.h>
#include <Xm/ScrolledW.h>
#include <Xm/Text.h>

typedef unsigned int uint32_t;

typedef struct RenderJob {
    int active;
    int cancel;

    int w;
    int h;
    double cx, cy, scale;
    int max_iter;

    uint32_t *fb;

    /* progress */
    int rows_done;              /* 0..h */
    pthread_mutex_t lock;

    /* worker threads */
    pthread_t t0;
    pthread_t t1;
} RenderJob;

typedef struct AppState {
    /* Xt/Motif */
    XtAppContext app;
    Widget toplevel;

    Widget paned;          /* vertical panes: top + bottom */
    Widget top_form;       /* contains canvas + stats side-by-side */
    Widget bottom_rc;      /* contains status + progress */

    Widget canvas;
    Widget stats_text;
    Widget status_label;
    Widget progress_scale;

    /* X rendering */
    Display *dpy;
    Visual  *visual;
    int      depth;
    GC       gc;
    XImage  *ximg;

    /* current framebuffer dims */
    int width;
    int height;
    uint32_t *fb;

    /* view */
    double cx;
    double cy;
    double scale;
    int max_iter;

    /* render control */
    RenderJob job;

    /* UI timer */
    XtIntervalId timer_id;
} AppState;

/* ---------- pixel packing ---------- */
static uint32_t rgb(unsigned r, unsigned g, unsigned b)
{
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

/* ---------- XImage handling ---------- */
static void recreate_ximage(AppState *s)
{
    if (s->ximg) {
        s->ximg->data = NULL;  /* don't free fb */
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

    if (!s->gc) s->gc = XCreateGC(s->dpy, win, 0, NULL);

    XPutImage(s->dpy, win, s->gc, s->ximg, 0, 0, 0, 0, s->width, s->height);
    XFlush(s->dpy);
}

/* ---------- rendering workers ---------- */

typedef struct WorkerArgs {
    AppState *s;
    int y0;
    int y1; /* [y0, y1) */
} WorkerArgs;

static void job_add_rows(RenderJob *j, int n)
{
    pthread_mutex_lock(&j->lock);
    j->rows_done += n;
    pthread_mutex_unlock(&j->lock);
}

static int job_is_cancelled(RenderJob *j)
{
    int c;
    pthread_mutex_lock(&j->lock);
    c = j->cancel;
    pthread_mutex_unlock(&j->lock);
    return c;
}

static void *render_worker(void *arg)
{
    WorkerArgs *wa;
    AppState *s;
    RenderJob *j;
    int x, y;
    int w, h;
    double cx, cy, scale;
    int max_iter;
    uint32_t *fb;

    wa = (WorkerArgs *)arg;
    s = wa->s;
    j = &s->job;

    w = j->w;
    h = j->h;
    cx = j->cx;
    cy = j->cy;
    scale = j->scale;
    max_iter = j->max_iter;
    fb = j->fb;

    for (y = wa->y0; y < wa->y1; y++) {
        double im;

        if (job_is_cancelled(j)) break;

        im = cy + (y - h / 2.0) * scale;

        for (x = 0; x < w; x++) {
            double re, zr, zi;
            int i;

            re = cx + (x - w / 2.0) * scale;
            zr = 0.0;
            zi = 0.0;
            i = 0;

            while (i < max_iter) {
                double zr2, zi2;
                zr2 = zr * zr - zi * zi + re;
                zi2 = 2.0 * zr * zi + im;
                zr = zr2;
                zi = zi2;
                if (zr * zr + zi * zi > 4.0) break;
                i++;
            }

            if (i == max_iter) {
                fb[y * w + x] = rgb(0, 0, 0);
            } else {
                unsigned v;
                v = (unsigned)(255.0 * (double)i / (double)max_iter);
                fb[y * w + x] = rgb(v, v, v);
            }
        }

        job_add_rows(j, 1);
    }

    free(wa);
    return NULL;
}

/* start a new render (2 threads) */
static void start_render(AppState *s)
{
    RenderJob *j;
    int mid;
    WorkerArgs *a0, *a1;

    j = &s->job;

    /* cancel old job if active */
    if (j->active) {
        pthread_mutex_lock(&j->lock);
        j->cancel = 1;
        pthread_mutex_unlock(&j->lock);

        pthread_join(j->t0, NULL);
        pthread_join(j->t1, NULL);
        j->active = 0;
    }

    /* setup new job */
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

    /* reset progress UI */
    XmScaleSetValue(s->progress_scale, 0);
    XtVaSetValues(s->status_label, XmNlabelString,
                  XmStringCreateLocalized("Rendering..."), NULL);

    /* split rows */
    mid = s->height / 2;

    a0 = (WorkerArgs *)malloc(sizeof(WorkerArgs));
    a1 = (WorkerArgs *)malloc(sizeof(WorkerArgs));
    if (!a0 || !a1) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    a0->s = s; a0->y0 = 0;   a0->y1 = mid;
    a1->s = s; a1->y0 = mid; a1->y1 = s->height;

    j->active = 1;

    pthread_create(&j->t0, NULL, render_worker, a0);
    pthread_create(&j->t1, NULL, render_worker, a1);
}

/* ---------- progress timer ---------- */
static void timer_tick(XtPointer client_data, XtIntervalId *id)
{
    AppState *s;
    RenderJob *j;
    int rows, pct, done;
    char buf[256];
    XmString xs;

    (void)id;

    s = (AppState *)client_data;
    j = &s->job;

    rows = 0;
    pthread_mutex_lock(&j->lock);
    rows = j->rows_done;
    done = (rows >= s->height) ? 1 : 0;
    pthread_mutex_unlock(&j->lock);

    if (s->height > 0) pct = (int)(100.0 * (double)rows / (double)s->height);
    else pct = 0;

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    XmScaleSetValue(s->progress_scale, pct);

    if (done && j->active) {
        /* join threads once, mark inactive */
        pthread_join(j->t0, NULL);
        pthread_join(j->t1, NULL);
        j->active = 0;

        draw(s);

        sprintf(buf, "Done. iter=%d  center=(%.6f, %.6f)  scale=%.6g",
                s->max_iter, s->cx, s->cy, s->scale);
        xs = XmStringCreateLocalized(buf);
        XtVaSetValues(s->status_label, XmNlabelString, xs, NULL);
        XmStringFree(xs);
    } else {
        sprintf(buf, "Rendering... %d%%  iter=%d", pct, s->max_iter);
        xs = XmStringCreateLocalized(buf);
        XtVaSetValues(s->status_label, XmNlabelString, xs, NULL);
        XmStringFree(xs);

        /* optional: progressive redraw (cheap but helps) */
        if (pct % 10 == 0) draw(s);
    }

    /* reschedule */
    s->timer_id = XtAppAddTimeOut(s->app, 50, timer_tick, (XtPointer)s);
}

/* ---------- callbacks ---------- */
static void expose_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s;
    (void)w; (void)cb;
    s = (AppState *)cd;
    draw(s);
}

static void resize_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s;
    Dimension nw, nh;

    (void)cb;
    s = (AppState *)cd;

    XtVaGetValues(w, XmNwidth, &nw, XmNheight, &nh, NULL);

    if ((int)nw <= 0 || (int)nh <= 0) return;
    if ((int)nw == s->width && (int)nh == s->height) return;

    /* cancel any running render */
    if (s->job.active) {
        pthread_mutex_lock(&s->job.lock);
        s->job.cancel = 1;
        pthread_mutex_unlock(&s->job.lock);
        pthread_join(s->job.t0, NULL);
        pthread_join(s->job.t1, NULL);
        s->job.active = 0;
    }

    s->width = (int)nw;
    s->height = (int)nh;

    s->fb = (uint32_t *)realloc(s->fb, (size_t)s->width * (size_t)s->height * sizeof(uint32_t));
    if (!s->fb) {
        fprintf(stderr, "Out of memory realloc framebuffer\n");
        exit(1);
    }

    /* keep ~3 complex units across width */
    s->scale = 3.0 / (double)s->width;

    recreate_ximage(s);
    start_render(s);
}

static void zoom(AppState *s, int px, int py, int in)
{
    double re, im;

    re = s->cx + (px - s->width  / 2.0) * s->scale;
    im = s->cy + (py - s->height / 2.0) * s->scale;

    s->cx = re;
    s->cy = im;

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
    AppState *s;
    XmDrawingAreaCallbackStruct *cbs;

    (void)w;
    s = (AppState *)cd;
    cbs = (XmDrawingAreaCallbackStruct *)cb;

    if (!cbs || !cbs->event) return;

    if (cbs->event->type == ButtonPress) {
        XButtonEvent *e;
        e = (XButtonEvent *)cbs->event;

        if (e->button == Button1) zoom(s, e->x, e->y, 1);
        else if (e->button == Button3) zoom(s, e->x, e->y, 0);
    }
}

/* ---------- UI creation ---------- */
static Widget make_stats_pane(Widget parent)
{
    Widget sw, text;
    XmString xs;

    sw = XtVaCreateManagedWidget(
        "statsSW",
        xmScrolledWindowWidgetClass,
        parent,
        XmNscrollingPolicy, XmAUTOMATIC,
        XmNvisualPolicy, XmVARIABLE,
        NULL
    );

    text = XtVaCreateManagedWidget(
        "statsText",
        xmTextWidgetClass,
        sw,
        XmNeditable, False,
        XmNcursorPositionVisible, False,
        XmNrows, 10,
        XmNcolumns, 32,
        XmNwordWrap, False,
        XmNscrollHorizontal, True,
        NULL
    );

    xs = XmStringCreateLocalized(
        "Cluster stats (placeholder)\n"
        "rank host         tiles/s   ms/tile  last\n"
        "---- ------------  -------   -------  ----\n"
        "0    octane        0.0       0.0      -\n"
    );
    XmTextSetString(text,
        "Cluster stats (placeholder)\n"
        "rank host         tiles/s   ms/tile  last\n"
        "---- ------------  -------   -------  ----\n"
        "0    octane        0.0       0.0      -\n"
    );
    XmStringFree(xs);

    return text;
}

int main(int argc, char **argv)
{
    AppState s;
    Dimension cw, ch;
    XmString xs;
    Widget stats_label;
    Widget stats_container;

    memset(&s, 0, sizeof(s));

    s.toplevel = XtVaAppInitialize(
        &s.app,
        "MandelUI",
        NULL, 0,
        &argc, argv,
        NULL,
        NULL
    );

    /* Outer vertical paned window: top area + bottom status/progress */
    s.paned = XtVaCreateManagedWidget(
        "paned",
        xmPanedWindowWidgetClass,
        s.toplevel,
        XmNsashWidth,  6,
        XmNsashHeight, 6,
        NULL
    );

    /* Top form (side-by-side) */
    s.top_form = XtVaCreateManagedWidget(
        "topForm",
        xmFormWidgetClass,
        s.paned,
        NULL
    );

    /* Bottom row (status + progress) */
    s.bottom_rc = XtVaCreateManagedWidget(
        "bottomForm",
        xmFormWidgetClass,
        s.paned,
        XmNmarginWidth, 8,
        XmNmarginHeight, 6,
        NULL
    );

    /* --- Canvas on the left --- */
    s.canvas = XtVaCreateManagedWidget(
        "canvas",
        xmDrawingAreaWidgetClass,
        s.top_form,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 70,          /* 70% canvas */
        XmNwidth, 800,
        XmNheight, 600,
        XmNresizePolicy, XmRESIZE_ANY,
        NULL
    );

    /* --- Stats pane on the right --- */
    stats_container = XtVaCreateManagedWidget(
        "statsContainer",
        xmFormWidgetClass,
        s.top_form,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_POSITION,
        XmNleftPosition, 70,
        XmNrightAttachment, XmATTACH_FORM,
        NULL
    );

    stats_label = XtVaCreateManagedWidget(
        "statsLabel",
        xmLabelWidgetClass,
        stats_container,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL
    );
    xs = XmStringCreateLocalized("Cluster / Stats");
    XtVaSetValues(stats_label, XmNlabelString, xs, NULL);
    XmStringFree(xs);

    s.stats_text = make_stats_pane(stats_container);
    XtVaSetValues(
        XtParent(s.stats_text),
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, stats_label,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        NULL
    );

    /* --- Bottom status + progress --- */
    s.status_label = XtVaCreateManagedWidget(
        "status",
        xmLabelWidgetClass,
        s.bottom_rc,
        NULL
    );
    xs = XmStringCreateLocalized("Ready.");
    XtVaSetValues(s.status_label, XmNlabelString, xs, NULL);
    XmStringFree(xs);

    s.progress_scale = XtVaCreateManagedWidget(
        "progress",
        xmScaleWidgetClass,
        s.bottom_rc,
        XmNorientation, XmHORIZONTAL,
        XmNminimum, 0,
        XmNmaximum, 100,
        XmNshowValue, True,
        XmNvalue, 0,
        XmNscaleWidth, 250,
        XmNsensitive, False,
        NULL
    );

    /* Callbacks on canvas */
    XtAddCallback(s.canvas, XmNexposeCallback, expose_cb, (XtPointer)&s);
    XtAddCallback(s.canvas, XmNresizeCallback, resize_cb, (XtPointer)&s);
    XtAddCallback(s.canvas, XmNinputCallback,  input_cb,  (XtPointer)&s);

    XtRealizeWidget(s.toplevel);

    /* X setup */
    s.dpy = XtDisplay(s.toplevel);
    s.visual = DefaultVisual(s.dpy, DefaultScreen(s.dpy));
    s.depth  = DefaultDepth(s.dpy, DefaultScreen(s.dpy));

    /* initial canvas size */
    XtVaGetValues(s.canvas, XmNwidth, &cw, XmNheight, &ch, NULL);
    s.width = (int)cw;
    s.height = (int)ch;

    s.fb = (uint32_t *)calloc((size_t)s.width * (size_t)s.height, sizeof(uint32_t));
    if (!s.fb) {
        fprintf(stderr, "Out of memory allocating framebuffer\n");
        return 1;
    }

    /* init view */
    s.cx = -0.5;
    s.cy = 0.0;
    s.scale = 3.0 / (double)s.width;
    s.max_iter = 256;

    /* init job lock */
    pthread_mutex_init(&s.job.lock, NULL);

    recreate_ximage(&s);

    /* start first render (2 threads) */
    start_render(&s);

    /* start UI progress timer */
    s.timer_id = XtAppAddTimeOut(s.app, 50, timer_tick, (XtPointer)&s);

    XtAppMainLoop(s.app);
    return 0;
}
