/*
 * tess_inventory.h - what one host is, gathered once and used three ways.
 *
 * DESIGN.md "How the shape is discovered": one implementation, three callers.
 * tess-probe runs it over arshell at preflight, tess-node reports it from its
 * hello path at MPI_Init, and the PMDA exports it. Gathering inventory more
 * than one way is how the numbers drift.
 *
 * C89. No MPI, no X: this links into anything.
 */

#ifndef TESS_INVENTORY_H
#define TESS_INVENTORY_H

#define TESS_INV_FORMAT 1               /* bump when a field's meaning changes */
#define TESS_HOSTLEN    64
#define TESS_RELLEN     32
#define TESS_WHYLEN     96

typedef struct TessInventory {
    char            host[TESS_HOSTLEN];
    int             cpus;               /* configured, _SC_NPROC_CONF */
    int             online;             /* available, _SC_NPROC_ONLN */
    int             mhz;                /* fastest CPU found, 0 if unknown */
    int             nodes;              /* NUMA nodes, aurora's shape tell */
    long            memkb;              /* physical */
    long            freekb;
    char            irix[TESS_RELLEN];  /* utsname.release, "?" off IRIX */
    char            mpt[TESS_RELLEN];   /* "?" until someone verifies how */
    int             abi;                /* 32 or 64, compile time */
    int             gm;                 /* libgm present where MPT dlopens it */
    int             hippi;              /* a hip* or ess* interface exists */
    char            memwhy[TESS_WHYLEN];/* which call answered, or how each failed */
} TessInventory;

/* Fills inv. Never fails: unknown fields are 0 or "?". */
void tess_inventory(TessInventory *inv);

/* The one-line machine-readable form documented in DESIGN.md. Writes at most
   len bytes including the terminator. */
void tess_inventory_line(const TessInventory *inv, char *buf, int len);

#endif /* TESS_INVENTORY_H */
