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
#include <stdlib.h>
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
#include "tess_cluster.h"

#define TESS_CTRL_W 256      /* control window width */
#define TESS_GAP    8
#define TESS_DECOR  14       /* window manager border and frame, per window */
#define TESS_USE    0.90     /* share of the screen width both windows take */

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
    int    by_owner;
} UiValues;

typedef struct Ui {
    Widget     toplevel;
    Widget     control;        /* second shell, same app context */
    Widget     canvas;
    Widget     status;
    Widget     log;
    Widget     cluster;
    Widget     clusterframe;
    TessCluster *cl;
    Widget     elapsed;
    TessParamPane *view_pane;
    TessParamPane *colour_pane;
    UiValues   val;
    XtAppContext app;
    XtIntervalId tick;
    int        ticks;
    double     epoch_t0;
    int        tiles_expected;

    /* the visual's own channel layout, not an assumed 0xRRGGBB */
    tess_u32   rank_pix[64];   /* each rank's host hue, as 0xRRGGBB */
    int        rshift, gshift, bshift;
    int        rbits, gbits, bbits;

    GC         bandgc;
    GC         statsgc;
    XFontStruct *statsfont;
    Colormap   cmap;          /* XOR, for the rubber band */
    int        screen_w, screen_h;
    int        origin_x;
    int        ctrl_x;
    int        ctrl_y;
    int        border_x, border_y;
    int        ready;      /* the pixel machinery exists; resize may touch it */
    int        attach;
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
    tess_u8   *owner;         /* which rank drew each pixel, for tinting */

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
    char       pending_log[160];
    char       savedir[256];
    tess_u32  *shade_rgb;
    char       nonce[TESS_NONCE_LEN];
    char       tree[512];
    char       hostlist[256];

    /* per-rank accounting, straight out of the tile headers */
    int        rank_tiles[64];
    double     rank_usec[64];
    long       bytes_in;
    TessTransport tr;
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
      TESS_LOCAL, { "black", "white", 0 } },
    { "by owner", TESS_P_BOOL, XtOffsetOf(UiValues, by_owner), 0.0, 1.0,
      TESS_LOCAL, { 0 } }
};

/* ------------------------------------------------------------- plumbing */

/*
 * What this window manager does to a window: how much bigger the frame is than
 * the client, and where the client sits inside it.
 *
 * These cannot be known before a window is realized, and a window cannot be
 * moved after it is mapped without the frame jumping. So they are measured on
 * the first run and remembered: the layout is approximate once, then exact
 * every time after. Estimating them cost four rounds of overlapping windows.
 */
typedef struct WmMetrics {
    int decor_x, decor_y;      /* frame size minus client size */
    int border_x, border_y;    /* client position within the frame */
    int pos_is_client;         /* does XmNx set the client or the frame? */
} WmMetrics;

static void wm_metrics_path(char *out, int len)
{
    const char *home = getenv("HOME");

    sprintf(out, "%s/.tess-wm", home ? home : "/tmp");
    out[len - 1] = '\0';
}

static void wm_metrics_load(WmMetrics *m)
{
    char path[512];
    FILE *f;

    m->decor_x = 16;           /* 4Dwm's usual, as a first guess */
    m->decor_y = 40;
    m->border_x = 8;
    m->border_y = 30;
    m->pos_is_client = 0;      /* most window managers place the frame */

    wm_metrics_path(path, (int)sizeof path);
    f = fopen(path, "r");
    if (!f) {
        return;
    }
    fscanf(f, "%d %d %d %d %d", &m->decor_x, &m->decor_y, &m->border_x,
           &m->border_y, &m->pos_is_client);
    fclose(f);
}

static void wm_metrics_save(const WmMetrics *m)
{
    char path[512];
    FILE *f;

    wm_metrics_path(path, (int)sizeof path);
    f = fopen(path, "w");
    if (!f) {
        return;
    }
    fprintf(f, "%d %d %d %d %d\n", m->decor_x, m->decor_y, m->border_x,
            m->border_y, m->pos_is_client);
    fclose(f);
}

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
static void stats_draw(Ui *u);

static void cluster_update(Ui *u)
{
    if (!u->cluster) {
        return;
    }
    stats_draw(u);
}

/*
 * Per-rank contribution, drawn rather than tabulated.
 *
 * One row per rank, named for the machine and CPU it is on rather than by an
 * MPI number, with a bar in that machine's colour showing its share of the
 * frame. The hues are the ones the design assigns per machine, and they are
 * the same ones tile-ownership colouring will use, so the panel and the
 * picture will agree.
 */
static double now_secs(void)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *)0);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static unsigned long pix(Ui *u, const char *spec)
{
    XColor want, exact;

    if (!XAllocNamedColor(u->dpy, u->cmap, (char *)spec, &want, &exact)) {
        return BlackPixel(u->dpy, DefaultScreen(u->dpy));
    }
    return want.pixel;
}

