/*
 * tess_wire.h - big-endian framing between the GUI and the master.
 *
 * Frame: magic u32, version u16, type u16, length u32, then length bytes.
 * Everything multi-byte is big-endian on the wire, so an IRIX master and a
 * little-endian development build interoperate. DESIGN.md section 4.
 */

#ifndef TESS_WIRE_H
#define TESS_WIRE_H

#include "tess_types.h"
#include "tess_proto.h"

#define TESS_FRAME_HDR   12
#define TESS_MAX_FRAME   (TESS_MAX_TILE * TESS_MAX_TILE * TESS_BYTES_PER_PX + 256)

/* Scalars in and out of a byte buffer. Each returns bytes consumed/written. */
int tess_put_u16(tess_u8 *p, tess_u16 v);
int tess_put_u32(tess_u8 *p, tess_u32 v);
int tess_put_f64(tess_u8 *p, double v);
tess_u16 tess_get_u16(const tess_u8 *p);
tess_u32 tess_get_u32(const tess_u8 *p);
double   tess_get_f64(const tess_u8 *p);

/* Structs. Return bytes used. */
int tess_put_job(tess_u8 *p, const TessJob *j);
int tess_get_job(const tess_u8 *p, TessJob *j);
int tess_put_tilehdr(tess_u8 *p, const TessTileHdr *t);
int tess_get_tilehdr(const tess_u8 *p, TessTileHdr *t);

#define TESS_JOB_WIRE      44
#define TESS_TILEHDR_WIRE  36

/*
 * Blocking frame I/O on a socket. Return 0 on success, -1 on error or EOF.
 * tess_frame_read fills type and len, and reads len bytes into buf, which must
 * hold TESS_MAX_FRAME. A frame larger than that is an error, not a truncation.
 */
int tess_frame_write(int fd, int type, const tess_u8 *body, int len);
int tess_frame_read(int fd, int *type, tess_u8 *buf, int *len);

/* Read exactly n bytes, or return -1. Handles short reads and EINTR. */
int tess_read_all(int fd, tess_u8 *buf, int n);
int tess_write_all(int fd, const tess_u8 *buf, int n);

#endif /* TESS_WIRE_H */
