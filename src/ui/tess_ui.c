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
 * v1 is deliberately one window: image, status line, click to zoom. The
 * two-window layout in design/ui-design.html comes next, against this same
 * protocol.
 *
 * C89 throughout. X and Motif idioms follow baseline/mandel1-motif-single.c,
 * which is known to build with MIPSpro and IRIX IM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>

#include "tess_types.h"
#include "tess_proto.h"
#include "tess_wire.h"
#include "tess_mandel.h"

typedef struct Ui {
    Widget     toplevel;
    Widget     canvas;
    Widget     status;
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
    int        palette;
    int        tiles_in;
    int        workers;
    double     last_msec;
} Ui;

/* ------------------------------------------------------------- plumbing */

static void die(const char *msg)
{
    fprintf(stderr, "tess-ui: %s\n", msg);
    exit(1);
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
        tess_shade(src, w, (int)u->job.max_iter, u->palette, dst);
    }
}

/* ----------------------------------------------------------- the job */

static void send_render(Ui *u)
{
    tess_u8 body[TESS_JOB_WIRE];
    int n;
    char msg[160];

    u->job.epoch++;
    u->job.width = (tess_u32)u->width;
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
    u->tiles_in++;
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
        u->workers = (int)tess_get_u32(buf + 8);
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
    }
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
        u->job.max_iter += 64;
    } else {
        u->job.scale *= 2.0;
        if (u->job.max_iter > 128) {
            u->job.max_iter -= 64;
        }
    }
    send_render(u);
}

static void input_cb(Widget w, XtPointer cd, XtPointer cb)
{
    Ui *u = (Ui *)cd;
    XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)cb;
    XButtonEvent *e;

    if (!cbs || !cbs->event || u->fd < 0) {
        return;
    }
    if (cbs->event->type != ButtonPress) {
        return;
    }
    e = (XButtonEvent *)cbs->event;
    if (e->button == Button1) {
        zoom_at(u, e->x, e->y, 1);
    } else if (e->button == Button3) {
        zoom_at(u, e->x, e->y, 0);
    } else if (e->button == Button2) {
        /* palette only: no cluster traffic, which is the point of shipping
           iteration counts rather than colour */
        u->palette = !u->palette;
        shade_tile(u, 0, 0, u->width, u->height);
        blit(u, 0, 0, u->width, u->height);
    }
}

/* ----------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    XtAppContext app;
    Widget form;
    Ui u;
    char *host = "localhost";
    int port = TESS_DEFAULT_PORT;
    int i;
    tess_u8 hello[4];
    Arg args[8];
    int nargs;

    memset((char *)&u, 0, sizeof u);
    u.width = 1024;
    u.height = 768;
    u.fd = -1;
    u.palette = 0;
    u.job.epoch = 0;
    u.job.max_iter = 1000;
    u.job.tile = TESS_TILE;
    u.job.cx = -0.6;
    u.job.cy = 0.0;
    u.job.scale = 3.2 / 1024.0;

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
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            u.height = atoi(argv[++i]);
        }
    }

    u.toplevel = XtVaAppInitialize(&app, "Tess", (XrmOptionDescList)0, 0,
                                  &argc, argv, (String *)0, NULL);
    form = XtVaCreateManagedWidget("form", xmFormWidgetClass, u.toplevel,
                                   NULL);

    nargs = 0;
    XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM); nargs++;
    u.status = XmCreateLabel(form, "status", args, (Cardinal)nargs);
    XtManageChild(u.status);

    nargs = 0;
    XtSetArg(args[nargs], XmNwidth, u.width); nargs++;
    XtSetArg(args[nargs], XmNheight, u.height); nargs++;
    XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_FORM); nargs++;
    XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_WIDGET); nargs++;
    XtSetArg(args[nargs], XmNbottomWidget, (XtArgVal)u.status); nargs++;
    u.canvas = XmCreateDrawingArea(form, "canvas", args, (Cardinal)nargs);
    XtManageChild(u.canvas);

    XtAddCallback(u.canvas, XmNexposeCallback, expose_cb, (XtPointer)&u);
    XtAddCallback(u.canvas, XmNresizeCallback, resize_cb, (XtPointer)&u);
    XtAddCallback(u.canvas, XmNinputCallback, input_cb, (XtPointer)&u);

    XtRealizeWidget(u.toplevel);

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

    u.fd = connect_master(host, port);
    if (u.fd < 0) {
        char msg[160];

        sprintf(msg, "no master at %s:%d - start tess-node -listen", host,
                port);
        set_status(&u, msg);
    } else {
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