static void stats_draw(Ui *u)
{
    Display *d = u->dpy;
    Window w;
    Dimension wid, hgt;
    char label[64];
    char num[64];
    int y, r, rows, barx, barw;
    double share;

    if (!XtIsRealized(u->cluster)) {
        return;
    }
    w = XtWindow(u->cluster);
    XtVaGetValues(u->cluster, XmNwidth, &wid, XmNheight, &hgt, NULL);

    if (!u->statsgc) {
        XGCValues gcv;

        gcv.foreground = BlackPixel(d, DefaultScreen(d));
        u->statsgc = XCreateGC(d, w, (unsigned long)GCForeground, &gcv);
        if (u->statsfont) {
            XSetFont(d, u->statsgc, u->statsfont->fid);
        }
    }

    XSetForeground(d, u->statsgc, pix(u, "#e4e7ea"));
    XFillRectangle(d, w, u->statsgc, 0, 0, (unsigned)wid, (unsigned)hgt);

    barx = 108;
    barw = (int)wid - barx - 76;
    if (barw < 20) {
        barw = 20;
    }

    y = 13;
    XSetForeground(d, u->statsgc, pix(u, "#7d8a93"));
    XDrawString(d, w, u->statsgc, 4, y, "node/cpu", 8);
    XDrawString(d, w, u->statsgc, barx, y, "share of frame", 14);
    y += 6;
    XDrawLine(d, w, u->statsgc, 4, y, (int)wid - 6, y);

    /*
     * Rank 0 first, always, even though it computes nothing. It is the master
     * on the display host and people look for it: leaving it out makes the
     * machine running the interface seem absent from its own cluster.
     */
    if (u->workers > 0 && y + 30 < (int)hgt) {
        y += 15;
        tess_cluster_rank_label(u->cl, 0, label, (int)sizeof label);
        XSetForeground(d, u->statsgc, pix(u, "#7d8a93"));
        XDrawString(d, w, u->statsgc, 4, y, label, (int)strlen(label));
        XDrawString(d, w, u->statsgc, barx, y, "master, schedules only", 22);
    }

    rows = 0;
    for (r = 1; r < 64 && rows < 7 && y + 30 < (int)hgt; r++) {
        if (u->rank_tiles[r] <= 0) {
            continue;
        }
        y += 15;
        rows++;
        share = u->tiles_in > 0 ?
                (double)u->rank_tiles[r] / (double)u->tiles_in : 0.0;

        tess_cluster_rank_label(u->cl, r, label, (int)sizeof label);
        XSetForeground(d, u->statsgc, pix(u, "#101519"));
        XDrawString(d, w, u->statsgc, 4, y, label, (int)strlen(label));

        XSetForeground(d, u->statsgc,
                       pix(u, tess_cluster_rank_colour(u->cl, r)));
        XFillRectangle(d, w, u->statsgc, barx, y - 8,
                       (unsigned)(int)(share * (double)barw), 9);
        XSetForeground(d, u->statsgc, pix(u, "#8f9298"));
        XDrawRectangle(d, w, u->statsgc, barx, y - 8, (unsigned)barw, 9);

        /*
         * How busy that CPU was: the rank's own compute time against the
         * frame's wall time. This is a real utilisation figure per rank, which
         * a host load average cannot give, and it shows stragglers directly.
         */
        {
            double wall = u->last_msec > 0.0 ? u->last_msec * 1000.0 :
                          (u->epoch_t0 > 0.0 ? (now_secs() - u->epoch_t0) * 1e6
                                             : 0.0);
            double busy = wall > 0.0 ? 100.0 * u->rank_usec[r] / wall : 0.0;

            if (busy > 100.0) {
                busy = 100.0;
            }
            sprintf(num, "%4d %3.0f%%", u->rank_tiles[r], busy);
        }
        XSetForeground(d, u->statsgc, pix(u, "#101519"));
        XDrawString(d, w, u->statsgc, (int)wid - 70, y, num,
                    (int)strlen(num));
    }

    if (!rows) {
        XSetForeground(d, u->statsgc, pix(u, "#7d8a93"));
        XDrawString(d, w, u->statsgc, 4, y + 18, "idle - no tiles yet", 19);
    }

    /*
     * Which interconnect actually carried it, measured rather than assumed.
     * This line exists because of the GM week: MPT falls back to TCP in
     * silence, and a job that quietly ran over ethernet looks exactly like one
     * that used Myrinet.
     */
    {
        char t[96];

        y += 17;
        if (y + 12 < (int)hgt) {
            XSetForeground(d, u->statsgc, pix(u, "#7d8a93"));
            if (!u->tr.known) {
                strcpy(t, "transport: MPT will not say");
            } else if (u->tr.kb_gm > u->tr.kb_tcp) {
                sprintf(t, "transport: GM  %lu KB", (unsigned long)u->tr.kb_gm);
            } else if (u->tr.kb_tcp > 0) {
                sprintf(t, "transport: TCP  %lu KB",
                        (unsigned long)u->tr.kb_tcp);
            } else if (u->tr.kb_hippi > 0) {
                sprintf(t, "transport: HIPPI  %lu KB",
                        (unsigned long)u->tr.kb_hippi);
            } else if (u->tr.kb_shmem > 0) {
                sprintf(t, "transport: shared memory  %lu KB",
                        (unsigned long)u->tr.kb_shmem);
            } else {
                strcpy(t, "transport: nothing measured yet");
            }
            XDrawString(d, w, u->statsgc, 4, y, t, (int)strlen(t));
        }
    }

    /* A coloured state line: green while the frame is arriving, grey when
       there is nothing to do. Visible from across the room, which is the
       point of a status light. */
    y = (int)hgt - 8;
    XSetForeground(d, u->statsgc,
                   pix(u, u->epoch_t0 > 0.0 ? "#5f9e4a" : "#a8b4bc"));
    XFillRectangle(d, w, u->statsgc, 4, y - 9, 10, 10);
    XSetForeground(d, u->statsgc, pix(u, "#101519"));
    if (u->epoch_t0 > 0.0) {
        sprintf(num, "rendering  %d/%d tiles  %d worker(s)", u->tiles_in,
                u->tiles_expected, u->workers);
    } else if (u->workers > 0) {
        sprintf(num, "idle  %d worker(s)  last %.2f s", u->workers,
                u->last_msec / 1000.0);
    } else {
        strcpy(num, "no cluster");
    }
    XDrawString(d, w, u->statsgc, 20, y, num, (int)strlen(num));
}

