#include "vcamimg_knobplugin.h"
#include "MotifKnobsP.h"

#include "DEVTYPE_LCASE_gui.h"
#include "DEVTYPE_LCASE_knobplugin.h"


static int DEVTYPE_UCASE_Create_m(DataKnob k, CxWidget parent)
{
    return VcamimgKnobpluginDoCreate(k, parent,
                                     NULL,
                                     DEVTYPE_LCASE_get_gui_dscr(),
                                     NULL, NULL);
}

dataknob_pzfr_vmt_t DEVTYPE_LCASE_pzfr_vmt =
{
    {DATAKNOB_PZFR, "DEVTYPE_LCASE",
        sizeof(vcamimg_knobplugin_t), NULL,
        0,
        DEVTYPE_UCASE_Create_m,  NULL/*Destroy*/,
        MotifKnobs_NoColorize_m, PzframeKnobpluginHandleCmd_m}
};
