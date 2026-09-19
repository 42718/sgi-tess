/*
 * tess_ui.c - the display half of Tess. Motif, no MPI, ever.
 *
 * DESIGN.md section 2: the GUI is a separate process from the MPI job, because
 * IRIX MPT has no multi-host MPI_Comm_spawn and a GUI that cannot deadlock on
 * MPI is worth more than one that shares its address space. It speaks the
 * big-endian frame protocol to the master over a socket, receives tiles as
 * iteration counts, and shades them locally - which is why changing the palette
 * costs nothing on the network.
 *
 *   tess-node -listen        (under mpirun, on the cluster)
 *   tess-ui -host lucy       (here, on the display)
 *
 * Two top-level shells on one app context, as design/ui-design.html has it:
 * the render window, and a control window whose panes are GENERATED from the
 * descriptor tables below rather than hand-built (DESIGN.md section 5). The
 * `where` column in those tables is the architecture in one field: TESS_LOCAL
 * parameters re-shade pixels already in hand and send nothing, everything else
 * costs an epoch.
 *
 * C89 throughout. X and Motif idioms follow baseline/mandel1-motif-single.c,
 * which is known to build with MIPSpro and IRIX IM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
/* after Xm.h, which pulls in Intrinsic.h: Shell.h needs externalref */
#include <X11/Shell.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Text.h>

#include "tess_params.h"

#define TESS_CTRL_W 420      /* control window width */
#define TESS_GAP    8
#define TESS_DECOR  14       /* window manager border and frame, per window */
#define TESS_USE    0.80     /* share of the screen width both windows take */

#include "tess_types.h"
#include "tess_proto.h"
#include "tess_wire.h"
#include "tess_mandel.h"

/*
 * Everything the panes edit. The descriptor tables below point into this by
 * offset, which is what lets one generic builder produce both panes.
 */
typedef struct UiValues {
    double cx;
    double cy;
    double scale;
    int    max_iter;
    int    auto_iter;
    int    ramp;
    int    cycles;
    int    rotate;
    int    interior;
} UiValues;

typedef struct Ui {
    Widget     toplevel;
    Widget     control;        /* second shell, same app context */
    Widget     canvas;
    Widget     status;
    Widget     log;
    Widget     cluster;
    Widget     clusterframe;
    Widget     elapsed;
    TessParamPane *view_pane;
    TessParamPane *colour_pane;
    UiValues   val;
    XtAppContext app;
    XtIntervalId tick;
    double     epoch_t0;
    int        tiles_expected;

    GC         bandgc;          /* XOR, for the rubber band */
    int        screen_w, screen_h;
    int        origin_x;
    int        dragging;
    int        drag_x0, drag_y0;
    int        drag_x1, drag_y1;
    Display   *dpy;
    Visual    *visual;
    int        depth;
    GC         gc;
    XImage    *ximg;

    int        width;
    int        height;

    tess_u32  *fb;            /* what X blits: 32-bit RGB */
    tess_u8   *iter;          /* what the cluster sent: 3 bytes per pixel */

    int        fd;            /* master socket */
    XtInputId  input_id;

    TessJob    job;
    TessPalette pal;
    int        tiles_in;
    int        workers;
    int        ranks;
    double     last_msec;
    char       host[64];
    int        port;

    /* per-rank accounting, straight out of the tile headers */
    int        rank_tiles[64];
    double     rank_usec[64];
    long       bytes_in;
} Ui;

/*
 * The tables. `where` is the interesting column: TESS_LOCAL parameters
 * re-shade pixels already in hand and send nothing, which is why the colour
 * pane has no Apply button and why a palette change is instant on a cluster
 * that may be minutes into a render.
 */
static const TessParamDesc view_params[] = {
    { "centre re",  TESS_P_DOUBLE, XtOffsetOf(UiValues, cx),       0.0, 0.0,
      TESS_CLUSTER, { 0 } },
    { "centre im",  TESS_P_DOUBLE, XtOffsetOf(UiValues, cy),       0.0, 0.0,
      TESS_CLUSTER, { 0 } },
    { "scale",      TESS_P_DOUBLE, XtOffsetOf(UiValues, scale),    1e-15, 1.0,
      TESS_CLUSTER, { 0 } },
    { "iterations", TESS_P_INT,    XtOffsetOf(UiValues, max_iter), 16.0, 65535.0,
      TESS_CLUSTER, { 0 } },
    { "auto iters", TESS_P_BOOL,   XtOffsetOf(UiValues, auto_iter), 0.0, 1.0,
      TESS_CLUSTER, { 0 } }
};

