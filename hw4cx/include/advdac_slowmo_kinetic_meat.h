//////////////////////////////////////////////////////////////////////

// a. Declarations
#define SHOW_SET_IMMED 1

//typedef struct
//{
//    void            *devptr;
//    cankoz_vmt_t    *lvmt;
//    int              devid;
//    int              handle;
//
//    int              n_count;
//    int              max_nsteps;
//
//    advdac_out_ch_t *outs;
//
//    int              _num_isc;
//    int              t_mode;
//} advdac_descr_t;

enum
{
    DO_CALC_R_SET_IMMED = 0,
    DO_CALC_R_SLOW_MOVE = 1,
};
enum {DO_DBG=0};
#ifndef DO_RETURN_CUR_PARAMS
#define DO_RETURN_CUR_PARAMS 0
#endif /* DO_RETURN_CUR_PARAMS */
#if DO_RETURN_CUR_PARAMS
enum
{
    CHAN_CURSPD = KOZDEV_CHAN_ADC_n_base + 98,
    CHAN_CURACC = KOZDEV_CHAN_ADC_n_base + 99,
};
#endif

// b. privrec-ignorant
static int DoCalcMovement(int32 trg, advdac_out_ch_t *ci, advdac_out_ch_t *rp)
{
  int32  mxs;
  int32  a1;
  int32  a2;
  int32  distance;
  int32  d_half;
  int32  pos;
  int32  spd;
  int32  prv;

    /* No speed specified => just set the value immediately */
    if (ci->spd == 0) return DO_CALC_R_SET_IMMED;
    /* Speed specified but accel isn't => go monotonously
       (with tac<=0 the HandleSlowmoHbt() would use just .spd, so
        there's no need to fill rp->'s fields) */
    if (ci->tac <= 0)
    {
        /* ...but we zero them anyway, to protect HandleSlowmoOUT_rw() from touching uninitialized garbage */
        bzero(rp, sizeof(*rp));

        return DO_CALC_R_SLOW_MOVE;
    }

    mxs = abs(ci->spd) / HEARTBEAT_FREQ;
    a1 = mxs / ci->tac;  if (a1 == 0) a1 = 1;
    a2 = a1 / 2;         if (a2 == 0) a2 = 1;

    distance = abs(trg - ci->cur);
    d_half   = distance / 2 - a2*0;

    pos = 0;
    spd = a2;
    prv = 0;
    while (pos/* + spd*/ <= d_half  &&  spd  < mxs)
    {
        prv = pos;
        pos += spd;
        if (DO_DBG) fprintf(stderr, "%7d@%-7d\n", pos, spd);
        spd += a1;
        if (spd > abs(ci->spd))
            spd = abs(ci->spd);
    }
    rp->mxs = mxs;
    rp->crs = a2;
    rp->mns = a2;
    rp->acs = a1;
    rp->dcd = (pos < d_half)? pos : d_half;
    if (DO_DBG) fprintf(stderr, "%d->%d: distance=%d d_half=%d pos=%d,prv=%d dcd=%d mxs=%d\n", ci->cur, trg, distance, d_half, pos, prv, rp->dcd, rp->mxs);

    return DO_CALC_R_SLOW_MOVE;
}

