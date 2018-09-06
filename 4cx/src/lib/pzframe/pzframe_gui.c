#include <stdio.h>
#include <limits.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>

#include "misclib.h"
#include "timeval_utils.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "Knobs.h"
#include "MotifKnobsP.h" // For ABSTRZE() and MOTIFKNOBS_INTER*_SPACING

#include "pzframe_gui.h"

#include "pixmaps/btn_mini_save.xpm"
#include "pixmaps/btn_mini_start.xpm"
#include "pixmaps/btn_mini_once.xpm"


psp_paramdescr_t  pzframe_gui_text2look[] =
{
    PSP_P_FLAG   ("foldctls",     pzframe_gui_look_t, foldctls,     1, 0),
    PSP_P_FLAG   ("nocontrols",   pzframe_gui_look_t, nocontrols,   1, 0),
    PSP_P_FLAG   ("save_button",  pzframe_gui_look_t, savebtn,      1, 0),

    PSP_P_MSTRING("subsys",       pzframe_gui_look_t, subsysname, NULL, 100),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

static void SetMiniToolButtonOnOff(Widget btn, int is_pushed)
{
  XtPointer  state;

    if (btn == NULL) return;

    XtVaGetValues(btn, XmNuserData, &state, NULL);
    if (ptr2lint(state) == is_pushed) return;

    XhInvertButton(ABSTRZE(btn));
    XtVaSetValues(btn, XmNuserData, lint2ptr(is_pushed), NULL);
}

static void ShowRunMode(pzframe_gui_t *gui)
{
    if (gui->loop_button != NULL)
        SetMiniToolButtonOnOff(gui->loop_button,  gui->pfr->cfg.running);
    if (gui->once_button != NULL  &&  0)
        SetMiniToolButtonOnOff(gui->once_button,  gui->pfr->cfg.oneshot);
    if (gui->once_button != NULL)
        XtSetSensitive        (gui->once_button, !gui->pfr->cfg.running);
}

static void SetRunMode (pzframe_gui_t *gui, int is_loop)
{
    PzframeDataSetRunMode(gui->pfr, is_loop, 0);
    ShowRunMode(gui);
}

//////////////////////////////////////////////////////////////////////

// GetCblistSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Cblist, pzframe_gui_cbinfo_t,
                                 cb, cb, NULL, (void*)1,
                                 0, 2, 0,
                                 gui->, gui,
                                 pzframe_gui_t *gui, pzframe_gui_t *gui)

void PzframeGuiCallCBs(pzframe_gui_t *gui,
                       int            reason,
                       int            info_int)
{
  int  i;

    if (gui->d->vmt.evproc != NULL)
        gui->d->vmt.evproc(gui, reason, info_int, NULL);

    for (i = 0;  i < gui->cb_list_allocd;  i++)
        if (gui->cb_list[i].cb != NULL)
            gui->cb_list[i].cb(gui, reason, 
                               info_int, gui->cb_list[i].privptr);
}

//////////////////////////////////////////////////////////////////////

static void UpdateParamKnob(pzframe_gui_t *gui, int cn)
{
  pzframe_data_t *pfr = gui->pfr;
  double          v;

    PzframeDataGetChanDbl(pfr, cn, &v);
    SetSimpleKnobValue(gui->k_params[cn], v);
    SetSimpleKnobState(gui->k_params[cn], choose_knobstate(NULL, pfr->cur_data[cn].rflags));
}

static void PzframeGuiEventProc(pzframe_data_t *pfr,
                                int             reason,
                                int             info_int,
                                void           *privptr)
{
  pzframe_gui_t       *gui = (pzframe_gui_t *)privptr;
  pzframe_type_dscr_t *ftd = pfr->ftd;
  int                  info_changed;
  int                  cn;

  static char          roller_states[4] = "/-\\|";
  char                 buf[100];

////fprintf(stderr, "%s %s\n", strcurtime(), __FUNCTION__);
    if      (reason == PZFRAME_REASON_DATA)
    {
        fps_frme(&(gui->fps_ctr));
        ShowRunMode(gui);

        info_changed = info_int || gui->pfr->other_info_changed;
        gui->pfr->other_info_changed = 0;
        PzframeGuiUpdate(gui, info_changed);

        /* Rotate the roller */
        gui->roller_ctr = (gui->roller_ctr + 1) & 3;
        sprintf(buf, "%c", roller_states[gui->roller_ctr]);
        if (gui->roller != NULL) XmTextSetString(gui->roller, buf);

        XhProcessPendingXEvents();
    }
    else if (reason == PZFRAME_REASON_PARAM)
    {
        cn = info_int;
        if (gui->k_params[cn] != NULL)
            UpdateParamKnob(gui, cn);
    }
    else if (reason == PZFRAME_REASON_RSLVSTAT)
    {
        if (info_int != CDA_RSLVSTAT_FOUND)
            for (cn = 0;  cn < ftd->chan_count;  cn++)
                if (gui->k_params[cn] != NULL)
                    SetSimpleKnobState(gui->k_params[cn], KNOBSTATE_NOTFOUND);
    }

    /* Call upstream */
    PzframeGuiCallCBs(gui, reason, info_int);
}

