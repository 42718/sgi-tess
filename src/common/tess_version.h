/*
 * tess_version.h - which Tess this is.
 *
 * The one place the version is written. Every program answers -version and
 * says it when it starts, and tess-ui puts it in both window titles, so a
 * screenshot or a pasted terminal says which build it came from. The package's
 * inst version is derived from it too.
 *
 * Separate from TESS_PROTO_VER in tess_proto.h, which changes only when the
 * wire format does. Two releases can share a protocol; a protocol change
 * always means a new release.
 *
 * C89.
 */

#ifndef TESS_VERSION_H
#define TESS_VERSION_H

#define TESS_VERSION "1.0-beta"

/* The architecture this was built for, uname -m, from the Makefile. */
#ifndef TESS_ARCH
#define TESS_ARCH "unknown"
#endif

/* "1.0-beta (IP27, built Nov 20 2026)": which release, which of the three
   builds in the shared tree, and how fresh it is. */
#define TESS_VERSION_LINE \
    TESS_VERSION " (" TESS_ARCH ", built " __DATE__ ")"

#endif /* TESS_VERSION_H */
