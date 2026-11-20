/*
 * tess_startcheck.c - the preflight checks, run as tess-ui starts.
 *
 * One check per timer tick rather than all of them in a row, so the window
 * paints as it goes: the probe and arrayd checks each take a noticeable
 * fraction of a second, and a dialog that appears only when everything is
 * finished looks like a hang.
 *
 * The report is a read-only text field rather than a row of labels, so a
 * remedy can be selected and pasted into a terminal.
 *
 * After this host's checks, the cluster: every configured host is asked at
 * once, in the background, through scripts/tess-hostcheck over arshell, and
 * gets SC_HOST_S seconds to answer. A host that does not answer is a SKIP,
 * not a failure, because a powered-off aurora or arthur is normal here and
 * must not hold the window open.
 *
 * C89, Motif 2.1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <Xm/Xm.h>
/* after Xm.h, which pulls in Intrinsic.h: Shell.h needs externalref */
#include <X11/Shell.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>

#include "tess_check.h"
#include "tess_startcheck.h"
#include "tess_version.h"

#define SC_MAX      40
#define SC_CLOSE_S  60         /* how long "all green" stays up, seconds */
#define SC_HOSTS    8
#define SC_HOST_S   20         /* how long a host gets to answer, seconds */

typedef struct HostRun {
    char   name[64];
    char   out[64];            /* where its answer is collected */
    pid_t  pid;
    int    done;
    int    timedout;
    int    status;
} HostRun;

typedef struct StartCheck {
    Widget          dialog;
    Widget          summary;
    Widget          text;
    Widget          closeb;
    Widget          quitb;     /* live only when something failed */
    int             left;      /* seconds until "all green" closes itself */
    XtAppContext    app;
    TessCheckCtx    ctx;
    TessCheckResult res[SC_MAX];
    int             n;
    int             next;
    XtIntervalId    closet;
    void          (*logf)(void *, const char *);
    void           *logctx;
    char            hostlist[256];
    HostRun         host[SC_HOSTS];
    int             nhosts;
    int             hostticks;
    XtIntervalId    hostt;     /* set while hosts are being asked */
} StartCheck;

/* One window per process, so one of these. */
static StartCheck sc;

static void sc_log(const char *text)
{
    if (sc.logf) {
        sc.logf(sc.logctx, text);
    }
}

static void sc_append(const char *text)
{
    XmTextInsert(sc.text, XmTextGetLastPosition(sc.text), (char *)text);
}

static void sc_summary(const char *text, const char *colour)
{
    Display *d = XtDisplay(sc.summary);
    Colormap cm;
    XColor want, exact;
    XmString s;

    /* the dialog's own colormap, which is not the default one on arthur */
    XtVaGetValues(sc.summary, XmNcolormap, &cm, NULL);

    s = XmStringCreateLocalized((char *)text);
    XtVaSetValues(sc.summary, XmNlabelString, s, NULL);
    XmStringFree(s);
    if (colour && XAllocNamedColor(d, cm, (char *)colour, &want, &exact)) {
        XtVaSetValues(sc.summary, XmNbackground, want.pixel, NULL);
    }
}

static void sc_close(void)
{
    if (sc.closet) {
        XtRemoveTimeOut(sc.closet);
        sc.closet = 0;
    }
    XtUnmanageChild(sc.dialog);
}

static void close_label(const char *text)
{
    XmString s = XmStringCreateLocalized((char *)text);

    XtVaSetValues(sc.closeb, XmNlabelString, s, NULL);
    XmStringFree(s);
}

/* Once a second, so the Close button can say how long is left. */
static void close_timeout_cb(XtPointer cd, XtIntervalId *id)
{
    char label[32];

    sc.closet = 0;
    sc.left--;
    if (sc.left <= 0) {
        sc_close();
        return;
    }
    sprintf(label, "Close (%d)", sc.left);
    close_label(label);
    sc.closet = XtAppAddTimeOut(sc.app, 1000, close_timeout_cb,
                                (XtPointer)0);
}