//////////////////////////////////////////////////////////////////////

void  PzframeGuiInit       (pzframe_gui_t *gui, pzframe_data_t *pfr,
                            pzframe_gui_dscr_t *gkd)
{
    bzero(gui, sizeof(*gui));
    gui->pfr = pfr;
    gui->d   = gkd;
    gui->curstate = (gkd->ftd->behaviour & PZFRAME_B_ARTIFICIAL) != 0 ?
        KNOBSTATE_NONE : KNOBSTATE_JUSTCREATED;
}

int   PzframeGuiRealize    (pzframe_gui_t *gui)
{
  int     chan_count = gui->pfr->ftd->chan_count;
  size_t  sz;

    if (chan_count > 0)
    {
        sz = sizeof(gui->k_params[0]) * chan_count;
        if ((gui->k_params = malloc(sz)) == NULL) return -1;
        bzero(gui->k_params, sz);
    }

    PzframeDataAddEvproc(gui->pfr, PzframeGuiEventProc, gui);
    ShowRunMode   (gui);

    return 0;
}

void  PzframeGuiUpdate     (pzframe_gui_t *gui, int info_changed)
{
  pzframe_data_t      *pfr = gui->pfr;
  pzframe_type_dscr_t *ftd = pfr->ftd;

  knobstate_t          newstate;

  int                  cn;

    /* Set visible state */
    newstate = choose_knobstate(NULL, pfr->rflags);
    if (gui->curstate != newstate)
    {
        gui->curstate = newstate;
        if (gui->d->vmt.newstate != NULL)
            gui->d->vmt.newstate(gui);
    }

    /* Display current params */
    for (cn = 0;  cn < ftd->chan_count;  cn++)
        if (gui->k_params[cn] != NULL)
            UpdateParamKnob(gui, cn);

    /* Renew displayed data */
    if (gui->d->vmt.do_renew != NULL)
        gui->d->vmt.do_renew(gui, info_changed);
}

