/*
 * sgidefs.h — minimal stand-in for the IRIX header, for the macOS syntax gate only.
 *
 * The real one lives in /usr/include on the SGI and defines the whole _MIPS_*
 * family. Compiling Tess's sources on the Mac against the genuine MPT headers
 * only needs the fixed-width types that mpio.h uses, so this supplies those and
 * nothing else. Replace it with the real sysroot copy when there is one:
 *
 *   rsync -a lucy:/usr/include/ sysroot/usr/include/
 *
 * Never reachable from an IRIX build — the SGI has the real header.
 */
#ifndef __TESS_MACGATE_SGIDEFS_H__
#define __TESS_MACGATE_SGIDEFS_H__

#include <stdint.h>

typedef int8_t   __int8_t_sgi;
typedef int32_t  __int32_t;
typedef uint32_t __uint32_t;
typedef int64_t  __int64_t;
typedef uint64_t __uint64_t;

/* IRIX n32/64 both use 32-bit int; the Mac gate is LP64 little-endian, which is
   why this catches prototype and type errors but never byte-order or pointer-size
   bugs. See doc/DESIGN.md §8. */
#define _MIPS_SZINT  32

#endif
