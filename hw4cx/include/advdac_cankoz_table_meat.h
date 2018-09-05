//////////////////////////////////////////////////////////////////////

// a. General-purpose
static void ReturnInt32Vect(int devid, int chan, int32 *vp, int nelems, rflags_t rflags)
{
  static cxdtype_t  dt_int32    = CXDTYPE_INT32;

    ReturnDataSet(devid, 1,
                  &chan, &dt_int32, &nelems,
                  (void **)(&vp), &rflags, NULL);
}

// b. privrec-ignorant
enum
{
    TMODE_NONE = 0,
    TMODE_PREP = 1, // In fact, NONE and PREP are identical
    TMODE_LOAD = 2,
    TMODE_ACTV = 3,
    TMODE_RUNN = 4,
    TMODE_PAUS = 5,
};

static void ReportTableStatus(int devid, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
static void ReportTableStatus(int devid, const char *format, ...)
{
  int         r;
  va_list     ap;
  char        buf[100];
  int         len;
  void       *vp;

  static cxdtype_t dt_text     = CXDTYPE_TEXT;
  static int       ch_errdescr = KOZDEV_CHAN_OUT_TAB_ERRDESCR;
  static rflags_t  zero_rflags = 0;

    if (format != NULL)
    {
        va_start(ap, format);
        r = vsnprintf(buf, sizeof(buf) - 1, format, ap);
        va_end(ap);

        if      (r < 0)
            strcpy(buf, "INTERNAL ERROR upon ReportTableStatus()");
        else if (((unsigned int) r) > sizeof(buf) - 1)
            buf[sizeof(buf) - 1] = '\0';
    }
    else
        buf[0] = '\0';

    len = (int)(strlen(buf));
    vp  = &buf;
    ReturnDataSet(devid,
                  1,
                  &ch_errdescr, &dt_text, &len,
                  &vp, &zero_rflags, NULL);
}

// c. privrec-related
/*
 *  Note:
 *    SetTmode should NOT do anything but mangle with DO_TAB_ and OUT_MODE channels
 *    21.06.2017: also TAB_ERRDESCR
 */
static void SetTmode(privrec_t *me, int mode, const char *message)
{
  static int32 table_ctls[][KOZDEV_CHAN_DO_TAB_cnt] =
  {
      [TMODE_NONE] = {0, 0, 2, 2, 2, 2},
      [TMODE_PREP] = {0, 0, 2, 2, 2, 2},
      [TMODE_LOAD] = {2, 2, 2, 2, 2, 2},
      [TMODE_ACTV] = {0, 3, 0, 0, 2, 2},
      [TMODE_RUNN] = {2, 3, 3, 0, 0, 2},
      [TMODE_PAUS] = {2, 3, 2, 0, 3, 0},
  };
  static int32 no_unicast_v[KOZDEV_CHAN_DO_TAB_cnt] =
                     {0, 0, 0, 0, 2, 2};
  
  int          x;
    
    me->t_mode = mode;
    memcpy(me->ch_cfg + KOZDEV_CHAN_DO_TAB_base,
           &(table_ctls[mode]), sizeof(table_ctls[mode]));
    if (me->supports_unicast_t_ctl == 0)
        for (x = 0;  x < KOZDEV_CHAN_DO_TAB_cnt;  x++)
            me->ch_cfg[KOZDEV_CHAN_DO_TAB_base + x] |= no_unicast_v[x];

    for (x = 0;  x < KOZDEV_CHAN_DO_TAB_cnt;  x++)
        ReturnCtlCh(me, KOZDEV_CHAN_DO_TAB_base + x);

    me->ch_cfg[KOZDEV_CHAN___T_MODE] =  mode;
    me->ch_cfg[KOZDEV_CHAN_OUT_MODE] = (mode < TMODE_LOAD? KOZDEV_OUT_MODE_NORM
                                                         : KOZDEV_OUT_MODE_TABLE);
    ReturnCtlCh(me, KOZDEV_CHAN___T_MODE);
    ReturnCtlCh(me, KOZDEV_CHAN_OUT_MODE);
    if (message != NULL)
        ReportTableStatus(me->devid, "%s", message);
}

static void EraseTable   (privrec_t *me, int wipe_channels, const char *message)
{
  int  l;

    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 2, DESC_FILE_CREATE, (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID]);
    me->lvmt->q_enq_v    (me->handle, SQ_ALWAYS, 2, DESC_FILE_CLOSE,  (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID]);

    if (wipe_channels)
    {
        me->t_times_nsteps = 0;
        bzero(me->t_nsteps, sizeof(me->t_nsteps));
        ReturnInt32Vect(me->devid, KOZDEV_CHAN_OUT_TAB_TIMES, me->t_times, me->t_times_nsteps, 0);
        for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
            ReturnInt32Vect(me->devid, KOZDEV_CHAN_OUT_TAB_n_base + l, me->t_vals[l], me->t_nsteps[l], 0);
    }
    for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
    {
        if (me->out[l].aff) SendRdRq(me, l);
        me->out[l].aff = 0;
        me->out[l].lkd = 0;
    }

    /* Request a "receipt" */
    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, DESC_GET_DAC_STAT);

    SetTmode(me, TMODE_NONE, message);
}

