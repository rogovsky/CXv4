
#include "datatree.h"
#include "datatreeP.h"
#include "Xh.h"
#include "Xh_globals.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_cda_leds.h"

#include "MotifKnobs_cda_leds_noop.h"


typedef struct
{
    MotifKnobs_leds_t  leds;
} leds_privrec_t;


static int CreateLedsNoop(DataKnob k, CxWidget parent)
{
  leds_privrec_t *me  = k->privptr;
  DataSubsys      sys = get_knob_subsys(k);

    if (sys == NULL) return -1;

    k->w = XhCreateGrid(parent, "alarmLeds");
    XhGridSetGrid   (k->w, 0, 0);
    XhGridSetPadding(k->w, 0, 0);

    MotifKnobs_leds_create(&(me->leds),
                           k->w, -15,
                           sys->cid, MOTIFKNOBS_LEDS_PARENT_GRID);

    return 0;
}

static void DestroyLedsNoop(DataKnob k)
{
  leds_privrec_t *me = k->privptr;

    MotifKnobs_leds_destroy(&(me->leds));

    if (k->w != NULL) XtDestroyWidget(CNCRTZE(k->w));
    k->w = NULL;
}

dataknob_noop_vmt_t motifknobs_cda_leds_vmt =
{
    {DATAKNOB_NOOP, "leds",
        sizeof(leds_privrec_t), NULL,
        0,
        CreateLedsNoop, DestroyLedsNoop, NULL, NULL}
};