static void stats_expose_cb(Widget w, XtPointer cd, XtPointer cb)
{
    stats_draw((Ui *)cd);
}

static void set_status(Ui *u, const char *text)
{
    XmString s;

    s = XmStringCreateLocalized((char *)text);
    XtVaSetValues(u->status, XmNlabelString, s, NULL);
    XmStringFree(s);
}

static void ui_log_cb(void *ctx, const char *msg);
static int attach_master(Ui *u, int port, const char *nonce);

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

static void mask_decode(unsigned long mask, int *shift, int *bits)
{
    int sh = 0, n = 0;

    if (!mask) {
        *shift = 0;
        *bits = 8;
        return;
    }
    while (!(mask & 1UL)) {
        mask >>= 1;
        sh++;
    }
    while (mask & 1UL) {
        mask >>= 1;
        n++;
    }
    *shift = sh;
    *bits = n;
}

/*
 * tess_shade() produces 0xRRGGBB, which is a convention, not a pixel. The
 * server has its own masks, and on this V12 they are evidently not the ones I
 * assumed: the set came out on a red field instead of blue through gold, while
 * greyscale looked right, which is exactly what a channel swap looks like when
 * r, g and b are equal.
 */
static tess_u32 to_visual(Ui *u, tess_u32 rgb)
{
    unsigned int r = (rgb >> 16) & 0xffu;
    unsigned int g = (rgb >> 8) & 0xffu;
    unsigned int b = rgb & 0xffu;

    return (tess_u32)(((r >> (8 - u->rbits)) << u->rshift) |
                      ((g >> (8 - u->gbits)) << u->gshift) |
                      ((b >> (8 - u->bbits)) << u->bshift));
}

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
/* Blend a shaded pixel towards the colour of the machine that drew it. */
static tess_u32 tint(Ui *u, tess_u32 rgb, int rank)
{
    unsigned long hue;
    unsigned int r, g, b, hr, hg, hb;

    hue = u->rank_pix[rank & 63];
    if (!hue) {
        return rgb;
    }
    r = (rgb >> 16) & 0xffu;
    g = (rgb >> 8) & 0xffu;
    b = rgb & 0xffu;
    hr = (unsigned int)((hue >> 16) & 0xffu);
    hg = (unsigned int)((hue >> 8) & 0xffu);
    hb = (unsigned int)(hue & 0xffu);
    r = (r * 3u + hr) / 4u;
    g = (g * 3u + hg) / 4u;
    b = (b * 3u + hb) / 4u;
    return (tess_u32)((r << 16) | (g << 8) | b);
}

