/*
 * tess_cluster.c - discover the cluster, size the job, own the mpirun.
 *
 * Discovery is tess-probe over arshell, which is the fallback path DESIGN.md
 * specifies and the one already proven: it needs only Array Services, which
 * MPI requires anyway, and it doubles as evidence that arshell works before
 * mpirun is attempted.
 *
 * Nothing about the cluster's size is stored. Aurora is four CPUs on one brick
 * and twenty on five, so a stored number would oversubscribe the everyday
 * shape by five times. Rescan asks again.
 *
 * Launch builds an argv and execs mpirun directly rather than writing the -f
 * arguments file DESIGN.md mentions. That is a deliberate, temporary
 * deviation: -f is MPT's own format and this code has not seen it documented
 * on the machine, while the colon form below is exactly what has been run by
 * hand all week. Cite, don't recall - so it stays argv until someone reads the
 * mpirun(1) page and writes the format down.
 *
 * C89, Motif 2.1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "tess_cluster.h"

struct TessCluster {
    Widget      frame;
    Widget      rows[TESS_MAX_HOSTS];
    Widget      info[TESS_MAX_HOSTS];
    Widget      rankf[TESS_MAX_HOSTS];
    Widget      onbox[TESS_MAX_HOSTS];
    Widget      state;
    Widget      launchb;
    Widget      stopb;

    TessHost    host[TESS_MAX_HOSTS];
    int         nhosts;

    char        tree[512];
    int         port;
    char        nonce[TESS_NONCE_LEN];

    pid_t       child;
    int         tries;
    int         relaunch;
    int         stopstage;
    XtIntervalId stopt;
    XtAppContext app;
    XtIntervalId poll;

    TessClusterReady ready;
    void       *ctx;
    void      (*logf)(void *, const char *);
    void       *logctx;
};

static void clog(TessCluster *c, const char *fmt, const char *a, int b)
{
    char msg[512];

    sprintf(msg, fmt, a, b);
    if (c->logf) {
        c->logf(c->logctx, msg);
    }
}

static void clog1(TessCluster *c, const char *text)
{
    if (c->logf) {
        c->logf(c->logctx, text);
    }
}

/* A button you can find without reading it. */
static void paint_button(Widget b, const char *spec)
{
    Display *d = XtDisplay(b);
    Colormap cm = DefaultColormap(d, DefaultScreen(d));
    XColor want, exact;

    if (XAllocNamedColor(d, cm, (char *)spec, &want, &exact)) {
        XtVaSetValues(b, XmNbackground, want.pixel, NULL);
    }
    XtVaSetValues(b,
                  XmNmarginWidth, 10,
                  XmNmarginHeight, 4,
                  NULL);
}

static void set_state(TessCluster *c, const char *text)
{
    XmString s;

    s = XmStringCreateLocalized((char *)text);
    XtVaSetValues(c->state, XmNlabelString, s, NULL);
    XmStringFree(s);
}

/*
 * Host hues, from design/ui-design.html: Octane blue, Origin green, Onyx
 * purple, with an amber fallback for anything unrecognised. Matched on the
 * machine type the probe reported, not on the hostname, so a new Origin gets
 * the Origin colour without anyone editing a table.
 */
static const char *host_colour(const TessHost *h)
{
    if (strcmp(h->arch, "IP30") == 0) {
        return "#4f8fbf";               /* Octane2 */
    }
    if (strcmp(h->arch, "IP35") == 0) {
        return "#5f9e4a";               /* Origin 350 */
    }
    if (strcmp(h->arch, "IP27") == 0) {
        return "#9a6cc0";               /* Onyx2 */
    }
    return "#c8863c";
}

/* rank -> which host, and which CPU of that host. -1 if it is past the end. */
static int rank_to_host(TessCluster *c, int rank, int *cpu)
{
    int i, base = 0;

    for (i = 0; i < c->nhosts; i++) {
        if (c->host[i].ranks <= 0) {
            continue;
        }
        if (rank < base + c->host[i].ranks) {
            if (cpu) {
                *cpu = rank - base;
            }
            return i;
        }
        base += c->host[i].ranks;
    }
    return -1;
}

