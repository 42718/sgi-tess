/*
 * tess_types.h - fixed-width integers without <stdint.h>.
 *
 * MIPSpro 7.4.4m is a C89 compiler. IRIX does ship <inttypes.h>, and the
 * baseline used it, but the wire format needs exact widths on three ABIs
 * (IRIX 64-bit big-endian, IRIX n32, macOS arm64 LP64 little-endian) so the
 * sizes are pinned here and checked at startup by tess_types_check().
 */

#ifndef TESS_TYPES_H
#define TESS_TYPES_H

typedef unsigned char  tess_u8;
typedef unsigned short tess_u16;
typedef unsigned int   tess_u32;
typedef signed char    tess_s8;
typedef short          tess_s16;
typedef int            tess_s32;

/* Returns 0 if every width is what the wire format assumes. */
int tess_types_check(void);

#endif /* TESS_TYPES_H */