static void shade_tile(Ui *u, int x, int y, int w, int h)
{
    static tess_u32 scratch[TESS_MAX_TILE * 4];
    int row, i, n;
    tess_u8 *src;
    tess_u32 *dst;

    while (w > 0) {
        n = w > (int)(sizeof scratch / sizeof scratch[0]) ?
            (int)(sizeof scratch / sizeof scratch[0]) : w;
        for (row = 0; row < h; row++) {
            src = u->iter + ((size_t)(y + row) * (size_t)u->width +
                             (size_t)x) * TESS_BYTES_PER_PX;
            dst = u->fb + (size_t)(y + row) * (size_t)u->width + (size_t)x;
            tess_shade(src, n, (int)u->job.max_iter, &u->pal, scratch);
            if (u->val.by_owner) {
                tess_u8 *own = u->owner + (size_t)(y + row) * u->width + x;

                for (i = 0; i < n; i++) {
                    dst[i] = to_visual(u, tint(u, scratch[i], own[i]));
                }
            } else {
                for (i = 0; i < n; i++) {
                    dst[i] = to_visual(u, scratch[i]);
                }
            }
        }
        x += n;
        w -= n;
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
 * 256 more iterations per halving of scale on top of a 500 floor. The first
 * attempt used 128 and still went soft at depth, which is the honest way to
 * pick this constant: watch where it fails and double it. Predictable, unlike
 * the usual exponential fits, and off by default so a manual budget stays
 * manual.
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
    it = (int)(500.0 + 256.0 * depth);
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

    /* Abandon whatever is in flight: the master stops handing out tiles and
       the epoch counter makes the ones already out harmless. */
    if (u->epoch_t0 > 0.0) {
        tess_u8 cb[4];

        tess_put_u32(cb, u->job.epoch);
        (void)tess_frame_write(u->fd, TESS_MSG_CANCEL, cb, 4);
    }

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
    bytes = (int)(TESS_SAMPLES(th) * TESS_BYTES_PER_PX);
    if (n + bytes > len) {
        return;
    }
    if (th.epoch != u->job.epoch) {
        return;                       /* an abandoned epoch, drop it */
    }
    if (th.x + th.w > (tess_u32)u->width || th.y + th.h > (tess_u32)u->height) {
        return;                       /* stale geometry after a resize */
    }

    {
        unsigned int step = th.step > 0 ? th.step : 1;
        unsigned int sw = (th.w + step - 1) / step;
        unsigned int col;

        /* One sample fills a step x step block: the eighth-scale pass lands as
           a blocky whole-frame preview, the fine pass overwrites it. */
        for (row = 0; row < (int)th.h; row++) {
            for (col = 0; col < th.w; col++) {
                size_t src = ((size_t)((unsigned)row / step) * sw +
                              (size_t)(col / step)) * TESS_BYTES_PER_PX;
                size_t dst = ((size_t)(th.y + row) * (size_t)u->width +
                              (size_t)(th.x + col));

                memcpy((char *)(u->iter + dst * TESS_BYTES_PER_PX),
                       (const char *)(px + src), TESS_BYTES_PER_PX);
                u->owner[dst] = (tess_u8)(th.rank & 0xff);
            }
        }
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
        {
            int r;

            for (r = 0; r < 64; r++) {
                const char *spec = tess_cluster_rank_colour(u->cl, r);
                unsigned int rr, gg, bb;

                if (spec && sscanf(spec, "#%2x%2x%2x", &rr, &gg, &bb) == 3) {
                    u->rank_pix[r] = (tess_u32)((rr << 16) | (gg << 8) | bb);
                } else {
                    u->rank_pix[r] = 0;
                }
            }
        }
        cluster_update(u);
        sprintf(msg, "connected: %d worker(s)", u->workers);
        set_status(u, msg);
        send_render(u);
    } else if (type == TESS_MSG_STATS) {
        u->tr.kb_tcp = tess_get_u32(buf);
        u->tr.kb_gm = tess_get_u32(buf + 4);
        u->tr.kb_gsn = tess_get_u32(buf + 8);
        u->tr.kb_shmem = tess_get_u32(buf + 12);
        u->tr.kb_hippi = tess_get_u32(buf + 16);
        u->tr.known = tess_get_u32(buf + 20);
        cluster_update(u);
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
    if (++u->ticks % 30 == 0 && u->cl) {
        tess_cluster_poll(u->cl);
    }
    u->tick = XtAppAddTimeOut(u->app, 200, tick_cb, (XtPointer)u);
}

/*
 * Attach to a master the cluster panel just started: connect on loopback, greet
 * it with the nonce it was given, and hand the socket to Xt.
 */
static int attach_master(Ui *u, int port, const char *nonce)
{
    tess_u8 hello[4 + TESS_NONCE_LEN];
    int n;

    if (u->fd >= 0) {
        return 0;
    }
    u->port = port;
    strncpy(u->host, "localhost", sizeof u->host - 1);
    strncpy(u->nonce, nonce, sizeof u->nonce - 1);
    u->fd = connect_master("localhost", port);
    if (u->fd < 0) {
        return -1;              /* not up yet: the panel will ask again */
    }
    u->input_id = XtAppAddInput(u->app, u->fd, (XtPointer)XtInputReadMask,
                                socket_cb, (XtPointer)u);
    n = tess_put_u32(hello, (tess_u32)TESS_PROTO_VER);
    memcpy((char *)hello + n, u->nonce, strlen(u->nonce) + 1);
    n += (int)strlen(u->nonce) + 1;
    if (tess_frame_write(u->fd, TESS_MSG_HELLO, hello, n) != 0) {
        set_status(u, "could not greet the master");
        return -1;
    }
    set_status(u, "connected, waiting for the cluster");
    ui_log(u, "connected to the master we launched");
    return 0;
}

static int cluster_ready_cb(void *ctx, int port, const char *nonce)
{
    return attach_master((Ui *)ctx, port, nonce);
}

static void ui_log_cb(void *ctx, const char *msg)
{
    ui_log((Ui *)ctx, msg);
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

    /*
     * Xt resizes children synchronously, so this fires from inside the
     * XtVaSetValues that lays the windows out at startup - before the display,
     * visual and framebuffer exist. XCreateImage then failed on a null
     * display. Nothing here is safe until the pixel machinery is built.
     */
    if (!u->ready) {
        return;
    }
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
    free((void *)u->owner);
    u->owner = (tess_u8 *)calloc((size_t)u->width * u->height, 1);
    if (!u->fb || !u->iter || !u->owner) {
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

static void save_cb(Widget w, XtPointer cd, XtPointer cb);
static void tile_cb(Widget w, XtPointer cd, XtPointer cb);
static void wm_frame_geom(Display *d, Window w, int *fx, int *fy,
                          int *fwid, int *fhgt);

static void paint_ui_button(Widget b, const char *spec)
{
    Display *d = XtDisplay(b);
    Colormap cm = DefaultColormap(d, DefaultScreen(d));
    XColor want, exact;

    if (XAllocNamedColor(d, cm, (char *)spec, &want, &exact)) {
        XtVaSetValues(b, XmNbackground, want.pixel, NULL);
    }
    XtVaSetValues(b, XmNmarginWidth, 10, XmNmarginHeight, 4, NULL);
}

/*
 * Save the frame as it is shaded now, PPM P6 and SGI .rgb.
 *
 * No library for the PPM and no endian question; the .rgb goes through
 * libimage so imgview opens it directly. Both write what is on screen, colour
 * by owner included, because that is what someone asking for a picture wants.
 */
static int write_ppm_file(Ui *u, const char *path)
{
    FILE *f;
    int x, y;
    tess_u32 v;

    f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    fprintf(f, "P6\n%d %d\n255\n", u->width, u->height);
    for (y = 0; y < u->height; y++) {
        for (x = 0; x < u->width; x++) {
            v = u->shade_rgb[(size_t)y * u->width + x];
            fputc((int)((v >> 16) & 0xff), f);
            fputc((int)((v >> 8) & 0xff), f);
            fputc((int)(v & 0xff), f);
        }
    }
    fclose(f);
    return 0;
}

static void save_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Ui *u = (Ui *)cd;
    char path[512];
    char msg[600];
    size_t npix;
    int y;

    npix = (size_t)u->width * u->height;
    u->shade_rgb = (tess_u32 *)malloc(npix * sizeof(tess_u32));
    if (!u->shade_rgb) {
        ui_log(u, "save: out of memory");
        return;
    }

    /* Re-shade into an RGB buffer: the framebuffer holds pixels in the
       visual's own channel order, which is not what a file wants. */
    for (y = 0; y < u->height; y++) {
        tess_u8 *src = u->iter + (size_t)y * u->width * TESS_BYTES_PER_PX;
        tess_u32 *dst = u->shade_rgb + (size_t)y * u->width;
        int i;

        tess_shade(src, u->width, (int)u->job.max_iter, &u->pal, dst);
        if (u->val.by_owner) {
            tess_u8 *own = u->owner + (size_t)y * u->width;

            for (i = 0; i < u->width; i++) {
                dst[i] = tint(u, dst[i], own[i]);
            }
        }
    }

    sprintf(path, "%s/tess-epoch%u.ppm", u->savedir, u->job.epoch);
    if (write_ppm_file(u, path) == 0) {
        sprintf(msg, "saved %s", path);
    } else {
        sprintf(msg, "could not write %s", path);
    }
    ui_log(u, msg);

    free((void *)u->shade_rgb);
    u->shade_rgb = (tess_u32 *)0;
}

/*
 * Lay both windows out from measurements, after both exist.
 *
 * Every attempt to do this before realize has been wrong by a border in one
 * direction or the other, because a position set before mapping means
 * different things to different window managers and cannot be measured until
 * it is too late to use. Done here, both frames are real and measurable: we
 * ask where each one is and how big it is, work out where the frames should
 * go, and convert to client coordinates with the offset we just measured.
 * Moving a mapped window is well defined, unlike guessing at one that does not
 * exist yet.
 */
static void tile_windows(Ui *u)
{
    Display *d = u->dpy;
    int rfx, rfy, rfw, rfh;
    int cfx, cfy, cfw, cfh;
    int rbx, rby, cbx, cby;
    Window child;
    int ax, ay, target_x;
    char msg[160];

    if (!u->dpy || !XtIsRealized(u->toplevel) || !XtIsRealized(u->control)) {
        return;
    }
    XSync(d, False);
    wm_frame_geom(d, XtWindow(u->toplevel), &rfx, &rfy, &rfw, &rfh);
    wm_frame_geom(d, XtWindow(u->control), &cfx, &cfy, &cfw, &cfh);

    XTranslateCoordinates(d, XtWindow(u->toplevel),
                          RootWindow(d, DefaultScreen(d)), 0, 0, &ax, &ay,
                          &child);
    rbx = ax - rfx;
    rby = ay - rfy;
    XTranslateCoordinates(d, XtWindow(u->control),
                          RootWindow(d, DefaultScreen(d)), 0, 0, &ax, &ay,
                          &child);
    cbx = ax - cfx;
    cby = ay - cfy;

    target_x = u->screen_w - (rfw + TESS_GAP + cfw);
    if (target_x < 0) {
        target_x = 0;
    }

    XMoveWindow(d, XtWindow(u->toplevel), target_x + rbx, TESS_GAP + rby);
    XMoveWindow(d, XtWindow(u->control),
                target_x + rfw + TESS_GAP + cbx, TESS_GAP + cby);
    XSync(d, False);

    sprintf(msg, "tiled: render %dx%d at %d, panel %dx%d at %d, screen %d",
            rfw, rfh, target_x, cfw, cfh, target_x + rfw + TESS_GAP,
            u->screen_w);
    ui_log(u, msg);
}

static void tile_cb(Widget w, XtPointer cd, XtPointer cb)
{
    tile_windows((Ui *)cd);
}

static void tile_once_cb(XtPointer cd, XtIntervalId *id)
{
    tile_windows((Ui *)cd);
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

/*
 * How much bigger the window manager makes a window than its contents.
 *
 * Guessing at this was wrong twice: too small and the windows overlap, too
 * large and they drift apart with a gap. 4Dwm reparents, so the frame is the
 * child of the root that contains our window, and the difference in their
 * geometries is the decoration.
 */
static void wm_frame_geom(Display *d, Window w, int *fx, int *fy,
                          int *fwid, int *fhgt)
{
    Window root, parent, *kids;
    unsigned int nkids;
    Window frame;
    int x, y;
    unsigned int fw, fh, iw, ih, bw, depth;

    *fx = 0;
    *fy = 0;
    *fwid = 0;
    *fhgt = 0;
    if (!XGetGeometry(d, w, &root, &x, &y, &iw, &ih, &bw, &depth)) {
        return;
    }
    *fwid = (int)iw;
    *fhgt = (int)ih;
    *fx = x;
    *fy = y;
    frame = w;
    for (;;) {
        kids = (Window *)0;
        if (!XQueryTree(d, frame, &root, &parent, &kids, &nkids)) {
            return;
        }
        if (kids) {
            XFree((char *)kids);
        }
        if (parent == root || parent == 0) {
            break;
        }
        frame = parent;
    }
    if (frame == w) {
        return;                     /* not reparented: the window is the frame */
    }
    if (!XGetGeometry(d, frame, &root, &x, &y, &fw, &fh, &bw, &depth)) {
        return;
    }
    /* Where the decorated window really is and how big it really is. Guessing
       at the decoration was wrong three times; this asks. */
    *fx = x;
    *fy = y;
    *fwid = (int)fw + 2 * (int)bw;
    *fhgt = (int)fh + 2 * (int)bw;
}

/* The second top-level shell: same app context, no MPI, no blocking. */
static void build_control(Ui *u)
{
    Widget form, buttons, b;

    u->control = XtVaAppCreateShell("control", "Tess",
                                    topLevelShellWidgetClass,
                                    XtDisplay(u->toplevel),
                                    XmNtitle, "Tess control",
                                    XmNx, u->ctrl_x,
                                    XmNy, u->ctrl_y,
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
                                      XmNpacking, XmPACK_COLUMN,
                                      XmNnumColumns, 1,
                                      XmNspacing, 4,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget,
                                      XtParent(u->colour_pane->form),
                                      XmNleftAttachment, XmATTACH_FORM,
                                      NULL);
    b = XtVaCreateManagedWidget("Render", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, render_cb, (XtPointer)u);
    paint_ui_button(b, "#d9a441");
    b = XtVaCreateManagedWidget("Home", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, home_cb, (XtPointer)u);
    paint_ui_button(b, "#c3cad0");
    b = XtVaCreateManagedWidget("Save", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, save_cb, (XtPointer)u);
    paint_ui_button(b, "#a8b4bc");
    b = XtVaCreateManagedWidget("Tile", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, tile_cb, (XtPointer)u);
    paint_ui_button(b, "#a8b4bc");

    {
        Widget cframe;

        cframe = XtVaCreateManagedWidget("cframe", xmFrameWidgetClass, form,
                                         XmNshadowType, XmSHADOW_ETCHED_IN,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, buttons,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         NULL);
        XtVaCreateManagedWidget("Status", xmLabelWidgetClass, cframe,
                                XmNchildType, XmFRAME_TITLE_CHILD,
                                NULL);
        /*
         * The drawing area is the frame's only child. Wrapping it in a
         * RowColumn cost it its height: it was clipped to a sliver and two
         * draws overlapped inside it. The elapsed line goes below the frame,
         * which keeps render progress and cluster progress together without
         * nesting managers that argue about size.
         */
        u->cluster = XtVaCreateManagedWidget("stats",
                                             xmDrawingAreaWidgetClass, cframe,
                                             XmNheight, 172,
                                             XmNwidth, TESS_CTRL_W - 28,
                                             NULL);
        XtAddCallback(u->cluster, XmNexposeCallback, stats_expose_cb,
                      (XtPointer)u);
        u->clusterframe = cframe;
    }

    u->elapsed = XtVaCreateManagedWidget("idle", xmLabelWidgetClass, form,
                                         XmNalignment, XmALIGNMENT_BEGINNING,
                                         XmNtopAttachment, XmATTACH_WIDGET,
                                         XmNtopWidget, u->clusterframe,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         NULL);

    u->cl = tess_cluster_create(form, u->tree, u->hostlist, u->port,
                                cluster_ready_cb, (void *)u,
                                ui_log_cb, (void *)u);
    XtVaSetValues(tess_cluster_widget(u->cl),
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, u->elapsed,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);

    u->log = XmCreateScrolledText(form, "log", (ArgList)0, 0);
    XtVaSetValues(u->log,
                  XmNeditable, False,
                  XmNeditMode, XmMULTI_LINE_EDIT,
                  XmNrows, 10,
                  XmNcolumns, 52,
                  NULL);
    XtVaSetValues(XtParent(u->log),
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, tess_cluster_widget(u->cl),
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
    "*info.fontList: -*-screen-medium-r-normal--10-*-*-*-*-*-iso8859-1",
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
    WmMetrics wm;
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
    strcpy(u.tree, "/cluster/dev/sgi-tess");
    strcpy(u.savedir, ".");
    strcpy(u.hostlist, "lucy,aurora");
    u.port = TESS_DEFAULT_PORT;
    u.val.ramp = u.pal.ramp;
    u.val.cycles = u.pal.cycles;
    u.val.rotate = u.pal.rotate;
    u.val.interior = u.pal.interior;

    if (tess_types_check() != 0) {
        die("integer widths are not what the wire format assumes");
    }
    wm_metrics_load(&wm);

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-savedir") == 0 && i + 1 < argc) {
            strncpy(u.savedir, argv[++i], sizeof u.savedir - 1);
        } else if (strcmp(argv[i], "-tree") == 0 && i + 1 < argc) {
            strncpy(u.tree, argv[++i], sizeof u.tree - 1);
        } else if (strcmp(argv[i], "-hosts") == 0 && i + 1 < argc) {
            strncpy(u.hostlist, argv[++i], sizeof u.hostlist - 1);
        } else if (strcmp(argv[i], "-attach") == 0) {
            u.attach = 1;
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
        int avail_h;

        /*
         * Both windows together take TESS_USE of the width, flush to the right
         * edge. The render window is 4:3, which is the shape of the machines
         * this runs on, and the control panel sits to its right with room for
         * the window manager's frame between them: the previous attempt
         * overlapped because it placed the panel at render_w + gap, ignoring
         * the border the WM adds on both windows.
         */
        u.screen_w = sw;
        u.screen_h = sh;

        if (!width_given) {
            /* screen = origin + render frame + gap + panel frame, solved for
               the render's inner width with the metrics we learned last time */
            u.width = sw - (int)((double)sw * (1.0 - TESS_USE)) -
                      wm.decor_x - TESS_GAP - TESS_CTRL_W - wm.decor_x;
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

        /*
         * Place the render window BEFORE it is realized, which is the only
         * time a position means "put the frame here" rather than "move the
         * client". The decoration is an estimate at this point; the panel is
         * placed later from a measurement, so the pair still meets exactly.
         */
        /*
         * Where the frame should start, plus the client offset only if this
         * window manager reads a position as the client's. Which of the two it
         * does is measured below and remembered: assuming either way has been
         * wrong once each, and the error is exactly one border.
         */
        u.origin_x = sw - (u.width + wm.decor_x + TESS_GAP + TESS_CTRL_W +
                           wm.decor_x);
        if (wm.pos_is_client) {
            u.origin_x += wm.border_x;
        }
        if (u.origin_x < 0) {
            u.origin_x = 0;
        }
        XtVaSetValues(u.toplevel,
                      XmNx, u.origin_x,
                      XmNy, TESS_GAP,
                      NULL);

        /*
         * The opening view is framed from the window we actually got, not from
         * a hardcoded 1024: 3.2 units across whatever width this is, so the set
         * sits in the frame at any aspect or screen size.
         */
        u.job.scale = 3.2 / (double)u.width;
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

    /*
     * Now that the render window exists, ask the window manager what it did to
     * it, and lay both windows out from the measurement: together they occupy
     * TESS_USE of the screen, flush right, with the panel touching the render
     * window's frame rather than floating away from it.
     */
    {
        int fx, fy, fwid, fhgt;
        Display *d = XtDisplay(u.toplevel);

        XSync(d, False);
        wm_frame_geom(d, XtWindow(u.toplevel), &fx, &fy, &fwid, &fhgt);

        /*
         * Only the control window is positioned from this measurement, and it
         * is positioned before it is realized. Moving a shell AFTER it is
         * mapped sets the client origin rather than the frame origin, so the
         * window manager's title bar ends up above the requested y - which is
         * exactly how the render window lost its title bar off the top of the
         * screen while the panel sat 40 pixels lower.
         */
        /*
         * Flush to the right screen edge. The panel's own frame is the same
         * width as the render window's, so its right edge lands on the screen
         * edge when placed at screen - (panel + decoration). The render window
         * is then resized, not moved, to meet it: resizing after realize is
         * safe, moving is not, and this is why the pair kept leaving a strip
         * of desktop on the right.
         */
        /*
         * Place the panel at the render window's measured frame edge and
         * resize nothing.
         *
         * The previous attempt resized the render window to make the pair
         * flush with the right screen edge, then re-measured to place the
         * panel. That cannot work: XSync waits for the SERVER, while the
         * resize is handled by the window manager, which is another client, so
         * the re-measurement still read the old geometry and the windows
         * overlapped by the difference. A few pixels of desktop on the right
         * is a much smaller problem than a panel with its labels underneath
         * the fractal.
         */
        /*
         * Where the window manager puts the CLIENT inside the frame it drew.
         *
         * Setting a shell's position before realize is honoured as the client
         * position here, not the frame position, so the frame lands that much
         * further left and the two windows overlapped by exactly one border.
         * Measuring the offset on the render window tells us what to add.
         */
        {
            Window child;
            int ax, ay;

            XTranslateCoordinates(d, XtWindow(u.toplevel),
                                  RootWindow(d, DefaultScreen(d)),
                                  0, 0, &ax, &ay, &child);
            u.border_x = ax - fx;
            u.border_y = ay - fy;
            if (u.border_x < 0) {
                u.border_x = 0;
            }
            if (u.border_y < 0) {
                u.border_y = 0;
            }
        }

        wm.decor_x = fwid - u.width;
        wm.decor_y = fhgt - u.height;
        wm.border_x = u.border_x;
        wm.border_y = u.border_y;

        /*
         * We asked for origin_x. If the FRAME landed there, the window manager
         * reads a position as the frame's; if the CLIENT did, it reads it as
         * the client's. One comparison settles a question that has cost four
         * rounds of windows a border out of place.
         */
        {
            int asked = u.origin_x;
            int client_x = fx + u.border_x;

            if (abs(client_x - asked) < abs(fx - asked)) {
                wm.pos_is_client = 1;
            } else {
                wm.pos_is_client = 0;
            }
        }
        if (wm.decor_x < 0) {
            wm.decor_x = 0;
        }
        if (wm.decor_y < 0) {
            wm.decor_y = 0;
        }
        wm_metrics_save(&wm);

        u.ctrl_x = fx + fwid + TESS_GAP;
        u.ctrl_y = fy;
        if (wm.pos_is_client) {
            u.ctrl_x += u.border_x;
            u.ctrl_y += u.border_y;
        }

        {
            char msg[160];

            sprintf(msg,
                    "render %dx%d frame %d,%d %dx%d border %d,%d pos=%s",
                    u.width, u.height, fx, fy, fwid, fhgt,
                    u.border_x, u.border_y,
                    wm.pos_is_client ? "client" : "frame");
            u.pending_log[0] = '\0';
            strncpy(u.pending_log, msg, sizeof u.pending_log - 1);
        }
    }

    job_to_values(&u);
    build_control(&u);
    if (u.pending_log[0]) {
        ui_log(&u, u.pending_log);
    }
    u.tick = XtAppAddTimeOut(app, 200, tick_cb, (XtPointer)&u);

    u.dpy = XtDisplay(u.toplevel);
    u.visual = DefaultVisual(u.dpy, DefaultScreen(u.dpy));
    u.depth = DefaultDepth(u.dpy, DefaultScreen(u.dpy));
    u.cmap = DefaultColormap(u.dpy, DefaultScreen(u.dpy));
    u.statsfont = XLoadQueryFont(u.dpy,
                                 "-*-screen-medium-r-normal--10-*-*-*-*-*-iso8859-1");
    if (!u.statsfont) {
        u.statsfont = XLoadQueryFont(u.dpy, "fixed");
    }
    mask_decode(u.visual->red_mask, &u.rshift, &u.rbits);
    mask_decode(u.visual->green_mask, &u.gshift, &u.gbits);
    mask_decode(u.visual->blue_mask, &u.bshift, &u.bbits);

    u.fb = (tess_u32 *)calloc((size_t)u.width * u.height, sizeof(tess_u32));
    u.iter = (tess_u8 *)calloc((size_t)u.width * u.height * TESS_BYTES_PER_PX,
                               1);
    u.owner = (tess_u8 *)calloc((size_t)u.width * u.height, 1);
    if (!u.fb || !u.iter || !u.owner) {
        die("out of memory");
    }
    recreate_ximage(&u);
    u.ready = 1;

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
    XtAppAddTimeOut(app, 700, tile_once_cb, (XtPointer)&u);

    u.fd = u.attach ? connect_master(host, port) : -1;
    if (u.fd < 0) {
        char msg[160];

        if (u.attach) {
            sprintf(msg, "no master at %s:%d", host, port);
        } else {
            sprintf(msg, "idle - Rescan, then Launch");
        }
        set_status(&u, msg);
        tess_cluster_rescan(u.cl);
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
