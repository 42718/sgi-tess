/*
 * tess_check.c - doc/DEPLOY.md sections 1 and 7, made executable.
 *
 * Every check here corresponds to a line in DEPLOY.md: a prerequisite from
 * section 1, or a symptom from the "When it does not work" table in section 7.
 * The remedy text is the fix from that table, in a form that can be pasted.
 *
 * Rules this file keeps to:
 *
 *   - No X, no MPI, no Motif. A machine whose Motif or MPT is broken must
 *     still be able to run the thing that says so.
 *   - Nothing here writes configuration. It reports, and it prints what you
 *     would type. tess_cluster.h makes the same promise about arrayd.conf.
 *   - One side effect only: the clock check writes and removes a temp file
 *     under the tree, because detecting NFS clock skew requires touching the
 *     filesystem whose clock is in question.
 *   - Inventory comes from tess_inventory(), never from a second code path.
 *
 * C89 throughout. The IRIX-only parts compile out on macOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tess_check.h"
#include "tess_inventory.h"

/* ------------------------------------------------------------------ */
/* small helpers                                                        */
/* ------------------------------------------------------------------ */

static void setr(TessCheckResult *r, int status, const char *detail,
                 const char *remedy)
{
    r->status = status;
    if (detail) {
        strncpy(r->detail, detail, TESS_CHECK_DETAILLEN - 1);
        r->detail[TESS_CHECK_DETAILLEN - 1] = '\0';
    }
    if (remedy) {
        strncpy(r->remedy, remedy, TESS_CHECK_REMEDYLEN - 1);
        r->remedy[TESS_CHECK_REMEDYLEN - 1] = '\0';
    }
}

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static int path_is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static long path_mtime(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_mtime;
}

/*
 * Run a command and capture its first len-1 bytes of stdout and stderr.
 * Returns the exit status, or -1 if the command could not be started.
 * Trailing newlines are trimmed so the text drops straight into a detail line.
 */
static int run_capture(const char *cmd, char *buf, int len)
{
    FILE *f;
    int got;
    int rc;

    buf[0] = '\0';
    f = popen(cmd, "r");
    if (!f) {
        return -1;
    }
    got = (int)fread(buf, 1, (size_t)(len - 1), f);
    if (got < 0) {
        got = 0;
    }
    buf[got] = '\0';
    rc = pclose(f);
    while (got > 0 && (buf[got - 1] == '\n' || buf[got - 1] == '\r')) {
        buf[--got] = '\0';
    }
    return rc;
}

/* The first line of a captured block, in place. */
static void first_line(char *s)
{
    char *nl;
    nl = strchr(s, '\n');
    if (nl) {
        *nl = '\0';
    }
}

/* Look for name on PATH. Returns 1 and fills out when found. */
static int find_in_path(const char *name, char *out, int len)
{
    const char *p;
    const char *q;
    char cand[512];
    int n;

    p = getenv("PATH");
    if (!p) {
        return 0;
    }
    while (*p) {
        q = strchr(p, ':');
        if (!q) {
            q = p + strlen(p);
        }
        n = (int)(q - p);
        if (n > 0 && n < (int)sizeof cand - 2 - (int)strlen(name)) {
            memcpy(cand, p, (size_t)n);
            cand[n] = '/';
            strcpy(cand + n + 1, name);
            if (access(cand, X_OK) == 0) {
                strncpy(out, cand, (size_t)(len - 1));
                out[len - 1] = '\0';
                return 1;
            }
        }
        p = (*q) ? q + 1 : q;
    }
    return 0;
}

/*
 * The newest mtime among *.c and *.h beneath dir, one level of subdirectories
 * deep, which is exactly how src/ is laid out. Returns -1 if nothing is found.
 */
static long newest_source(const char *dir, int depth)
{
    DIR *d;
    struct dirent *e;
    char path[512];
    long newest;
    long t;
    size_t n;

    newest = -1;
    d = opendir(dir);
    if (!d) {
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') {
            continue;
        }
        if (strlen(dir) + strlen(e->d_name) + 2 >= sizeof path) {
            continue;
        }
        sprintf(path, "%s/%s", dir, e->d_name);
        if (path_is_dir(path)) {
            if (depth > 0) {
                t = newest_source(path, depth - 1);
                if (t > newest) {
                    newest = t;
                }
            }
            continue;
        }
        n = strlen(e->d_name);
        if (n > 2 && e->d_name[n - 2] == '.' &&
            (e->d_name[n - 1] == 'c' || e->d_name[n - 1] == 'h')) {
            t = path_mtime(path);
            if (t > newest) {
                newest = t;
            }
        }
    }
    closedir(d);
    return newest;
}

