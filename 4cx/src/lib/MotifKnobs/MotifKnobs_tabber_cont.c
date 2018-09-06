#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_tabber_cont.h"

#if XM_TABBER_AVAILABLE


#if (XmVERSION*1000+XmREVISION) < 2003
  #define _XmExt_h_
  #define XmNstackedEffect "stackedEffect"
  #define XmNtabSide "tabSide"
  #define XmNtabSelectedCallback "tabSelectedCallback"
  #define XmNtabSelectPixmap "tabSelectPixmap"
  #define XmNtabSelectColor "tabSelectColor"
  #define XmNtabRaisedCallback "tabRaisedCallback"
  #define XmNtabPixmapPlacement "tabPixmapPlacement"
  #define XmNtabOrientation "tabOrientation"
  #define XmNtabOffset "tabOffset"
  #define XmNtabMode "tabMode"
  #define XmNtabMarginWidth "tabMarginWidth"
  #define XmNtabMarginHeight "tabMarginHeight"
  #define XmNtabList "tabList"
  #define XmNtabLabelString "tabLabelString"
  #define XmNtabLabelSpacing "tabLabelSpacing"
  #define XmNtabLabelPixmap "tabLabelPixmap"
  #define XmNtabForeground "tabForeground"
  #define XmNtabEdge "tabEdge"
  #define XmNtabCornerPercent "tabCornerPercent"
  #define XmNtabBoxWidget "tabBoxWidget"
  #define XmNtabBackgroundPixmap "tabBackgroundPixmap"
  #define XmNtabBackground "tabBackground"
  #define XmNtabAutoSelect "tabAutoSelect"
  #define XmNtabArrowPlacement "tabArrowPlacement"
  #define XmNtabAlignment "tabAlignment"
  #define XmNuniformTabSize ((char*)&_XmStrings[1773])
#endif
#include <Xm/TabStack.h>
#include <Xm/Form.h>

typedef struct
{
    int tab_side;
    int stacked;
} tabber_privrec_t;

static psp_paramdescr_t text2tabberopts[] =
{
    PSP_P_FLAG("top",     tabber_privrec_t, tab_side, XmTABS_ON_TOP,    1),
    PSP_P_FLAG("bottom",  tabber_privrec_t, tab_side, XmTABS_ON_BOTTOM, 0),
    PSP_P_FLAG("left",    tabber_privrec_t, tab_side, XmTABS_ON_LEFT,   0),
    PSP_P_FLAG("right",   tabber_privrec_t, tab_side, XmTABS_ON_RIGHT,  0),
    PSP_P_FLAG("oneline", tabber_privrec_t, stacked,  0,                1),
    PSP_P_FLAG("stacked", tabber_privrec_t, stacked,  1,                0),
    PSP_P_END()
};

static int CreateTabberCont(DataKnob k, CxWidget parent)
{
  tabber_privrec_t *me = (tabber_privrec_t *)k->privptr;

  Widget            tabbox;

  int               n;
  Widget            intmdt;
  DataKnob          ck;
  char              lbuf[1000];
  char              tbuf[1000];
  char             *caption;
  char             *tip;
  XmString          s;

    k->w = ABSTRZE(
        XtVaCreateManagedWidget("tabStack", xmTabStackWidgetClass, CNCRTZE(parent),
                                XmNtabSide,         me->tab_side,
                                XmNtabMode,         me->stacked? XmTABS_STACKED:XmTABS_BASIC,
                                XmNuniformTabSize,  me->stacked?           True:False,
                                XmNstackedEffect,   False,
                                XmNtabOffset,       0,
                                XmNmarginWidth,     0,
                                XmNmarginHeight,    0,
                                XmNtabMarginWidth,  0,
                                XmNtabMarginHeight, 0,
                                NULL));
    /* A pseudo-fix for OpenMotif bug#1337 "Keyboard traversal in XmTabStack is nonfunctional" */
    if ((tabbox = XtNameToWidget(CNCRTZE(k->w), "tabBox")) != NULL)
        XtVaSetValues(tabbox, XmNtraversalOn, False, NULL);

    for (n = 0;  n < k->u.c.count;  n++)
    {
        intmdt = XtVaCreateManagedWidget("intmdtForm", xmFormWidgetClass, CNCRTZE(k->w),
                 XmNshadowThickness, 0,
                 NULL);
        ck = k->u.c.content + n;
        if (CreateKnob(ck, ABSTRZE(intmdt)) < 0) return -1;

        get_knob_rowcol_label_and_tip(k->u.c.str2, n, ck,
                                      lbuf, sizeof(lbuf), tbuf, sizeof(tbuf),
                                      &caption, &tip);
        if (*caption == '\0') caption = " "; /* An escape from OpenMotif bug#1346 <<XmTabStack segfaults with side tabs and XmNtabLabelString=="">> */
        XtVaSetValues(intmdt,
                      XmNtabLabelString, s = XmStringCreateLtoR(caption, xh_charset),
                      NULL);
        XmStringFree(s);
    }

    return 0;
}

dataknob_cont_vmt_t motifknobs_tabber_vmt =
{
    {DATAKNOB_CONT, "tabber",
        sizeof(tabber_privrec_t), text2tabberopts,
        0,
        CreateTabberCont, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL,
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m,
    MotifKnobs_CommonAlarmNewData_m, NULL, NULL, NULL, NULL
};


#endif /* XM_TABBER_AVAILABLE */