int   PzframeGuiAddEvproc  (pzframe_gui_t *gui,
                            pzframe_gui_evproc_t cb, void *privptr)
{
  int                   id;
  pzframe_gui_cbinfo_t *slot;

    if (cb == NULL) return 0;

    id = GetCblistSlot(gui);
    if (id < 0) return -1;
    slot = AccessCblistSlot(id, gui);

    slot->cb      = cb;
    slot->privptr = privptr;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void ShowStats  (pzframe_gui_t *gui)
{
  int             dfps = fps_dfps(&(gui->fps_ctr));
  char            buf[100];
  struct timeval  timenow;
  struct timeval  timediff;
  long int        secs;

    if      (!gui->pfr->cfg.running) buf[0] = '\0';
    else if (dfps < 100)    sprintf(buf, "%d.%d fps", dfps / 10, dfps % 10);
    else                    sprintf(buf, "%3d fps",   dfps / 10);
    XmTextSetString(gui->fps_show, buf);

    if      (!gui->pfr->cfg.running  ||  gui->pfr->timeupd.tv_sec == 0) buf[0] = '\0';
    else
    {
        gettimeofday(&timenow, NULL);
        timeval_subtract(&timediff, &timenow, &(gui->pfr->timeupd));
        if (timediff.tv_sec > 3600 * 99)
            sprintf(buf, "---");
        else
        {
            secs = timediff.tv_sec;
            if      (secs > 3600)
                sprintf(buf, " %ld:%02ld:%02ld",
                        secs / 3600, (secs / 60) % 60, secs % 60);
            else if (secs > 59)
                sprintf(buf, " %ld:%02ld",
                        secs / 60, secs % 60);
            else if (secs < 10)
                sprintf(buf, " %ld.%lds",
                        secs, timediff.tv_usec / 100000);
            else
                sprintf(buf, " %lds",
                        secs);
        }
    }
    XmTextSetString(gui->time_show, buf);
}

static void Fps10HzProc(XtPointer     closure,
                        XtIntervalId *id      __attribute__((unused)))
{
  pzframe_gui_t *gui = (pzframe_gui_t *)closure;

    /*!!! should save XtAppAddTimeOut() for "destroy()" to be possible */
    XtAppAddTimeOut(xh_context, 1000, Fps10HzProc, gui);
    fps_beat(&(gui->fps_ctr));
    ShowStats(gui);
}

static void SaveBtnCB(Widget     w,
                      XtPointer  closure,
                      XtPointer  call_data __attribute__((unused)))
{
  pzframe_gui_t     *gui   = closure;
  char               type_sys[PATH_MAX];
  char               filename[PATH_MAX];

    if (gui->pfr->ftd->vmt.save == NULL) return;

    check_snprintf(type_sys, sizeof(type_sys), "%s%s%s",
                   gui->pfr->ftd->type_name,
                   gui->look.subsysname == NULL? "" : "-",
                   gui->look.subsysname == NULL? "" : gui->look.subsysname);
    check_snprintf(filename, sizeof(filename), DATATREE_DATA_PATTERN_FMT,
                   type_sys);
    XhFindFilename(filename, filename, sizeof(filename));

    if (gui->pfr->ftd->vmt.save(gui->pfr, filename, gui->look.subsysname, NULL) == 0)
        XhMakeMessage(XhWindowOf(ABSTRZE(w)), "Data saved to \"%s\"", filename);
    else
        XhMakeMessage(XhWindowOf(ABSTRZE(w)), "Error saving to \"%s\": %s", filename, strerror(errno));
}

static void LoopBtnCB(Widget     w,
                      XtPointer  closure,
                      XtPointer  call_data __attribute__((unused)))
{
  pzframe_gui_t     *gui   = closure;

    SetRunMode(gui, !gui->pfr->cfg.running);
}

static void OnceBtnCB(Widget     w,
                      XtPointer  closure,
                      XtPointer  call_data __attribute__((unused)))
{
  pzframe_gui_t     *gui   = closure;

    PzframeDataSetRunMode(gui->pfr, -1, 1);
    ShowRunMode(gui);
}

void  PzframeGuiMkCommons  (pzframe_gui_t *gui, Widget parent)
{
  Widget     w;
  XmString   s;

    if (gui->commons != NULL) return;

    gui->commons = XtVaCreateManagedWidget("form", xmFormWidgetClass, parent,
                                           XmNshadowThickness, 0,
                                           NULL);

    w = NULL;

    if (gui->look.noleds == 0)
    {
        gui->local_leds_form = XtVaCreateManagedWidget("alarmLeds", xmFormWidgetClass, gui->commons,
                                                       XmNtraversalOn, False,
                                                       NULL);
        attachleft(gui->local_leds_form, w, NULL);

        w = gui->local_leds_form;
    }

    gui->roller = XtVaCreateManagedWidget("text_o", xmTextWidgetClass, gui->commons,
                                          XmNvalue, ".",
                                          XmNcursorPositionVisible, False,
                                          XmNcolumns,               1,
                                          XmNscrollHorizontal,      False,
                                          XmNeditMode,              XmSINGLE_LINE_EDIT,
                                          XmNeditable,              False,
                                          XmNtraversalOn,           False,
                                          NULL);
    attachleft(gui->roller, w, 0);
    w = gui->roller;

    fps_init(&(gui->fps_ctr));
    gui->fps_show = XtVaCreateManagedWidget("text_o", xmTextWidgetClass, gui->commons,
                                            XmNvalue, "",
                                            XmNcursorPositionVisible, False,
                                            XmNcolumns,               strlen("999fps "),
                                            XmNscrollHorizontal,      False,
                                            XmNeditMode,              XmSINGLE_LINE_EDIT,
                                            XmNeditable,              False,
                                            XmNtraversalOn,           False,
                                            NULL);
    attachleft(gui->fps_show, w, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w = gui->fps_show;

    gui->time_show = XtVaCreateManagedWidget("text_o", xmTextWidgetClass, gui->commons,
                                             XmNvalue, "",
                                             XmNcursorPositionVisible, False,
                                             XmNcolumns,               strlen(" hh:mm:ss"),
                                             XmNscrollHorizontal,      False,
                                             XmNeditMode,              XmSINGLE_LINE_EDIT,
                                             XmNeditable,              False,
                                             XmNtraversalOn,           False,
                                             NULL);
    attachleft(gui->time_show, w, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w = gui->time_show;

    if (gui->look.savebtn)
    {
        gui->save_button =
            XtVaCreateManagedWidget("miniToolButton", xmPushButtonWidgetClass, gui->commons,
                                    XmNlabelString, s = XmStringCreateLtoR(">F", xh_charset),
                                    XmNtraversalOn, False,
                                    NULL);
        XmStringFree(s);
        XhAssignPixmap(gui->save_button, btn_mini_save_xpm);
        XhSetBalloon  (gui->save_button, "Save data to file");
        XtAddCallback (gui->save_button, XmNactivateCallback, SaveBtnCB, gui);
        attachleft    (gui->save_button, w, 0);
        w = gui->save_button;
    }
    if (gui->look.noleds == 0) /*!!! "noleds"? Not "nocontrols"? */
    {
        gui->loop_button =
            XtVaCreateManagedWidget("miniToolButton", xmPushButtonWidgetClass, gui->commons,
                                    XmNlabelString, s = XmStringCreateLtoR(">", xh_charset),
                                    XmNtraversalOn, False,
                                    NULL);
        XmStringFree(s);
        XhAssignPixmap(gui->loop_button, btn_mini_start_xpm);
        XhSetBalloon  (gui->loop_button, "Periodic measurement");
        XtAddCallback (gui->loop_button, XmNactivateCallback, LoopBtnCB, gui);
        attachleft    (gui->loop_button, w, 0);
        w = gui->loop_button;

        gui->once_button =
            XtVaCreateManagedWidget("miniToolButton", xmPushButtonWidgetClass, gui->commons,
                                    XmNlabelString, s = XmStringCreateLtoR("|>", xh_charset),
                                    XmNtraversalOn, False,
                                    NULL);
        XmStringFree(s);
        XhAssignPixmap(gui->once_button, btn_mini_once_xpm);
        XhSetBalloon  (gui->once_button, "One measurement");
        XtAddCallback (gui->once_button, XmNactivateCallback, OnceBtnCB, gui);
        attachleft    (gui->once_button, w, 0);
        w = gui->once_button;
    }

    /*!!! should save XtAppAddTimeOut() for "destroy()" to be possible */
    XtAppAddTimeOut(xh_context, 1000, Fps10HzProc, gui);
}

static void LEDS_KeepaliveProc(XtPointer     closure,
                               XtIntervalId *id      __attribute__((unused)))
{
  pzframe_gui_t *gui = closure;

    XtAppAddTimeOut(xh_context, 1000, LEDS_KeepaliveProc, closure);
    MotifKnobs_leds_update(&(gui->leds));
}
void  PzframeGuiMkLeds     (pzframe_gui_t *gui)
{
  CxWidget  leds_grid;

    if (gui->local_leds_form == NULL) return;
    leds_grid = XhCreateGrid(gui->local_leds_form, "alarmLeds");
    attachtop   (leds_grid, NULL, 0);
    attachbottom(leds_grid, NULL, 0);
    XhGridSetGrid(leds_grid, 0, 0);
    MotifKnobs_leds_create(&(gui->leds),
                           leds_grid, -15,
                           gui->pfr->cid, MOTIFKNOBS_LEDS_PARENT_GRID);
    LEDS_KeepaliveProc((XtPointer)gui, NULL);
}

static void ParamKCB(DataKnob k, double v, void *privptr)
{
  pzframe_data_dpl_t *dpl    = privptr;
  pzframe_data_t     *pfr   = dpl->p;
  int                 cn    = dpl->n;

    if (pfr->cfg.readonly) return;

//fprintf(stderr, ":=%8.3f\n", v);
    cda_set_dcval(pfr->refs[cn], v);
}
Widget PzframeGuiMkparknob (pzframe_gui_t *gui,
                            Widget parent, const char *spec, int cn)
{
  pzframe_type_dscr_t *ftd = gui->pfr->ftd;
  int                  options = gui->pfr->cfg.readonly? SIMPLEKNOB_OPT_READONLY
                                                       : 0;

    if (cn >= ftd->chan_count)
    {
        fprintf(stderr, "%s(type_name=\"%s\"): request for invalid cn=%d (chan_count=%d)\n",
                __FUNCTION__, ftd->type_name, cn, ftd->chan_count);
        return XtVaCreateManagedWidget("", widgetClass, parent,
                                       XmNwidth,       10,
                                       XmNheight,      10,
                                       XmNborderWidth, 0,
                                       XmNbackground,  XhGetColor(XH_COLOR_JUST_RED),
                                       NULL);
    }

    if (ftd->chan_dscrs[cn].name == NULL)
        fprintf(stderr, "%s(type_name=\"%s\"): WARNING: request for unused cn=%d\n",
                __FUNCTION__, ftd->type_name, cn);

    if ((ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_RW_ONLY_MASK) != 0
        &&
        gui->pfr->cfg.readonly)
    {
        return XtVaCreateManagedWidget("", widgetClass, parent,
                                       XmNwidth,       1,
                                       XmNheight,      1,
                                       XmNborderWidth, 0,
                                       NULL);
    }

    gui->k_params[cn] = CreateSimpleKnob(spec, options, ABSTRZE(parent),
                                         ParamKCB, gui->pfr->dpls + cn);
    if (!gui->pfr->cfg.readonly  &&  0/*!!!gui->pfr->cfg.param_iv.p[pn] >= 0*/)
            SetSimpleKnobValue(gui->k_params[cn], 0/*!!!gui->pfr->cfg.param_iv.p[pn]*/);
    else    SetSimpleKnobState(gui->k_params[cn], KNOBSTATE_JUSTCREATED);

    return CNCRTZE(GetKnobWidget(gui->k_params[cn]));
}