int tess_cluster_rank_label(TessCluster *c, int rank, char *buf, int len)
{
    int cpu = 0;
    int i = rank_to_host(c, rank, &cpu);

    if (i < 0) {
        sprintf(buf, "rank %d", rank);
        return -1;
    }
    /*
     * The display host's first rank is the master and computes nothing, so its
     * workers start at cpu1. Everywhere else rank n is cpu n.
     */
    sprintf(buf, "%.20s/cpu%d", c->host[i].name, cpu);
    buf[len - 1] = '\0';
    return i;
}

const char *tess_cluster_rank_colour(TessCluster *c, int rank)
{
    int i = rank_to_host(c, rank, (int *)0);

    if (i < 0) {
        return "#8f9298";
    }
    return host_colour(&c->host[i]);
}

/* ------------------------------------------------------------ discovery */

/*
 * One line of output from a command, trimmed. Returns 0 on success.
 *
 * Keeps stderr, because a failure here is the interesting case and "no answer"
 * on its own tells you nothing: arshell's own complaint is what says whether
 * the host is down, the array is misconfigured, or the binary is missing.
 */
static int run_capture(const char *cmd, char *out, int len)
{
    FILE *f;
    char *p;

    out[0] = '\0';
    f = popen(cmd, "r");
    if (!f) {
        return -1;
    }
    if (!fgets(out, len, f)) {
        pclose(f);
        return -1;
    }
    pclose(f);
    for (p = out; *p; p++) {
        if (*p == '\n' || *p == '\r') {
            *p = '\0';
            break;
        }
    }
    return out[0] ? 0 : -1;
}

static int field_int(const char *line, const char *key)
{
    const char *p = strstr(line, key);

    if (!p) {
        return 0;
    }
    return atoi(p + strlen(key));
}

static void probe_host(TessCluster *c, int i)
{
    TessHost *h = &c->host[i];
    char cmd[768];
    char line[512];

    h->reachable = 0;
    h->arch[0] = '\0';
    h->cpus = 0;
    h->online = 0;
    strcpy(h->note, "no answer");

    sprintf(cmd, "arshell %s uname -m 2>&1", h->name);
    if (run_capture(cmd, line, (int)sizeof line) != 0) {
        clog1(c, "probe: no output from:");
        clog1(c, cmd);
        return;
    }
    if (strchr(line, ' ') || strlen(line) > 8) {
        /* Not an architecture: arshell said something, and what it said is
           worth more than "no answer". */
        strncpy(h->note, line, sizeof h->note - 1);
        h->note[sizeof h->note - 1] = '\0';
        clog1(c, "probe: unexpected reply:");
        clog1(c, line);
        return;
    }
    strncpy(h->arch, line, sizeof h->arch - 1);
    h->arch[sizeof h->arch - 1] = '\0';

    sprintf(cmd, "arshell %s %s/build/%s/tess-probe 2>&1",
            h->name, c->tree, h->arch);
    if (run_capture(cmd, line, (int)sizeof line) != 0) {
        strcpy(h->note, "no tess-probe");
        clog1(c, "probe: no output from:");
        clog1(c, cmd);
        return;
    }
    if (!strstr(line, "cpus=")) {
        strncpy(h->note, line, sizeof h->note - 1);
        h->note[sizeof h->note - 1] = '\0';
        clog1(c, "probe: not a probe line:");
        clog1(c, line);
        return;
    }
    h->cpus = field_int(line, "cpus=");
    h->online = field_int(line, "online=");
    h->reachable = 1;
    sprintf(h->note, "%d of %d CPUs", h->online, h->cpus);
}

