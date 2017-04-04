#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/PushB.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_grid_cont.h"


typedef struct
{
    CxWidget     box;
    CxPixel      deffg;
    CxPixel      defbg;
    CxPixel      defam;
    rflags_t     currflags;
    knobstate_t  curstate;
    
    Boolean      is_pressed;

    int8         resizable;
    int8         compact;
    int8         noclsbtn;
} subwin_privrec_t;

static psp_paramdescr_t text2subwinopts[] =
{
    PSP_P_FLAG   ("resizable", subwin_privrec_t, resizable, 1,           0),
    PSP_P_FLAG   ("compact",   subwin_privrec_t, compact,   1,           0),
    PSP_P_FLAG   ("noclsbtn",  subwin_privrec_t, noclsbtn,  1,           0),
    PSP_P_END()
};

static void DoColorize(DataKnob k)
{
  subwin_privrec_t *me = (subwin_privrec_t *)k->privptr;
  knobstate_t       colstate = me->curstate;
  CxPixel           fg       = me->deffg;
  CxPixel           bg       = me->defbg;
  CxPixel           am       = me->defam;

    if (colstate == KNOBSTATE_NONE)
    {
        if (me->currflags & CXCF_FLAG_ALARM_RELAX) colstate = KNOBSTATE_RELAX;
        if (me->currflags & CXCF_FLAG_OTHEROP)     colstate = KNOBSTATE_OTHEROP;
        if (me->currflags & CXCF_FLAG_ALARM_ALARM) /*colstate = ???*/;
    }
    if (me->is_pressed) bg = am;
    MotifKnobs_ChooseColors(k->colz_style, colstate, fg, bg, &fg, &bg);
    if (me->currflags & CXCF_FLAG_ALARM_ALARM) bg = XhGetColor(XH_COLOR_JUST_RED);
    XtVaSetValues(k->w,
                  XmNforeground, fg,
                  XmNbackground, bg,
                  NULL);
}

static void ReflectShowness(DataKnob k)
{
  subwin_privrec_t *me = (subwin_privrec_t *)k->privptr;
  Boolean           is_managed = XtIsManaged(CNCRTZE(me->box));

    if (is_managed == me->is_pressed) return;
    me->is_pressed = is_managed;

    XhInvertButton(ABSTRZE(k->w));
    XtVaSetValues(k->w,
                  XmNarmColor, me->is_pressed? me->defbg : me->defam,
                  NULL);
    DoColorize(k);
}

static void ShowHideSubwin(DataKnob k)
{
  subwin_privrec_t *me = (subwin_privrec_t *)k->privptr;

    if (XtIsManaged(CNCRTZE(me->box)))
        XhStdDlgHide(me->box);
    else
        XhStdDlgShow(me->box);

    ReflectShowness(k);
}

static void ActivateCB(Widget     w         __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data __attribute__((unused)))
{
  DataKnob          k  = (DataKnob)          closure;

    ShowHideSubwin(k);
}

static void Mouse3Handler(Widget     w,
                          XtPointer  closure,
                          XEvent    *event,
                          Boolean   *continue_to_dispatch)
{
  DataKnob             k  = (DataKnob)             closure;
  XButtonPressedEvent *ev = (XButtonPressedEvent *)event;
  
    if (ev->button != Button3) return;
    *continue_to_dispatch = False;
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);
    ShowHideSubwin(k);
}

static void CloseCB(Widget     w         __attribute__((unused)),
                    XtPointer  closure,
                    XtPointer  call_data __attribute__((unused)))
{
  DataKnob             k  = (DataKnob)             closure;

    ReflectShowness(k);
}

static int CreateSubwinCont(DataKnob k, CxWidget parent)
{
  subwin_privrec_t *me = (subwin_privrec_t *)k->privptr;
  char             *ls;
  XmString          s;

  int               n;
  DataKnob          ck;
  int               dlgflags;
  const char       *subwintitle;

    /* The [...] button... */
    if (!get_knob_label(k, &ls)) ls = "...";
    k->w = ABSTRZE(XtVaCreateManagedWidget("push_i",
                                           xmPushButtonWidgetClass,
                                           CNCRTZE(parent),
                                           XmNtraversalOn, True,
                                           XmNlabelString, s = XmStringCreateLtoR(ls , xh_charset),
                                           NULL));
    XmStringFree(s);
    XtAddCallback    (CNCRTZE(k->w), XmNactivateCallback, ActivateCB, (XtPointer)k);
    XtAddEventHandler(CNCRTZE(k->w), ButtonPressMask, False, Mouse3Handler, (XtPointer)k);
    MotifKnobs_TuneButtonKnob(k, k->w, 0);
    MotifKnobs_DecorateKnob(k);
    XtVaGetValues(k->w,
                  XmNforeground, &(me->deffg),
                  XmNbackground, &(me->defbg),
                  XmNarmColor,   &(me->defam),
                  NULL);
    me->curstate = KNOBSTATE_JUSTCREATED;
    DoColorize(k);
    
    /* Create a subwindow... */
    dlgflags = XhStdDlgFOk | XhStdDlgFNoDefButton;
    if (me->resizable) dlgflags |= XhStdDlgFResizable | XhStdDlgFZoomable;
    if (me->compact)   dlgflags |= XhStdDlgFNoMargins;
    if (me->noclsbtn)  dlgflags |= XhStdDlgFNothing;
    subwintitle = k->u.c.str3;
    if (subwintitle == NULL) subwintitle = k->tip;
    me->box = XhCreateStdDlg(XhWindowOf(parent), k->ident, subwintitle,
                             "Close", NULL, dlgflags);
    XtAddCallback(XtParent(CNCRTZE(me->box)), XmNpopdownCallback, CloseCB, (XtPointer)k);

    /* ...and populate it */
#if 1
    if (k->u.c.count > 0  &&
        CreateKnob(k->u.c.content, me->box) < 0) return -1;
#else
    for (n = 0, ck = k->u.c.content;
         n < k->u.c.count;
         n++)
    {
        if (CreateKnob(ck, me->box) < 0) return -1;
    }
#endif
    
    return 0;
}

static void DestroySubwinCont(DataKnob k)
{
  subwin_privrec_t *me = (subwin_privrec_t *)k->privptr;

    if (me->box != NULL) XtDestroyWidget(CNCRTZE(me->box));
    MotifKnobs_CommonDestroy_m(k);
}

static void SubwinNewData(DataKnob k, int synthetic)
{
  subwin_privrec_t *me = (subwin_privrec_t *)k->privptr;
  knobstate_t       newstate;

    MotifKnobs_CommonAlarmNewData_m(k, synthetic);

    newstate = choose_knobstate(NULL, k->currflags);
    if (me->currflags != k->currflags  ||
        me->curstate  != newstate)
    {
////fprintf(stderr, "%d,%s -> %d,%s\n", me->currflags, strknobstate_short(me->curstate), k->currflags, strknobstate_short(newstate));
        me->currflags = k->currflags;
        me->curstate  = newstate;
        DoColorize(k);
    }
}

dataknob_cont_vmt_t motifknobs_subwin_vmt =
{
    {DATAKNOB_CONT, "subwin",
        sizeof(subwin_privrec_t), text2subwinopts,
        0,
        CreateSubwinCont, DestroySubwinCont, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL,
#if 0 /* If "1" then alarm propagates to parent (which contains [Button]),
         instead of blinking in the content */
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m,
#else
    NULL, NULL,
#endif
    SubwinNewData, NULL, NULL, NULL
};