static void CrunchTableIntoFile(privrec_t *me);

static void DoTabActivate(privrec_t *me)
{
  int    l;
  int    fc; // "First Channel" -- among affected
  int    tr;
  int    nsteps;
  int32  spd_per_step;

  int32  this_val;
  int32  prev_val;

  int    f_ofs;
  int    f_wrn;

  uint8  data[8];
  uint8  dsep[1];

    /*****************************************************************
        Scenario:
            0. Check everything:
               a. Presence of table(s)
               b. Consistency of tables
               c. Whether jumps from current to initial values are within speed limits
               d. Whether changes are within speed limits
            1. (Dev-specific) Create file
            2. Lock participating channels, confiscating from slowmo, if used
            3. Pre-program initial values
            4. Send file to device, ending with DESC_FILE_CLOSE
    *****************************************************************/

    /* 0. Check everything */
    
    /* a. Presence of table(s) */
    if (me->t_times_nsteps < 1)
    {
        ReportTableStatus(me->devid, "ACTIVATE: no table (times are empty)");
        return;
    }
    /* Which channels are affected? */
    for (fc = -1, l = DEVSPEC_CHAN_OUT_n_count;  l >= 0;  l--)
    {
        me->out[l].aff = (me->t_nsteps[l] > 0  &&
                          me->t_vals  [l][0] > DEVSPEC_TABLE_VAL_DISABLE_CHAN);
        if (me->out[l].aff) fc = l;
    }
    if (fc < 0)
    {
        ReportTableStatus(me->devid, "ACTIVATE: no table (no active channels)");
        return;
    }

    /* b. Consistency of tables */
    for (l = fc;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
        if (me->out[l].aff  &&  me->t_nsteps[l] != me->t_times_nsteps)
        {
            ReportTableStatus(me->devid, "ACTIVATE: line%d nsteps=%d, !=times_nsteps=%d",
                              l, me->t_nsteps[l], me->t_times_nsteps);
            return;
        }

    for (l = fc;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
        if (me->out[l].aff  &&  me->t_vals[l][0] >= DEVSPEC_TABLE_VAL_START_FROM_CUR  &&
            me->out[l].rcv == 0)
        {
            ReportTableStatus(me->devid, "ACTIVATE: line%d START-FROM-CUR (step0=%duV), but cur value isn't received yet",
                              l, me->t_vals[l][0]);
            return;
        }

    /* c. Whether jumps from current to initial values are within speed limits */
    for (l = fc;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
        if (me->out[l].aff  &&
            me->out[l].spd != 0) /* Is speed limit present? */
        {
            prev_val = me->out[l].cur;
            this_val = me->t_vals[l][0];
            if (this_val < DEVSPEC_TABLE_VAL_START_FROM_CUR  &&
                !
                (
                 /* Is it an absolute-value-decrement? */
                 (
                  me->out[l].spd > 0  &&
                  abs(this_val) < abs(prev_val)  &&
                  (
                   this_val == 0  ||
                   (
                    (this_val > 0  &&  prev_val > 0)  ||
                    (this_val < 0  &&  prev_val < 0)
                   )
                  )
                 )
                 ||
                 /* Or is this step less than speed? */
                 (
                  abs(this_val - prev_val) <= abs(me->out[l].spd) / HEARTBEAT_FREQ
                 )
                )
               )
            {
                ReportTableStatus(me->devid, "ACTIVATE: line%d too-fast jump from cur to first (cur=%duV, step0=%duV, spd=%duV/s, frq=%d)",
                                  l, prev_val, this_val, me->out[l].spd, HEARTBEAT_FREQ);
                return;
            }
        }

    /* d. Whether changes are within speed limits */
    for (l = fc;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
        if (me->out[l].aff  &&
            me->out[l].spd != 0) /* Is speed limit present? */
        {
            spd_per_step = abs(me->out[l].spd) / DEVSPEC_TABLE_STEPS_PER_SEC;
            if (spd_per_step < 1) spd_per_step = 1;
            prev_val = me->t_vals[l][0];
            if (prev_val >= DEVSPEC_TABLE_VAL_START_FROM_CUR)
                prev_val = me->out[l].cur;
            for (tr = 1;  tr < me->t_times_nsteps;  tr++)
            {
                nsteps = me->t_times[tr] / DEVSPEC_TABLE_USECS_IN_STEP;
                if (nsteps < 1) nsteps = 1;

                this_val = me->t_vals[l][tr];
                if (!
                    (
                     /* Is it an absolute-value-decrement? */
                     (
                      me->out[l].spd > 0  &&
                      abs(this_val) < abs(prev_val)  &&
                      (
                       this_val == 0  ||
                       (
                        (this_val > 0  &&  prev_val > 0)  ||
                        (this_val < 0  &&  prev_val < 0)
                       )
                      )
                     )
                     ||
                     /* Or is this step less than speed? */
                     (
                      abs(this_val - prev_val) / nsteps <= spd_per_step
                     )
                    )
                   )
                {
                    ReportTableStatus(me->devid, "ACTIVATE: line%d too-fast step %d (prev=%duV, this=%duV, time=%dus, spd=%duV/s)",
                                      l, tr, prev_val, this_val, nsteps * DEVSPEC_TABLE_USECS_IN_STEP, me->out[l].spd);
                    return;
                }

                prev_val = this_val;
            }
        }

    /* 1. (Dev-specific) Create file */
    CrunchTableIntoFile(me);

    SetTmode(me, TMODE_LOAD, "Activating");

    /* 2. Lock participating channels, confiscating from slowmo, if used */
    for (l = fc;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
    {
        me->out[l].lkd = me->out[l].aff;
        if (me->out[l].aff  &&  me->out[l].isc)
        {
            me->num_isc--;
            me->out[l].isc = 0;
        }
    }

    /* 3. Pre-program initial values */
    for (l = fc;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
        if (me->out[l].aff)
        {
            SendWrRq(me, l, me->t_vals[l][0] < DEVSPEC_TABLE_VAL_START_FROM_CUR? me->t_vals[l][0] : me->out[l].cur);
            SendRdRq(me, l);
        }

    if (me->t_times_nsteps == 1)
    {
        SetTmode(me, TMODE_ACTV, NULL);
        SetTmode(me, TMODE_NONE, "1-step init-only done");
        for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
            me->out[l].lkd = 0;
        return;
    }

    /* 4. Send file to device, ending with DESC_FILE_CLOSE */
    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 2, DESC_FILE_CREATE, (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID]);
    data[0] = DESC_FILE_WR_SEQ;
#ifdef DEVSPEC_TLOAD_SEP_DESC
    dsep[0] = DEVSPEC_TLOAD_SEP_DESC;
#else
    dsep[0] = CANKOZ_DESC_GETDEVSTAT;
#endif
    for (f_ofs = 0;  f_ofs < me->t_file_bytes;  f_ofs += f_wrn)
    {
        f_wrn = me->t_file_bytes - f_ofs;
        if (f_wrn > 7) f_wrn = 7;
        memcpy(data + 1, me->t_file + f_ofs, f_wrn);
        me->lvmt->q_enq_ons(me->handle, SQ_ALWAYS, 1 + f_wrn, data);
        me->lvmt->q_enq_ons(me->handle, SQ_ALWAYS, 1,         dsep);
    }
    me->lvmt->q_enq_v    (me->handle, SQ_ALWAYS, 2, DESC_FILE_CLOSE,  (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID]);
}

static void HandleTableHbt(privrec_t *me)
{
  int        l;       // Line #

    /* Request current values of table-driven channels */
    if (me->t_mode == TMODE_RUNN)
        for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
            if (me->out[l].aff) SendRdRq(me, l);

    /* Request GET_DAC_STAT once a second if table is present */
    me->t_GET_DAC_STAT_hbt_ctr++;
    if (!TAB_MODE_IS_NORM()  &&
        (me->t_GET_DAC_STAT_hbt_ctr < 0  ||
         me->t_GET_DAC_STAT_hbt_ctr >= HEARTBEAT_FREQ))
    {
        me->t_GET_DAC_STAT_hbt_ctr = 0;
        me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1, DESC_GET_DAC_STAT);
    }
}

static void HandleDESC_FILE_CLOSE  (privrec_t *me, int desc, size_t dlc, uint8 *data)
{
    /* Note: we ignore bad packets (i.e. if() is BEFORE erase_and_send_next())
       for re-request to be sent, since otherwise a CLOSE packet would never be sent again */
    if (dlc < 4) return;
    me->lvmt->q_erase_and_send_next_v(me->handle, 2, desc, (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID]);
    
    /*!!! Note: here should analyze current t_mode and
      if it is <TMODE_LOAD -- i.e., TAB_MODE_IS_NORM() -- then EraseTable(,0) */
    /*!!!
      and CLOSE packet(s?) should be sent at init */

    DoDriverLog(me->devid, 0, "DESC_FILE_CLOSE: ID=%02X size=%d (file_bytes=%d)",
                data[1], data[2] + data[3]*256, me->t_file_bytes);
    if (me->t_mode == TMODE_LOAD)
    {
        if (data[2] + data[3]*256 == me->t_file_bytes)
        {
            SetTmode(me, TMODE_ACTV, "READY");
        }
        else
        {
            EraseTable(me, 0, "DESC_FILE_CLOSE: file size mismatch!!!");
        }
    }
}

static void HandleDESC_GET_DAC_STAT(privrec_t *me,
                                    int        desc,
                                    size_t     dlc,
                                    uint8     *data)
{
    /* Note: we require only 1st data byte to be present,
             albeit "good" packets should be 7 bytes long */
    if (dlc < 2) return;
#if 0
    DoDriverLog(me->devid, DRIVERLOG_NOTICE*0 | 0*DRIVERLOG_C_PKTINFO,
                "0xFD, ID=0x%x, mode=%d stat=0x%02x %zd:0x%02x,1x%02x,2x%02x,3x%02x,4x%02x,5x%02x,6x%02x,7x%02x",
                data[2], me->t_mode, data[1], dlc, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
#else
    DoDriverLog(me->devid, DRIVERLOG_NOTICE*0 | 1*DRIVERLOG_C_PKTINFO,
                "0xFD, ID=0x%x, mode=%d stat=0x%02x",
                data[2], me->t_mode, data[1]);
#endif
    if      (me->t_mode == TMODE_RUNN  &&
             ((data[1] & (DAC_STAT_EXECING|DAC_STAT_EXEC_RQ)) == 0))
    {
        EraseTable(me, 0, "END");
    }
    /*!!! Note: here should try to deduce "true" table mode
      and act depending on current known t_mode */
    else if (0){}
    else if (me->t_mode == TMODE_ACTV  &&
             ((data[1] & (DAC_STAT_EXECING|DAC_STAT_EXEC_RQ)) != 0))
    {
        SetTmode(me, TMODE_RUNN, "detected START");
    }
    else if (me->t_mode == TMODE_RUNN  &&
             ((data[1] & (DAC_STAT_PAUSED|DAC_STAT_PAUS_RQ)) != 0)  &&
             ((data[1] & (DAC_STAT_CONT_RQ)) == 0)
            )
    {
        SetTmode(me, TMODE_PAUS, "detected PAUSE");
    }
    else if (me->t_mode == TMODE_PAUS  &&
             (
              ((data[1] & (DAC_STAT_CONT_RQ)) != 0)  ||
              ((data[1] & (DAC_STAT_PAUSED|DAC_STAT_PAUS_RQ)) == 0)
             )
            )
    {
        SetTmode(me, TMODE_RUNN, "detected RESUME");
    }
    me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
}

static void OnSendTabCtlCmdCB(int         devid __attribute__((unused)),
                              void       *devptr,
                              sq_try_n_t  try_n __attribute__((unused)),
                              void       *privptr)
{
  privrec_t  *me   = (privrec_t *)devptr;
  int         mode = ptr2lint(privptr);
  const char *msg;
  int         l;

    /*!!! Maybe ?*/
    if (1) me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1, DESC_GET_DAC_STAT);

    if      (mode == TMODE_NONE)
    {
        EraseTable(me, 0, "STOP");
        return;
    }

    msg = NULL;
    if      (mode == TMODE_PAUS) msg = "Pausing";
    else if (mode == TMODE_RUNN) msg = "Starting";
    SetTmode(me, mode, msg);

    if (mode == TMODE_PAUS)
        for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
            if (me->out[l].aff) SendRdRq(me, l);
}
static void SendTabCtlCmd(privrec_t *me, int cmd, int mode)
{
    if (cmd == DESC_FILE_START  || /* Note: START is ALWAYS sent synchronously */
        me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                              SQ_TRIES_DIR, 0,
                              OnSendTabCtlCmdCB, lint2ptr(mode),
                              0, 3,
                              cmd, (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID], 0) == SQ_ERROR)
        me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                              SQ_TRIES_ONS, 0,
                              OnSendTabCtlCmdCB, lint2ptr(mode),
                              0, 3,
                              cmd, (TABLE_NO << 4) | me->ch_cfg[KOZDEV_CHAN_OUT_TAB_ID], 0);
}

static void HandleTable_rw(privrec_t *me,
                           int action, int chn,
                           int n,
                           cxdtype_t *dtypes, int *nelems, void **values)
{
  int          l;     // Line #
  int32        val;   // Value -- in volts
  int          nel;   // NELems, =nelems[n]
  int          step;  // For tables

    if (action == DRVA_WRITE  &&
        (chn == KOZDEV_CHAN_OUT_TAB_TIMES  ||  chn == KOZDEV_CHAN_OUT_TAB_ALL
         ||
         (chn >= KOZDEV_CHAN_OUT_TAB_n_base  &&
          chn <  KOZDEV_CHAN_OUT_TAB_n_base + DEVSPEC_CHAN_OUT_n_count)
        )
       )
    {
        if ((dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
            return;
        val = 0xFFFFFFFF; // That's just to make GCC happy
    }
    else if (action == DRVA_WRITE) /* Classic driver's dtype-check */
    {
        if (nelems[n] != 1  ||
            (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
            return;
        val = *((int32*)(values[n]));
        ////fprintf(stderr, " write #%d:=%d\n", chn, val);
    }
    else
        val = 0xFFFFFFFF; // That's just to make GCC happy

    /* Process table-control channels similarly */
    if      (chn >= KOZDEV_CHAN_DO_TAB_base  &&
             chn <  KOZDEV_CHAN_DO_TAB_base + KOZDEV_CHAN_DO_TAB_cnt)
    {
        if (action == DRVA_WRITE        &&
            val    == CX_VALUE_COMMAND  &&
            (me->ch_cfg[chn] & CX_VALUE_DISABLED_MASK) == 0)
        {
            if      (chn == KOZDEV_CHAN_DO_TAB_DROP)
                EraseTable   (me, 1, "DROP");
            else if (chn == KOZDEV_CHAN_DO_TAB_ACTIVATE)
                DoTabActivate(me);
            else if (chn == KOZDEV_CHAN_DO_TAB_START)
                SendTabCtlCmd(me, DESC_FILE_START,    TMODE_RUNN);
            else if (chn == KOZDEV_CHAN_DO_TAB_STOP)
                SendTabCtlCmd(me, DESC_U_FILE_STOP,   TMODE_NONE);
            else if (chn == KOZDEV_CHAN_DO_TAB_PAUSE)
                SendTabCtlCmd(me, DESC_U_FILE_PAUSE,  TMODE_PAUS);
            else if (chn == KOZDEV_CHAN_DO_TAB_RESUME)
                SendTabCtlCmd(me, DESC_U_FILE_RESUME, TMODE_RUNN);
        }

        /* This is for ack of WRITE actions only,
           as READs are processed as all other CONFIGs */
        ReturnCtlCh(me, chn);
    }
    else if (chn == KOZDEV_CHAN_OUT_TAB_ID)
    {
        if (action == DRVA_WRITE  &&
            TAB_MODE_IS_NORM())
        {
            if (val < 0)   val = 0;
            if (val > 0xF) val = 0xF;
            me->ch_cfg[chn] = val;
        }
        ReturnCtlCh(me, chn);
    }
    else if (chn == KOZDEV_CHAN_OUT_TAB_TIMES)
    {
        if (action == DRVA_WRITE  &&  TAB_MODE_IS_NORM())
        {
            nel = nelems[n];
            if (nel > DEVSPEC_TABLE_MAX_NSTEPS) nel = DEVSPEC_TABLE_MAX_NSTEPS;

            me->t_times_nsteps = nel;
            if (nel > 0)
            {
                memcpy(me->t_times, values[n], nel * sizeof(int32));
                for (step = 0;  step < nel;  step++)
                {
                    if (me->t_times[step] < 0)
                        me->t_times[step] = 0;
                    if (me->t_times[step] > DEVSPEC_TABLE_MAX_STEP_COUNT * DEVSPEC_TABLE_USECS_IN_STEP)
                        me->t_times[step] = DEVSPEC_TABLE_MAX_STEP_COUNT * DEVSPEC_TABLE_USECS_IN_STEP;
                }
            }
        }
        ReturnInt32Vect(me->devid, chn, me->t_times, me->t_times_nsteps, 0);
    }
    else if (chn == KOZDEV_CHAN_OUT_TAB_ALL)
    {
        if (action == DRVA_WRITE  &&  TAB_MODE_IS_NORM())
        {
        }
    }
    else if (chn >= KOZDEV_CHAN_OUT_TAB_n_base  &&
             chn <  KOZDEV_CHAN_OUT_TAB_n_base + DEVSPEC_CHAN_OUT_n_count)
    {
        l = chn - KOZDEV_CHAN_OUT_TAB_n_base;
        if (action == DRVA_WRITE  &&  TAB_MODE_IS_NORM())
        {
            nel = nelems[n];
            if (nel > DEVSPEC_TABLE_MAX_NSTEPS) nel = DEVSPEC_TABLE_MAX_NSTEPS;

            me->t_nsteps[l] = nel;
            if (nel > 0)
            {
                memcpy(me->t_vals[l], values[n], nel * sizeof(int32));
                for (step = 0;  step < nel;  step++)
                {
                    if (step == 0  &&
                        (me->t_vals[l][0] <= DEVSPEC_TABLE_VAL_DISABLE_CHAN  ||
                         me->t_vals[l][0] >= DEVSPEC_TABLE_VAL_START_FROM_CUR))
                    {/* Don't perform checks, leave special signal-values as is */}
                    else 
                    {
                        if (me->t_vals[l][step] < MIN_ALWD_VAL)
                            me->t_vals[l][step] = MIN_ALWD_VAL;
                        if (me->t_vals[l][step] > MAX_ALWD_VAL)
                            me->t_vals[l][step] = MAX_ALWD_VAL;
                        /* Run-time-specified min/max checks */
                        if (me->out[l].min < me->out[l].max)
                        {
                            if (me->t_vals[l][step] < me->out[l].min)
                                me->t_vals[l][step] = me->out[l].min;
                            if (me->t_vals[l][step] > me->out[l].max)
                                me->t_vals[l][step] = me->out[l].max;
                        }
                    };
                }
            }
        }
        ReturnInt32Vect(me->devid, chn, me->t_vals[l], me->t_nsteps[l], 0);
    }
    else if (chn == KOZDEV_CHAN_OUT_TAB_ERRDESCR)
    {/* Do-nothing, is returned upon change */}
}

//////////////////////////////////////////////////////////////////////
