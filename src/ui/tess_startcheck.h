/*
 * tess_startcheck.h - the preflight checks, run as tess-ui starts.
 *
 * The same checks as tess-preflight, from the same library, in a window that
 * opens over the render window. All green: it says so and closes itself
 * after a minute, counting down on the Close button.
 * Anything amber or red: it stays, and lists what to do about each one, in
 * the order to do it, with a Recheck button for the fix-and-look loop.
 *
 * Then each host in hostlist, all at once and in the background: answering
 * through Array Services without stray output, the shared tree there, a
 * build of this version for its architecture, its clock against the share,
 * and no tess-node left running. A host that does not answer in time is a
 * SKIP, so a powered-off machine never holds the window open.
 *
 * C89, Motif 2.1.
 */

#ifndef TESS_STARTCHECK_H
#define TESS_STARTCHECK_H

#include <Xm/Xm.h>

void tess_startcheck_run(Widget parent, const char *tree, int port,
                         const char *hostlist,
                         void (*logf)(void *, const char *), void *logctx);

#endif /* TESS_STARTCHECK_H */
