/*
 * tess_inventory.c - one host, described from documented IRIX calls.
 *
 * Every call here is the one PLATFORM-FACTS.md "IRIX statistics APIs" names,
 * so that table stays the single source of truth:
 *
 *   CPU count   sysconf(_SC_NPROC_CONF / _SC_NPROC_ONLN)
 *   CPU MHz     getinvent(3), inv_controller on INV_PROCESSOR
 *   memory      sysget(SGT_RMINFO) x getpagesize()
 *   NUMA nodes  sysmp(MP_NUMNODES)
 *
 * Two fields are honest question marks rather than invented values:
 *
 *   mpt=   no documented C API gives the MPT release. MPI_Get_library_version
 *          is MPI-3 and MPT 1.9 is MPI 1.2 + parts of 2. `versions -b mpt`
 *          knows, but its output format is unverified here, so the launcher
 *          fills this in and the probe reports "?".
 *   irix=  utsname.release is "6.5" on every 6.5.x machine. The patch level
 *          that distinguishes 6.5.30 comes from `uname -R`, which is a command,
 *          not a call. Reported as release, and the launcher refines it.
 *
 * Builds and runs on macOS too, where the IRIX-only parts compile out, so the
 * portability gate in DESIGN.md section 8 keeps working.
 *
 * C89 throughout.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>

#ifdef __sgi
#include <errno.h>
#include <time.h>
#include <invent.h>
#include <sys/sysmp.h>
#include <sys/sysget.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#include "tess_inventory.h"

#define GM_LIB_PATH "/usr/myricom/lib64/libgm.so"   /* the path MPT dlopens */

static void set_unknown(char *s, int len)
{
    if (len > 1) {
        s[0] = '?';
        s[1] = '\0';
    } else if (len == 1) {
        s[0] = '\0';
    }
}

#ifdef __sgi
/* Fastest processor clock the inventory knows about, 0 if none reported. */
static int probe_mhz(void)
{
    inventory_t *inv;
    int best;

    best = 0;
    setinvent();
    while ((inv = getinvent()) != (inventory_t *)0) {
        if (inv->inv_class == INV_PROCESSOR) {
            if ((int)inv->inv_controller > best) {
                best = (int)inv->inv_controller;
            }
        }
    }
    endinvent();
    return best;
}

/*
 * Physical and free memory, in pages, from whichever call answers.
 *
 * PLATFORM-FACTS.md prefers sysget(SGT_RMINFO) and says never MP_KERNADDR.
 * sysget is also the unprivileged path, which matters because the probe runs
 * as whoever arshell lands as. sysmp(MP_SAGET, MPSA_RMINFO) stays behind it as
 * a fallback, and why[] records which one answered so -v can say rather than
 * imply.
 */
static int probe_memory(long *memkb, long *freekb, char *why, int whylen)
{
    struct rminfo rmi;
    sgt_cookie_t ck;
    long pgkb;
    int rc;

    *memkb = 0;
    *freekb = 0;
    pgkb = (long)getpagesize() / 1024;
    if (pgkb < 1) {
        pgkb = 1;
    }

    /*
     * The fifth argument is a cookie STRUCT, not a null pointer: sysget.h
     * declares sysget(int, char *, int, int, sgt_cookie_t *) and the kernel
     * dereferences it, which is why every call here returned EFAULT. The
     * header's own macro initialises it to mean "all cells", and SGT_SUM adds
     * them together, which is what a whole-machine total means on a NUMA box.
     */
    memset((char *)&rmi, 0, sizeof rmi);
    SGT_COOKIE_INIT(&ck);
    rc = sysget(SGT_RMINFO, (char *)&rmi, sizeof rmi, SGT_READ | SGT_SUM,
                &ck);
    if (rc != -1 && rmi.physmem > 0) {
        *memkb = (long)rmi.physmem * pgkb;
        *freekb = (long)rmi.freemem * pgkb;
        sprintf(why, "sysget(SGT_RMINFO) ok");
        why[whylen - 1] = '\0';
        return 1;
    }
    sprintf(why, "sysget rc=%d errno=%d physmem=%ld; ", rc, errno,
            (long)rmi.physmem);

    memset((char *)&rmi, 0, sizeof rmi);
    rc = (int)sysmp(MP_SAGET, MPSA_RMINFO, (char *)&rmi, sizeof rmi);
    if (rc != -1 && rmi.physmem > 0) {
        *memkb = (long)rmi.physmem * pgkb;
        *freekb = (long)rmi.freemem * pgkb;
        strncat(why, "sysmp(MP_SAGET,MPSA_RMINFO) ok", whylen - strlen(why) - 1);
        why[whylen - 1] = '\0';
        return 1;
    }
    sprintf(why + strlen(why), "sysmp rc=%d errno=%d physmem=%ld", rc, errno,
            (long)rmi.physmem);
    why[whylen - 1] = '\0';
    return 0;
}

