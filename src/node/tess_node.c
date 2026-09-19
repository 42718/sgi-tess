/*
 * tess_node.c - the MPI half of Tess: one master, N workers.
 *
 * Rank 0 is the master. It owns the tile queue, hands tiles out on request,
 * collects results, and either writes a PPM (-o) or serves them to the GUI over
 * a socket (-listen). It computes nothing itself, which is DESIGN.md section 3b:
 * the display host runs X, the GUI and the shading pass, and 3-10% of throughput
 * is worth paying to keep it responsive.
 *
 * Ranks 1..N-1 are workers: ask, compute, return, ask again. Credit-based
 * prefetch is not in v1.
 *
 *   mpirun ... lucy 1 tess-node -listen : aurora 4 tess-node -listen
 *   mpirun ... lucy 1 tess-node -o out.ppm -w 1920 -h 1200 : aurora 4 ...
 *
 * C89 throughout. No Motif, ever: this binary never links X.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>

#include <mpi.h>

#include "tess_types.h"
#include "tess_proto.h"
#include "tess_wire.h"
#include "tess_mandel.h"
#include "tess_inventory.h"

static int verbose = 0;
static int idle_us = 2000;
static const char *nonce = (const char *)0;      /* -idle-us: sleep between polls when idle */

/*
 * Wait for rank 0 to say something, cheaply.
 *
 * MPT spin-waits inside a blocking MPI_Probe and MPI_NAP=2 does not change
 * that: measured on aurora, four parked workers burned thirty seconds of CPU
 * in thirty seconds of wall time while the GUI sat idle between frames. So the
 * worker polls instead. It spins briefly first, because during a frame the
 * next tile arrives in microseconds and sleeping there would cost throughput,
 * then falls back to a short sleep, which is what makes an idle cluster idle.
 *
 * select() with no descriptors is the sleep: usleep's declaration varies with
 * feature-test macros on IRIX, and this needs no header archaeology.
 */
#define TESS_SPINS 500

static void wait_for_master(MPI_Status *st)
{
    struct timeval tv;
    int flag, spins;

    spins = 0;
    for (;;) {
        flag = 0;
        MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, st);
        if (flag) {
            return;
        }
        if (spins < TESS_SPINS) {
            spins++;
            continue;
        }
        tv.tv_sec = 0;
        tv.tv_usec = idle_us;
        select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
    }
}

#define VLOG if (verbose) vlog

/* stderr, unbuffered by convention, and prefixed with the rank so a hang in a
   five-rank job says which side stopped talking. */
static void vlog(int rank, const char *fmt, int a, int b)
{
    fprintf(stderr, "[rank %d] ", rank);
    fprintf(stderr, fmt, a, b);
    fprintf(stderr, "\n");
    fflush(stderr);
}

typedef struct Tile {
    tess_u32 x, y, w, h;
    int      order;          /* centre-out rank, smaller is sooner */
} Tile;

static double now_sec(void)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *)0);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

/* ---------------------------------------------------------------- tiles */

static int tile_cmp(const void *pa, const void *pb)
{
    const Tile *a = (const Tile *)pa;
    const Tile *b = (const Tile *)pb;

    if (a->order < b->order) return -1;
    if (a->order > b->order) return 1;
    return 0;
}

/*
 * Centre-out, because the interesting part of a fractal is the middle and a
 * human watching the frame fill should see it resolve there first.
 * DESIGN.md section 3.
 */
static Tile *plan_tiles(const TessJob *j, int *ntiles)
{
    Tile *t;
    unsigned int x, y;
    int n, cap;
    double cxp, cyp, dx, dy;

    cap = (int)(((j->width + j->tile - 1) / j->tile) *
                ((j->height + j->tile - 1) / j->tile));
    t = (Tile *)malloc((size_t)cap * sizeof(Tile));
    if (!t) {
        *ntiles = 0;
        return (Tile *)0;
    }

    cxp = (double)j->width * 0.5;
    cyp = (double)j->height * 0.5;
    n = 0;
    for (y = 0; y < j->height; y += j->tile) {
        for (x = 0; x < j->width; x += j->tile) {
            t[n].x = x;
            t[n].y = y;
            t[n].w = (x + j->tile <= j->width) ? j->tile : j->width - x;
            t[n].h = (y + j->tile <= j->height) ? j->tile : j->height - y;
            dx = ((double)x + (double)t[n].w * 0.5) - cxp;
            dy = ((double)y + (double)t[n].h * 0.5) - cyp;
            t[n].order = (int)(dx * dx + dy * dy);
            n++;
        }
    }
    qsort((void *)t, (size_t)n, sizeof(Tile), tile_cmp);
    *ntiles = n;
    return t;
}