static const TessParamDesc colour_params[] = {
    { "ramp",     TESS_P_ENUM, XtOffsetOf(UiValues, ramp),     0.0, 1.0,
      TESS_LOCAL, { "blue-gold", "grey", 0 } },
    { "cycles",   TESS_P_INT,  XtOffsetOf(UiValues, cycles),   1.0, 32.0,
      TESS_LOCAL, { 0 } },
    { "rotate",   TESS_P_INT,  XtOffsetOf(UiValues, rotate),   0.0, 255.0,
      TESS_LOCAL, { 0 } },
    { "interior", TESS_P_ENUM, XtOffsetOf(UiValues, interior), 0.0, 1.0,
      TESS_LOCAL, { "black", "white", 0 } }
};

/* ------------------------------------------------------------- plumbing */

static void die(const char *msg)
{
    fprintf(stderr, "tess-ui: %s\n", msg);
    exit(1);
}

static void ui_log(Ui *u, const char *text)
{
    XmTextPosition last;

    if (!u->log) {
        return;
    }
    last = XmTextGetLastPosition(u->log);
    XmTextInsert(u->log, last, (char *)text);
    XmTextInsert(u->log, XmTextGetLastPosition(u->log), "\n");
    XmTextShowPosition(u->log, XmTextGetLastPosition(u->log));
}

static void set_elapsed(Ui *u, const char *text)
{
    XmString s;

    if (!u->elapsed) {
        return;
    }
    s = XmStringCreateLocalized((char *)text);
    XtVaSetValues(u->elapsed, XmNlabelString, s, NULL);
    XmStringFree(s);
}

/*
 * Cluster status, entirely from data already arriving: every tile header
 * carries the rank that computed it and how long it took, so who is pulling
 * their weight needs no extra protocol and no second connection.
 */
static void cluster_update(Ui *u)
{
    char buf[1024];
    char line[128];
    int r, shown;

    if (!u->cluster) {
        return;
    }
    sprintf(buf, "master   %s:%d\nranks    %d total, %d worker(s)\n",
            u->host, u->port, u->ranks, u->workers);
    sprintf(line, "epoch    %u, %d/%d tiles, %.0f KB in\n",
            u->job.epoch, u->tiles_in, u->tiles_expected,
            (double)u->bytes_in / 1024.0);
    strcat(buf, line);
    if (u->last_msec > 0.0) {
        sprintf(line, "last     %.2f s\n", u->last_msec / 1000.0);
        strcat(buf, line);
    }
    strcat(buf, "\nrank  tiles   avg ms   share\n");

    shown = 0;
    for (r = 1; r < 64 && shown < 12; r++) {
        if (u->rank_tiles[r] > 0) {
            double avg = u->rank_usec[r] / (double)u->rank_tiles[r] / 1000.0;
            double share = u->tiles_in > 0 ?
                           100.0 * (double)u->rank_tiles[r] /
                           (double)u->tiles_in : 0.0;

            sprintf(line, "%4d  %5d  %7.1f  %4.0f%%\n", r, u->rank_tiles[r],
                    avg, share);
            strcat(buf, line);
            shown++;
        }
    }
    if (!shown) {
        strcat(buf, "  (no tiles yet)\n");
    }
    XmTextSetString(u->cluster, buf);
}

static void set_status(Ui *u, const char *text)
{
    XmString s;

    s = XmStringCreateLocalized((char *)text);
    XtVaSetValues(u->status, XmNlabelString, s, NULL);
    XmStringFree(s);
}

