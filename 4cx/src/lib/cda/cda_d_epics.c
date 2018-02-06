#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "misc_macros.h"
#include "cxscheduler.h"

#include "cda.h"
#include "cdaP.h"

#include "cda_d_epics.h"


/* From libCom/osi/epicsTime.h::POSIX_TIME_AT_EPICS_EPOCH */
static long diff_1990_1970 = 631152000u;

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;

    cda_dataref_t  dataref; // "Backreference" to corr.entry in the global table

    char          *name;
} hwrinfo_t;

typedef struct
{
    cda_hwcnref_t  hwr;
    const char    *name;
} hwrname_t;

typedef struct
{
    cda_srvconn_t  sid;

    hwrinfo_t     *hwrs_list;
    int            hwrs_list_allocd;
} cda_d_epics_privrec_t;

enum
{
    HWR_MIN_VAL   = 1,
    HWR_MAX_COUNT = 1000000,  // An arbitrary limit
    HWR_ALLOC_INC = 100,
};


//--------------------------------------------------------------------

// GetHwrSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Hwr, hwrinfo_t,
                                 hwrs, in_use, 0, 1,
                                 HWR_MIN_VAL, HWR_ALLOC_INC, HWR_MAX_COUNT,
                                 me->, me,
                                 cda_d_epics_privrec_t *me, cda_d_epics_privrec_t *me)

static void RlsHwrSlot(cda_hwcnref_t hwr, cda_d_epics_privrec_t *me)
{
  hwrinfo_t *hi = AccessHwrSlot(hwr, me);

    if (hi->in_use == 0) return; /*!!! In fact, an internal error */
    hi->in_use = 0;

    safe_free(hi->name);
}

//////////////////////////////////////////////////////////////////////


static int  cda_d_epics_new_chan(cda_dataref_t ref, const char *name,
                                 int options,
                                 cxdtype_t dtype, int nelems)
{
  cda_d_epics_privrec_t *me;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(epics), NULL, CDA_DAT_P_GET_SERVER_OPT_NONE);
    if (me == NULL) return CDA_DAT_P_ERROR;

    hwr = GetHwrSlot(me);
    if (hwr < 0) return CDA_DAT_P_ERROR;
    hi = AccessHwrSlot(hwr, me);
    if ((hi->name = strdup(name)) == NULL)
    {
        RlsHwrSlot(hwr, me);
        return CDA_DAT_P_ERROR;
    }

    return CDA_DAT_P_NOTREADY;
}

//////////////////////////////////////////////////////////////////////

static int  cda_d_epics_new_srv (cda_srvconn_t  sid, void *pdt_privptr,
                                 int            uniq,
                                 const char    *srvrspec,
                                 const char    *argv0,
                                 int            srvtype  __attribute((unused)))
{
  cda_d_epics_privrec_t *me = pdt_privptr;
  int                    ec;

////fprintf(stderr, "ZZZ %s(%s)\n", __FUNCTION__, srvrspec);
    me->sid = sid;

    return CDA_DAT_P_NOTREADY;
}

static int  cda_d_epics_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_epics_privrec_t *me = pdt_privptr;

    return CDA_DAT_P_DEL_SRV_SUCCESS;  
}

//////////////////////////////////////////////////////////////////////

static int  cda_d_epics_init_m(void)
{
#if 0
  struct tm t1990;

    bzero(&t1990, sizeof(t1990));
    t1990.tm_mday = 1;
    t1990.tm_year = 1990 - 1900;
    diff_1990_1970 = mktime(&t1990);
    /*!!! This gives 631126800, while EPICS uses diff=631152000, which is 7hrs less; why? 7 is NSK time, GMT+6/7.  timegm() could save us, but it is Linux-specific... */
    /*!!! Should just use diff_1990_1970=631152000  */
#endif
////fprintf(stderr, "1990-1970=%ld\n", diff_1990_1970);

    return 0;
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_DAT_PLUGIN(epics, "EPICS (Channel Access) data-access plugin",
                      cda_d_epics_init_m, NULL,
                      sizeof(cda_d_epics_privrec_t),
                      '.', ':', '@',
                      cda_d_epics_new_chan, NULL,
                      NULL,                 NULL,
                      cda_d_epics_new_srv,  cda_d_epics_del_srv);
