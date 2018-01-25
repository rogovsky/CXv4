#include <stdio.h>
#include <errno.h>

#include "misclib.h"

#include "cxsd_driver.h"
#include "cda.h"


enum
{
    HEARTBEAT_USECS = 1000000/100*10,
};

enum {MAX_CHANS = 1000};
typedef struct
{
    int            devid;
    const char    *auxinfo;
    int            do_deregister;
    cda_context_t  cid;
    cda_dataref_t  refs[MAX_CHANS];
} privrec_t;

static void chan_evproc(int            uniq,
                        void          *devptr,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  privrec_t    *me = devptr;
  int32         v;
  int           r;
  static int32  zero_val = 0;

    cda_get_ref_data(ref, 0, sizeof(v), &v);
    if (v != 0)
    fprintf(stderr, "%s chan#%ld(ref=%d) %d\n", __FUNCTION__, ptr2lint(privptr2), ref, v);
    if (v == 12345  &&  ref == me->refs[101])
    {
        cda_snd_ref_data(ref, CXDTYPE_INT32, 1, &zero_val);
        r = cda_del_context(me->cid); me->cid = CDA_CONTEXT_ERROR;
        fprintf(stderr, "DELETING=%d:%d\n", r, errno);
    }
}
static void DoCdaAction(privrec_t *me, int do_register, int do_log)
{
  int  r;
  int  x;
  char buf[100];

    if (do_register)
    {
        me->cid = cda_new_context(me->devid, me,
                                  me->auxinfo, 0,
                                  NULL,
                                  0,
                                  NULL, NULL);
        if (me->cid < 0)
        {
            fprintf(stderr, "%s: cid=%d: %s\n", __FUNCTION__, me->cid, strerror(errno));
            return;
        }
        for (x = 0;  x < countof(me->refs);  x++)
        {
            sprintf(buf, "%d", x + 1);
            me->refs[x] = cda_add_chan(me->cid, NULL, buf, 0,
                                       CXDTYPE_INT32, 1,
                                       CDA_REF_EVMASK_UPDATE, chan_evproc, lint2ptr(x));
        }
        fprintf(stderr, "%s cid=%d, refs[0]=%d refs[%d]=%d\n",
                strcurtime(), me->cid, me->refs[0], MAX_CHANS-1, me->refs[MAX_CHANS-1]);
    }
    else
    {
        errno = 0;
        r = cda_del_context(me->cid); me->cid = CDA_CONTEXT_ERROR;
        fprintf(stderr, "del_c=%d:%d\n", r, errno);
        for (x = 0;  x < countof(me->refs);  x++)
            me->refs[x] = CDA_DATAREF_ERROR;
    }
}

static void test_cda_del_hbt(int devid, void *devptr,
                             sl_tid_t tid  __attribute__((unused)),
                             void *privptr __attribute__((unused)))
{
  privrec_t *me = devptr;

    DoCdaAction(me, !(me->do_deregister), 0);
    me->do_deregister = !(me->do_deregister);

    sl_enq_tout_after(me->devid, me, HEARTBEAT_USECS, test_cda_del_hbt, NULL);
}

static int  test_cda_del_init_d(int devid, void *devptr,
                                int businfocount, int businfo[],
                                const char *auxinfo)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, DRIVERLOG_NOTICE, "%s([%d], <%s>:\"%s\")",
                __FUNCTION__, businfocount, GetDevTypename(devid), auxinfo);

    me->devid = devid;

    if (auxinfo == NULL)
    {
        DoDriverLog(devid, DRIVERLOG_NOTICE, "auxinfo is NULL; terminating");
        return DEVSTATE_OFFLINE;
    }
    if ((me->auxinfo = strdup(auxinfo)) == NULL)
    {
        DoDriverLog(devid, DRIVERLOG_NOTICE, "strdup(auxinfo) fail; terminating");
        return DEVSTATE_OFFLINE;
    }

    DoCdaAction(me, 1, 1);
    me->do_deregister = 1;

    sl_enq_tout_after(me->devid, me, HEARTBEAT_USECS, test_cda_del_hbt, NULL);

    return DEVSTATE_OPERATING;
}

static void test_cda_del_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, DRIVERLOG_NOTICE, "%s()", __FUNCTION__);
    DoCdaAction(me, 0, 0);
    safe_free(me->auxinfo); me->auxinfo = NULL;
}

// w3i {result:0; input:1; coeff:2}
static void test_cda_del_rw_p(int devid, void *devptr,
                              int action,
                              int count, int *addrs,
                              cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;

  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

#if 0
        if      (chn == 0)
        {
            ReturnInt32Datum    (devid, chn, 0, 0);
        }
        else if (chn == 1)
        {
            if (action == DRVA_READ)
                ReturnInt32Datum(devid, chn, 0, 0);
            else
            {
                ReturnInt32Datum(devid, chn, val, 0);
                ReturnInt32Datum(devid, 0,   val, 0);
            }
        }
        else
        {
            if (action == DRVA_READ)
                ReturnInt32Datum(devid, chn, 0,   0);
            else
            {
                ReturnInt32Datum(devid, chn, val, 0);
                SetChanRDs      (devid, 0, 1, val, 0);
            }
        }
#endif
 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(test_cda_del, "Driver for cda_del_* testing",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 1000,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   test_cda_del_init_d, test_cda_del_term_d, test_cda_del_rw_p);
