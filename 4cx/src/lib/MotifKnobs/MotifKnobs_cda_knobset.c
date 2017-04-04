#include "datatreeP.h"
#include "KnobsP.h"

#include "MotifKnobs_cda.h"

#include "MotifKnobs_cda_scenario_knob.h"
#include "MotifKnobs_cda_leds_noop.h"


static knobs_knobset_t MotifKnobs_cda_knobset =
{
    (dataknob_unif_vmt_t *[]){
        (dataknob_unif_vmt_t *)&motifknobs_cda_scenario_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_cda_leds_vmt,
        NULL
    },
    NULL
};

void MotifKnobs_cdaRegisterKnobset(void)
{
    RegisterKnobset(&MotifKnobs_cda_knobset);
}