static void sc_finish(void)
{
    char line[160];
    int i, fails = 0, warns = 0, skips = 0;

    for (i = 0; i < sc.n; i++) {
        if (sc.res[i].status == TESS_FAIL) {
            fails++;
        } else if (sc.res[i].status == TESS_WARN) {
            warns++;
        } else if (sc.res[i].status == TESS_SKIP) {
            skips++;
        }
    }

    if (fails == 0 && warns == 0) {
        if (skips > 0) {
            /* With nothing failing, a skip is a host that did not answer. */
            sprintf(line, "All %d checks passed; %d host(s) did not answer",
                    sc.n - skips, skips);
        } else {
            sprintf(line, "All %d checks passed", sc.n);
        }
        sc_summary(line, "#5f9e4a");
        sprintf(line, "preflight: all %d checks passed", sc.n);
        sc_log(line);
        sc.left = SC_CLOSE_S;
        sprintf(line, "Close (%d)", sc.left);
        close_label(line);
        sc.closet = XtAppAddTimeOut(sc.app, 1000, close_timeout_cb,
                                    (XtPointer)0);
        return;
    }

    /* A failure usually means the UI cannot work as started - not root is
       the common one - and the fix is to quit, fix, and start again. A
       warning leaves it usable, so Quit stays ghosted for those. */
    XtSetSensitive(sc.quitb, fails > 0);
    sprintf(line, "%d failed, %d warning(s). Fix them in this order, then "
            "Recheck", fails, warns);
    sc_summary(line, fails ? "#c0504d" : "#d9a33a");
    sprintf(line, "preflight: %d failed, %d warning(s)", fails, warns);
    sc_log(line);

    sc_append("\nWhat to do\n----------\n");
    for (i = 0; i < sc.n; i++) {
        if (sc.res[i].status != TESS_FAIL && sc.res[i].status != TESS_WARN) {
            continue;
        }
        sprintf(line, "\n%s  %s\n",
                tess_check_status_name(sc.res[i].status), sc.res[i].title);
        sc_append(line);
        if (sc.res[i].remedy[0]) {
            sc_append(sc.res[i].remedy);
            sc_append("\n");
        }
    }
    /* A SKIP is the shadow of something above it, not a fault of its own. */
    sc_append("\nSKIP lines clear once what blocked them is fixed.\n");
    XmTextShowPosition(sc.text, 0);
}

/* Stop any host still being asked, and forget them: Recheck, or a close. */
static void hosts_stop(void)
{
    int i, st;

    if (sc.hostt) {
        XtRemoveTimeOut(sc.hostt);
        sc.hostt = 0;
    }
    for (i = 0; i < sc.nhosts; i++) {
        if (!sc.host[i].done && sc.host[i].pid > 0) {
            kill(-sc.host[i].pid, SIGKILL);
            waitpid(sc.host[i].pid, &st, 0);
        }
        unlink(sc.host[i].out);
    }
    sc.nhosts = 0;
}

/* Add to a result: worse status wins, remedies accumulate. */
static void note(TessCheckResult *r, int status, const char *detail,
                 const char *remedy)
{
    if (status > r->status) {
        r->status = status;
    }
    if (detail && *detail) {
        if (r->detail[0]) {
            strncat(r->detail, "; ",
                    sizeof r->detail - strlen(r->detail) - 1);
        }
        strncat(r->detail, detail, sizeof r->detail - strlen(r->detail) - 1);
    }
    if (remedy && *remedy) {
        strncat(r->remedy, remedy, sizeof r->remedy - strlen(r->remedy) - 1);
        strncat(r->remedy, "\n", sizeof r->remedy - strlen(r->remedy) - 1);
    }
}