#ifdef __sgi
#define TESS_CPU_TICKS_FILE "/tmp/.tess-cpu-ticks"

/*
 * The kernel's cumulative per-state tick counters, summed across cells.
 *
 * SGT_SINFO through sysget rather than sysmp(MP_SAGET): same data,
 * unprivileged, and summed across cells by the same cookie mechanism.
 * MP_SAGET stays as the fallback for anything that refuses.
 */
static int read_cpu_ticks(long *now)
{
    struct sysinfo si;
    sgt_cookie_t ck;
    int i;

    memset((char *)&si, 0, sizeof si);
    SGT_COOKIE_INIT(&ck);
    if (sysget(SGT_SINFO, (char *)&si, sizeof si, SGT_READ | SGT_SUM,
               &ck) == -1 &&
        sysmp(MP_SAGET, MPSA_SINFO, (char *)&si, sizeof si) == -1) {
        return -1;
    }
    for (i = 0; i < CPU_STATES; i++) {
        now[i] = (long)si.cpu[i];
    }
    return 0;
}

/*
 * Busy percentage over the gap between this run and the previous one.
 *
 * A one-shot probe has no history, so it used to sample twice, 200 ms apart.
 * At 100 Hz that is 40 ticks on a two-CPU machine and 80 on a four-CPU one,
 * which quantises the answer to 2.5% and 1.25%: a quiet machine reads exactly
 * 0.0, and one busy tick reads 2.4%. Measured on lucy and aurora over arshell,
 * 19 September 2026, which is what made the panel look frozen at zero.
 *
 * Leaving the counters in a file means consecutive runs difference across the
 * real interval between them. The UI polls about once a second, so that is
 * 200 to 800 ticks and about a tenth of a percent of resolution, with no added
 * latency: nothing sleeps.
 *
 * The file is advisory. A stale one is ignored rather than trusted, and a run
 * that cannot write it still answers from whatever it could read. Returns -1
 * when there is no usable previous run, and the caller falls back to sampling.
 */
#define TESS_CPU_TICKS_MAX_AGE 60       /* seconds; older is not an interval */

static double cpu_busy_since_last_run(void)
{
    long now[CPU_STATES];
    long ptot, pidle, ntot, nidle, dtot, didle;
    long when;
    double busy;
    time_t t;
    FILE *f;
    int i;

    if (read_cpu_ticks(now) != 0) {
        return -1.0;
    }
    ntot = 0;
    for (i = 0; i < CPU_STATES; i++) {
        ntot += now[i];
    }
    nidle = now[CPU_IDLE];
    t = time((time_t *)0);

    busy = -1.0;
    ptot = 0;
    pidle = 0;
    when = 0;
    f = fopen(TESS_CPU_TICKS_FILE, "r");
    if (f) {
        if (fscanf(f, "%ld %ld %ld", &ptot, &pidle, &when) != 3) {
            when = 0;
        }
        fclose(f);
    }
    if (when > 0 && (long)t >= when && (long)t - when <= TESS_CPU_TICKS_MAX_AGE) {
        dtot = ntot - ptot;
        didle = nidle - pidle;
        if (dtot > 0 && didle >= 0 && didle <= dtot) {
            busy = 100.0 * (double)(dtot - didle) / (double)dtot;
        }
    }

    f = fopen(TESS_CPU_TICKS_FILE, "w");
    if (f) {
        fprintf(f, "%ld %ld %ld\n", ntot, nidle, (long)t);
        fclose(f);
    }
    return busy;
}
#endif /* __sgi */

