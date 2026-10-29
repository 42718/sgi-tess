/*
 * tess_check.h - the preflight checks, one implementation, three callers.
 *
 * doc/DEPLOY.md section 1 lists what Tess needs and section 7 lists how it
 * fails. Both were written from the machines, and both were still only prose:
 * someone bringing Tess up elsewhere had to read them and check by hand, and
 * the first report back was that the UI never reached the backend. These are
 * those two sections made executable.
 *
 * Three callers, same checks, for the same reason tess_inventory.h gives:
 * asking the question more than one way is how the answers drift.
 *
 *   tess-preflight  prints them on any host, and over arshell on every host
 *   tess-setup      the Motif wizard, which shows them and offers the remedy
 *   tess-ui         a RAG indicator over the cheap subset
 *
 * No X and no MPI in here, deliberately. A machine whose Motif or MPT is the
 * fault still has to be able to run the checks that say so.
 *
 * C89. Builds on macOS too, so the portability gate keeps working.
 */

#ifndef TESS_CHECK_H
#define TESS_CHECK_H

#define TESS_CHECK_FORMAT     1   /* bump when a check's meaning changes */
#define TESS_CHECK_IDLEN      16
#define TESS_CHECK_TITLELEN   40
#define TESS_CHECK_DETAILLEN  192
#define TESS_CHECK_REMEDYLEN  640

/* Worst-first ordering matters: tess_check_worst() relies on it. */
#define TESS_PASS  0
#define TESS_WARN  1
#define TESS_FAIL  2
#define TESS_SKIP  3

/* Where a check is allowed to run. */
#define TESS_SCOPE_ANY      1   /* every host, including over arshell */
#define TESS_SCOPE_DISPLAY  2   /* the display host only */

typedef struct TessCheckCtx {
    char tree[256];     /* the shared tree, as this host sees it */
    int  port;          /* the master's loopback port */
    int  display;       /* 1: this is the display host, run those checks too */
} TessCheckCtx;

typedef struct TessCheckResult {
    char id[TESS_CHECK_IDLEN];
    char title[TESS_CHECK_TITLELEN];
    int  status;
    char detail[TESS_CHECK_DETAILLEN];
    char remedy[TESS_CHECK_REMEDYLEN];   /* empty when status is TESS_PASS */
} TessCheckResult;

/* tree=/cluster/dev/sgi-tess, port=7333, display=1. Matches tess-ui. */
void tess_check_ctx_default(TessCheckCtx *ctx);

int         tess_check_count(void);
const char *tess_check_id_at(int i);
const char *tess_check_title_at(int i);
int         tess_check_scope_at(int i);

/*
 * Run check i. The results already gathered are passed in so a check whose
 * prerequisite failed reports TESS_SKIP rather than a second, misleading
 * failure: "mpirun across the array" failing tells you nothing useful when
 * arrayd is not running.
 *
 * Checks are ordered so that a prerequisite always precedes its dependants,
 * which is also the order a person should fix them in.
 */
void tess_check_run_one(int i, const TessCheckCtx *ctx,
                        const TessCheckResult *done, int ndone,
                        TessCheckResult *out);

/* Run them all in order. Returns how many results were written. */
int tess_check_run_all(const TessCheckCtx *ctx, TessCheckResult *out, int max);

const char *tess_check_status_name(int status);

/* The worst status present, for a single RAG light. SKIP counts as FAIL's
   shadow and does not improve the verdict. */
int tess_check_worst(const TessCheckResult *r, int n);

#endif /* TESS_CHECK_H */