static void refresh_rows(TessCluster *c)
{
    char buf[128];
    XmString s;
    int i;

    for (i = 0; i < c->nhosts; i++) {
        /*
         * Say how many CPUs will actually compute, not how many exist. On the
         * display host the first rank is the master and computes nothing, so
         * two ranks there means one computing CPU. Updates as the rank field
         * is edited, which is the number the person is actually choosing.
         */
        {
            int compute = c->host[i].ranks - (i == 0 ? 1 : 0);

            if (compute < 0 || !c->host[i].enabled) {
                compute = 0;
            }
            if (c->host[i].reachable) {
                sprintf(buf, "%-7s %-5s %d of %d CPUs", c->host[i].name,
                        c->host[i].arch, compute, c->host[i].online);
            } else {
                sprintf(buf, "%-7s %-5s %s", c->host[i].name,
                        c->host[i].arch[0] ? c->host[i].arch : "-",
                        c->host[i].note);
            }
        }
        s = XmStringCreateLocalized(buf);
        XtVaSetValues(c->info[i], XmNlabelString, s, NULL);
        XmStringFree(s);

        sprintf(buf, "%d", c->host[i].ranks);
        XmTextFieldSetString(c->rankf[i], buf);
    }
}

void tess_cluster_rescan(TessCluster *c)
{
    int i;

    set_state(c, "scanning...");
    for (i = 0; i < c->nhosts; i++) {
        probe_host(c, i);
        /*
         * Host 0 is the display host: it runs the GUI, the master and the
         * shading pass, so it contributes one rank and no compute by default
         * (DESIGN.md 3b). Everyone else offers one rank per online CPU.
         */
        /*
         * The reserved CPU belongs to the display host only. It runs X, the
         * GUI, the shading pass and the master rank, so it offers online-1
         * (DESIGN.md 3b). A compute host has nothing else to do and offers
         * every CPU it has: reserving one there was simply throwing away a
         * quarter of aurora.
         */
        /*
         * Every host offers one rank per online CPU. On the display host the
         * first of those is the master, which schedules and shades rather than
         * computing, so lucy's two CPUs give a master on cpu0 and a worker on
         * cpu1: the reserved CPU is the one the interface is already using,
         * not an idle one.
         */
        c->host[i].ranks = c->host[i].online > 0 ? c->host[i].online : 0;
        if (i == 0 && c->host[i].ranks < 1) {
            c->host[i].ranks = 1;           /* the master must exist */
        }
        if (!c->host[i].reachable) {
            c->host[i].enabled = 0;
        }
        clog(c, "%s: %d rank(s)", c->host[i].name, c->host[i].ranks);
    }
    refresh_rows(c);
    set_state(c, "scanned");
}

/* --------------------------------------------------------------- launch */

static void stop_sweep(TessCluster *c);

static void make_nonce(char *out)
{
    struct timeval tv;
    unsigned long a, b;
    int i;
    static const char hex[] = "0123456789abcdef";

    gettimeofday(&tv, (struct timezone *)0);
    a = (unsigned long)tv.tv_sec ^ ((unsigned long)getpid() << 16);
    b = (unsigned long)tv.tv_usec * 2654435761UL;
    for (i = 0; i < 8; i++) {
        out[i] = hex[(a >> (i * 4)) & 0xf];
        out[8 + i] = hex[(b >> (i * 4)) & 0xf];
    }
    out[16] = '\0';
}

static void poll_cb(XtPointer cd, XtIntervalId *id)
{
    TessCluster *c = (TessCluster *)cd;
    int status;

    if (c->child > 0 && waitpid(c->child, &status, WNOHANG) == c->child) {
        c->child = 0;
        set_state(c, "job exited");
        clog(c, "mpirun exited (%s%d)", "status ", status);
        XtSetSensitive(c->launchb, True);
        XtSetSensitive(c->stopb, False);
        return;
    }
    /*
     * Ask the GUI to attach. It reports whether it managed it, so there is no
     * throwaway probe connection: the master accepts one client at a time, and
     * a probe that connects and closes looks exactly like the GUI arriving and
     * leaving again, which used to shut the whole job down.
     */
    c->tries++;
    if (c->ready && c->ready(c->ctx, c->port, c->nonce) == 0) {
        set_state(c, "running");
        clog(c, "attached to the master on port %s%d", "", c->port);
        return;
    }
    if (c->tries > 100) {
        set_state(c, "master never answered");
        clog(c, "gave up waiting for the master%s%d", "", 0);
        return;
    }
    c->poll = XtAppAddTimeOut(c->app, 300, poll_cb, (XtPointer)c);
}

