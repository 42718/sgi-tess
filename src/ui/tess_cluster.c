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

#include "tess_cluster.h"

struct TessCluster {
    Widget      frame;
    Widget      rows[TESS_MAX_HOSTS];
    Widget      info[TESS_MAX_HOSTS];
    Widget      rankf[TESS_MAX_HOSTS];
    Widget      state;
    Widget      launchb;
    Widget      stopb;

    TessHost    host[TESS_MAX_HOSTS];
    int         nhosts;

    char        tree[512];
    int         port;
    char        nonce[TESS_NONCE_LEN];

    pid_t       child;
    XtAppContext app;
    XtIntervalId poll;

    TessClusterReady ready;
    void       *ctx;
    void      (*logf)(void *, const char *);
    void       *logctx;
};

static void clog(TessCluster *c, const char *fmt, const char *a, int b)
{
    char msg[256];

    sprintf(msg, fmt, a, b);
    if (c->logf) {
        c->logf(c->logctx, msg);
    }
}

static void set_state(TessCluster *c, const char *text)
{
    XmString s;

    s = XmStringCreateLocalized((char *)text);
    XtVaSetValues(c->state, XmNlabelString, s, NULL);
    XmStringFree(s);
}

/* ------------------------------------------------------------ discovery */

/* One line of output from a command, trimmed. Returns 0 on success. */
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

    sprintf(cmd, "arshell %s uname -m 2>/dev/null", h->name);
    if (run_capture(cmd, line, (int)sizeof line) != 0) {
        return;
    }
    strncpy(h->arch, line, sizeof h->arch - 1);
    h->arch[sizeof h->arch - 1] = '\0';

    sprintf(cmd, "arshell %s %s/build/%s/tess-probe 2>/dev/null",
            h->name, c->tree, h->arch);
    if (run_capture(cmd, line, (int)sizeof line) != 0) {
        strcpy(h->note, "no tess-probe");
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
        sprintf(buf, "%-8s %-5s %s", c->host[i].name,
                c->host[i].arch[0] ? c->host[i].arch : "-",
                c->host[i].note);
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
        if (i == 0) {
            c->host[i].ranks = 1;
        } else {
            c->host[i].ranks = c->host[i].online;
        }
        clog(c, "%s: %d rank(s)", c->host[i].name, c->host[i].ranks);
    }
    refresh_rows(c);
    set_state(c, "scanned");
}

/* --------------------------------------------------------------- launch */

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

/* Can we reach the master yet? Used to decide when the job is up. */
static int master_up(int port)
{
    struct sockaddr_in sa;
    int fd, ok;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }
    memset((char *)&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ok = connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0;
    close(fd);
    return ok;
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
    if (master_up(c->port)) {
        set_state(c, "running");
        clog(c, "master is listening on port %s%d", "", c->port);
        if (c->ready) {
            c->ready(c->ctx, c->port, c->nonce);
        }
        return;                 /* stop polling: the GUI owns the socket now */
    }
    c->poll = XtAppAddTimeOut(c->app, 300, poll_cb, (XtPointer)c);
}

void tess_cluster_launch(TessCluster *c)
{
    char *argv[64];
    char spec[TESS_MAX_HOSTS][512];
    char ranks[TESS_MAX_HOSTS][16];
    char portstr[16];
    int argc, i, groups;

    if (c->child > 0) {
        clog(c, "already running%s%d", "", 0);
        return;
    }
    make_nonce(c->nonce);
    sprintf(portstr, "%d", c->port);

    argc = 0;
    argv[argc++] = "mpirun";
    argv[argc++] = "-d";
    argv[argc++] = c->tree;

    groups = 0;
    for (i = 0; i < c->nhosts && argc < 50; i++) {
        if (!c->host[i].reachable || c->host[i].ranks <= 0) {
            continue;
        }
        if (groups > 0) {
            argv[argc++] = ":";
        }
        sprintf(ranks[i], "%d", c->host[i].ranks);
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

void tess_cluster_stop(TessCluster *c)
{
    if (c->child <= 0) {
        return;
    }
    kill(-c->child, SIGTERM);
    set_state(c, "stopping...");
    clog(c, "sent SIGTERM to the job%s%d", "", 0);
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
        c->info[i] = XtVaCreateManagedWidget("info", xmLabelWidgetClass, row,
                                             XmNalignment,
                                             XmALIGNMENT_BEGINNING,
                                             XmNleftAttachment, XmATTACH_FORM,
                                             XmNrightAttachment,
                                             XmATTACH_POSITION,
                                             XmNrightPosition, 74,
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
                                      XmNorientation, XmHORIZONTAL, NULL);
    b = XtVaCreateManagedWidget("Rescan", xmPushButtonWidgetClass, buttons,
                                NULL);
    XtAddCallback(b, XmNactivateCallback, rescan_cb, (XtPointer)c);
    c->launchb = XtVaCreateManagedWidget("Launch", xmPushButtonWidgetClass,
                                         buttons, NULL);
    XtAddCallback(c->launchb, XmNactivateCallback, launch_cb, (XtPointer)c);
    c->stopb = XtVaCreateManagedWidget("Stop", xmPushButtonWidgetClass,
                                       buttons, NULL);
    XtAddCallback(c->stopb, XmNactivateCallback, stop_cb, (XtPointer)c);
    XtSetSensitive(c->stopb, False);

    c->state = XtVaCreateManagedWidget("not scanned", xmLabelWidgetClass, rc,
                                       XmNalignment, XmALIGNMENT_BEGINNING,
                                       NULL);
    refresh_rows(c);
    return c;
}