static int connect_master(const char *host, int port)
{
    struct sockaddr_in sa;
    struct hostent *he;
    int fd;

    he = gethostbyname(host);
    if (!he) {
        return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    memset((char *)&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    memcpy((char *)&sa.sin_addr, (char *)he->h_addr, (size_t)he->h_length);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* --------------------------------------------------------------- pixels */

static void recreate_ximage(Ui *u)
{
    if (u->ximg) {
        u->ximg->data = (char *)0;
        XDestroyImage(u->ximg);
        u->ximg = (XImage *)0;
    }
    u->ximg = XCreateImage(u->dpy, u->visual, (unsigned)u->depth, ZPixmap, 0,
                           (char *)u->fb, (unsigned)u->width,
                           (unsigned)u->height, 32, 0);
    if (!u->ximg) {
        die("XCreateImage failed");
    }
}

static void blit(Ui *u, int x, int y, int w, int h)
{
    Window win;

    if (!XtIsRealized(u->canvas)) {
        return;
    }
    win = XtWindow(u->canvas);
    if (!win) {
        return;
    }
    if (!u->gc) {
        u->gc = XCreateGC(u->dpy, win, 0, (XGCValues *)0);
    }
    XPutImage(u->dpy, win, u->gc, u->ximg, x, y, x, y,
              (unsigned)w, (unsigned)h);
}

static void reshade_all(Ui *u);

/* Shade one tile out of the iteration buffer straight into the framebuffer. */
static void shade_tile(Ui *u, int x, int y, int w, int h)
{
    int row;
    tess_u8 *src;
    tess_u32 *dst;

    for (row = 0; row < h; row++) {
        src = u->iter + ((size_t)(y + row) * (size_t)u->width + (size_t)x) *
              TESS_BYTES_PER_PX;
        dst = u->fb + (size_t)(y + row) * (size_t)u->width + (size_t)x;
        tess_shade(src, w, (int)u->job.max_iter, &u->pal, dst);
    }
}

/* ----------------------------------------------------------- the job */

static void reshade_all(Ui *u)
{
    shade_tile(u, 0, 0, u->width, u->height);
    blit(u, 0, 0, u->width, u->height);
}

/*
 * Iterations that follow the depth.
 *
 * A fixed budget goes soft as you descend: past about 2^18 the boundary needs
 * more work than the opening view to stay sharp. The rule is linear in depth,
 * 128 more iterations per halving of scale on top of a 500 floor, which tracks
 * the escape-time cost closely enough and is predictable, unlike the usual
 * exponential fits. Off by default so a manual budget stays manual.
 */
static void auto_iters(Ui *u)
{
    double home, depth;
    int it;

    if (!u->val.auto_iter) {
        return;
    }
    home = 3.2 / (double)(u->width > 0 ? u->width : 1024);
    if (u->job.scale <= 0.0) {
        return;
    }
    depth = log(home / u->job.scale) / log(2.0);
    if (depth < 0.0) {
        depth = 0.0;
    }
    it = (int)(500.0 + 128.0 * depth);
    if (it < 64) {
        it = 64;
    }
    if (it > 65535) {
        it = 65535;
    }
    u->job.max_iter = (tess_u32)it;
}

/* The one place the edited values become a job. */
static void values_to_job(Ui *u)
{
    u->job.cx = u->val.cx;
    u->job.cy = u->val.cy;
    u->job.scale = u->val.scale;
    u->job.max_iter = (tess_u32)u->val.max_iter;
    u->pal.ramp = u->val.ramp;
    u->pal.cycles = u->val.cycles;
    u->pal.rotate = u->val.rotate;
    u->pal.interior = u->val.interior;
}

static void job_to_values(Ui *u)
{
    u->val.cx = u->job.cx;
    u->val.cy = u->job.cy;
    u->val.scale = u->job.scale;
    u->val.max_iter = (int)u->job.max_iter;
}

static void send_render(Ui *u)
{
    tess_u8 body[TESS_JOB_WIRE];
    int n;
    char msg[160];

    auto_iters(u);
    job_to_values(u);
    if (u->view_pane) {
        tess_params_refresh(u->view_pane);
    }

    u->job.epoch++;
    u->job.width = (tess_u32)u->width;
    memset((char *)u->rank_tiles, 0, sizeof u->rank_tiles);
    memset((char *)u->rank_usec, 0, sizeof u->rank_usec);
    u->bytes_in = 0;
    u->job.height = (tess_u32)u->height;
    u->tiles_in = 0;

    n = tess_put_job(body, &u->job);
    if (tess_frame_write(u->fd, TESS_MSG_RENDER, body, n) != 0) {
        set_status(u, "master connection lost");
        return;
    }
    sprintf(msg, "epoch %u  %dx%d  max %u  scale %.3e  rendering...",
            u->job.epoch, u->width, u->height, u->job.max_iter, u->job.scale);
    set_status(u, msg);

    /* Tiles the master will produce, computed here rather than asked for: the
       GUI knows the geometry, so the progress line needs no protocol support. */
    {
        int tw = (int)(u->job.tile ? u->job.tile : TESS_TILE);

        u->tiles_expected = ((u->width + tw - 1) / tw) *
                            ((u->height + tw - 1) / tw);
    }
    u->epoch_t0 = 0.0;
}

/*
 * A parameter changed. This is the whole architecture in one branch: shading
 * parameters re-colour what is already here, everything else costs a round
 * trip to the cluster.
 */
static void param_applied(void *ctx, const TessParamDesc *d)
{
    Ui *u = (Ui *)ctx;
    char msg[160];

    values_to_job(u);
    if (d->where == TESS_LOCAL) {
        reshade_all(u);
        sprintf(msg, "%s changed: re-shaded locally, no cluster traffic",
                d->label);
        ui_log(u, msg);
    } else if (u->fd >= 0) {
        sprintf(msg, "%s changed: new epoch", d->label);
        ui_log(u, msg);
        send_render(u);
    }
}

static void handle_tile(Ui *u, const tess_u8 *body, int len)
{
    TessTileHdr th;
    int n, row, bytes;
    const tess_u8 *px;

    n = tess_get_tilehdr(body, &th);
    px = body + n;
    bytes = (int)(th.w * th.h * TESS_BYTES_PER_PX);
    if (n + bytes > len) {
        return;
    }
    if (th.epoch != u->job.epoch) {
        return;                       /* an abandoned epoch, drop it */
    }
    if (th.x + th.w > (tess_u32)u->width || th.y + th.h > (tess_u32)u->height) {
        return;                       /* stale geometry after a resize */
    }

    for (row = 0; row < (int)th.h; row++) {
        memcpy((char *)(u->iter + ((size_t)(th.y + row) * (size_t)u->width +
                                   (size_t)th.x) * TESS_BYTES_PER_PX),
               (const char *)(px + (size_t)row * th.w * TESS_BYTES_PER_PX),
               (size_t)th.w * TESS_BYTES_PER_PX);
    }
    shade_tile(u, (int)th.x, (int)th.y, (int)th.w, (int)th.h);
    blit(u, (int)th.x, (int)th.y, (int)th.w, (int)th.h);
    if (u->tiles_in == 0) {
        struct timeval tv;

        gettimeofday(&tv, (struct timezone *)0);
        u->epoch_t0 = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
    }
    u->tiles_in++;
    if (th.rank < 64) {
        u->rank_tiles[th.rank]++;
        u->rank_usec[th.rank] += (double)th.usec;
    }
    u->bytes_in += (long)bytes;
}

/* Xt calls this whenever the master has something to say, which is how the
   GUI stays responsive without threads: no blocking read in the main loop. */
static void socket_cb(XtPointer cd, int *src, XtInputId *id)
{
    Ui *u = (Ui *)cd;
    tess_u8 buf[TESS_MAX_FRAME];
    char msg[160];
    int type, len;

    if (tess_frame_read(u->fd, &type, buf, &len) != 0) {
        set_status(u, "master closed the connection");
        XtRemoveInput(u->input_id);
        close(u->fd);
        u->fd = -1;
        return;
    }

    if (type == TESS_MSG_TILE) {
        handle_tile(u, buf, len);
    } else if (type == TESS_MSG_WELCOME) {
        u->ranks = (int)tess_get_u32(buf + 4);
        u->workers = (int)tess_get_u32(buf + 8);
        cluster_update(u);
        sprintf(msg, "connected: %d worker(s)", u->workers);
        set_status(u, msg);
        send_render(u);
    } else if (type == TESS_MSG_DONE) {
        tess_u32 tiles = tess_get_u32(buf + 4);
        tess_u32 msec = tess_get_u32(buf + 8);

        u->last_msec = (double)msec;
        sprintf(msg, "epoch %u  %u tiles  %.2f s  %d worker(s)  scale %.3e",
                tess_get_u32(buf), tiles, (double)msec / 1000.0, u->workers,
                u->job.scale);
        set_status(u, msg);
        ui_log(u, msg);
        set_elapsed(u, "idle");
        u->epoch_t0 = 0.0;
        cluster_update(u);
    }
}

static void tick_cb(XtPointer cd, XtIntervalId *id)
{
    Ui *u = (Ui *)cd;
    char msg[160];
    double dt;
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *)0);
    dt = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6 - u->epoch_t0;

    if (u->epoch_t0 > 0.0 && u->tiles_in < u->tiles_expected) {
        double eta = 0.0;

        if (u->tiles_in > 0) {
            eta = dt * (double)(u->tiles_expected - u->tiles_in) /
                  (double)u->tiles_in;
        }
        sprintf(msg, "%d/%d tiles   %.1f s elapsed   %.1f s left",
                u->tiles_in, u->tiles_expected, dt, eta);
        set_elapsed(u, msg);
        cluster_update(u);
    }
    u->tick = XtAppAddTimeOut(u->app, 200, tick_cb, (XtPointer)u);
}

/* ------------------------------------------------------------ callbacks */

static void expose_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Ui *u = (Ui *)cd;

    blit(u, 0, 0, u->width, u->height);
}