void tess_cluster_launch(TessCluster *c)
{
    char *argv[64];
    char spec[TESS_MAX_HOSTS][512];
    char ranks[TESS_MAX_HOSTS][16];
    char portstr[16];
    int argc, i, groups, ranks_here;

    /*
     * Start means apply. A change to the host set or the rank counts only
     * reaches the cluster through a new mpirun, so pressing Start with a job
     * running stops it first and relaunches: DESIGN.md section 2's "changing
     * the cluster relaunches the job", rather than a button that silently does
     * nothing because something is already running.
     */
    if (c->child > 0) {
        c->relaunch = 1;
        clog(c, "restarting to apply the new cluster%s%d", "", 0);
        tess_cluster_stop(c);
        return;
    }

    /*
     * Clear anything left from a previous job before starting a new one. A
     * master still holding the port makes the new job fail to bind while the
     * GUI happily connects to the old one, which then rejects the new nonce:
     * the user sees "master closed the connection" and nothing explains it.
     */
    stop_sweep(c);

    make_nonce(c->nonce);
    sprintf(portstr, "%d", c->port);

    argc = 0;
    argv[argc++] = "mpirun";
    argv[argc++] = "-d";
    argv[argc++] = c->tree;

    groups = 0;
    for (i = 0; i < c->nhosts && argc < 50; i++) {
        ranks_here = c->host[i].ranks;
        if (i == 0) {
            /*
             * The master runs where the GUI runs: it listens on loopback and
             * the GUI connects to it there. So the display host cannot be
             * switched off entirely - unticking it, or setting zero, means no
             * compute here, master only.
             */
            if (!c->host[i].enabled || ranks_here < 1) {
                ranks_here = 1;
            }
        } else if (!c->host[i].enabled || !c->host[i].reachable ||
                   ranks_here <= 0) {
            continue;
        }
        if (!c->host[i].reachable) {
            continue;
        }
        if (groups > 0) {
            argv[argc++] = ":";
        }
        sprintf(ranks[i], "%d", ranks_here);
        sprintf(spec[i], "%s/build/%s/tess-node", c->tree, c->host[i].arch);
        argv[argc++] = c->host[i].name;
        argv[argc++] = ranks[i];
        argv[argc++] = spec[i];
        argv[argc++] = "-listen";
        argv[argc++] = portstr;
        argv[argc++] = "-nonce";
        argv[argc++] = c->nonce;
        groups++;
    }
    argv[argc] = (char *)0;

    if (groups == 0) {
        set_state(c, "no hosts with ranks");
        return;
    }

    c->tries = 0;
    c->child = fork();
    if (c->child < 0) {
        set_state(c, "fork failed");
        c->child = 0;
        return;
    }
    if (c->child == 0) {
        setpgid(0, 0);          /* own group, so Stop can take the job down */
        execvp("mpirun", argv);
        _exit(127);
    }

    set_state(c, "starting...");
    clog(c, "launched %s%d rank group(s)", "", groups);
    XtSetSensitive(c->launchb, False);
    XtSetSensitive(c->stopb, True);
    c->poll = XtAppAddTimeOut(c->app, 300, poll_cb, (XtPointer)c);
}

/*
 * Take the job down the way Ctrl-C does.
 *
 * SIGTERM left tess-node running on aurora: MPT treats an interrupt as "tear
 * the job down" and a terminate as "die now", and dying now leaves the remote
 * ranks parented to Array Services with nobody to reap them. So this escalates
 * the way a person would: interrupt, then terminate, then kill, and finally a
 * sweep over the hosts with arshell for anything still standing.
 */
static void stop_step(XtPointer cd, XtIntervalId *id);
static void stop_sweep(TessCluster *c);

static void stop_sweep(TessCluster *c)
{
    char cmd[512];
    int i;

    for (i = 0; i < c->nhosts; i++) {
        if (!c->host[i].reachable) {
            continue;
        }
        sprintf(cmd, "arshell %s killall tess-node >/dev/null 2>&1",
                c->host[i].name);
        system(cmd);
        clog(c, "swept leftover ranks on %s%d", c->host[i].name, 0);
    }
}

