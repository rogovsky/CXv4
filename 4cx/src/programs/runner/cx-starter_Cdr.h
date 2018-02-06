#ifndef __CX_STARTER_CDR_H
#define __CX_STARTER_CDR_H


#include "misc_macros.h"

#include "cda.h"
#include "Cdr.h"
#include "MotifKnobs_cda_leds.h"

#include "cx-starter_cfg.h"


typedef struct
{
    int             in_use;
    cfgline_t       cfg;

    DataSubsys      ds;

    CxWidget        btn;
    CxPixel         btn_defbg;
    CxWidget        a_led;
    CxWidget        d_led;
    rflags_t        oldrflags;

    MotifKnobs_leds_t  leds;
    int             oldleds_count;

    struct timeval  last_start_time;
} subsys_t;

typedef struct
{
    subsys_t *sys_list;
    int       sys_list_allocd;
} subsyslist_t;
// GetSubsysSlot()
GENERIC_SLOTARRAY_DECLARE(, Subsys, subsys_t,
                          subsyslist_t *sl, subsyslist_t *sl)


subsyslist_t *CreateSubsysListFromCfgBase(cfgbase_t *cfg);
int           RealizeSubsysList          (subsyslist_t *sl,
                                          cda_context_evproc_t evproc);
void          DestroySubsysList          (subsyslist_t *sl);


#endif /* __CX_STARTER_CDR_H */