static void resize_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Ui *u = (Ui *)cd;
    Dimension nw, nh;

    XtVaGetValues(w, XmNwidth, &nw, XmNheight, &nh, NULL);
    if ((int)nw == u->width && (int)nh == u->height) {
        return;
    }
    u->width = (int)nw;
    u->height = (int)nh;

    free((void *)u->fb);
    free((void *)u->iter);
    u->fb = (tess_u32 *)calloc((size_t)u->width * u->height,
                               sizeof(tess_u32));
    u->iter = (tess_u8 *)calloc((size_t)u->width * u->height *
                                TESS_BYTES_PER_PX, 1);
    if (!u->fb || !u->iter) {
        die("out of memory on resize");
    }
    recreate_ximage(u);
    if (u->fd >= 0) {
        send_render(u);
    }
}

static void zoom_at(Ui *u, int px, int py, int in)
{
    double re, im;

    re = u->job.cx + ((double)px - (double)u->width * 0.5) * u->job.scale;
    im = u->job.cy + ((double)py - (double)u->height * 0.5) * u->job.scale;
    u->job.cx = re;
    u->job.cy = im;
    if (in) {
        u->job.scale *= 0.5;
        if (!u->val.auto_iter) {
            u->job.max_iter += 64;
        }
    } else {
        u->job.scale *= 2.0;
        if (!u->val.auto_iter && u->job.max_iter > 128) {
            u->job.max_iter -= 64;
        }
    }
    job_to_values(u);
    if (u->view_pane) {
        tess_params_refresh(u->view_pane);
    }
    send_render(u);
}