// c. privrec-related
static void HandleSlowmoHbt(privrec_t *me)
{
  int         l;       // Line #
  int32       val;     // Value -- in volts

    for (l = 0;  l < countof(me->out);  l++)
        if (me->out[l].isc  &&  me->out[l].nxt)
        {
            if (1 && me->out[l].tac > 0)
            {
                if (0)
                {
                }
                /* We use separate branches for ascend and descend
                   to simplify arithmetics and escape juggling with sign */
                else if (me->out[l].trg > me->out[l].dsr)
                {
                    val = me->out[l].dsr + me->out[l].crs;
                    if (val >= me->out[l].trg)
                    {
                        val =  me->out[l].trg;
                        me->out[l].fin = 1;
                    }
                }
                else
                {
                    val = me->out[l].dsr - me->out[l].crs;
                    if (val <= me->out[l].trg)
                    {
                        val =  me->out[l].trg;
                        me->out[l].fin = 1;
                    }
                }

                if      (abs(me->out[l].trg - val) <= me->out[l].dcd)
                {
                if (DO_DBG) fprintf(stderr, "-1 %+10d %+10d %8d %7d -%-7d", me->out[l].dsr, me->out[l].cur, -abs(me->out[l].trg - val), me->out[l].crs, me->out[l].acs);
                    me->out[l].crs -= (me->out[l].crs < me->out[l].mxs)? me->out[l].acs : me->out[l].mns;
                    if (me->out[l].crs < me->out[l].mns)
                        me->out[l].crs = me->out[l].mns;
                if (DO_DBG) fprintf(stderr, " %d\n", me->out[l].crs);
#if DO_RETURN_CUR_PARAMS
                ReturnInt32Datum(me->devid, CHAN_CURSPD,  me->out[l].crs, 0);
                ReturnInt32Datum(me->devid, CHAN_CURACC, -me->out[l].acs, 0);
#endif
                }
                else if (me->out[l].crs < me->out[l].mxs)
                {
                if (DO_DBG) fprintf(stderr, "+1 %+10d %+10d %8d %7d +%-7d", me->out[l].dsr,  me->out[l].cur, -abs(me->out[l].trg - val), me->out[l].crs, me->out[l].acs);
                    me->out[l].crs += me->out[l].acs;
                    if (me->out[l].crs > me->out[l].mxs)
                        me->out[l].crs = me->out[l].mxs;
                if (DO_DBG) fprintf(stderr, " %d\n", me->out[l].crs);
#if DO_RETURN_CUR_PARAMS
                ReturnInt32Datum(me->devid, CHAN_CURSPD,  me->out[l].crs, 0);
                ReturnInt32Datum(me->devid, CHAN_CURACC, +me->out[l].acs, 0);
#endif
                }
                else
                if (DO_DBG) fprintf(stderr, " 0 %+10d %+10d %8d %7d +%-7d %d\n", me->out[l].dsr,  me->out[l].cur, -abs(me->out[l].trg - val), me->out[l].crs, me->out[l].acs, me->out[l].crs);

                me->out[l].nxt = 0; // Drop the "next may be sent..." flag
                SendWrRq(me, l, val);
                SendRdRq(me, l);
            }
            else
            {
                /* Guard against dropping speed to 0 in process */
                if      (me->out[l].spd == 0)
                {
                    val =  me->out[l].trg;
                    me->out[l].fin = 1;
                }
                /* We use separate branches for ascend and descend
                   to simplify arithmetics and escape juggling with sign */
                else if (me->out[l].trg > me->out[l].dsr)
                {
                    val = me->out[l].dsr + abs(me->out[l].spd) / HEARTBEAT_FREQ;
                    if (val >= me->out[l].trg)
                    {
                        val =  me->out[l].trg;
                        me->out[l].fin = 1;
                    }
                }
                else
                {
                    val = me->out[l].dsr - abs(me->out[l].spd) / HEARTBEAT_FREQ;
                    if (val <= me->out[l].trg)
                    {
                        val =  me->out[l].trg;
                        me->out[l].fin = 1;
                    }
                }

                me->out[l].nxt = 0; // Drop the "next may be sent..." flag
                SendWrRq(me, l, val);
                SendRdRq(me, l);
            }

            me->out[l].dsr = val;
        }
}

static void HandleSlowmoREADDAC_in(privrec_t *me, int l, int32 val, rflags_t rflags)
{
    me->out[l].cur = val;
    me->out[l].rcv = 1;
    me->out[l].nxt = 1;

    if (abs(me->out[l].dsr - me->out[l].cur) >= (THE_QUANT+1))
        me->out[l].dsr = me->out[l].cur;

    if (me->out[l].fin)
    {
        if (me->out[l].isc) me->num_isc--;
        me->out[l].isc = 0;
    }

    /* Return data: */
    // - Current
    ReturnInt32Datum(me->devid, DEVSPEC_CHAN_OUT_CUR_n_base + l,
                     me->out[l].cur, rflags);
    // - Immediate-channel, if present
    if (DEVSPEC_CHAN_OUT_IMM_n_base > 0)
        ReturnInt32Datum(me->devid, DEVSPEC_CHAN_OUT_IMM_n_base + l,
                         me->out[l].cur, rflags);
    // Setting, if slowmo is not in action
    if (!me->out[l].isc  ||  !SHOW_SET_IMMED)
        ReturnInt32Datum(me->devid, DEVSPEC_CHAN_OUT_n_base + l,
                         me->out[l].cur, rflags);
    // Setting, if first answer at begin of slowmo
    else if (me->out[l].fst)
    {
        me->out[l].fst = 0;
        ReturnInt32Datum(me->devid, DEVSPEC_CHAN_OUT_n_base + l,
                         val_to_daccode_to_val(me->out[l].trg), rflags);
    }
}

