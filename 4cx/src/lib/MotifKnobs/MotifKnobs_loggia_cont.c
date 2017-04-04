#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_grid_cont.h"


static int CreateLoggiaCont(DataKnob k, CxWidget parent)
{
  XhWindow  win = XhWindowOf(parent);
  CxWidget  tl  = XhGetToolbarLoggia(win);
  CxWidget  top;

    if (tl == NULL)
    {
        top =
        k->w = XtVaCreateManagedWidget("inlineLoggia",
                                        xmFormWidgetClass,
                                        parent,
                                        XmNshadowThickness, 0,
                                        NULL);
    }
    else
    {
        top = tl;
        k->w = XtVaCreateManagedWidget("XXX", widgetClass, parent,
                                       XmNwidth,       1,
                                       XmNheight,      1,
                                       XmNborderWidth, 0,
                                       NULL);
    }

    if (k->u.c.count > 0  &&
        CreateKnob(k->u.c.content, top) < 0) return -1;

    return 0;
}

dataknob_cont_vmt_t motifknobs_loggia_vmt =
{
    {DATAKNOB_CONT, "loggia",
        0, NULL,
        0,
        CreateLoggiaCont, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL,
#if 0 /* If "1" then alarm propagates to parent (which contains [Button]),
         instead of blinking in the content */
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m,
#else
    NULL, NULL,
#endif
    NULL, NULL, NULL, NULL
};