/*
 * Zoom to a dragged box. The box sets both the centre and the scale, and the
 * larger of the two ratios is used so the whole selection fits rather than
 * being cropped by the window's aspect. Iterations follow the depth: every
 * halving of the scale adds 64, which is the same rule click-zoom uses.
 */
static void zoom_box(Ui *u, int x0, int y0, int x1, int y1)
{
    double bw, bh, rx, ry, ratio, cxp, cyp;
    int t;

    if (x1 < x0) { t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { t = y0; y0 = y1; y1 = t; }
    bw = (double)(x1 - x0);
    bh = (double)(y1 - y0);
    if (bw < 4.0 || bh < 4.0) {
        zoom_at(u, x0, y0, 1);          /* a click, not a drag */
        return;
    }

    cxp = (double)(x0 + x1) * 0.5;
    cyp = (double)(y0 + y1) * 0.5;
    u->job.cx += (cxp - (double)u->width * 0.5) * u->job.scale;
    u->job.cy += (cyp - (double)u->height * 0.5) * u->job.scale;

    rx = bw / (double)u->width;
    ry = bh / (double)u->height;
    ratio = rx > ry ? rx : ry;
    if (ratio < 1e-6) {
        ratio = 1e-6;
    }
    u->job.scale *= ratio;

    if (!u->val.auto_iter) {
        double depth = log(1.0 / ratio) / log(2.0);
        int add = (int)(depth * 64.0);

        if (add > 0) {
            u->job.max_iter += (tess_u32)add;
        }
    }

    job_to_values(u);
    if (u->view_pane) {
        tess_params_refresh(u->view_pane);
    }
    send_render(u);
}

static void band_draw(Ui *u)
{
    int x, y, w, h;

    if (!u->bandgc || !XtIsRealized(u->canvas)) {
        return;
    }
    x = u->drag_x0 < u->drag_x1 ? u->drag_x0 : u->drag_x1;
    y = u->drag_y0 < u->drag_y1 ? u->drag_y0 : u->drag_y1;
    w = u->drag_x1 > u->drag_x0 ? u->drag_x1 - u->drag_x0 : u->drag_x0 - u->drag_x1;
    h = u->drag_y1 > u->drag_y0 ? u->drag_y1 - u->drag_y0 : u->drag_y0 - u->drag_y1;
    if (w > 0 && h > 0) {
        XDrawRectangle(u->dpy, XtWindow(u->canvas), u->bandgc, x, y,
                       (unsigned)w, (unsigned)h);
    }
}

/*
 * All mouse handling in one place. The DrawingArea's XmNinputCallback does not
 * report motion, and a rubber band needs it, so this is an event handler
 * rather than a Motif callback.
 */
static void mouse_eh(Widget w, XtPointer cd, XEvent *ev, Boolean *cont)
{
    Ui *u = (Ui *)cd;
    XButtonEvent *be;
    XMotionEvent *me;

    if (u->fd < 0) {
        return;
    }

    if (ev->type == ButtonPress) {
        be = (XButtonEvent *)ev;
        if (be->button == Button1) {
            u->dragging = 1;
            u->drag_x0 = u->drag_x1 = be->x;
            u->drag_y0 = u->drag_y1 = be->y;
        } else if (be->button == Button3) {
            zoom_at(u, be->x, be->y, 0);
        } else if (be->button == Button2) {
            u->val.ramp = !u->val.ramp;
            values_to_job(u);
            if (u->colour_pane) {
                tess_params_refresh(u->colour_pane);
            }
            reshade_all(u);
            ui_log(u, "ramp toggled: re-shaded locally, no cluster traffic");
        }
    } else if (ev->type == MotionNotify && u->dragging) {
        me = (XMotionEvent *)ev;
        band_draw(u);                   /* XOR: erases the previous box */
        u->drag_x1 = me->x;
        u->drag_y1 = me->y;
        band_draw(u);
    } else if (ev->type == ButtonRelease && u->dragging) {
        be = (XButtonEvent *)ev;
        if (be->button != Button1) {
            return;
        }
        band_draw(u);                   /* erase */
        u->dragging = 0;
        u->drag_x1 = be->x;
        u->drag_y1 = be->y;
        zoom_box(u, u->drag_x0, u->drag_y0, u->drag_x1, u->drag_y1);
    }
}

static void render_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Ui *u = (Ui *)cd;

    values_to_job(u);
    if (u->fd >= 0) {
        send_render(u);
    }
}

