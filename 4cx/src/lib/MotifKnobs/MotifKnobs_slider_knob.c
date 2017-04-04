#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/ScrollBar.h>

#include "misc_macros.h"
#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_fallbacks.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_slider_knob.h"


static int  CreateHSlider(DataKnob k, CxWidget parent, int vertical)
{
}

static int  CreateHSlider(DataKnob k, CxWidget parent)
{
    return DoCreate(k, parent, 0);
}

static int  CreateVSlider(DataKnob k, CxWidget parent)
{
    return DoCreate(k, parent, 1);
}
}

static void SliderSetValue_m(DataKnob k, double v)
{
}

static void SliderColorize_m(DataKnob k, knobstate_t newstate)
{
}

dataknob_knob_vmt_t motifknobs_hslider_vmt =
{
    {DATAKNOB_KNOB, "hslider",
        sizeof(slider_privrec_t), text2slideropts,
        0,
        CreateHSlider, MotifKnobs_CommonDestroy_m,
        SliderColorize_m, NULL},
    SliderSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_vslider_vmt =
{
    {DATAKNOB_KNOB, "vslider",
        sizeof(slider_privrec_t), text2slideropts,
        0,
        CreateVSlider, MotifKnobs_CommonDestroy_m,
        Colorize_m, NULL},
    SliderSetValue_m, NULL
};
