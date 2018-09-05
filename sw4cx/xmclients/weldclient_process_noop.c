
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "MotifKnobsP.h"

#include "weldclient_process_noop.h"


typedef struct
{
} privrec_t;

static psp_paramdescr_t text2processopts[] =
{
    PSP_P_END()
};

static int CreateWeldclientProcessNoop(DataKnob k, CxWidget parent)
{
  privrec_t *me = (privrec_t *)k->privptr;

}

static void DestroyWeldclientProcessNoop(DataKnob k)
{
}

dataknob_noop_vmt_t weldclient_process_noop_vmt =
{
    {DATAKNOB_NOOP, "weldclient_process",
        sizeof(privrec_t), text2processopts,
        0,
        CreateWeldclientProcessNoop, DestroyWeldclientProcessNoop, NULL, NULL}
};