void tess_cluster_stop(TessCluster *c)
{
    if (c->child <= 0) {
        stop_sweep(c);          /* nothing of ours, but tidy anyway */
        return;
    }
    c->stopstage = 0;
    kill(-c->child, SIGINT);
    set_state(c, "stopping...");
    clog(c, "sent SIGINT to the job%s%d", "", 0);
    c->stopt = XtAppAddTimeOut(c->app, 1500, stop_step, (XtPointer)c);
}

static void stop_step(XtPointer cd, XtIntervalId *id)
{
    TessCluster *c = (TessCluster *)cd;
    int status;

    if (c->child > 0 && waitpid(c->child, &status, WNOHANG) == c->child) {
        c->child = 0;
    }
    if (c->child <= 0) {
        set_state(c, "stopped");
        XtSetSensitive(c->launchb, True);
        XtSetSensitive(c->stopb, False);
        stop_sweep(c);          /* remote ranks outlive mpirun often enough */
        if (c->relaunch) {
            c->relaunch = 0;
            tess_cluster_launch(c);
        }
        return;
    }

    c->stopstage++;
    if (c->stopstage == 1) {
        kill(-c->child, SIGTERM);
        clog(c, "still there: sent SIGTERM%s%d", "", 0);
    } else if (c->stopstage == 2) {
        kill(-c->child, SIGKILL);
        clog(c, "still there: sent SIGKILL%s%d", "", 0);
    } else {
        c->child = 0;
        set_state(c, "stopped (forced)");
        XtSetSensitive(c->launchb, True);
        XtSetSensitive(c->stopb, False);
        stop_sweep(c);
        return;
    }
    c->stopt = XtAppAddTimeOut(c->app, 1500, stop_step, (XtPointer)c);
}

int tess_cluster_running(TessCluster *c)
{
    return c->child > 0;
}

Widget tess_cluster_widget(TessCluster *c)
{
    return c->frame;
}

/* ---------------------------------------------------------------- panel */

static void rescan_cb(Widget w, XtPointer cd, XtPointer cb)
{
    tess_cluster_rescan((TessCluster *)cd);
}

static void launch_cb(Widget w, XtPointer cd, XtPointer cb)
{
    tess_cluster_launch((TessCluster *)cd);
}

static void stop_cb(Widget w, XtPointer cd, XtPointer cb)
{
    tess_cluster_stop((TessCluster *)cd);
}

static void enable_cb(Widget w, XtPointer cd, XtPointer cb)
{
    TessCluster *c = (TessCluster *)cd;
    XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)cb;
    int i;

    for (i = 0; i < c->nhosts; i++) {
        if (c->onbox[i] == w) {
            c->host[i].enabled = s->set ? 1 : 0;
            clog(c, "%s: %s", c->host[i].name,
                 0);
            clog1(c, c->host[i].enabled ? "  enabled" :
                     "  disabled");
            return;
        }
    }
}

static void rank_cb(Widget w, XtPointer cd, XtPointer cb)
{
    TessCluster *c = (TessCluster *)cd;
    char *text;
    int i;

    for (i = 0; i < c->nhosts; i++) {
        if (c->rankf[i] == w) {
            text = XmTextFieldGetString(w);
            if (text) {
                c->host[i].ranks = atoi(text);
                if (c->host[i].ranks < 0) {
                    c->host[i].ranks = 0;
                }
                XtFree(text);
            }
            refresh_rows(c);
            return;
        }
    }
}

