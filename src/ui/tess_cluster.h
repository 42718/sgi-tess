/*
 * tess_cluster.h - the cluster panel, and the job it owns.
 *
 * DESIGN.md section 2: changing the host set relaunches the MPI job, and the
 * windows never blink. That only works if the GUI, not a shell, owns the
 * mpirun process - so this discovers what each host is, lets you set the rank
 * counts, starts the job, and hands the port and nonce back when it is up.
 *
 * The UI never writes arrayd.conf. It reads what Array Services already knows,
 * through arshell and tess-probe.
 *
 * C89, Motif 2.1. No MPI here either: this forks mpirun, it does not link it.
 */

#ifndef TESS_CLUSTER_H
#define TESS_CLUSTER_H

#include <Xm/Xm.h>

#define TESS_MAX_HOSTS 8
#define TESS_NONCE_LEN 17          /* 16 hex digits and a terminator */

typedef struct TessHost {
    char name[64];
    char arch[16];                 /* IP30, IP35, ... from uname -m */
    int  cpus;                     /* configured */
    int  online;                   /* available now: aurora's shape */
    int  ranks;                    /* what we will launch; 0 is legal */
    int  enabled;                  /* unticked: contributes nothing */
    int  reachable;
    double load;                   /* 1-minute average, -1 unknown */
    char note[64];
} TessHost;

typedef struct TessCluster TessCluster;

/*
 * Try to attach to the master. Returns 0 when connected, non-zero to be asked
 * again shortly: the master accepts one client at a time, so the attempt to
 * attach has to BE the connection rather than a probe alongside it.
 */
typedef int (*TessClusterReady)(void *ctx, int port, const char *nonce);

TessCluster *tess_cluster_create(Widget parent, const char *tree,
                                 const char *hostlist, int port,
                                 TessClusterReady ready, void *ctx,
                                 void (*logf)(void *, const char *),
                                 void *logctx);

/*
 * Which machine and which CPU a rank landed on, and the colour that host owns.
 *
 * MPT assigns ranks in mpirun's group order, so the launch configuration is
 * the mapping: group one takes the first ranks, group two the next, and so on.
 * The hues are the ones design/ui-design.html assigns per machine, and they
 * double as the tile-ownership colours in build 3.
 */
int tess_cluster_rank_label(TessCluster *c, int rank, char *buf, int len);
const char *tess_cluster_rank_colour(TessCluster *c, int rank);

void tess_cluster_rescan(TessCluster *c);
void tess_cluster_poll(TessCluster *c);      /* load averages only */
void tess_cluster_launch(TessCluster *c);
void tess_cluster_stop(TessCluster *c);
int  tess_cluster_running(TessCluster *c);
Widget tess_cluster_widget(TessCluster *c);

#endif /* TESS_CLUSTER_H */