static void HandleSlowmoOUT_rw     (privrec_t *me, int chn,
                                    int action, int32 val)
{
  int              l;     // Line #
  advdac_out_ch_t *ci;

    //if (action == DRVA_WRITE) fprintf(stderr, "\t[%d]:=%d\n", chn, val);
    l  = chn - DEVSPEC_CHAN_OUT_n_base;
    ci = me->out + l;
    /* May we touch this channel now? */
    if (ci->lkd) return;
    
    if (action == DRVA_WRITE)
    {
        /* Prevent integer overflow/slip */
        if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
        if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;

        if (ci->min < ci->max)
        {
            if (val < ci->min) val = ci->min;
            if (val > ci->max) val = ci->max;
        }

        /* ...and how should we perform the change: */
        if (// No mangling with now-changing channels...
            (!ci->isc  ||  ci->spd == 0  ||  ci->tac <= 0) // ...at least with kinetic ones
            &&
            (
             /* No speed limit? */
             ci->spd == 0
             ||
             /* Or is it an absolute-value-decrement? */
             (
              ci->spd > 0  &&
              abs(val) < abs(ci->cur)  &&
              (
               val == 0  ||
               (
                (val > 0  &&  ci->cur > 0)  ||
                (val < 0  &&  ci->cur < 0)
               )
              )
             )
             ||
             /* Or is this step less than speed? */
             (
              abs(val - ci->cur) < abs(ci->spd) / HEARTBEAT_FREQ
             )
            )
           )
        /* Just do write?... */
        {
            SendWrRq(me, l, val);
            /* Stop slowmo */
            if (ci->isc)
            {
                me->num_isc--;
                ci->isc = 0;
            }
        }
        else
        /* ...or initiate slow change? */
        {
          advdac_out_ch_t ccr;

            if (DoCalcMovement(val, me->out + l, &ccr) == DO_CALC_R_SET_IMMED)
            {
                SendWrRq(me, l, val); /*!!! G-r-r-r!!! Duplicate!!!  */
                /* Stop slowmo */
                if (ci->isc)
                {
                    me->num_isc--;
                    ci->isc = 0;
                }
            }
            else
            {
                ci->mxs = ccr.mxs;
                ci->crs = ccr.crs;
                ci->mns = ccr.mns;
                ci->acs = ccr.acs;
                ci->dcd = ccr.dcd;

                if (ci->isc == 0) me->num_isc++;
                ci->dsr = ci->cur;
                ci->trg = val;
                ci->isc = 1;
                ci->fst = 1;
                ci->fin = 0;
            }
        }
    }

    /* Perform read anyway */
    SendRdRq(me, l);
}

static void HandleSlowmoOUT_IMM_rw (privrec_t *me, int chn,
                                    int action, int32 val)
{
  int          l;     // Line #

    l = chn - DEVSPEC_CHAN_OUT_IMM_n_base;
    /* May we touch this channel now? */
    if (me->out[l].lkd) return;
    
    if (action == DRVA_WRITE)
    {
        /* Stop slowmo */
        if (me->out[l].isc)
        {
            me->num_isc--;
            me->out[l].isc = 0;
        }

        /* Prevent integer overflow/slip */
        if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
        if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;

        if (me->out[l].min < me->out[l].max)
        {
            if (val < me->out[l].min) val = me->out[l].min;
            if (val > me->out[l].max) val = me->out[l].max;
        }

        SendWrRq(me, l, val);
    }

    /* Perform read anyway */
    SendRdRq(me, l);
}

static void HandleSlowmoOUT_RATE_rw(privrec_t *me, int chn,
                                    int action, int32 val)
{
  int          l;     // Line #

    l = chn - DEVSPEC_CHAN_OUT_RATE_n_base;
    if (action == DRVA_WRITE)
    {
        /* Note: no bounds-checking for value, since it isn't required */
        if (abs(val) < THE_QUANT/*!!!*/) val = 0;
        me->out[l].spd = val;
    }
    ReturnInt32Datum(me->devid, chn, me->out[l].spd, 0);
}

static void HandleSlowmoOUT_TAC_rw (privrec_t *me, int chn,
                                    int action, int32 val)
{
  int          l;     // Line #

    l = chn - DEVSPEC_CHAN_OUT_TAC_n_base;
    if (action == DRVA_WRITE)
    {
        /* Note: no bounds-checking for value, since it isn't required */
        if (val < -1)   val = -1;
        if (val > 1000) val = 1000;
        me->out[l].tac = val;
    }
    ReturnInt32Datum(me->devid, chn, me->out[l].tac, 0);
}

//////////////////////////////////////////////////////////////////////
