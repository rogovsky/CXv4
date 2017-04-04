#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Separator.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_separator_noop.h"


static int CreateSeparatorKnob(DataKnob k, CxWidget parent, int vertical)
{
  unsigned char  orientation = vertical? XmVERTICAL : XmHORIZONTAL;
  unsigned char  septype     = XmSHADOW_ETCHED_IN;

    k->w =
      ABSTRZE(XtVaCreateManagedWidget("separator",
                                      xmSeparatorWidgetClass,
                                      CNCRTZE(parent),
                                      XmNorientation,        orientation,
                                      XmNseparatorType,      septype,
                                      XmNhighlightThickness, 0,
                                      NULL));

    return 0;
}

static int CreateHSeparator(DataKnob k, CxWidget parent)
{
    return CreateSeparatorKnob(k, parent, 0);
}

static int CreateVSeparator(DataKnob k, CxWidget parent)
{
    return CreateSeparatorKnob(k, parent, 1);
}

dataknob_noop_vmt_t motifknobs_hseparator_vmt =
{
    {DATAKNOB_NOOP, "hseparator",
        0, NULL,
        0,
        CreateHSeparator, MotifKnobs_CommonDestroy_m, NULL, NULL}
};

dataknob_noop_vmt_t motifknobs_vseparator_vmt =
{
    {DATAKNOB_NOOP, "vseparator",
        0, NULL,
        0,
        CreateVSeparator, MotifKnobs_CommonDestroy_m, NULL, NULL}
};
