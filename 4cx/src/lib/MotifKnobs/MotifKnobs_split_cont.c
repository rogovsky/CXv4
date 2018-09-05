#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Paned.h>
#include <Xm/PanedW.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_split_cont.h"


typedef struct
{
    int is_horizontal;
} split_privrec_t;

static psp_paramdescr_t text2splitopts[] =
{
    PSP_P_FLAG  ("vertical",   split_privrec_t, is_horizontal, 0, 1),
    PSP_P_FLAG  ("horizontal", split_privrec_t, is_horizontal, 1, 0),

    PSP_P_END(),
};

static int CreateSplitCont(DataKnob k, CxWidget parent)
{
  split_privrec_t *me = (split_privrec_t *)k->privptr;

  int              n;

    k->w = ABSTRZE(
        XtVaCreateManagedWidget("splitView",
                                xmPanedWidgetClass,
                                CNCRTZE(parent),
                                XmNorientation, me->is_horizontal? XmHORIZONTAL
                                                                 : XmVERTICAL,
                                NULL));

    for (n = 0;
         n < k->u.c.count;
         n++)
    {
        if (CreateKnob(k->u.c.content + n, k->w) < 0) return -1;
    }

    return 0;
}

dataknob_cont_vmt_t motifknobs_split_vmt =
{
    {DATAKNOB_CONT, "split",
        sizeof(split_privrec_t), text2splitopts,
        0,
        CreateSplitCont, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL,
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m,
    MotifKnobs_CommonAlarmNewData_m, NULL, NULL, NULL, NULL
};
