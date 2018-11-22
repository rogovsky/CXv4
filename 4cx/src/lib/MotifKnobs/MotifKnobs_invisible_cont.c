#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_invisible_cont.h"


typedef struct
{
    int with_content;
} invisible_privrec_t;

static psp_paramdescr_t text2invisibleopts[] =
{
    PSP_P_FLAG("no_content",   invisible_privrec_t, with_content, 0, 1),
    PSP_P_FLAG("with_content", invisible_privrec_t, with_content, 1, 0),
    PSP_P_END()
};

static int CreateInvisibleCont(DataKnob k, CxWidget parent)
{
  invisible_privrec_t *me = (invisible_privrec_t *)k->privptr;

  int                  n;

    if (me->with_content)
    {
        k->w = XtVaCreateManagedWidget("invisible", compositeWidgetClass, parent,
                                       XmNwidth,       1,
                                       XmNheight,      1,
                                       XmNborderWidth, 0,
                                       XmNsensitive,   0,
                                       NULL);
        XtVaCreateManagedWidget("umbrella", widgetClass, k->w,
                                XmNwidth,       1,
                                XmNheight,      1,
                                XmNborderWidth, 0,
                                NULL);
        for (n = 0;
             n < k->u.c.count;
             n++)
        {
            if (CreateKnob(k->u.c.content + n, k->w) < 0) return -1;
        }
    }
    else
        k->w = XtVaCreateManagedWidget("invisible", widgetClass, parent,
                                       XmNwidth,       1,
                                       XmNheight,      1,
                                       XmNborderWidth, 0,
                                       NULL);

    return 0;
}

dataknob_cont_vmt_t motifknobs_invisible_vmt =
{
    {DATAKNOB_CONT, "invisible",
        sizeof(invisible_privrec_t), text2invisibleopts,
        0,
        CreateInvisibleCont, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL, NULL, NULL, NULL
};
