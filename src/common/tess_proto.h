/*
 * tess_proto.h - the two protocols, as DESIGN.md section 4 specifies them.
 *
 *   GUI <-> master   tagged, length-prefixed, big-endian frames. Versioned,
 *                    so a little-endian dev build and an IRIX master can talk.
 *   master <-> node  native fixed-layout C structs over MPI_BYTE. MPT has no
 *                    heterogeneous clusters, so native layout is safe and free.
 *
 * Tile payload is u16 iteration count + u8 smooth fraction = 3 bytes per pixel.
 * The cluster never sends colour: the GUI shades, so palette changes are local
 * and instant.
 */

#ifndef TESS_PROTO_H
#define TESS_PROTO_H

#include "tess_types.h"

#define TESS_MAGIC        0x54455353u    /* "TESS" */
#define TESS_PROTO_VER    3
#define TESS_DEFAULT_PORT 7333
#define TESS_TILE         64             /* default tile edge, DESIGN.md 3 */
#define TESS_MAX_TILE     256
#define TESS_BYTES_PER_PX 3              /* u16 iters + u8 fraction */

/* GUI -> master */
#define TESS_MSG_HELLO    1              /* client version handshake */
#define TESS_MSG_RENDER   2              /* TessJobWire: start an epoch */
#define TESS_MSG_CANCEL   3              /* abandon the current epoch */
#define TESS_MSG_BYE      4

/* master -> GUI */
#define TESS_MSG_WELCOME  16             /* TessWelcomeWire */
#define TESS_MSG_TILE     17             /* TessTileWire + payload */
#define TESS_MSG_DONE     18             /* TessDoneWire: epoch complete */
#define TESS_MSG_LOG      19             /* NUL-terminated text */
#define TESS_MSG_STATS    20             /* TessTransport: who carried it */

/* The job. Doubles cross the wire as IEEE-754 bit patterns, big-endian, which
   both machine types use natively; no format conversion, only byte order. */
typedef struct TessJob {
    tess_u32 epoch;
    tess_u32 width;
    tess_u32 height;
    tess_u32 max_iter;
    tess_u32 tile;
    double   cx;          /* centre, complex plane */
    double   cy;
    double   scale;       /* units per pixel */
} TessJob;

typedef struct TessTileHdr {
    tess_u32 epoch;
    tess_u32 x;
    tess_u32 y;
    tess_u32 w;            /* area covered, in image pixels */
    tess_u32 h;
    tess_u32 rank;         /* who computed it, for colour-by-owner */
    tess_u32 usec;         /* how long it took that rank */
    tess_u32 step;         /* 1 = every pixel; 8 = the eighth-scale pass */
    tess_u32 load;         /* that host's 1-minute load x100, live telemetry */
} TessTileHdr;

typedef struct TessDone {
    tess_u32 epoch;
    tess_u32 tiles;
    tess_u32 msec;        /* wall time for the epoch, at the master */
} TessDone;

/*
 * Which interconnect actually carried the traffic, in kilobytes, measured
 * rather than assumed. The GM work taught this the hard way: MPT falls back to
 * TCP silently, and a job that quietly ran over ethernet looks exactly like one
 * that used Myrinet.
 */
typedef struct TessTransport {
    tess_u32 kb_tcp;
    tess_u32 kb_gm;
    tess_u32 kb_gsn;
    tess_u32 kb_shmem;
    tess_u32 kb_hippi;
    tess_u32 known;        /* 0: MPT would not tell us */
} TessTransport;

typedef struct TessWelcome {
    tess_u32 version;
    tess_u32 ranks;       /* total in the MPI job, master included */
    tess_u32 workers;
} TessWelcome;

/*
 * master <-> worker, native layout over MPI_BYTE.
 *
 * One request/assign pair per tile, plus a result header followed by
 * w*h*3 bytes. Credit-based prefetch (DESIGN.md 3) is not in v1: a worker
 * asks, computes, returns, asks again.
 */
#define TESS_TAG_REQ    1
#define TESS_TAG_ASSIGN 2
#define TESS_TAG_RESULT 3
#define TESS_TAG_STOP   4

typedef struct TessAssign {
    tess_u32 epoch;
    tess_u32 x;
    tess_u32 y;
    tess_u32 w;
    tess_u32 h;
    tess_u32 step;        /* sample every step-th pixel in both directions */
    tess_u32 width;       /* whole-image geometry, so the worker can map */
    tess_u32 height;
    tess_u32 max_iter;
    double   cx;
    double   cy;
    double   scale;
} TessAssign;

typedef struct TessResultHdr {
    tess_u32 epoch;
    tess_u32 x;
    tess_u32 y;
    tess_u32 w;
    tess_u32 h;
    tess_u32 rank;
    tess_u32 usec;
    tess_u32 step;
    tess_u32 load;        /* sampled at most once a second on the worker */
} TessResultHdr;

/* Samples in a tile, given its coverage and its step. */
#define TESS_SAMPLES(t) ((((t).w + (t).step - 1) / (t).step) * \
                         (((t).h + (t).step - 1) / (t).step))

#endif /* TESS_PROTO_H */
