#ifndef __VDEV_H
#define __VDEV_H


#include "cxsd_driver.h"
#include "cda.h"


//// Part 1: observer ////////////////////////////////////////////////

enum
{
    VDEV_IMPR = 1 << 0,  // Internally-IMPoRtant channel
    VDEV_PRIV = 1 << 1,  // Privately-used channel
    VDEV_TUBE = 1 << 2,  // Tube -- map our channel to target's one

    VDEV_DO_RD_CONV = 1 << 31, // Do NOT use CDA_DATAREF_OPT_NO_RD_CONV
};

typedef struct
{
    const char *name;
    int         mode;
    int         ourc;
    int         subdev_n;
    cxdtype_t   dtype;
    int         max_nelems;
} vdev_sodc_dsc_t;

typedef struct
{
    cda_dataref_t  ref;
    void          *current_val; // Pointer to current-value-buffer (if !=NULL)...
    CxAnyVal_t     v;           // Buffer for small-sized values; current_val=NULL
    rflags_t       flgs;
    cx_time_t      ts;
    int            current_nelems;
    int            rcvd;  // Was the value received?
} vdev_sodc_cur_t;

//// Part 2: state machine & Co. /////////////////////////////////////

typedef int  (*vdev_sw_alwd_t)(void *devptr, int prev_state);
typedef void (*vdev_swch_to_t)(void *devptr, int prev_state);

// Note: there's no "ctx" parameter for sodc_cb, since each device should use a SINGLE vdev, so devptr provides enough info
typedef void (*vdev_sodc_cb_t)(int devid, void *devptr,
                               int sodc, int32 val);

typedef struct
{
    int  ourc;
    int  state;
    int  enabled_val;
    int  disabled_val;
} vdev_sr_chan_dsc_t;

typedef struct
{
    int             delay_us;   // to wait after switch (in us; 0=>no-delay-required)
    int             next_state; // state-# to automatically switch in work_hbt() after specified delay or when sw_alwd; -1=>this-state-is-stable
    vdev_sw_alwd_t  sw_alwd;
    vdev_swch_to_t  swch_to;
    int            *impr_chans; // -1-terminated list of channels, which may affect readiness to switch to this state
} vdev_state_dsc_t;

typedef struct
{
    /* Driver-filled info */
    /* a. Data I/O */
    int                 num_sodcs;
    vdev_sodc_dsc_t    *map;
    vdev_sodc_cur_t    *cur;
    int                 devstate_count;
    const char        **devstate_names;
    vdev_sodc_cur_t    *devstate_cur;

    CxsdDevChanProc     do_rw;
    vdev_sodc_cb_t      sodc_cb;

    /* b. State machine */
    int                 our_numchans;
    int                 chan_state_n;
    int                 state_unknown_val;
    int                 state_determine_val;
    int                 state_count;
    vdev_state_dsc_t   *state_descr;
    vdev_sr_chan_dsc_t *state_related_channels;
    int                *state_important_channels; /* if !=NULL, this table must be [state_count*num_sodcs] */

    /* vdev-private data */
    int                 devid;
    void               *devptr;
    int                 work_hbt_period;
    sl_tid_t            work_hbt_tid;

    cda_context_t       cid;
    int                 cur_state;
    struct timeval      next_state_time;
    void               *buf;
} vdev_context_t;


int  vdev_init(vdev_context_t *ctx,
               int devid, void *devptr,
               int work_hbt_period,
               const char *base);
int  vdev_fini(vdev_context_t *ctx);

void vdev_set_state(vdev_context_t *ctx, int nxt_state);

void vdev_forget_all(vdev_context_t *ctx);

int  vdev_get_int(vdev_context_t *ctx, int sodc);


#endif /* __VDEV_H */