/* Turn one host's answer into a result line. */
static void host_result(HostRun *h, TessCheckResult *r)
{
    FILE *f;
    char line[256], key[16], val[200], arch[16], tmp[400];
    int header = 0, clockok = 0, n;

    memset((char *)r, 0, sizeof *r);
    strncpy(r->id, h->name, sizeof r->id - 1);
    sprintf(r->title, "cluster host %.24s", h->name);
    r->status = TESS_PASS;
    arch[0] = '\0';

    if (h->timedout) {
        r->status = TESS_SKIP;
        sprintf(r->detail, "no answer in %d s: powered off?", SC_HOST_S);
        return;
    }
    f = fopen(h->out, "r");
    if (!f) {
        note(r, TESS_FAIL, "no answer collected", "");
        return;
    }
    while (fgets(line, sizeof line, f)) {
        n = (int)strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (!header) {
            if (strcmp(line, "tess-hostcheck 1") == 0) {
                header = 1;
            } else if (strstr(line, "asrcmdv failed")) {
                /* arshell's own message when the other arrayd does not
                   answer, a powered-off host among them: not login noise */
                r->status = TESS_SKIP;
                sprintf(r->detail, "no answer: powered off? (%.120s)", line);
                fclose(f);
                return;
            } else if (line[0]) {
                sprintf(tmp, "prints \"%.60s\" before anything", line);
                note(r, TESS_FAIL, tmp,
                     "Output from a login file breaks every mpirun launch.\n"
                     "Usually an unguarded stty in that host's /.cshrc:\n"
                     "    if ( $?prompt ) stty intr ^C\n"
                     "Its own text above says which. doc/MPI-HELLO.md, trap 2.");
                break;
            }
            continue;
        }
        key[0] = val[0] = '\0';
        if (sscanf(line, "%15s %199[^\n]", key, val) < 1) {
            continue;
        }
        if (strcmp(key, "arch") == 0) {
            strncpy(arch, val, sizeof arch - 1);
            arch[sizeof arch - 1] = '\0';
        } else if (strcmp(key, "tree") == 0 && strcmp(val, "ok") != 0) {
            sprintf(tmp, "%.40s has no %.100s", h->name, sc.ctx.tree);
            note(r, TESS_FAIL, tmp,
                 "Mount the share there at the same path as on this host.");
        } else if (strcmp(key, "build") == 0) {
            if (strcmp(val, "none") == 0) {
                sprintf(tmp, "no %s build", arch);
                note(r, TESS_FAIL, tmp, "");
                sprintf(tmp, "Build it there:\n    arshell %.40s \"cd %.150s ; "
                        "gmake fresh\"", h->name, sc.ctx.tree);
                note(r, TESS_FAIL, "", tmp);
            } else if (strcmp(val, TESS_VERSION) != 0) {
                sprintf(tmp, "%s build is %.30s, this UI %s", arch, val,
                        TESS_VERSION);
                note(r, TESS_WARN, tmp, "");
                sprintf(tmp, "Rebuild it there so both run one version:\n"
                        "    arshell %.40s \"cd %.150s ; gmake fresh\"",
                        h->name, sc.ctx.tree);
                note(r, TESS_WARN, "", tmp);
            }
        } else if (strcmp(key, "clock") == 0 && strncmp(val, "PASS", 4) == 0) {
            clockok = 1;
        } else if (strcmp(key, "clock") == 0 && strcmp(val, "unknown") != 0) {
            /* tess-preflight's line is "FAIL  clock  <what>": keep <what> */
            char *w = val;
            int k;

            for (k = 0; k < 2; k++) {
                while (*w && *w != ' ') {
                    w++;
                }
                while (*w == ' ') {
                    w++;
                }
            }
            sprintf(tmp, "clock: %.120s", *w ? w : val);
            note(r, TESS_WARN, tmp,
                 "Its clock and the share's disagree, so make there can skip\n"
                 "edited files. timeslave against the file server: INSTALL.md,\n"
                 "section 2.");
        } else if (strcmp(key, "nodes") == 0 && atoi(val) > 0) {
            sprintf(tmp, "%d tess-node already running", atoi(val));
            note(r, TESS_WARN, tmp, "");
            sprintf(tmp, "Left from an earlier job, or another UI's. If no "
                    "job should be running:\n    arshell %.40s killall "
                    "tess-node", h->name);
            note(r, TESS_WARN, "", tmp);
        }
    }
    fclose(f);
    if (!header && r->status == TESS_PASS) {
        sprintf(tmp, "arshell gave nothing back (status %d)",
                WIFEXITED(h->status) ? WEXITSTATUS(h->status) : -1);
        note(r, TESS_FAIL, tmp,
             "Array Services did not run the check there. As root:\n"
             "    array uptime\n"
             "should answer from every host. scripts/tess-setup on that host\n"
             "says what is missing.");
    }
    if (r->status == TESS_PASS) {
        sprintf(r->detail, "%s, %s, %s", arch[0] ? arch : "?", TESS_VERSION,
                clockok ? "clock agrees" : "clock not checked");
    }
}

static void sc_finish(void);