/*
 * How busy this machine's CPUs have been since the last call, as a percentage.
 *
 * PLATFORM-FACTS.md: "two samples of struct sysinfo cpu[] ticks". Keeping the
 * previous sample in a static means a caller polling once a second gets the
 * average over that second with no sleep and no second syscall, which is what
 * the worker needs: it is called between tiles, not between frames.
 *
 * Returns -1 until there is a baseline to difference against, which is the
 * first call, and after any failure.
 */
double tess_cpu_busy(void)
{
#ifdef __sgi
    static int have_prev = 0;
    static long prev[CPU_STATES];
    long now[CPU_STATES];
    long dtot = 0, didle = 0;
    int i;

    /*
     * SGT_SINFO through sysget rather than sysmp(MP_SAGET): same data,
     * unprivileged, and summed across cells by the same cookie mechanism.
     * MP_SAGET stays as the fallback for anything that refuses.
     */
    if (read_cpu_ticks(now) != 0) {
        return -1.0;
    }
    if (!have_prev) {
        for (i = 0; i < CPU_STATES; i++) {
            prev[i] = now[i];
        }
        have_prev = 1;
        return -1.0;
    }
    for (i = 0; i < CPU_STATES; i++) {
        long d = now[i] - prev[i];

        if (d < 0) {
            d = 0;
        }
        dtot += d;
        if (i == CPU_IDLE) {
            didle += d;
        }
        prev[i] = now[i];
    }
    if (dtot <= 0) {
        return -1.0;
    }
    return 100.0 * (double)(dtot - didle) / (double)dtot;
#else
    return -1.0;
#endif
}

/*
 * One-minute load average.
 *
 * PLATFORM-FACTS.md: sysget(SGT_KSYM, "avenrun") divided by 1024.0, which is
 * what uptime does and needs no privilege. Reported as -1 when the call is not
 * available rather than as a plausible zero.
 */
double tess_load1(void)
{
#ifdef __sgi
    int avenrun[3];
    sgt_cookie_t ck;

    /*
     * A kernel symbol is named through the cookie, not through the buffer
     * argument: SGT_COOKIE_SET_KSYM writes the name into the cookie's opaque
     * area after the cell id. KSYM_AVENRUN is the header's own name for it.
     *
     * int, not long, even in a -64 build: the kernel's avenrun is 32-bit.
     * Measured on lucy 19 September 2026, a long[3] buffer read 255852544.08,
     * which is 1024 x ((61 << 32) | 82) - the 1-minute and 5-minute figures
     * of an idle machine, 0.06 and 0.08, glued into one 64-bit word by a
     * big-endian read of twice the width.
     */
    memset((char *)avenrun, 0, sizeof avenrun);
    SGT_COOKIE_INIT(&ck);
    SGT_COOKIE_SET_KSYM(&ck, KSYM_AVENRUN);
    if (sysget(SGT_KSYM, (char *)avenrun, sizeof avenrun, SGT_READ,
               &ck) == -1) {
        return -1.0;
    }
    return (double)avenrun[0] / 1024.0;
#else
    return -1.0;
#endif
}

#ifdef __sgi
static double probe_load(void)
{
    return tess_load1();
}
#endif

