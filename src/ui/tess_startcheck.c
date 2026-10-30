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
 * C89, Motif 2.1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>

#include "tess_check.h"
#include "tess_startcheck.h"

#define SC_MAX      32
#define SC_CLOSE_S  60         /* how long "all green" stays up, seconds */

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
    Colormap cm = DefaultColormap(d, DefaultScreen(d));
    XColor want, exact;
    XmString s;

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
    int i, fails = 0, warns = 0;

    for (i = 0; i < sc.n; i++) {
        if (sc.res[i].status == TESS_FAIL) {
            fails++;
        } else if (sc.res[i].status == TESS_WARN) {
            warns++;
        }
    }

    if (fails == 0 && warns == 0) {
        sprintf(line, "All %d checks passed", sc.n);
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

static void step_cb(XtPointer cd, XtIntervalId *id)
{
    char line[TESS_CHECK_DETAILLEN + 64];

    if (sc.next >= sc.n) {
        sc_finish();
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
    if (sc.closet) {
        XtRemoveTimeOut(sc.closet);
        sc.closet = 0;
    }
    close_label("Close");
    XtSetSensitive(sc.quitb, False);
    XmTextSetString(sc.text, "");
    sc.n = tess_check_count();
    if (sc.n > SC_MAX) {
        sc.n = SC_MAX;
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
    sc_close();
}

/* Only reachable after a failed check, before anything has been launched in
   the normal course, so there is no job to take down first. */
static void quit_cb(Widget w, XtPointer cd, XtPointer cb)
{
    exit(1);
}

void tess_startcheck_run(Widget parent, const char *tree, int port,
                         void (*logf)(void *, const char *), void *logctx)
{
    Arg args[4];
    int n;
    Widget buttons, recheck, close, quit;
    XmString title;

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
    sc.dialog = XmCreateFormDialog(parent, "preflight", args, n);
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