static void home_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Ui *u = (Ui *)cd;

    u->val.cx = -0.6;
    u->val.cy = 0.0;
    u->val.scale = 3.2 / (double)(u->width > 0 ? u->width : 1024);
    u->val.max_iter = 1000;
    values_to_job(u);
    tess_params_refresh(u->view_pane);
    ui_log(u, "home");
    if (u->fd >= 0) {
        send_render(u);
    }
}

/* The second top-level shell: same app context, no MPI, no blocking. */
static void build_control(Ui *u)
{
    Widget form, buttons, b;

    u->control = XtVaAppCreateShell("control", "Tess",
                                    topLevelShellWidgetClass,
                                    XtDisplay(u->toplevel),
                                    XmNtitle, "Tess control",
                                    XmNx, u->origin_x + u->width +
                                          TESS_GAP + 2 * TESS_DECOR,
                                    XmNy, TESS_GAP,
                                    XmNwidth, TESS_CTRL_W,
                                    NULL);
    form = XtVaCreateManagedWidget("cform", xmFormWidgetClass, u->control,
                                   NULL);

    u->view_pane = tess_params_build(form, "View",
                                     view_params,
                                     (int)(sizeof view_params /
                                           sizeof view_params[0]),
                                     (void *)&u->val, param_applied,
                                     (void *)u);
    XtVaSetValues(XtParent(u->view_pane->form),
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);

    u->colour_pane = tess_params_build(form, "Colour",
                                       colour_params,
                                       (int)(sizeof colour_params /
                                             sizeof colour_params[0]),
                                       (void *)&u->val, param_applied,
                                       (void *)u);
    XtVaSetValues(XtParent(u->colour_pane->form),
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, XtParent(u->view_pane->form),
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);

    buttons = XtVaCreateManagedWidget("buttons", xmRowColumnWidgetClass, form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget,
                                      XtParent(u->colour_pane->form),
                                      XmNleftAttachment, XmATTACH_FORM,
                                      NULL);
    b = XtVaCreateManagedWidget("Render", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, render_cb, (XtPointer)u);
    b = XtVaCreateManagedWidget("Home", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, home_cb, (XtPointer)u);

    u->elapsed = XtVaCreateManagedWidget("idle", xmLabelWidgetClass, form,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, buttons,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         NULL);

    {
        Widget cframe;

        cframe = XtVaCreateManagedWidget("cframe", xmFrameWidgetClass, form,
                                         XmNshadowType, XmSHADOW_ETCHED_IN,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, u->elapsed,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         NULL);
        XtVaCreateManagedWidget("Cluster", xmLabelWidgetClass, cframe,
                                XmNchildType, XmFRAME_TITLE_CHILD,
                                NULL);
        u->cluster = XmCreateText(cframe, "cluster", (ArgList)0, 0);
        XtVaSetValues(u->cluster,
                      XmNeditable, False,
                      XmNeditMode, XmMULTI_LINE_EDIT,
                      XmNcursorPositionVisible, False,
                      XmNrows, 11,
                      XmNcolumns, 40,
                      NULL);
        XtManageChild(u->cluster);
        u->clusterframe = cframe;
    }

    u->log = XmCreateScrolledText(form, "log", (ArgList)0, 0);
    XtVaSetValues(u->log,
                  XmNeditable, False,
                  XmNeditMode, XmMULTI_LINE_EDIT,
                  XmNrows, 10,
                  XmNcolumns, 52,
                  NULL);
    XtVaSetValues(XtParent(u->log),
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, u->clusterframe,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  NULL);
    XtManageChild(u->log);

    XtRealizeWidget(u->control);
}