/* A HIPPI interface by either driver's name: SGI's hip*, Essential's ess*. */
static int probe_hippi(void)
{
    struct ifconf ifc;
    struct ifreq buf[64];
    struct ifreq *ifr;
    int s, n, i, found;

    found = 0;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return 0;
    }
    memset(buf, 0, sizeof buf);
    ifc.ifc_len = (int)sizeof buf;
    ifc.ifc_buf = (caddr_t)buf;
    if (ioctl(s, SIOCGIFCONF, (char *)&ifc) == 0) {
        n = ifc.ifc_len / (int)sizeof(struct ifreq);
        for (i = 0; i < n; i++) {
            ifr = &buf[i];
            if (strncmp(ifr->ifr_name, "hip", 3) == 0 ||
                strncmp(ifr->ifr_name, "ess", 3) == 0) {
                found = 1;
            }
        }
    }
    close(s);
    return found;
}
#endif /* __sgi */

void tess_inventory(TessInventory *inv)
{
    struct utsname u;

    memset(inv, 0, sizeof *inv);
    inv->abi = (int)(sizeof(long) * 8);
    set_unknown(inv->memwhy, TESS_WHYLEN);

    if (gethostname(inv->host, TESS_HOSTLEN) != 0) {
        set_unknown(inv->host, TESS_HOSTLEN);
    }
    inv->host[TESS_HOSTLEN - 1] = '\0';

    set_unknown(inv->irix, TESS_RELLEN);
    set_unknown(inv->mpt, TESS_RELLEN);
    if (uname(&u) == 0) {
        strncpy(inv->irix, u.release, TESS_RELLEN - 1);
        inv->irix[TESS_RELLEN - 1] = '\0';
    }

#ifdef _SC_NPROC_CONF
    inv->cpus = (int)sysconf(_SC_NPROC_CONF);
#elif defined(_SC_NPROCESSORS_CONF)
    inv->cpus = (int)sysconf(_SC_NPROCESSORS_CONF);
#endif
#ifdef _SC_NPROC_ONLN
    inv->online = (int)sysconf(_SC_NPROC_ONLN);
#elif defined(_SC_NPROCESSORS_ONLN)
    inv->online = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (inv->cpus < 0) {
        inv->cpus = 0;
    }
    if (inv->online < 0) {
        inv->online = 0;
    }

#ifdef __sgi
    {
        inv->mhz = probe_mhz();

        inv->nodes = (int)sysmp(MP_NUMNODES);
        if (inv->nodes < 1) {
            inv->nodes = 1;
        }

        (void)probe_memory(&inv->memkb, &inv->freekb, inv->memwhy,
                           TESS_WHYLEN);
        inv->load1 = probe_load();
        /* Difference against the previous run of this program if there was a
           recent one, which is both longer and free. Only when there was not
           does this fall back to sampling itself twice, a fifth of a second
           apart, which is quick but quantised to one tick in 40. */
        inv->busy = cpu_busy_since_last_run();
        if (inv->busy < 0.0) {
            (void)tess_cpu_busy();
            {
                struct timeval tv;

                tv.tv_sec = 0;
                tv.tv_usec = 200000;
                select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
            }
            inv->busy = tess_cpu_busy();
        }

        inv->gm    = (access(GM_LIB_PATH, F_OK) == 0) ? 1 : 0;
        inv->hippi = probe_hippi();
    }
#else
    inv->nodes = 1;
    inv->load1 = -1.0;
    inv->busy = -1.0;
#endif
}

void tess_inventory_line(const TessInventory *inv, char *buf, int len)
{
    /* Format 1, as documented in DESIGN.md "How the shape is discovered".
       Leading '#' so a captured line can be pasted into a doc or a script
       without editing. Keep the field order stable; add at the end. */
    sprintf(buf,
            "# tess-probe %d host=%s cpus=%d online=%d mhz=%d nodes=%d"
            " memkb=%ld freekb=%ld load=%.2f busy=%.1f irix=%s mpt=%s"
            " abi=%d gm=%d hippi=%d",
            TESS_INV_FORMAT, inv->host, inv->cpus, inv->online, inv->mhz,
            inv->nodes, inv->memkb, inv->freekb, inv->load1, inv->busy,
            inv->irix, inv->mpt, inv->abi, inv->gm, inv->hippi);
    buf[len - 1] = '\0';
}
