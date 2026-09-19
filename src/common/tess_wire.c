/*
 * tess_wire.c - big-endian framing. No MPI, no X: the GUI links this too.
 *
 * Doubles go over as their IEEE-754 bit pattern, byte-swapped to big-endian.
 * Both machine types are IEEE-754, so only order differs; this is deliberately
 * not a printf/atof round trip, which would lose the last bits of a zoom.
 *
 * C89 throughout.
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "tess_wire.h"

int tess_types_check(void)
{
    if (sizeof(tess_u8) != 1) return -1;
    if (sizeof(tess_u16) != 2) return -2;
    if (sizeof(tess_u32) != 4) return -3;
    if (sizeof(double) != 8) return -4;
    return 0;
}

int tess_put_u16(tess_u8 *p, tess_u16 v)
{
    p[0] = (tess_u8)((v >> 8) & 0xff);
    p[1] = (tess_u8)(v & 0xff);
    return 2;
}

int tess_put_u32(tess_u8 *p, tess_u32 v)
{
    p[0] = (tess_u8)((v >> 24) & 0xff);
    p[1] = (tess_u8)((v >> 16) & 0xff);
    p[2] = (tess_u8)((v >> 8) & 0xff);
    p[3] = (tess_u8)(v & 0xff);
    return 4;
}

tess_u16 tess_get_u16(const tess_u8 *p)
{
    return (tess_u16)(((tess_u16)p[0] << 8) | (tess_u16)p[1]);
}

tess_u32 tess_get_u32(const tess_u8 *p)
{
    return ((tess_u32)p[0] << 24) | ((tess_u32)p[1] << 16) |
           ((tess_u32)p[2] << 8)  | (tess_u32)p[3];
}

/* The double's own bytes, most significant first. memcpy rather than a union
   or a cast, so no aliasing assumption and no alignment requirement. */
int tess_put_f64(tess_u8 *p, double v)
{
    tess_u8 tmp[8];
    int i, big;
    tess_u32 probe;

    memcpy((char *)tmp, (char *)&v, 8);
    probe = 1;
    big = (*(tess_u8 *)&probe) == 0;
    if (big) {
        memcpy((char *)p, (char *)tmp, 8);
    } else {
        for (i = 0; i < 8; i++) {
            p[i] = tmp[7 - i];
        }
    }
    return 8;
}

double tess_get_f64(const tess_u8 *p)
{
    tess_u8 tmp[8];
    double v;
    int i, big;
    tess_u32 probe;

    probe = 1;
    big = (*(tess_u8 *)&probe) == 0;
    if (big) {
        memcpy((char *)tmp, (char *)p, 8);
    } else {
        for (i = 0; i < 8; i++) {
            tmp[i] = p[7 - i];
        }
    }
    memcpy((char *)&v, (char *)tmp, 8);
    return v;
}

int tess_put_job(tess_u8 *p, const TessJob *j)
{
    int n = 0;

    n += tess_put_u32(p + n, j->epoch);
    n += tess_put_u32(p + n, j->width);
    n += tess_put_u32(p + n, j->height);
    n += tess_put_u32(p + n, j->max_iter);
    n += tess_put_u32(p + n, j->tile);
    n += tess_put_f64(p + n, j->cx);
    n += tess_put_f64(p + n, j->cy);
    n += tess_put_f64(p + n, j->scale);
    return n;
}

int tess_get_job(const tess_u8 *p, TessJob *j)
{
    int n = 0;

    j->epoch    = tess_get_u32(p + n); n += 4;
    j->width    = tess_get_u32(p + n); n += 4;
    j->height   = tess_get_u32(p + n); n += 4;
    j->max_iter = tess_get_u32(p + n); n += 4;
    j->tile     = tess_get_u32(p + n); n += 4;
    j->cx       = tess_get_f64(p + n); n += 8;
    j->cy       = tess_get_f64(p + n); n += 8;
    j->scale    = tess_get_f64(p + n); n += 8;
    return n;
}

int tess_put_tilehdr(tess_u8 *p, const TessTileHdr *t)
{
    int n = 0;

    n += tess_put_u32(p + n, t->epoch);
    n += tess_put_u32(p + n, t->x);
    n += tess_put_u32(p + n, t->y);
    n += tess_put_u32(p + n, t->w);
    n += tess_put_u32(p + n, t->h);
    n += tess_put_u32(p + n, t->rank);
    n += tess_put_u32(p + n, t->usec);
    n += tess_put_u32(p + n, t->step);
    n += tess_put_u32(p + n, t->load);
    return n;
}

int tess_get_tilehdr(const tess_u8 *p, TessTileHdr *t)
{
    int n = 0;

    t->epoch = tess_get_u32(p + n); n += 4;
    t->x     = tess_get_u32(p + n); n += 4;
    t->y     = tess_get_u32(p + n); n += 4;
    t->w     = tess_get_u32(p + n); n += 4;
    t->h     = tess_get_u32(p + n); n += 4;
    t->rank  = tess_get_u32(p + n); n += 4;
    t->usec  = tess_get_u32(p + n); n += 4;
    t->step  = tess_get_u32(p + n); n += 4;
    t->load  = tess_get_u32(p + n); n += 4;
    return n;
}

int tess_read_all(int fd, tess_u8 *buf, int n)
{
    int got = 0;
    int r;

    while (got < n) {
        r = (int)read(fd, (char *)buf + got, (unsigned)(n - got));
        if (r > 0) {
            got += r;
        } else if (r == 0) {
            return -1;                  /* peer closed */
        } else if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

int tess_write_all(int fd, const tess_u8 *buf, int n)
{
    int put = 0;
    int r;

    while (put < n) {
        r = (int)write(fd, (const char *)buf + put, (unsigned)(n - put));
        if (r > 0) {
            put += r;
        } else if (r < 0 && errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

int tess_frame_write(int fd, int type, const tess_u8 *body, int len)
{
    tess_u8 hdr[TESS_FRAME_HDR];
    int n = 0;

    n += tess_put_u32(hdr + n, TESS_MAGIC);
    n += tess_put_u16(hdr + n, (tess_u16)TESS_PROTO_VER);
    n += tess_put_u16(hdr + n, (tess_u16)type);
    n += tess_put_u32(hdr + n, (tess_u32)len);
    if (tess_write_all(fd, hdr, TESS_FRAME_HDR) != 0) {
        return -1;
    }
    if (len > 0 && tess_write_all(fd, body, len) != 0) {
        return -1;
    }
    return 0;
}

int tess_frame_read(int fd, int *type, tess_u8 *buf, int *len)
{
    tess_u8 hdr[TESS_FRAME_HDR];
    tess_u32 magic;
    tess_u16 ver;
    tess_u32 n;

    if (tess_read_all(fd, hdr, TESS_FRAME_HDR) != 0) {
        return -1;
    }
    magic = tess_get_u32(hdr);
    ver   = tess_get_u16(hdr + 4);
    *type = (int)tess_get_u16(hdr + 6);
    n     = tess_get_u32(hdr + 8);

    if (magic != TESS_MAGIC || ver != TESS_PROTO_VER) {
        return -1;
    }
    if (n > (tess_u32)TESS_MAX_FRAME) {
        return -1;
    }
    if (n > 0 && tess_read_all(fd, buf, (int)n) != 0) {
        return -1;
    }
    *len = (int)n;
    return 0;
}
