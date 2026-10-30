/*
 * tess_startcheck.h - the preflight checks, run as tess-ui starts.
 *
 * The same checks as tess-preflight, from the same library, in a window that
 * opens over the render window. All green: it says so and closes itself
 * after a minute, counting down on the Close button.
 * Anything amber or red: it stays, and lists what to do about each one, in
 * the order to do it, with a Recheck button for the fix-and-look loop.
 *
 * This is the cheap half of what PREFLIGHT.md planned as tess-setup plus a
 * light in the panel. The cluster-wide tests are not here: they need every
 * host up, and a powered-off aurora would hold the window open on a timeout.
 *
 * C89, Motif 2.1.
 */

#ifndef TESS_STARTCHECK_H
#define TESS_STARTCHECK_H

#include <Xm/Xm.h>

void tess_startcheck_run(Widget parent, const char *tree, int port,
                         void (*logf)(void *, const char *), void *logctx);

#endif /* TESS_STARTCHECK_H */