/* ---------------------------------------------------------------- worker */

static void worker_loop(int rank)
{
    TessAssign a;
    TessResultHdr rh;
    MPI_Status st;
    tess_u8 *buf;
    int req;
    double t0;

    buf = (tess_u8 *)malloc((size_t)TESS_MAX_TILE * TESS_MAX_TILE *
                            TESS_BYTES_PER_PX);
    if (!buf) {
        fprintf(stderr, "rank %d: out of memory\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (;;) {
        req = rank;
        MPI_Send(&req, 1, MPI_INT, 0, TESS_TAG_REQ, MPI_COMM_WORLD);
        VLOG(rank, "asked for work (%d,%d)", 0, 0);
        wait_for_master(&st);
        if (st.MPI_TAG == TESS_TAG_STOP) {
            MPI_Recv(&req, 1, MPI_INT, 0, TESS_TAG_STOP, MPI_COMM_WORLD, &st);
            break;
        }
        MPI_Recv((void *)&a, (int)sizeof a, MPI_BYTE, 0, TESS_TAG_ASSIGN,
                 MPI_COMM_WORLD, &st);
        VLOG(rank, "got tile at (%d,%d)", (int)a.x, (int)a.y);

        t0 = now_sec();
        tess_mandel_tile(&a, buf);

        rh.epoch = a.epoch;
        rh.x = a.x;
        rh.y = a.y;
        rh.w = a.w;
        rh.h = a.h;
        rh.rank = (tess_u32)rank;
        rh.usec = (tess_u32)((now_sec() - t0) * 1e6);
        MPI_Send((void *)&rh, (int)sizeof rh, MPI_BYTE, 0, TESS_TAG_RESULT,
                 MPI_COMM_WORLD);
        MPI_Send((void *)buf, (int)(a.w * a.h * TESS_BYTES_PER_PX), MPI_BYTE, 0,
                 TESS_TAG_RESULT, MPI_COMM_WORLD);
        VLOG(rank, "sent tile at (%d,%d)", (int)a.x, (int)a.y);
    }
    free((void *)buf);
}

/* ---------------------------------------------------------------- master */

/*
 * Run one epoch to completion, calling sink() for each tile as it lands.
 * Returns tiles completed. The sink is what makes this reusable: PPM assembly
 * and the GUI socket are the same loop with a different sink.
 */
typedef void (*TileSink)(void *ctx, const TessResultHdr *rh, const tess_u8 *px);

static int run_epoch(const TessJob *job, int nranks, int *parked,
                     TileSink sink, void *ctx)
{
    Tile *tiles;
    TessAssign a;
    TessResultHdr rh;
    MPI_Status st;
    tess_u8 *px;
    int ntiles, next, done, src, req;
    int i;

    tiles = plan_tiles(job, &ntiles);
    if (!tiles) {
        return 0;
    }
    px = (tess_u8 *)malloc((size_t)TESS_MAX_TILE * TESS_MAX_TILE *
                           TESS_BYTES_PER_PX);
    if (!px) {
        free((void *)tiles);
        return 0;
    }

    next = 0;
    done = 0;

    /*
     * Workers parked at the end of the previous epoch are blocked waiting for
     * a reply from us, so a new epoch starts by handing them work. This is why
     * an epoch must never stop a worker: a stopped worker calls MPI_Finalize
     * and exits, and the next frame has nobody to compute it. Stops belong to
     * the end of the job, in stop_workers().
     */
    for (i = 1; i < nranks && next < ntiles; i++) {
        if (parked[i]) {
            a.epoch = job->epoch;
            a.x = tiles[next].x;
            a.y = tiles[next].y;
            a.w = tiles[next].w;
            a.h = tiles[next].h;
            a.width = job->width;
            a.height = job->height;
            a.max_iter = job->max_iter;
            a.cx = job->cx;
            a.cy = job->cy;
            a.scale = job->scale;
            MPI_Send((void *)&a, (int)sizeof a, MPI_BYTE, i,
                     TESS_TAG_ASSIGN, MPI_COMM_WORLD);
            next++;
            parked[i] = 0;
            VLOG(0, "woke parked rank %d with tile %d", i, next);
        }
    }

    /*
     * Run until every tile is IN, not until every worker has been stopped.
     * Stopping a worker that still has a result in flight leaves it blocked in
     * MPI_Send on a payload nobody will receive, because MPT uses a rendezvous
     * protocol above its eager threshold and a 64x64 tile is 12 KB. That hangs
     * the job rather than failing it, and with one worker the message ordering
     * hides it: the bug needs two workers finishing together to appear.
     *
     * A worker that asks when the queue is empty is parked, not stopped, so its
     * results are still collected. Stops go out once nothing is outstanding.
     */
    while (done < ntiles) {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
        src = st.MPI_SOURCE;

        if (st.MPI_TAG == TESS_TAG_REQ) {
            MPI_Recv(&req, 1, MPI_INT, src, TESS_TAG_REQ, MPI_COMM_WORLD, &st);
            if (next < ntiles) {
                a.epoch = job->epoch;
                a.x = tiles[next].x;
                a.y = tiles[next].y;
                a.w = tiles[next].w;
                a.h = tiles[next].h;
                a.width = job->width;
                a.height = job->height;
                a.max_iter = job->max_iter;
                a.cx = job->cx;
                a.cy = job->cy;
                a.scale = job->scale;
                MPI_Send((void *)&a, (int)sizeof a, MPI_BYTE, src,
                         TESS_TAG_ASSIGN, MPI_COMM_WORLD);
                next++;
                VLOG(0, "assigned tile %d of %d", next, ntiles);
            } else if (!parked[src]) {
                parked[src] = 1;
                VLOG(0, "parked rank %d, %d tiles still out", src,
                     ntiles - done);
            }
        } else if (st.MPI_TAG == TESS_TAG_RESULT) {
            MPI_Recv((void *)&rh, (int)sizeof rh, MPI_BYTE, src,
                     TESS_TAG_RESULT, MPI_COMM_WORLD, &st);
            MPI_Recv((void *)px, (int)(rh.w * rh.h * TESS_BYTES_PER_PX),
                     MPI_BYTE, src, TESS_TAG_RESULT, MPI_COMM_WORLD, &st);
            if (rh.epoch == job->epoch) {
                sink(ctx, &rh, px);
                done++;
                VLOG(0, "have %d of %d tiles", done, ntiles);
            }
            /* a tile from an abandoned epoch is dropped, which is what the
               epoch counter is for: DESIGN.md section 3 */
        } else {
            MPI_Recv(&req, 1, MPI_INT, src, st.MPI_TAG, MPI_COMM_WORLD, &st);
        }
    }

    VLOG(0, "epoch complete, %d of %d tiles", done, ntiles);

    free((void *)px);
    free((void *)tiles);
    return done;
}

/*
 * End of job, not end of epoch. Parked workers are told directly; the rest are
 * still computing or about to ask, so we wait for each to speak and answer with
 * a stop. Late results are drained so nobody is left blocked in MPI_Send.
 */
static void stop_workers(int nranks, int *parked)
{
    TessResultHdr rh;
    MPI_Status st;
    tess_u8 *px;
    int stopped, i, src, req, stop;

    px = (tess_u8 *)malloc((size_t)TESS_MAX_TILE * TESS_MAX_TILE *
                           TESS_BYTES_PER_PX);
    stopped = 0;
    for (i = 1; i < nranks; i++) {
        if (parked[i]) {
            stop = 0;
            MPI_Send(&stop, 1, MPI_INT, i, TESS_TAG_STOP, MPI_COMM_WORLD);
            parked[i] = 0;
            stopped++;
        }
    }
    while (stopped < nranks - 1) {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
        src = st.MPI_SOURCE;
        if (st.MPI_TAG == TESS_TAG_REQ) {
            MPI_Recv(&req, 1, MPI_INT, src, TESS_TAG_REQ, MPI_COMM_WORLD, &st);
            stop = 0;
            MPI_Send(&stop, 1, MPI_INT, src, TESS_TAG_STOP, MPI_COMM_WORLD);
            stopped++;
        } else if (px) {
            MPI_Recv((void *)&rh, (int)sizeof rh, MPI_BYTE, src, st.MPI_TAG,
                     MPI_COMM_WORLD, &st);
            MPI_Recv((void *)px, (int)(rh.w * rh.h * TESS_BYTES_PER_PX),
                     MPI_BYTE, src, TESS_TAG_RESULT, MPI_COMM_WORLD, &st);
        }
    }
    if (px) {
        free((void *)px);
    }
    VLOG(0, "stopped %d worker(s) (%d)", stopped, 0);
}

/* ------------------------------------------------------------ PPM sink */

typedef struct PpmCtx {
    tess_u8 *iter;          /* whole image, 3 bytes per pixel */
    const TessJob *job;
} PpmCtx;

static void ppm_sink(void *ctx, const TessResultHdr *rh, const tess_u8 *px)
{
    PpmCtx *c = (PpmCtx *)ctx;
    unsigned int row;
    size_t dst, src, rowbytes;

    rowbytes = (size_t)rh->w * TESS_BYTES_PER_PX;
    for (row = 0; row < rh->h; row++) {
        dst = ((size_t)(rh->y + row) * (size_t)c->job->width + (size_t)rh->x) *
              TESS_BYTES_PER_PX;
        src = (size_t)row * rowbytes;
        memcpy((char *)c->iter + dst, (const char *)px + src, rowbytes);
    }
}

static int write_ppm(const char *path, const TessJob *job, const tess_u8 *iter)
{
    FILE *f;
    tess_u32 *rgb;
    unsigned int y, x;
    size_t npix;

    npix = (size_t)job->width * (size_t)job->height;
    rgb = (tess_u32 *)malloc(npix * sizeof(tess_u32));
    if (!rgb) {
        return -1;
    }
    {
        TessPalette pal;

        tess_palette_default(&pal);
        tess_shade(iter, (int)npix, (int)job->max_iter, &pal, rgb);
    }

    f = fopen(path, "wb");
    if (!f) {
        free((void *)rgb);
        return -1;
    }
    fprintf(f, "P6\n%u %u\n255\n", job->width, job->height);
    for (y = 0; y < job->height; y++) {
        for (x = 0; x < job->width; x++) {
            tess_u32 v = rgb[(size_t)y * job->width + x];
            fputc((int)((v >> 16) & 0xff), f);
            fputc((int)((v >> 8) & 0xff), f);
            fputc((int)(v & 0xff), f);
        }
    }
    fclose(f);
    free((void *)rgb);
    return 0;
}

/* ------------------------------------------------------------ GUI sink */

typedef struct GuiCtx {
    int fd;
    int broken;
} GuiCtx;

static void gui_sink(void *ctx, const TessResultHdr *rh, const tess_u8 *px)
{
    GuiCtx *g = (GuiCtx *)ctx;
    tess_u8 body[TESS_MAX_FRAME];
    TessTileHdr th;
    int n, bytes;

    if (g->broken) {
        return;
    }
    th.epoch = rh->epoch;
    th.x = rh->x;
    th.y = rh->y;
    th.w = rh->w;
    th.h = rh->h;
    th.rank = rh->rank;
    th.usec = rh->usec;

    n = tess_put_tilehdr(body, &th);
    bytes = (int)(rh->w * rh->h * TESS_BYTES_PER_PX);
    memcpy((char *)body + n, (const char *)px, (size_t)bytes);
    if (tess_frame_write(g->fd, TESS_MSG_TILE, body, n + bytes) != 0) {
        g->broken = 1;
    }
}

/* Wait for one GUI to connect, then serve epochs until it goes away. */
/*
 * With a nonce, bind loopback only and refuse a client that cannot quote it.
 * The GUI and the master share a machine by design, so nothing legitimate
 * connects from the network, and an unauthenticated public port on a cluster
 * head node is not a thing to leave lying around.
 */
static int serve_gui(int port, int nranks, int *parked)
{
    int lfd, cfd, one, type, len, bye;
    struct sockaddr_in sa;
    tess_u8 buf[TESS_MAX_FRAME];
    TessJob job;
    TessWelcome wel;
    GuiCtx g;
    double t0;
    int tiles;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        perror("socket");
        return -1;
    }
    one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof one);
    memset((char *)&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = nonce ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
    sa.sin_port = htons((unsigned short)port);
    if (bind(lfd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        perror("bind");
        close(lfd);
        return -1;
    }
    listen(lfd, 1);
    printf("tess-node: master listening on %s port %d, %d rank(s)\n",
           nonce ? "loopback" : "any", port, nranks);
    fflush(stdout);

    /* null length: IRIX declares the third argument int *, macOS socklen_t *,
       and a null pointer satisfies both without a per-platform typedef. */
    /*
     * Serve clients one at a time, for as long as the job lives. Accepting
     * once was wrong: the GUI probes the port to find out whether the master
     * is up yet, and that probe was being taken for the GUI itself, so the
     * master saw an immediate EOF and shut the whole job down before the real
     * connection arrived. Re-accepting also means the GUI can be restarted
     * without relaunching the cluster.
     */
    for (;;) {
        cfd = accept(lfd, (struct sockaddr *)0, (void *)0);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            close(lfd);
            return -1;
        }
        printf("tess-node: client connected\n");
        fflush(stdout);

        g.fd = cfd;
        g.broken = 0;
        bye = 0;

        for (;;) {
            if (tess_frame_read(cfd, &type, buf, &len) != 0) {
                break;
            }
            if (type == TESS_MSG_HELLO) {
                tess_u8 wb[12];
                int n = 0;

                if (nonce) {
                    char got[64];
                    int glen = len - 4;

                    if (glen < 0) {
                        glen = 0;
                    }
                    if (glen > (int)sizeof got - 1) {
                        glen = (int)sizeof got - 1;
                    }
                    memcpy(got, (char *)buf + 4, (size_t)glen);
                    got[glen] = '\0';
                    if (strcmp(got, nonce) != 0) {
                        fprintf(stderr, "tess-node: client failed the nonce\n");
                        fflush(stderr);
                        break;
                    }
                }

                wel.version = TESS_PROTO_VER;
                wel.ranks = (tess_u32)nranks;
                wel.workers = (tess_u32)(nranks - 1);
                n += tess_put_u32(wb + n, wel.version);
                n += tess_put_u32(wb + n, wel.ranks);
                n += tess_put_u32(wb + n, wel.workers);
                if (tess_frame_write(cfd, TESS_MSG_WELCOME, wb, n) != 0) {
                    break;
                }
            } else if (type == TESS_MSG_RENDER) {
                tess_u8 db[12];
                int n = 0;

                tess_get_job(buf, &job);
                if (job.tile == 0 || job.tile > TESS_MAX_TILE) {
                    job.tile = TESS_TILE;
                }
                t0 = now_sec();
                tiles = run_epoch(&job, nranks, parked, gui_sink, (void *)&g);
                if (g.broken) {
                    break;
                }
                n += tess_put_u32(db + n, job.epoch);
                n += tess_put_u32(db + n, (tess_u32)tiles);
                n += tess_put_u32(db + n,
                                  (tess_u32)((now_sec() - t0) * 1000.0));
                if (tess_frame_write(cfd, TESS_MSG_DONE, db, n) != 0) {
                    break;
                }
            } else if (type == TESS_MSG_BYE) {
                bye = 1;
                break;
            }
        }

        close(cfd);
        printf("tess-node: client gone%s\n", bye ? ", exiting" : ", waiting");
        fflush(stdout);
        if (bye) {
            break;
        }
    }

    close(lfd);
    return 0;
}

