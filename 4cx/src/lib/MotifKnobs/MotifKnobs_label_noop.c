#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Label.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_label_noop.h"


static int CreateLabelNoop(DataKnob k, CxWidget parent, int vertical)
{
  char          *text;
  XmString       s;
  unsigned char  halign = XmALIGNMENT_CENTER;
  int            valign = 0;
  
    if (!get_knob_label(k, &text)) text = "";
    
    k->w =
        ABSTRZE(XtVaCreateManagedWidget("rowlabel",
                                        xmLabelWidgetClass,
                                        CNCRTZE(parent),
                                        XmNlabelString, s = XmStringCreateLtoR(text, xh_charset),
                                        XmNalignment,   halign,
                                        NULL));
    XmStringFree(s);
                                
    if (vertical)
        XhAssignVertLabel(k->w, NULL, valign);

    return 0;
}

static int CreateHLabel(DataKnob k, CxWidget parent)
{
    return CreateLabelNoop(k, parent, 0);
}

static int CreateVLabel(DataKnob k, CxWidget parent)
{
    return CreateLabelNoop(k, parent, 1);
}

static int CreateDefNoop(DataKnob k, CxWidget parent)
{
    return CreateLabelNoop(k, parent, 0);
}

dataknob_noop_vmt_t motifknobs_hlabel_vmt =
{
    {DATAKNOB_NOOP, "hlabel",
        0, NULL,
        0,
        CreateHLabel, MotifKnobs_CommonDestroy_m, NULL, NULL}
};

dataknob_noop_vmt_t motifknobs_vlabel_vmt =
{
    {DATAKNOB_NOOP, "vlabel",
        0, NULL,
        0,
        CreateVLabel, MotifKnobs_CommonDestroy_m, NULL, NULL}
};

dataknob_noop_vmt_t motifknobs_def_noop_vmt =
{
    {DATAKNOB_NOOP, DEFAULT_KNOB_LOOK_NAME,
        0, NULL,
        0,
        CreateVLabel, MotifKnobs_CommonDestroy_m, NULL, NULL}
};
