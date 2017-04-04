#include "fastadc_knobplugin.h"
#include "MotifKnobsP.h"

#include "two812ch_gui.h"
#include "two812ch_knobplugin.h"


static int TWO812CH_Create_m(DataKnob k, CxWidget parent)
{
    return FastadcKnobpluginDoCreate(k, parent,
                                     NULL,
                                     two812ch_get_gui_dscr(),
                                     NULL, NULL);
}

dataknob_pzfr_vmt_t two812ch_pzfr_vmt =
{
    {DATAKNOB_PZFR, "two812ch",
        sizeof(fastadc_knobplugin_t), NULL,
        0,
        TWO812CH_Create_m,       NULL/*Destroy*/,
        MotifKnobs_NoColorize_m, PzframeKnobpluginHandleCmd_m}
};