/* ---------------------------------------------------------------- main */

static void usage(const char *me)
{
    fprintf(stderr, "usage: %s [-listen [port]] [-nonce hex] [-o file.ppm]\n",
            me);
    fprintf(stderr, "          [-w px] [-h px] [-max n] [-tile n]\n");
    fprintf(stderr, "          [-cx v] [-cy v] [-scale v] [-idle-us n]\n");
    fprintf(stderr, "          [-v] [-help]\n");
    fflush(stderr);
}

int main(int argc, char **argv)
{
    int rank, nranks, i;
    int want_help = 0;
    int listen_port = 0;
    const char *ppm = (const char *)0;
    TessJob job;
    TessInventory inv;
    int *parked;

    /* -v before the real parse, so the trace covers the parse itself. */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        }
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    if (verbose) {
        fprintf(stderr, "[rank %d] argc=%d:", rank, argc);
        for (i = 0; i < argc; i++) {
            fprintf(stderr, " %s", argv[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    if (tess_types_check() != 0) {
        fprintf(stderr, "rank %d: integer widths are not what the wire format "
                        "assumes\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    job.epoch = 1;
    job.width = 1024;
    job.height = 768;
    job.max_iter = 1000;
    job.tile = TESS_TILE;
    job.cx = -0.6;
    job.cy = 0.0;
    job.scale = 3.2 / 1024.0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-listen") == 0) {
            listen_port = TESS_DEFAULT_PORT;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                listen_port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            ppm = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            job.width = (tess_u32)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            job.height = (tess_u32)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-max") == 0 && i + 1 < argc) {
            job.max_iter = (tess_u32)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-tile") == 0 && i + 1 < argc) {
            job.tile = (tess_u32)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-cx") == 0 && i + 1 < argc) {
            job.cx = atof(argv[++i]);
        } else if (strcmp(argv[i], "-cy") == 0 && i + 1 < argc) {
            job.cy = atof(argv[++i]);
        } else if (strcmp(argv[i], "-nonce") == 0 && i + 1 < argc) {
            nonce = argv[++i];
        } else if (strcmp(argv[i], "-idle-us") == 0 && i + 1 < argc) {
            idle_us = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-help") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            want_help = 1;
        } else if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc) {
            job.scale = atof(argv[++i]);
        } else {
            /*
             * Warn, never exit. mpirun's colon form can hand a rank arguments
             * this parser has not seen, and exiting here kills rank 0 before
             * MPI_Finalize, leaving every worker spinning on a message that
             * will never arrive. That is a silent cluster-wide hang caused by
             * one unrecognised token. The 2001 tiler ignores unknown arguments
             * entirely, which is why it survives the same treatment.
             */
            if (rank == 0) {
                fprintf(stderr, "tess-node: ignoring unknown argument \"%s\"\n",
                        argv[i]);
                fflush(stderr);
            }
        }
    }

    /* Every rank leaves together: a rank that exits alone strands the rest. */
    if (want_help) {
        if (rank == 0) {
            usage(argv[0]);
        }
        MPI_Finalize();
        return 0;
    }

    if (nranks < 2) {
        if (rank == 0) {
            fprintf(stderr, "tess-node: need at least 2 ranks, master computes "
                            "nothing\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (rank != 0) {
        worker_loop(rank);
        MPI_Finalize();
        return 0;
    }

    /* master */
    parked = (int *)calloc((size_t)nranks, sizeof(int));
    if (!parked) {
        fprintf(stderr, "tess-node: out of memory\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    tess_inventory(&inv);
    printf("tess-node: master on %s, %d rank(s), %d worker(s)\n",
           inv.host, nranks, nranks - 1);
    fflush(stdout);

    if (listen_port > 0) {
        serve_gui(listen_port, nranks, parked);
    } else {
        PpmCtx c;
        double t0;
        int tiles;

        c.job = &job;
        c.iter = (tess_u8 *)calloc((size_t)job.width * job.height *
                                   TESS_BYTES_PER_PX, 1);
        if (!c.iter) {
            fprintf(stderr, "tess-node: out of memory\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        t0 = now_sec();
        tiles = run_epoch(&job, nranks, parked, ppm_sink, (void *)&c);
        printf("tess-node: %d tiles in %.2f s\n", tiles, now_sec() - t0);
        if (ppm) {
            if (write_ppm(ppm, &job, c.iter) != 0) {
                fprintf(stderr, "tess-node: could not write %s\n", ppm);
            } else {
                printf("tess-node: wrote %s\n", ppm);
            }
        }
        free((void *)c.iter);
    }

    stop_workers(nranks, parked);
    free((void *)parked);

    MPI_Finalize();
    return 0;
}