TessCluster *tess_cluster_create(Widget parent, const char *tree,
                                 const char *hostlist, int port,
                                 TessClusterReady ready, void *ctx,
                                 void (*logf)(void *, const char *),
                                 void *logctx)
{
    TessCluster *c;
    Widget rc, row, buttons, b;
    char list[512];
    char *p, *q;
    int i;

    c = (TessCluster *)calloc(1, sizeof(TessCluster));
    if (!c) {
        return (TessCluster *)0;
    }
    strncpy(c->tree, tree, sizeof c->tree - 1);
    c->port = port;
    c->ready = ready;
    c->ctx = ctx;
    c->logf = logf;
    c->logctx = logctx;
    c->app = XtWidgetToApplicationContext(parent);

    strncpy(list, hostlist, sizeof list - 1);
    list[sizeof list - 1] = '\0';
    p = list;
    while (p && *p && c->nhosts < TESS_MAX_HOSTS) {
        q = strchr(p, ',');
        if (q) {
            *q = '\0';
        }
        strncpy(c->host[c->nhosts].name, p,
                sizeof c->host[0].name - 1);
        c->host[c->nhosts].ranks = c->nhosts == 0 ? 1 : 0;
        strcpy(c->host[c->nhosts].note, "not scanned");
        c->nhosts++;
        p = q ? q + 1 : (char *)0;
    }

    c->frame = XtVaCreateManagedWidget("clframe", xmFrameWidgetClass, parent,
                                       XmNshadowType, XmSHADOW_ETCHED_IN,
                                       NULL);
    XtVaCreateManagedWidget("Cluster", xmLabelWidgetClass, c->frame,
                            XmNchildType, XmFRAME_TITLE_CHILD, NULL);
    rc = XtVaCreateManagedWidget("clrc", xmRowColumnWidgetClass, c->frame,
                                 XmNorientation, XmVERTICAL, NULL);

    for (i = 0; i < c->nhosts; i++) {
        row = XtVaCreateManagedWidget("clrow", xmFormWidgetClass, rc,
                                      XmNfractionBase, 100, NULL);
        {
            XmString empty = XmStringCreateLocalized("");

            c->onbox[i] = XtVaCreateManagedWidget("on",
                                                  xmToggleButtonWidgetClass,
                                                  row,
                                                  XmNlabelString, empty,
                                                  XmNleftAttachment,
                                                  XmATTACH_FORM,
                                                  NULL);
            XmStringFree(empty);
            XmToggleButtonSetState(c->onbox[i], True, False);
            c->host[i].enabled = 1;
            XtAddCallback(c->onbox[i], XmNvalueChangedCallback, enable_cb,
                          (XtPointer)c);
        }
        c->info[i] = XtVaCreateManagedWidget("info", xmLabelWidgetClass, row,
                                             XmNalignment,
                                             XmALIGNMENT_BEGINNING,
                                             XmNleftAttachment, XmATTACH_WIDGET,
                                             XmNleftWidget, c->onbox[i],
                                             XmNrightAttachment,
                                             XmATTACH_POSITION,
                                             XmNrightPosition, 78,
                                             NULL);
        c->rankf[i] = XtVaCreateManagedWidget("rf", xmTextFieldWidgetClass,
                                              row,
                                              XmNcolumns, 3,
                                              XmNleftAttachment,
                                              XmATTACH_POSITION,
                                              XmNleftPosition, 76,
                                              NULL);
        XtAddCallback(c->rankf[i], XmNactivateCallback, rank_cb,
                      (XtPointer)c);
        c->rows[i] = row;
    }

    buttons = XtVaCreateManagedWidget("clbuttons", xmRowColumnWidgetClass, rc,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNpacking, XmPACK_COLUMN,
                                      XmNnumColumns, 1,
                                      XmNspacing, 4,
                                      NULL);
    b = XtVaCreateManagedWidget("Rescan", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, rescan_cb, (XtPointer)c);
    paint_button(b, "#6fc0d4");
    c->launchb = XtVaCreateManagedWidget("Start", xmPushButtonWidgetClass,
                                         buttons, NULL);
    XtAddCallback(c->launchb, XmNactivateCallback, launch_cb, (XtPointer)c);
    paint_button(c->launchb, "#5f9e4a");
    c->stopb = XtVaCreateManagedWidget("Stop", xmPushButtonWidgetClass,
                                       buttons, NULL);
    XtAddCallback(c->stopb, XmNactivateCallback, stop_cb, (XtPointer)c);
    paint_button(c->stopb, "#e0685c");
    XtSetSensitive(c->stopb, False);

    c->state = XtVaCreateManagedWidget("not scanned", xmLabelWidgetClass, rc,
                                       XmNalignment, XmALIGNMENT_BEGINNING,
                                       NULL);
    refresh_rows(c);
    return c;
}
