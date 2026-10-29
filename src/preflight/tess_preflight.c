/*
 * tess_preflight.c - run the preflight checks on this host and print them.
 *
 * The text half of the setup wizard, and the half that keeps working when the
 * thing at fault is X, Motif or MPT itself. It needs nothing but libc, so it
 * builds and runs anywhere, including over arshell on a compute host:
 *
 *     ./build/IP30/tess-preflight
 *     arshell aurora /cluster/dev/sgi-tess/build/IP35/tess-preflight -q
 *
 * Exit status is the worst result, so it drops into a script:
 *   0 all passed, 1 warnings only, 2 something failed.
 *
 * C89 throughout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tess_check.h"

#define MAXRESULTS 64

static void usage(const char *me)
{
    fprintf(stderr,
            "usage: %s [-tree <path>] [-port <n>] [-compute] [-q] [-1]\n"
            "  -tree <path>  the shared tree (default /cluster/dev/sgi-tess)\n"
            "  -port <n>     the master's loopback port (default 7333)\n"
            "  -compute      skip the display-host checks: Motif, X, root, port\n"
            "  -q            one line per check, no remedies\n"
            "  -1            a single summary line, for a caller that wants a light\n",
            me);
}

int main(int argc, char **argv)
{
    TessCheckCtx ctx;
    TessCheckResult r[MAXRESULTS];
    int n;
    int i;
    int quiet;
    int oneline;
    int worst;
    int npass;
    int nwarn;
    int nfail;
    int nskip;
    char *p;
    char *nl;

    tess_check_ctx_default(&ctx);
    quiet = 0;
    oneline = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-tree") == 0 && i + 1 < argc) {
            i++;
            strncpy(ctx.tree, argv[i], sizeof ctx.tree - 1);
            ctx.tree[sizeof ctx.tree - 1] = '\0';
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            i++;
            ctx.port = atoi(argv[i]);
        } else if (strcmp(argv[i], "-compute") == 0) {
            ctx.display = 0;
        } else if (strcmp(argv[i], "-q") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "-1") == 0) {
            oneline = 1;
            quiet = 1;
        } else {
            /* tess-ui ignores unknown switches silently and DEPLOY.md calls
               that out as a trap. Do not repeat it here. */
            usage(argv[0]);
            return 2;
        }
    }

    n = tess_check_run_all(&ctx, r, MAXRESULTS);
    worst = tess_check_worst(r, n);

    npass = nwarn = nfail = nskip = 0;
    for (i = 0; i < n; i++) {
        switch (r[i].status) {
        case TESS_PASS: npass++; break;
        case TESS_WARN: nwarn++; break;
        case TESS_FAIL: nfail++; break;
        default:        nskip++; break;
        }
    }

    if (oneline) {
        printf("tess-preflight %d %s pass=%d warn=%d fail=%d skip=%d\n",
               TESS_CHECK_FORMAT, tess_check_status_name(worst),
               npass, nwarn, nfail, nskip);
        fflush(stdout);
        return worst == TESS_FAIL ? 2 : (worst == TESS_WARN ? 1 : 0);
    }

    for (i = 0; i < n; i++) {
        printf("%-4s  %-12s %s\n", tess_check_status_name(r[i].status),
               r[i].id, r[i].detail);
        if (!quiet && r[i].remedy[0] != '\0') {
            /* Indent every line of the remedy so a multi-line snippet stays
               visually attached to the check that produced it. */
            printf("        remedy:\n");
            p = r[i].remedy;
            while (*p) {
                nl = strchr(p, '\n');
                if (nl) {
                    *nl = '\0';
                }
                printf("        %s\n", p);
                if (!nl) {
                    break;
                }
                p = nl + 1;
            }
            printf("\n");
        }
    }

    printf("\n%d checks: %d pass, %d warn, %d fail, %d skipped. Verdict %s.\n",
           n, npass, nwarn, nfail, nskip, tess_check_status_name(worst));

    if (nfail > 0) {
        printf("\nFix them in the order listed: a failure early makes the\n");
        printf("checks after it meaningless, which is what SKIP means here.\n");
    }

    fflush(stdout);
    return worst == TESS_FAIL ? 2 : (worst == TESS_WARN ? 1 : 0);
}