/* Every quarter second until each host has answered or run out of time. */
static void hosts_poll_cb(XtPointer cd, XtIntervalId *id)
{
    char line[TESS_CHECK_DETAILLEN + 64];
    int i, st, left = 0;

    sc.hostt = 0;
    sc.hostticks++;
    for (i = 0; i < sc.nhosts; i++) {
        HostRun *h = &sc.host[i];

        if (h->done) {
            continue;
        }
        if (waitpid(h->pid, &st, WNOHANG) == h->pid) {
            h->done = 1;
            h->status = st;
        } else if (sc.hostticks >= SC_HOST_S * 4) {
            kill(-h->pid, SIGKILL);
            waitpid(h->pid, &st, 0);
            h->done = 1;
            h->timedout = 1;
        } else {
            left++;
        }
    }
    if (left > 0) {
        sc.hostt = XtAppAddTimeOut(sc.app, 250, hosts_poll_cb, (XtPointer)0);
        return;
    }

    for (i = 0; i < sc.nhosts && sc.n < SC_MAX; i++) {
        host_result(&sc.host[i], &sc.res[sc.n]);
        unlink(sc.host[i].out);
        sprintf(line, "%-4s  %-8s %s\n",
                tess_check_status_name(sc.res[sc.n].status),
                sc.res[sc.n].id, sc.res[sc.n].detail);
        sc_append(line);
        sc.n++;
    }
    sc.nhosts = 0;
    sc_finish();
}

/* Ask every configured host at once. 0 if there were none to ask. */
static int hosts_start(void)
{
    char list[256], cmd[1024], line[160];
    char *p, *q;
    pid_t pid;

    strncpy(list, sc.hostlist, sizeof list - 1);
    list[sizeof list - 1] = '\0';
    sc.nhosts = 0;
    sc.hostticks = 0;
    for (p = list; p && *p && sc.nhosts < SC_HOSTS; p = q ? q + 1 : q) {
        HostRun *h = &sc.host[sc.nhosts];

        q = strchr(p, ',');
        if (q) {
            *q = '\0';
        }
        if (!*p) {
            continue;
        }
        memset((char *)h, 0, sizeof *h);
        strncpy(h->name, p, sizeof h->name - 1);
        sprintf(h->out, "/tmp/tess-hc.%d.%d", (int)getpid(), sc.nhosts);
        sprintf(cmd, "arshell %.60s sh %.300s/scripts/tess-hostcheck %.300s "
                "> %s 2>&1", h->name, sc.ctx.tree, sc.ctx.tree, h->out);
        pid = fork();
        if (pid < 0) {
            continue;
        }
        if (pid == 0) {
            setpgid(0, 0);      /* so a timeout takes arshell down too */
            execl("/bin/sh", "sh", "-c", cmd, (char *)0);
            _exit(127);
        }
        h->pid = pid;
        sc.nhosts++;
    }
    if (sc.nhosts == 0) {
        return 0;
    }
    sprintf(line, "Asking %d cluster host(s), up to %d s", sc.nhosts,
            SC_HOST_S);
    sc_summary(line, (const char *)0);
    sc.hostt = XtAppAddTimeOut(sc.app, 250, hosts_poll_cb, (XtPointer)0);
    return 1;
}

static void step_cb(XtPointer cd, XtIntervalId *id)
{
    char line[TESS_CHECK_DETAILLEN + 64];

    if (sc.next >= sc.n) {
        if (!hosts_start()) {
            sc_finish();
        }
        return;
    }
    sprintf(line, "Checking %d of %d: %s", sc.next + 1, sc.n,
            tess_check_title_at(sc.next));
    sc_summary(line, (const char *)0);
    XmUpdateDisplay(sc.dialog);

    tess_check_run_one(sc.next, &sc.ctx, sc.res, sc.next, &sc.res[sc.next]);
    sprintf(line, "%-4s  %-8s %s\n",
            tess_check_status_name(sc.res[sc.next].status),
            sc.res[sc.next].id, sc.res[sc.next].detail);
    sc_append(line);
    sc.next++;
    XtAppAddTimeOut(sc.app, 0, step_cb, (XtPointer)0);
}

static void sc_start(void)
{
    hosts_stop();
    if (sc.closet) {
        XtRemoveTimeOut(sc.closet);
        sc.closet = 0;
    }
    close_label("Close");
    XtSetSensitive(sc.quitb, False);
    XmTextSetString(sc.text, "");
    sc.n = tess_check_count();
    if (sc.n > SC_MAX - SC_HOSTS) {
        sc.n = SC_MAX - SC_HOSTS;
    }
    sc.next = 0;
    XtManageChild(sc.dialog);
    XtAppAddTimeOut(sc.app, 0, step_cb, (XtPointer)0);
}

static void recheck_cb(Widget w, XtPointer cd, XtPointer cb)
{
    sc_start();
}

static void close_cb(Widget w, XtPointer cd, XtPointer cb)
{
    hosts_stop();
    sc_close();
}

/* Only reachable after a failed check, before anything has been launched in
   the normal course, so there is no job to take down first. */
static void quit_cb(Widget w, XtPointer cd, XtPointer cb)
{
    hosts_stop();
    exit(1);
}

