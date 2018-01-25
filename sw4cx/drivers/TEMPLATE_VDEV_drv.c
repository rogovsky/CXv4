#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/kurrez_cac208_drv_i.h"


enum
{

    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t sodc2ourc_mapping[SUBORD_NUMCHANS] =
{
};

static int ourc2sodc[KURREZ_CAC208_NUMCHANS];

static const char *devstate_names[] = {"_devstate"};

enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

//--------------------------------------------------------------------

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];
    
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    REZ_STATE_UNKNOWN   = 0,
    REZ_STATE_DETERMINE,         // 1

    REZ_STATE_count
};

//--------------------------------------------------------------------
static void SwchToUNKNOWN(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    vdev_forget_all(&me->ctx);
}

static void SwchToDETERMINE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

}

static vdev_state_dsc_t state_descr[] =
{
    [REZ_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [REZ_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},

};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int kurrez_cac208_init_mod(void)
{
  int  sn;
  int  x;

    /* Fill interesting_chan[][] mapping */
    bzero(state_important_channels, sizeof(state_important_channels));
    for (sn = 0;  sn < countof(state_descr);  sn++)
        if (state_descr[sn].impr_chans != NULL)
            for (x = 0;  state_descr[sn].impr_chans[x] >= 0;  x++)
                state_important_channels[sn][state_descr[sn].impr_chans[x]] = 1;

    /* ...and ourc->sodc mapping too */
    for (x = 0;  x < countof(ourc2sodc);  x++)
        ourc2sodc[x] = -1;
    for (x = 0;  x < countof(sodc2ourc_mapping);  x++)
        if (sodc2ourc_mapping[x].mode != 0  &&  sodc2ourc_mapping[x].ourc >= 0)
            ourc2sodc[sodc2ourc_mapping[x].ourc] = x;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void kurrez_cac208_rw_p(int devid, void *devptr,
                               int action,
                               int count, int *addrs,
                               cxdtype_t *dtypes, int *nelems, void **values);
static void kurrez_cac208_sodc_cb(int devid, void *devptr,
                                  int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {-1,                         -1,                     0,                 0},
};

static int kurrez_cac208_init_d(int devid, void *devptr,
                                int businfocount, int businfo[],
                                const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;

    me->devid = devid;
    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-CAC208-device name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = sodc2ourc_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = kurrez_cac208_rw_p;
    me->ctx.sodc_cb        = kurrez_cac208_sodc_cb;

    me->ctx.our_numchans             = KURREZ_CAC208_NUMCHANS;
    me->ctx.chan_state_n             = KURREZ_CAC208_CHAN_VDEV_STATE;
    me->ctx.state_unknown_val        = REZ_STATE_UNKNOWN;
    me->ctx.state_determine_val      = REZ_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    /*!!!SetChanRDs, SetChanReturnType*/

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, p);
}

static void kurrez_cac208_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void kurrez_cac208_sodc_cb(int devid, void *devptr,
                                  int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == REZ_STATE_UNKNOWN) return;
}

static vdev_sr_chan_dsc_t *find_sdp(int ourc)
{
  vdev_sr_chan_dsc_t *sdp;

    for (sdp = state_related_channels;
         sdp != NULL  &&  sdp->ourc >= 0;
         sdp++)
        if (sdp->ourc == ourc) return sdp;

    return NULL;
}

static void kurrez_cac208_rw_p(int devid, void *devptr,
                               int action,
                               int count, int *addrs,
                               cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t          *me = (privrec_t *)devptr;
  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value
  int                 sodc;
  vdev_sr_chan_dsc_t *sdp;

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

        sodc = ourc2sodc[chn];

        if      (sodc >= 0  &&  sodc2ourc_mapping[sodc].mode & VDEV_TUBE)
        {
            if (action == DRVA_WRITE)
                SndCVal(me, sodc, val);
            else
                if (me->cur[sodc].rcvd)
                    ReturnInt32DatumTimed(devid, chn, me->cur[sodc].v.i32,
                                          me->cur[sodc].flgs,
                                          me->cur[sodc].ts);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(kurrez_cac208, "CAC208-controlled rezonator by Kurkin",
                   kurrez_cac208_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   kurrez_cac208_init_d, kurrez_cac208_term_d, kurrez_cac208_rw_p);