/* This host's architecture, as the build directory spells it. */
static void this_arch(char *out, int len)
{
    struct utsname u;

    out[0] = '\0';
    if (uname(&u) == 0) {
        strncpy(out, u.machine, (size_t)(len - 1));
        out[len - 1] = '\0';
    }
}

/* Did an earlier check with this id fail or get skipped? */
static int blocked_by(const TessCheckResult *done, int ndone, const char *id)
{
    int i;
    for (i = 0; i < ndone; i++) {
        if (strcmp(done[i].id, id) == 0) {
            if (done[i].status == TESS_FAIL || done[i].status == TESS_SKIP) {
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* the checks                                                           */
/* ------------------------------------------------------------------ */

/*
 * DEPLOY.md section 1: "Every machine must be 64-bit MIPS, R10000-class or
 * later", because DESIGN.md section 8 requires identical machine code so that
 * tiles computed on different hosts land on the same escape-time boundaries.
 * -mips4 is R8000, R10000, R12000, R14000, R16000. An R4400 or R5000 cannot
 * run these binaries, and building -mips3 would break the guarantee rather
 * than merely run slower.
 */
static void chk_arch(const TessCheckCtx *ctx, TessCheckResult *r)
{
    char arch[32];
    char detail[TESS_CHECK_DETAILLEN];

    (void)ctx;
    this_arch(arch, (int)sizeof arch);
    if (arch[0] == '\0') {
        setr(r, TESS_FAIL, "uname(2) gave no machine type", "");
        return;
    }
    if (strcmp(arch, "IP27") == 0 || strcmp(arch, "IP30") == 0 ||
        strcmp(arch, "IP35") == 0) {
        sprintf(detail, "%s, a tested machine type", arch);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    if (arch[0] == 'I' && arch[1] == 'P') {
        sprintf(detail, "%s, an untested IRIX machine type", arch);
        setr(r, TESS_WARN, detail,
             "Tess is verified on IP27, IP30 and IP35, all R10000-class.\n"
             "An older IP is likely R4400 or R5000, which -mips4 binaries\n"
             "cannot run. Check with: hinv -c processor\n"
             "Do not build -mips3 to make it fit: DESIGN.md section 8 needs\n"
             "identical machine code on every host.");
        return;
    }
    sprintf(detail, "%s, not IRIX", arch);
    setr(r, TESS_WARN, detail,
         "This is the portability gate, not a target. The MPI and Motif\n"
         "checks below will not mean anything here. Run `gmake gate` to\n"
         "verify the portable sources, and run this tool on the SGIs.");
}

/* DEPLOY.md section 1: IRIX 6.5.30, and 6.5.x should be fine. */
static void chk_irix(const TessCheckCtx *ctx, TessCheckResult *r)
{
    struct utsname u;
    char detail[TESS_CHECK_DETAILLEN];

    (void)ctx;
    if (uname(&u) != 0) {
        setr(r, TESS_FAIL, "uname(2) failed", "");
        return;
    }
    if (strcmp(u.sysname, "IRIX64") != 0 && strcmp(u.sysname, "IRIX") != 0) {
        sprintf(detail, "%s %s", u.sysname, u.release);
        setr(r, TESS_WARN, detail, "");
        return;
    }
    /* utsname.release is "6.5" on every 6.5.x machine; the patch level comes
       from `uname -R`, which is a command and not a call. See
       tess_inventory.c, which documents the same limitation. */
    if (strncmp(u.release, "6.5", 3) == 0) {
        sprintf(detail, "%s %s", u.sysname, u.release);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    sprintf(detail, "%s %s, expected 6.5.x", u.sysname, u.release);
    setr(r, TESS_WARN, detail,
         "The statistics calls Tess uses are 6.5 APIs. Earlier IRIX may\n"
         "compile and report zeroes. See doc/PLATFORM-FACTS.md.");
}

/* The binary running this check has to be the 64-bit one. */
static void chk_abi(const TessCheckCtx *ctx, TessCheckResult *r)
{
    (void)ctx;
    if (sizeof(void *) == 8) {
        setr(r, TESS_PASS, "this binary is 64-bit", "");
    } else {
        setr(r, TESS_FAIL, "this binary is 32-bit",
             "The build is -64 -mips4 and MPT's 64-bit libraries are the\n"
             "ones Tess links. Rebuild with: gmake fresh\n"
             "If that still produces a 32-bit object, check that CFLAGS in\n"
             "your environment is not overriding the Makefile's ABI.");
    }
}

/* DEPLOY.md section 1: MIPSpro 7.4.4m, cc and not gcc. */
static void chk_cc(const TessCheckCtx *ctx, TessCheckResult *r)
{
    char where[512];
    char out[512];
    char detail[TESS_CHECK_DETAILLEN];

    (void)ctx;
    if (!find_in_path("cc", where, (int)sizeof where)) {
        setr(r, TESS_FAIL, "no cc on PATH",
             "MIPSpro is needed, not gcc: the build is -64 -mips4 and gcc\n"
             "on IRIX cannot produce it. Install the MIPSpro 7.4 compilers\n"
             "and check PATH includes /usr/bin.");
        return;
    }
    run_capture("cc -version 2>&1", out, (int)sizeof out);
    first_line(out);
    if (strstr(out, "MIPSpro") != NULL) {
        strncpy(detail, out, TESS_CHECK_DETAILLEN - 1);
        detail[TESS_CHECK_DETAILLEN - 1] = '\0';
        setr(r, TESS_PASS, detail, "");
        return;
    }
    if (out[0] != '\0') {
        sprintf(detail, "cc at %s does not identify as MIPSpro", where);
        setr(r, TESS_WARN, detail,
             "`cc -version` did not mention MIPSpro. If cc is gcc, the\n"
             "-64 -mips4 build will fail. Verify with: cc -version\n"
             "Tess was built with MIPSpro 7.4.4m.");
        return;
    }
    sprintf(detail, "cc at %s, version not reported", where);
    setr(r, TESS_WARN, detail, "");
}

/* DEPLOY.md section 1: GNU make, as gmake. The Makefile uses :=, ifeq, VPATH. */
static void chk_gmake(const TessCheckCtx *ctx, TessCheckResult *r)
{
    char where[512];
    char out[512];
    char detail[TESS_CHECK_DETAILLEN];

    (void)ctx;
    if (!find_in_path("gmake", where, (int)sizeof where)) {
        setr(r, TESS_FAIL, "no gmake on PATH",
             "The Makefile uses :=, ifeq and VPATH, which IRIX make does not\n"
             "understand. Install GNU make. On IRIX it is usually\n"
             "/usr/freeware/bin/gmake or /usr/sbin/gmake; add that to PATH.");
        return;
    }
    run_capture("gmake --version 2>&1", out, (int)sizeof out);
    first_line(out);
    if (strstr(out, "GNU Make") != NULL) {
        sprintf(detail, "%s", out);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    sprintf(detail, "gmake at %s is not GNU make", where);
    setr(r, TESS_FAIL, detail,
         "Something called gmake exists but is not GNU make. The Makefile\n"
         "needs the real thing.");
}

/* DEPLOY.md section 1: MPT 1.9, needs libmpi and mpirun on PATH. */
static void chk_mpirun(const TessCheckCtx *ctx, TessCheckResult *r)
{
    char where[512];
    char detail[TESS_CHECK_DETAILLEN];

    (void)ctx;
    if (!find_in_path("mpirun", where, (int)sizeof where)) {
        setr(r, TESS_FAIL, "no mpirun on PATH",
             "SGI MPT is not installed, or /usr/bin is not on PATH.\n"
             "MPT ships on the IRIX 6.5 Applications media as mpt.sw64.\n"
             "tess-ui forks mpirun directly, so PATH must carry it for the\n"
             "user that runs the UI, which is root.");
        return;
    }
    sprintf(detail, "%s", where);
    setr(r, TESS_PASS, detail, "");
}

static void chk_libmpi(const TessCheckCtx *ctx, TessCheckResult *r)
{
    (void)ctx;
    if (path_exists("/usr/lib64/libmpi.so")) {
        setr(r, TESS_PASS, "/usr/lib64/libmpi.so", "");
        return;
    }
    if (path_exists("/usr/lib32/libmpi.so") || path_exists("/usr/lib/libmpi.so")) {
        setr(r, TESS_FAIL, "libmpi present, but not the 64-bit one",
             "Tess is a -64 build and links /usr/lib64/libmpi.so.\n"
             "Install the 64-bit MPT subsystem (mpt.sw64).");
        return;
    }
    setr(r, TESS_FAIL, "no libmpi found",
         "Install SGI MPT 1.9 (mpt.sw64 and mpt.sw.mpi) from the IRIX 6.5\n"
         "Applications media. Verify with:\n"
         "    versions -b mpt\n"
         "    ls /usr/lib64/libmpi.so");
}

/*
 * Optional. The Makefile turns transport counters off when the header is
 * absent, so this is a WARN and never a FAIL.
 */
static void chk_mpiext(const TessCheckCtx *ctx, TessCheckResult *r,
                       const TessCheckResult *done, int ndone)
{
    (void)ctx;
    if (blocked_by(done, ndone, "libmpi")) {
        setr(r, TESS_SKIP, "blocked by libmpi", "");
        return;
    }
    if (path_exists("/usr/include/mpi_ext.h")) {
        setr(r, TESS_PASS, "/usr/include/mpi_ext.h", "");
        return;
    }
    setr(r, TESS_WARN, "no mpi_ext.h: transport counters disabled",
         "MPI_SGI_stat_get lives in mpi_ext.h. Without it the build sets\n"
         "TESS_HAVE_MPI_EXT=0 and the UI cannot show which interconnect the\n"
         "job actually used. Everything else works. To build explicitly\n"
         "without it:\n"
         "    gmake fresh TESS_MPI_STAT=0");
}

/*
 * DEPLOY.md section 7: "Every host shows no answer" means arshell is not
 * running as root or arrayd is not configured with these hosts. Array Services
 * is how MPT launches, so this precedes every multi-host check.
 */
static void chk_arrayd(const TessCheckCtx *ctx, TessCheckResult *r)
{
    char out[1024];
    char detail[TESS_CHECK_DETAILLEN];
    int rc;
    int lines;
    char *p;

    (void)ctx;
    rc = run_capture("ainfo machines 2>&1", out, (int)sizeof out);
    if (rc != 0 || out[0] == '\0') {
        first_line(out);
        if (out[0] != '\0') {
            sprintf(detail, "ainfo: %.150s", out);
        } else {
            strcpy(detail, "ainfo machines gave nothing");
        }
        setr(r, TESS_FAIL, detail,
             "Array Services is not answering. MPT launches through arrayd,\n"
             "not rsh, so nothing multi-host will work until it does:\n"
             "    chkconfig array on\n"
             "    /etc/init.d/array start\n"
             "    ainfo machines\n"
             "Every host in the job must appear in /usr/lib/array/arrayd.conf\n"
             "on every host, and the names there must match the names you\n"
             "pass to -hosts. See doc/MPI-HELLO.md.");
        return;
    }
    lines = 0;
    for (p = out; *p; p++) {
        if (*p == '\n') {
            lines++;
        }
    }
    sprintf(detail, "ainfo machines answered, %d line(s)", lines + 1);
    setr(r, TESS_PASS, detail, "");
}

/* DEPLOY.md section 2: one shared checkout, same path on every machine. */
static void chk_tree(const TessCheckCtx *ctx, TessCheckResult *r)
{
    char path[512];
    char detail[TESS_CHECK_DETAILLEN];

    if (!path_is_dir(ctx->tree)) {
        sprintf(detail, "%.150s is not a directory", ctx->tree);
        setr(r, TESS_FAIL, detail,
             "The shared tree is not where this host expects it. It must be\n"
             "mounted at the SAME path on every machine, because mpirun is\n"
             "given one binary path per host group and the UI builds those\n"
             "paths from -tree. Mount the share and pass -tree if it differs:\n"
             "    tess-preflight -tree /your/path\n"
             "doc/NFS-SERVER.md covers the share and the UID agreement.");
        return;
    }
    sprintf(path, "%s/Makefile", ctx->tree);
    if (!path_exists(path)) {
        sprintf(detail, "%.140s has no Makefile", ctx->tree);
        setr(r, TESS_FAIL, detail,
             "That directory exists but is not a Tess checkout. Clone into\n"
             "the share so every SGI sees it at the same path:\n"
             "    git clone git@github.com:42718/sgi-tess.git <share>/dev/sgi-tess");
        return;
    }
    sprintf(detail, "%.150s", ctx->tree);
    setr(r, TESS_PASS, detail, "");
}

/*
 * DEPLOY.md section 3: output goes to build/$(uname -m), and building on lucy
 * leaves aurora's binaries stale. An IP35 binary runs on lucy because both are
 * -64 -mips4, so a missing build does not announce itself.
 */
static void chk_build(const TessCheckCtx *ctx, TessCheckResult *r,
                      const TessCheckResult *done, int ndone)
{
    char arch[32];
    char path[512];
    char detail[TESS_CHECK_DETAILLEN];
    int have_node;
    int have_ui;
    int have_probe;

    if (blocked_by(done, ndone, "tree")) {
        setr(r, TESS_SKIP, "blocked by tree", "");
        return;
    }
    this_arch(arch, (int)sizeof arch);
    sprintf(path, "%s/build/%s/tess-node", ctx->tree, arch);
    have_node = path_exists(path);
    sprintf(path, "%s/build/%s/tess-probe", ctx->tree, arch);
    have_probe = path_exists(path);
    sprintf(path, "%s/build/%s/tess-ui", ctx->tree, arch);
    have_ui = path_exists(path);

    if (have_node && have_probe && (have_ui || !ctx->display)) {
        sprintf(detail, "build/%s has what this host needs", arch);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    sprintf(detail, "build/%s: node %s, probe %s, ui %s", arch,
            have_node ? "yes" : "NO", have_probe ? "yes" : "NO",
            have_ui ? "yes" : "NO");
    setr(r, TESS_FAIL, detail,
         "This machine has not built its own binaries. Every machine builds\n"
         "natively from the shared tree, into build/`uname -m`, so one\n"
         "machine building is not deployment:\n"
         "    cd <tree> && gmake fresh\n"
         "Run that on every host in the job. A binary from another host of\n"
         "the same ABI will run and silently be the wrong version.");
}

/*
 * DEPLOY.md section 3 and 7, and the costliest trap in the project: the SGI
 * clocks run ahead of the Mac's, so a file written over NFS arrives looking
 * older than the binary built from the previous version. make reports success
 * having done nothing and you debug code that was never compiled.
 */
static void chk_stale(const TessCheckCtx *ctx, TessCheckResult *r,
                      const TessCheckResult *done, int ndone)
{
    char arch[32];
    char path[512];
    char src[512];
    char detail[TESS_CHECK_DETAILLEN];
    long newest;
    long built;

    if (blocked_by(done, ndone, "build")) {
        setr(r, TESS_SKIP, "blocked by build", "");
        return;
    }
    this_arch(arch, (int)sizeof arch);
    sprintf(src, "%s/src", ctx->tree);
    newest = newest_source(src, 2);
    sprintf(path, "%s/build/%s/tess-node", ctx->tree, arch);
    built = path_mtime(path);

    if (newest < 0 || built < 0) {
        setr(r, TESS_WARN, "could not compare source and binary times", "");
        return;
    }
    if (built >= newest) {
        sprintf(detail, "tess-node is %ld s newer than the newest source",
                built - newest);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    sprintf(detail, "tess-node is %ld s OLDER than the newest source",
            newest - built);
    setr(r, TESS_FAIL, detail,
         "The binary predates the source, so this machine is running code\n"
         "that is not what the tree says. Usually NFS clock skew after an\n"
         "edit from another machine:\n"
         "    cd <tree> && gmake fresh\n"
         "`fresh` cleans only this architecture and rebuilds it. Do not\n"
         "trust a plain `gmake` here: it will report success having done\n"
         "nothing. Unpack any archive with `tar xmf`, which restamps with\n"
         "the local clock.");
}

/*
 * The clock skew itself, measured rather than inferred. Writes a file into the
 * tree and compares the mtime the filesystem gives it with this host's clock.
 * This is the one check with a side effect, and it is removed again.
 */
static void chk_clock(const TessCheckCtx *ctx, TessCheckResult *r,
                      const TessCheckResult *done, int ndone)
{
    char path[512];
    char detail[TESS_CHECK_DETAILLEN];
    FILE *f;
    long now;
    long stamp;
    long skew;

    if (blocked_by(done, ndone, "tree")) {
        setr(r, TESS_SKIP, "blocked by tree", "");
        return;
    }
    sprintf(path, "%s/.tess-clock-probe", ctx->tree);
    f = fopen(path, "w");
    if (!f) {
        sprintf(detail, "cannot write %.140s", path);
        setr(r, TESS_WARN, detail,
             "The tree is not writable by this user, so clock skew cannot be\n"
             "measured here. The build needs to write build/ anyway, so this\n"
             "will matter: check the share's permissions and the UID\n"
             "agreement in doc/NFS-SERVER.md.");
        return;
    }
    fputs("tess clock probe\n", f);
    fclose(f);
    now = (long)time((time_t *)0);
    stamp = path_mtime(path);
    remove(path);

    if (stamp < 0) {
        setr(r, TESS_WARN, "could not stat the probe file", "");
        return;
    }
    /* Signed, deliberately. Which way round it is decides whether the trap is
       benign or not: a tree clock BEHIND this host means files written from
       elsewhere arrive looking older than binaries built here, and make skips
       them silently. Ahead is merely untidy. Throwing the sign away hides the
       difference, which an earlier version of this check did. */
    skew = stamp - now;
    /* 2 s, not 0: time() and the mtime are both whole seconds, and the stat
       follows the write, so a disciplined clock can still read 1 s out. lucy
       on timeslave reads 0 s; it read 51 s before, and the old 30 s threshold
       would have passed half that. */
    if (skew <= 2 && skew >= -2) {
        sprintf(detail, "tree and local clock agree within %ld s", labs(skew));
        setr(r, TESS_PASS, detail, "");
        return;
    }
    if (skew > 0) {
        sprintf(detail, "tree clock is %ld s AHEAD of this host", skew);
    } else {
        sprintf(detail, "tree clock is %ld s BEHIND this host", -skew);
    }
    setr(r, TESS_WARN, detail,
         "The filesystem holding the tree disagrees with this host's clock.\n"
         "That is what makes `make` skip edited files and report success.\n"
         "Use `gmake fresh` on this tree until it is fixed. The fix is\n"
         "timeslave against the file server, which is the time master:\n"
         "doc/PLATFORM-FACTS.md, \"Clock synchronisation\".\n"
         "This is a warning, not a failure: `fresh` works around it.");
}

/*
 * DEPLOY.md section 7: "A host shows no CPU percentage" means its tess-probe
 * is stale and predates the busy= field. Running the probe is also the
 * cheapest proof that this host's binaries actually execute.
 */
static void chk_probe(const TessCheckCtx *ctx, TessCheckResult *r,
                      const TessCheckResult *done, int ndone)
{
    char arch[32];
    char cmd[640];
    char out[2048];
    char head[512];
    char detail[TESS_CHECK_DETAILLEN];

    if (blocked_by(done, ndone, "build")) {
        setr(r, TESS_SKIP, "blocked by build", "");
        return;
    }
    this_arch(arch, (int)sizeof arch);
    /* -v, because the `via` line that says which memory call answered exists
       only in the verbose block. The machine-readable first line stops at
       hippi= and carries no memwhy field: see tess_inventory_line(). */
    sprintf(cmd, "%s/build/%s/tess-probe -v 2>&1", ctx->tree, arch);
    run_capture(cmd, out, (int)sizeof out);
    strncpy(head, out, sizeof head - 1);
    head[sizeof head - 1] = '\0';
    first_line(head);

    if (strstr(head, "tess-probe") == NULL) {
        sprintf(detail, "probe did not report: %.140s", head);
        setr(r, TESS_FAIL, detail,
             "This host's tess-probe did not produce its inventory line.\n"
             "Rebuild it:  cd <tree> && gmake fresh\n"
             "If it still fails, run it directly to see the error.");
        return;
    }
    if (strstr(head, "busy=") == NULL) {
        setr(r, TESS_WARN, "probe runs but predates the busy= field",
             "The UI will show this host with no CPU percentage. Rebuild:\n"
             "    cd <tree> && gmake fresh");
        return;
    }
    /* The via line distinguishes the documented path from the fallback, which
       reads zero for an unprivileged caller. See DEPLOY.md section 4. */
    if (strstr(out, "sysget(SGT_RMINFO) ok") == NULL) {
        setr(r, TESS_WARN, "probe fell back off sysget(SGT_RMINFO)",
             "Memory figures came from the sysmp(MP_SAGET) fallback, which\n"
             "reports zero for an ordinary user. Run `tess-probe -v` and read\n"
             "the `via` line. doc/PLATFORM-FACTS.md records both paths.");
        return;
    }
    sprintf(detail, "%.150s", head);
    setr(r, TESS_PASS, detail, "");
}

/*
 * DEPLOY.md section 4: "arshell needs root here, which is why the UI does
 * too." Run as an ordinary user it returns nothing and every host shows
 * "no answer" - which is precisely the symptom reported from outside.
 */
static void chk_root(const TessCheckCtx *ctx, TessCheckResult *r)
{
    (void)ctx;
    if (geteuid() == 0) {
        setr(r, TESS_PASS, "running as root", "");
        return;
    }
    setr(r, TESS_FAIL, "not root",
         "arshell needs root on these machines, and the UI discovers the\n"
         "cluster through arshell, so tess-ui must run as root. As an\n"
         "ordinary user every host reports \"no answer\" and the UI never\n"
         "reaches the backend:\n"
         "    su\n"
         "    cd <tree> && ./build/`uname -m`/tess-ui");
}

/* DEPLOY.md section 1: Motif 2.1 under /usr/Motif-2.1, display host only. */
static void chk_motif(const TessCheckCtx *ctx, TessCheckResult *r)
{
    int lib;
    int inc;

    (void)ctx;
    lib = path_exists("/usr/Motif-2.1/lib64/libXm.so");
    inc = path_exists("/usr/Motif-2.1/include/Xm/Xm.h");
    if (lib && inc) {
        setr(r, TESS_PASS, "/usr/Motif-2.1, headers and lib64", "");
        return;
    }
    if (lib || inc) {
        setr(r, TESS_FAIL,
             lib ? "libXm.so present, headers missing"
                 : "Motif headers present, lib64 missing",
             "The UI needs both. The Makefile looks in\n"
             "/usr/Motif-2.1/include and /usr/Motif-2.1/lib64.\n"
             "Install the Motif 2.1 development subsystem.");
        return;
    }
    setr(r, TESS_FAIL, "no Motif 2.1 at /usr/Motif-2.1",
         "tess-ui is Motif 2.1 with libSgm. Install it, or point the build\n"
         "elsewhere:\n"
         "    gmake ui MOTIF=/path/to/motif\n"
         "Only the display host needs it. Compute hosts build tess-node,\n"
         "which has no Motif at all.");
}

/* DEPLOY.md section 1: a running X server, and DISPLAY set. */
static void chk_display(const TessCheckCtx *ctx, TessCheckResult *r)
{
    const char *d;
    char detail[TESS_CHECK_DETAILLEN];

    (void)ctx;
    d = getenv("DISPLAY");
    if (d && *d) {
        sprintf(detail, "DISPLAY=%.140s", d);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    setr(r, TESS_FAIL, "DISPLAY is not set",
         "The UI needs an X server. At the console:\n"
         "    setenv DISPLAY :0\n"
         "Note that root's environment is not the console user's. If you\n"
         "su'd, DISPLAY may be unset or point at the wrong server, and you\n"
         "may also need:\n"
         "    xhost +localhost\n"
         "This check only reads the variable. tess-setup proves the server\n"
         "answers by opening it.");
}

/*
 * DEPLOY.md section 7, first row: a master from a previous job still holding
 * the port is why "master closed the connection" happens. The GUI attaches to
 * the old master, which rejects the new nonce.
 */
static void chk_port(const TessCheckCtx *ctx, TessCheckResult *r)
{
    int s;
    int on;
    struct sockaddr_in a;
    char detail[TESS_CHECK_DETAILLEN];

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        setr(r, TESS_WARN, "could not make a socket to test the port", "");
        return;
    }
    /* Deliberately NOT SO_REUSEADDR: the question is whether something is
       listening, and REUSEADDR would let the bind succeed anyway. */
    on = 0;
    (void)on;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)ctx->port);
    a.sin_addr.s_addr = htonl(0x7f000001);

    if (bind(s, (struct sockaddr *)&a, (int)sizeof a) == 0) {
        close(s);
        sprintf(detail, "port %d is free", ctx->port);
        setr(r, TESS_PASS, detail, "");
        return;
    }
    close(s);
    sprintf(detail, "port %d is already in use", ctx->port);
    setr(r, TESS_FAIL, detail,
         "A master from a previous job still holds the port. The new job\n"
         "cannot bind, the GUI attaches to the old master, and that master\n"
         "rejects the new nonce as \"master closed the connection\".\n"
         "Clear it on every host:\n"
         "    killall tess-node\n"
         "    arshell <host> killall tess-node\n"
         "Or run the UI on another port:  tess-ui -port 7334");
}

/* ------------------------------------------------------------------ */
/* the table                                                            */
/* ------------------------------------------------------------------ */

typedef void (*TessCheckFn)(const TessCheckCtx *, TessCheckResult *);
typedef void (*TessCheckFnDep)(const TessCheckCtx *, TessCheckResult *,
                               const TessCheckResult *, int);

typedef struct TessCheckDef {
    const char     *id;
    const char     *title;
    int             scope;
    TessCheckFn     fn;
    TessCheckFnDep  fndep;      /* used instead of fn when non-null */
} TessCheckDef;

/*
 * Order is dependency order, and it is also the order to fix things in: a
 * failure early makes everything after it meaningless, which is why the
 * dependants report SKIP rather than a second failure.
 */
static const TessCheckDef checks[] = {
    { "arch",    "64-bit MIPS, R10000 class",   TESS_SCOPE_ANY,     chk_arch,    0 },
    { "irix",    "IRIX 6.5",                    TESS_SCOPE_ANY,     chk_irix,    0 },
    { "abi",     "this binary is 64-bit",       TESS_SCOPE_ANY,     chk_abi,     0 },
    { "cc",      "MIPSpro C compiler",          TESS_SCOPE_ANY,     chk_cc,      0 },
    { "gmake",   "GNU make, as gmake",          TESS_SCOPE_ANY,     chk_gmake,   0 },
    { "mpirun",  "mpirun on PATH",              TESS_SCOPE_ANY,     chk_mpirun,  0 },
    { "libmpi",  "64-bit libmpi",               TESS_SCOPE_ANY,     chk_libmpi,  0 },
    { "mpiext",  "mpi_ext.h, for counters",     TESS_SCOPE_ANY,     0, chk_mpiext },
    { "arrayd",  "Array Services answering",    TESS_SCOPE_ANY,     chk_arrayd,  0 },
    { "tree",    "shared tree at this path",    TESS_SCOPE_ANY,     chk_tree,    0 },
    { "build",   "this machine's binaries",     TESS_SCOPE_ANY,     0, chk_build  },
    { "stale",   "binaries newer than source",  TESS_SCOPE_ANY,     0, chk_stale  },
    { "clock",   "tree clock agrees",           TESS_SCOPE_ANY,     0, chk_clock  },
    { "probe",   "tess-probe runs and is current", TESS_SCOPE_ANY,  0, chk_probe  },
    { "root",    "running as root",             TESS_SCOPE_DISPLAY, chk_root,    0 },
    { "motif",   "Motif 2.1 installed",         TESS_SCOPE_DISPLAY, chk_motif,   0 },
    { "display", "DISPLAY is set",              TESS_SCOPE_DISPLAY, chk_display, 0 },
    { "port",    "master port free",            TESS_SCOPE_DISPLAY, chk_port,    0 }
};

#define NCHECKS ((int)(sizeof checks / sizeof checks[0]))

/* ------------------------------------------------------------------ */
/* the interface                                                        */
/* ------------------------------------------------------------------ */

void tess_check_ctx_default(TessCheckCtx *ctx)
{
    memset(ctx, 0, sizeof *ctx);
    strcpy(ctx->tree, "/cluster/dev/sgi-tess");
    ctx->port = 7333;
    ctx->display = 1;
}

int tess_check_count(void)
{
    return NCHECKS;
}

const char *tess_check_id_at(int i)
{
    if (i < 0 || i >= NCHECKS) {
        return "";
    }
    return checks[i].id;
}

const char *tess_check_title_at(int i)
{
    if (i < 0 || i >= NCHECKS) {
        return "";
    }
    return checks[i].title;
}

int tess_check_scope_at(int i)
{
    if (i < 0 || i >= NCHECKS) {
        return 0;
    }
    return checks[i].scope;
}

void tess_check_run_one(int i, const TessCheckCtx *ctx,
                        const TessCheckResult *done, int ndone,
                        TessCheckResult *out)
{
    memset(out, 0, sizeof *out);
    if (i < 0 || i >= NCHECKS) {
        return;
    }
    strncpy(out->id, checks[i].id, TESS_CHECK_IDLEN - 1);
    strncpy(out->title, checks[i].title, TESS_CHECK_TITLELEN - 1);

    if ((checks[i].scope & TESS_SCOPE_DISPLAY) != 0 && !ctx->display) {
        setr(out, TESS_SKIP, "not the display host", "");
        return;
    }
    if (checks[i].fndep) {
        checks[i].fndep(ctx, out, done, ndone);
    } else if (checks[i].fn) {
        checks[i].fn(ctx, out);
    }
}

int tess_check_run_all(const TessCheckCtx *ctx, TessCheckResult *out, int max)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < NCHECKS && n < max; i++) {
        tess_check_run_one(i, ctx, out, n, &out[n]);
        n++;
    }
    return n;
}

const char *tess_check_status_name(int status)
{
    switch (status) {
    case TESS_PASS: return "PASS";
    case TESS_WARN: return "WARN";
    case TESS_FAIL: return "FAIL";
    case TESS_SKIP: return "SKIP";
    default:        return "????";
    }
}

int tess_check_worst(const TessCheckResult *r, int n)
{
    int i;
    int worst;

    worst = TESS_PASS;
    for (i = 0; i < n; i++) {
        /* A SKIP caused by not being the display host is not a problem; one
           caused by a failed prerequisite is already counted as that failure. */
        if (r[i].status == TESS_SKIP) {
            continue;
        }
        if (r[i].status > worst) {
            worst = r[i].status;
        }
    }
    return worst;
}