void tess_startcheck_run(Widget parent, const char *tree, int port,
                         const char *hostlist,
                         void (*logf)(void *, const char *), void *logctx)
{
    Arg args[4];
    int n;
    Widget buttons, recheck, close, quit, shell, top;
    XmString title;
    Visual *vis;
    int depth;
    Colormap cmap;

    strncpy(sc.hostlist, hostlist ? hostlist : "", sizeof sc.hostlist - 1);
    if (sc.dialog) {
        sc_start();
        return;
    }
    sc.app = XtWidgetToApplicationContext(parent);
    sc.logf = logf;
    sc.logctx = logctx;
    tess_check_ctx_default(&sc.ctx);
    strncpy(sc.ctx.tree, tree, sizeof sc.ctx.tree - 1);
    sc.ctx.port = port;

    /* The title is a pointer, so it goes through the varargs call: through
       an Arg array MIPSpro warns at -64, and no cast silences it. */
    n = 0;
    XtSetArg(args[n], XmNautoUnmanage, False); n++;
    XtSetArg(args[n], XmNhorizontalSpacing, 6); n++;
    XtSetArg(args[n], XmNverticalSpacing, 6); n++;
    /*
     * XmCreateFormDialog, by hand, so the shell can be told its visual. The
     * parent is the render window, which on arthur is not on the default
     * visual; a dialog shell takes its parent's depth and colormap but the
     * root window's visual, and that mismatch fails when it is mapped.
     */
    for (top = parent; top && !XtIsShell(top); top = XtParent(top)) {
        ;
    }
    XtVaGetValues(top ? top : parent, XtNvisual, &vis, XmNdepth, &depth,
                  XmNcolormap, &cmap, NULL);
    shell = XtVaCreatePopupShell("preflight_popup", xmDialogShellWidgetClass,
                                 parent,
                                 XtNvisual, vis,
                                 XmNdepth, depth,
                                 XmNcolormap, cmap,
                                 XmNallowShellResize, True,
                                 NULL);
    sc.dialog = XmCreateForm(shell, "preflight", args, n);
    title = XmStringCreateLocalized("Tess preflight");
    XtVaSetValues(sc.dialog, XmNdialogTitle, title, NULL);
    XmStringFree(title);

    sc.summary = XtVaCreateManagedWidget("summary", xmLabelWidgetClass,
                                         sc.dialog,
                                         XmNalignment, XmALIGNMENT_BEGINNING,
                                         XmNmarginHeight, 4,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNleftAttachment, XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         NULL);

    buttons = XtVaCreateManagedWidget("scbuttons", xmFormWidgetClass,
                                      sc.dialog,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNrightAttachment, XmATTACH_FORM,
                                      NULL);
    close = XtVaCreateManagedWidget("Close", xmPushButtonWidgetClass, buttons,
                                    XmNrightAttachment, XmATTACH_FORM,
                                    NULL);
    recheck = XtVaCreateManagedWidget("Recheck", xmPushButtonWidgetClass,
                                      buttons,
                                      XmNrightAttachment, XmATTACH_WIDGET,
                                      XmNrightWidget, close,
                                      XmNrightOffset, 6,
                                      NULL);
    quit = XtVaCreateManagedWidget("Quit", xmPushButtonWidgetClass, buttons,
                                   XmNrightAttachment, XmATTACH_WIDGET,
                                   XmNrightWidget, recheck,
                                   XmNrightOffset, 6,
                                   XmNsensitive, False,
                                   NULL);
    sc.closeb = close;
    sc.quitb = quit;
    XtAddCallback(close, XmNactivateCallback, close_cb, (XtPointer)0);
    XtAddCallback(quit, XmNactivateCallback, quit_cb, (XtPointer)0);
    XtAddCallback(recheck, XmNactivateCallback, recheck_cb, (XtPointer)0);

    /* Named "log" so it takes the fixed-width face the UI's log pane uses,
       which keeps the status and id columns straight. */
    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNeditable, False); n++;
    XtSetArg(args[n], XmNrows, 24); n++;
    XtSetArg(args[n], XmNcolumns, 76); n++;
    sc.text = XmCreateScrolledText(sc.dialog, "log", args, n);
    XtVaSetValues(XtParent(sc.text),
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, sc.summary,
                  XmNbottomAttachment, XmATTACH_WIDGET,
                  XmNbottomWidget, buttons,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    XtManageChild(sc.text);

    sc_start();
}
