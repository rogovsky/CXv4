#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/ScrolledW.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_scroll_cont.h"


typedef struct
{
    int  width;
    int  height;
} scroll_privrec_t;

static psp_paramdescr_t text2scrollopts[] =
{
    PSP_P_INT  ("width",   scroll_privrec_t, width,  0, 0, 0),
    PSP_P_INT  ("height",  scroll_privrec_t, height, 0, 0, 0),

    PSP_P_END(),
};

static int CreateScrollCont(DataKnob k, CxWidget parent)
{
  scroll_privrec_t *me = (scroll_privrec_t *)k->privptr;

    k->w = ABSTRZE(
        XtVaCreateManagedWidget("scrolledView",
                                xmScrolledWindowWidgetClass,
                                CNCRTZE(parent),
                                XmNscrollingPolicy, XmAUTOMATIC,
                                XmNvisualPolicy,    XmVARIABLE,
                                NULL));
    if (me->width  > 0) XtVaSetValues(k->w, XmNwidth,  me->width,  NULL);
    if (me->height > 0) XtVaSetValues(k->w, XmNheight, me->height, NULL);

    if (k->u.c.count > 0  &&
        CreateKnob(k->u.c.content, k->w) < 0) return -1;

    return 0;
}

dataknob_cont_vmt_t motifknobs_scroll_vmt =
{
    {DATAKNOB_CONT, "scroll",
        sizeof(scroll_privrec_t), text2scrollopts,
        0,
        CreateScrollCont, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL,
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m,
    MotifKnobs_CommonAlarmNewData_m, NULL, NULL, NULL, NULL
};