/* ----------------------------------------------------------------- main */

/*
 * Fallback resources. Motif's defaults are built for 1989 screens: big fonts,
 * 2-pixel shadows, generous margins everywhere. These tighten it and switch on
 * the IRIX Interactive Desktop look that PLATFORM-FACTS.md recommends. A real
 * app-defaults file under /usr/lib/X11/app-defaults/Tess overrides all of it,
 * which is the point of the resource class being the framework, not the module.
 */
static String fallbacks[] = {
    "*sgiMode: True",
    "*useSchemes: none",
    "*fontList: -*-helvetica-medium-r-normal--10-*-*-*-*-*-iso8859-1",
    "*XmTextField.fontList: -*-helvetica-medium-r-normal--10-*-*-*-*-*-iso8859-1",
    "*log.fontList: -*-screen-medium-r-normal--10-*-*-*-*-*-iso8859-1",
    "*cluster.fontList: -*-screen-medium-r-normal--10-*-*-*-*-*-iso8859-1",
    "*shadowThickness: 1",
    "*highlightThickness: 1",
    "*XmRowColumn.marginHeight: 1",
    "*XmRowColumn.marginWidth: 1",
    "*XmRowColumn.spacing: 2",
    "*XmFrame.marginHeight: 2",
    "*XmFrame.marginWidth: 2",
    "*XmLabel.marginHeight: 1",
    "*XmLabel.marginWidth: 2",
    "*XmTextField.marginHeight: 1",
    "*XmPushButton.marginHeight: 2",
    "*XmToggleButton.marginHeight: 0",
    "*status.marginHeight: 2",
    (String)0
};

