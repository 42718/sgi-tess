/*
 * mandel_c89.c — IRIX Motif Mandelbrot (C89 clean)
 *
 * Build:
 *   cc -o mandel mandel_c89.c -lXm -lXt -lX11 -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/DrawingA.h>

/* ---- Application state ---- */
typedef struct AppState {
    Widget toplevel;
    Widget canvas;

    Display *dpy;
    Visual  *visual;
    int      depth;

    int width;
    int height;

    GC gc;

    XImage   *ximg;
    uint32_t *fb;

    double cx;
    double cy;
    double scale;
    int max_iter;
} AppState;

/* ---- Utilities ---- */
static uint32_t rgb(unsigned r, unsigned g, unsigned b)
{
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

/* ---- Mandelbrot renderer ---- */
static void render_mandel(AppState *s)
{
    int x, y;
    int w = s->width;
    int h = s->height;

    for (y = 0; y < h; y++) {
        double im = s->cy + (y - h / 2.0) * s->scale;
        for (x = 0; x < w; x++) {
            double re = s->cx + (x - w / 2.0) * s->scale;
            double zr = 0.0, zi = 0.0;
            int i = 0;

            while (i < s->max_iter) {
                double zr2 = zr * zr - zi * zi + re;
                double zi2 = 2.0 * zr * zi + im;
                zr = zr2;
                zi = zi2;
                if (zr * zr + zi * zi > 4.0)
                    break;
                i++;
            }

            if (i == s->max_iter)
                s->fb[y * w + x] = rgb(0, 0, 0);
            else {
                unsigned v = (unsigned)(255.0 * i / s->max_iter);
                s->fb[y * w + x] = rgb(v, v, v);
            }
        }
    }
}

/* ---- XImage management ---- */
static void recreate_ximage(AppState *s)
{
    if (s->ximg) {
        s->ximg->data = NULL;
        XDestroyImage(s->ximg);
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

    if (!XtIsRealized(s->canvas))
        return;

    win = XtWindow(s->canvas);
    if (!win)
        return;

    if (!s->gc)
        s->gc = XCreateGC(s->dpy, win, 0, NULL);

    XPutImage(s->dpy, win, s->gc, s->ximg,
              0, 0, 0, 0, s->width, s->height);
}

/* ---- Callbacks ---- */
static void expose_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s = (AppState *)cd;
    draw(s);
}

static void resize_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s = (AppState *)cd;
    Dimension nw, nh;

    XtVaGetValues(w, XmNwidth, &nw, XmNheight, &nh, NULL);

    if ((int)nw == s->width && (int)nh == s->height)
        return;

    s->width  = (int)nw;
    s->height = (int)nh;

    s->fb = (uint32_t *)realloc(
        s->fb,
        s->width * s->height * sizeof(uint32_t)
    );

    recreate_ximage(s);
    render_mandel(s);
    draw(s);
}

static void zoom(AppState *s, int px, int py, int in)
{
    double re = s->cx + (px - s->width  / 2.0) * s->scale;
    double im = s->cy + (py - s->height / 2.0) * s->scale;

    s->cx = re;
    s->cy = im;

    if (in) {
        s->scale *= 0.5;
        s->max_iter += 32;
    } else {
        s->scale *= 2.0;
        if (s->max_iter > 64)
            s->max_iter -= 32;
    }

    render_mandel(s);
    draw(s);
}

static void input_cb(Widget w, XtPointer cd, XtPointer cb)
{
    AppState *s = (AppState *)cd;
    XmDrawingAreaCallbackStruct *cbs =
        (XmDrawingAreaCallbackStruct *)cb;

    if (!cbs || !cbs->event)
        return;

    if (cbs->event->type == ButtonPress) {
        XButtonEvent *e = (XButtonEvent *)cbs->event;
        if (e->button == Button1)
            zoom(s, e->x, e->y, 1);
        else if (e->button == Button3)
            zoom(s, e->x, e->y, 0);
    }
}

/* ---- main ---- */
int main(int argc, char **argv)
{
    AppState s;
    XtAppContext app;
    Dimension w, h;

    memset(&s, 0, sizeof(s));

    s.toplevel = XtVaAppInitialize(
        &app,
        "MandelC89",
        NULL, 0,
        &argc, argv,
        NULL,
        NULL
    );

    s.canvas = XtVaCreateManagedWidget(
        "canvas",
        xmDrawingAreaWidgetClass,
        s.toplevel,
        XmNwidth, 800,
        XmNheight, 600,
        XmNresizePolicy, XmRESIZE_ANY,
        NULL
    );

    XtAddCallback(s.canvas, XmNexposeCallback, expose_cb, &s);
    XtAddCallback(s.canvas, XmNresizeCallback, resize_cb, &s);
    XtAddCallback(s.canvas, XmNinputCallback,  input_cb,  &s);

    XtRealizeWidget(s.toplevel);

    s.dpy    = XtDisplay(s.toplevel);
    s.visual = DefaultVisual(s.dpy, DefaultScreen(s.dpy));
    s.depth  = DefaultDepth(s.dpy, DefaultScreen(s.dpy));

    XtVaGetValues(s.canvas, XmNwidth, &w, XmNheight, &h, NULL);
    s.width  = (int)w;
    s.height = (int)h;

    s.fb = (uint32_t *)calloc(
        s.width * s.height,
        sizeof(uint32_t)
    );

    s.cx = -0.5;
    s.cy =  0.0;
    s.scale = 3.0 / s.width;
    s.max_iter = 256;

    recreate_ximage(&s);
    render_mandel(&s);
    draw(&s);

    XtAppMainLoop(app);
    return 0;
}
