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
#include <invent.h>
#include <sys/sysmp.h>
#include <sys/sysget.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
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
        struct rminfo rmi;
        long pgkb;

        inv->mhz = probe_mhz();

        inv->nodes = (int)sysmp(MP_NUMNODES);
        if (inv->nodes < 1) {
            inv->nodes = 1;
        }

        pgkb = (long)getpagesize() / 1024;
        if (pgkb < 1) {
            pgkb = 1;
        }
        if (sysget(SGT_RMINFO, (char *)&rmi, sizeof rmi, SGT_READ, (void *)0)
            != -1) {
            inv->memkb  = (long)rmi.physmem * pgkb;
            inv->freekb = (long)rmi.freemem * pgkb;
        }

        inv->gm    = (access(GM_LIB_PATH, F_OK) == 0) ? 1 : 0;
        inv->hippi = probe_hippi();
    }
#else
    inv->nodes = 1;
#endif
}

void tess_inventory_line(const TessInventory *inv, char *buf, int len)
{
    /* Format 1, as documented in DESIGN.md "How the shape is discovered".
       Leading '#' so a captured line can be pasted into a doc or a script
       without editing. Keep the field order stable; add at the end. */
    sprintf(buf,
            "# tess-probe %d host=%s cpus=%d online=%d mhz=%d nodes=%d"
            " memkb=%ld freekb=%ld irix=%s mpt=%s abi=%d gm=%d hippi=%d",
            TESS_INV_FORMAT, inv->host, inv->cpus, inv->online, inv->mhz,
            inv->nodes, inv->memkb, inv->freekb, inv->irix, inv->mpt,
            inv->abi, inv->gm, inv->hippi);
    buf[len - 1] = '\0';
}