int main(int argc, char **argv)
{
    XtAppContext app;
    Widget form;
    Ui u;
    char *host = "localhost";
    int port = TESS_DEFAULT_PORT;
    int width_given = 0;
    int height_given = 0;
    int i;
    tess_u8 hello[4];

    memset((char *)&u, 0, sizeof u);
    u.width = 1024;
    u.height = 768;
    u.fd = -1;
    u.job.epoch = 0;
    u.job.max_iter = 1000;
    u.job.tile = TESS_TILE;
    u.job.cx = -0.6;
    u.job.cy = 0.0;
    u.job.scale = 3.2 / 1024.0;
    tess_palette_default(&u.pal);
    u.val.ramp = u.pal.ramp;
    u.val.cycles = u.pal.cycles;
    u.val.rotate = u.pal.rotate;
    u.val.interior = u.pal.interior;

    if (tess_types_check() != 0) {
        die("integer widths are not what the wire format assumes");
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            u.width = atoi(argv[++i]);
            width_given = 1;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            u.height = atoi(argv[++i]);
            height_given = 1;
        }
    }

    u.toplevel = XtVaAppInitialize(&app, "Tess", (XrmOptionDescList)0, 0,
                                  &argc, argv, fallbacks, NULL);
    u.app = app;

    /*
     * Fit the screen, and put the two windows side by side rather than on top
     * of each other. The render window takes what is left after the control
     * panel, so on a 1280x1024 head that is 844 pixels of fractal with the
     * panel beside it instead of two overlapping shells to drag apart on every
     * launch. -w and -h still win, for reproducible frame sizes.
     */
    {
        Display *d = XtDisplay(u.toplevel);
        int scr = DefaultScreen(d);
        int sw = DisplayWidth(d, scr);
        int sh = DisplayHeight(d, scr);
        int total, avail_h;

        /*
         * Both windows together take TESS_USE of the width, flush to the right
         * edge. The render window is 4:3, which is the shape of the machines
         * this runs on, and the control panel sits to its right with room for
         * the window manager's frame between them: the previous attempt
         * overlapped because it placed the panel at render_w + gap, ignoring
         * the border the WM adds on both windows.
         */
        total = (int)((double)sw * TESS_USE);
        u.screen_w = sw;
        u.screen_h = sh;

        if (!width_given) {
            u.width = total - TESS_CTRL_W - TESS_GAP - 2 * TESS_DECOR;
            if (u.width < 320) {
                u.width = 320;
            }
        }
        if (!height_given) {
            u.height = u.width * 3 / 4;
            avail_h = sh - 2 * TESS_GAP - 2 * TESS_DECOR - 40;
            if (u.height > avail_h) {
                u.height = avail_h;
                if (!width_given) {
                    u.width = u.height * 4 / 3;
                }
            }
        }

        u.origin_x = sw - (u.width + TESS_CTRL_W + TESS_GAP + 2 * TESS_DECOR);
        if (u.origin_x < 0) {
            u.origin_x = 0;
        }
        XtVaSetValues(u.toplevel,
                      XmNx, u.origin_x,
                      XmNy, TESS_GAP,
                      NULL);
    }

    form = XtVaCreateManagedWidget("form", xmFormWidgetClass, u.toplevel,
                                   NULL);

    /* The varargs form, not XtSetArg: passing a Widget through an Arg array
       converts a pointer to XtArgVal, which MIPSpro warns about at -64 and
       which no cast can silence, since XtSetArg casts again inside the macro.
       Varargs pass the pointer as a pointer. */
    u.status = XtVaCreateManagedWidget("status", xmLabelWidgetClass, form,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       XmNrightAttachment, XmATTACH_FORM,
                                       XmNbottomAttachment, XmATTACH_FORM,
                                       NULL);

    u.canvas = XtVaCreateManagedWidget("canvas", xmDrawingAreaWidgetClass,
                                       form,
                                       XmNwidth, u.width,
                                       XmNheight, u.height,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       XmNrightAttachment, XmATTACH_FORM,
                                       XmNtopAttachment, XmATTACH_FORM,
                                       XmNbottomAttachment, XmATTACH_WIDGET,
                                       XmNbottomWidget, u.status,
                                       NULL);

    XtAddCallback(u.canvas, XmNexposeCallback, expose_cb, (XtPointer)&u);
    XtAddCallback(u.canvas, XmNresizeCallback, resize_cb, (XtPointer)&u);
    XtAddEventHandler(u.canvas,
                      (EventMask)(ButtonPressMask | ButtonReleaseMask |
                                  ButtonMotionMask),
                      False, mouse_eh, (XtPointer)&u);

    XtRealizeWidget(u.toplevel);

    job_to_values(&u);
    build_control(&u);
    u.tick = XtAppAddTimeOut(app, 200, tick_cb, (XtPointer)&u);

    u.dpy = XtDisplay(u.toplevel);
    u.visual = DefaultVisual(u.dpy, DefaultScreen(u.dpy));
    u.depth = DefaultDepth(u.dpy, DefaultScreen(u.dpy));

    u.fb = (tess_u32 *)calloc((size_t)u.width * u.height, sizeof(tess_u32));
    u.iter = (tess_u8 *)calloc((size_t)u.width * u.height * TESS_BYTES_PER_PX,
                               1);
    if (!u.fb || !u.iter) {
        die("out of memory");
    }
    recreate_ximage(&u);

    {
        XGCValues gcv;

        gcv.function = GXxor;
        gcv.foreground = WhitePixel(u.dpy, DefaultScreen(u.dpy)) ^
                         BlackPixel(u.dpy, DefaultScreen(u.dpy));
        gcv.line_width = 0;
        gcv.subwindow_mode = IncludeInferiors;
        u.bandgc = XCreateGC(u.dpy, XtWindow(u.canvas),
                             (unsigned long)(GCFunction | GCForeground |
                                             GCLineWidth | GCSubwindowMode),
                             &gcv);
    }

    strncpy(u.host, host, sizeof u.host - 1);
    u.host[sizeof u.host - 1] = '\0';
    u.port = port;
    u.fd = connect_master(host, port);
    if (u.fd < 0) {
        char msg[160];

        sprintf(msg, "no master at %s:%d - start tess-node -listen", host,
                port);
        set_status(&u, msg);
    } else {
        {
            char msg[160];

            sprintf(msg, "display %dx%d, render %dx%d", u.screen_w,
                    u.screen_h, u.width, u.height);
            ui_log(&u, msg);
        }
        u.input_id = XtAppAddInput(app, u.fd, (XtPointer)XtInputReadMask,
                                   socket_cb, (XtPointer)&u);
        memset((char *)hello, 0, sizeof hello);
        tess_put_u32(hello, (tess_u32)TESS_PROTO_VER);
        if (tess_frame_write(u.fd, TESS_MSG_HELLO, hello, 4) != 0) {
            set_status(&u, "could not greet the master");
        } else {
            set_status(&u, "connected, waiting for the cluster");
        }
    }

    XtAppMainLoop(app);
    return 0;
}
