#include <stdarg.h>

#include "cxsd_driver.h"
#include "cankoz_lyr.h"
#include "cankoz_numbers.h"
#include "cankoz_strdb.h"

#ifndef CANHAL_FILE_H
  #error The "CANHAL_FILE_H" macro is undefined
#else
  #include CANHAL_FILE_H
#endif /* CANHAL_FILE_H */

#ifndef CANLYR_NAME
  #error The "CANLYR_NAME" macro is undefined
#endif /* CANLYR_NAME */


enum {CANKOZ_MAXPKTBYTES = 8};

enum
{
    CANKOZ_DEFAULT_TIMEOUT    =  1 *  100 * 1000, // 100 ms
    CANKOZ_0xFF_TIMEOUT       = 10 * 1000 * 1000, // 10 s
    CANKOZ_KEEPALIVE_TIMEOUT  = 60 * 1000 * 1000, // 1 m /*!!! What is this?!*/
    CANKOZ_MISTRUST_TIMEOUT   = 60 * 1000 * 1000, // 1 m pause before first ping
    CANKOZ_CONFIDENCE_TIMEOUT =  2 * 1000 * 1000, // 2 s interval between successing pings
};

enum {DEVCODE_CHK_COUNT = 5};

typedef struct
{
    sq_eprops_t       props;

    CanKozOnSendProc  on_send;
    void             *privptr;

    int               prio;
    int               dlc;     /* 'dlc' MUST be signed (instead of size_t) in order to permit partial-comparison (triggered by negative dlc) */
    uint8             data[CANKOZ_MAXPKTBYTES];
} canqelem_t;

enum
{
    NUMLINES    = 16,  // Was raised from 8 @12.02.2015, to support Canner's with 5xPCI 2-port adapters
    DEVSPERLINE = 64
};

typedef struct
{
    /* Driver properties */
    int             line;     // For back-referencing when passed just dp
    int             kid;      // The same
    int             devid;
    void           *devptr;
    int             devcodes[10];
    CanKozOnIdProc  ffproc;
    CanKozPktinProc inproc;
    int             options;

    /* Device properties */
    int             hw_ver;
    int             sw_ver;
    int             hw_code;

    /* Support */
    sq_q_t          q;
    sl_tid_t        tid;
    int             devcode_chk_ctr;

    /* I/O regs support */
    struct
    {
        int    base;
        int    rcvd;
        int    pend;
        
        uint8  cur_val;
        uint8  req_val;
        uint8  req_msk;
        uint8  cur_inp;
    }               ioregs;
} kozdevinfo_t;

typedef struct
{
    int           fd;
    sl_fdh_t      fdhandle;
    int           last_rd_r;
    int           last_wr_r;
    int           last_ex_id;
    kozdevinfo_t  devs[DEVSPERLINE];
} lineinfo_t;


static int log_frame_allowed = 1;

static int         my_lyrid;

static lineinfo_t  lines[NUMLINES];

static inline int  lp2line(lineinfo_t *lp)
{
    return lp - lines;
}

static inline int  encode_handle(int line, int kid)
{
    return (line << 16)  |  kid;
}

static inline void decode_handle(int handle, int *line_p, int *kid_p)
{
    *line_p = handle >> 16;
    *kid_p  = handle & 63;
}

static int devid2handle(int devid)
{
  int  l;
  int  d;

    for (l = 0;  l < countof(lines);  l++)
        for (d = 0;  d < countof(lines[l].devs);  d++)
            if (lines[l].devs[d].devid == devid)
                return encode_handle(l, d);

    return -1;
}

static inline int  encode_frameid(int kid, int prio)
{
    return (kid << 2) | (prio << 8);
}

static inline void decode_frameid(int frameid, int *kid_p, int *prio_p)
{
    *kid_p  = (frameid >> 2) & 63;
    *prio_p = (frameid >> 8) & 7;
}

static void log_frame(int devid, const char *pfx,
                      int id, int dlc, uint8 *data)
{
  int   x;
  int   kid;
  int   prio;
  int   desc;
  char  data_s[100];
  char *p;

    if (!log_frame_allowed) return;

    decode_frameid(id, &kid, &prio);
    desc = dlc > 0? data[0] : -1;
    
    for (x = 0, p = data_s;  x < dlc;  x++)
        p += sprintf(p, "%s%02X", x == 0? "" : ",", data[x]);
  
    DoDriverLog(devid, 0 | DRIVERLOG_C_PKTDUMP,
                "%s id=%u, kid=%d, prio=%d, desc=0x%02X data[%d]={%s}",
                pfx, id, kid, prio, desc, dlc, data_s);
}


static int SendFrame(lineinfo_t *lp, int kid,
                     int prio, int dlc, uint8 *data)
{
  int   r;
  int   errflg;
  char *errstr;
    
    r = canhal_send_frame(lp->fd, encode_frameid(kid, prio), dlc, data);
    log_frame(prio == CANKOZ_PRIO_BROADCAST    ||
              kid < 0  ||  kid >= DEVSPERLINE  ||
              lp->devs[kid].devid <= 0 ? my_lyrid
                                       : lp->devs[kid].devid,
              "SNDFRAME", encode_frameid(kid, prio), dlc, data);
    
    if (r != CANHAL_OK  &&  r != lp->last_wr_r)
    {
        errflg = 0;
        errstr = "";
        if      (r == CANHAL_ZERO)   errstr = ": ZERO";
        else if (r == CANHAL_BUSOFF) errstr = ": BUSOFF";
        else                         errflg = DRIVERLOG_ERRNO;
        
        DoDriverLog(my_lyrid, 0 | errflg, "%s(%d:k%d/%d, dlc=%d, cmd=0x%02x)%s",
                    __FUNCTION__, lp2line(lp), kid, prio, dlc,
                    dlc > 0? data[0] : 0xFFFFFFFF,
                    errstr);
    }

    lp->last_wr_r = r;

    return r;
}

//////////////////////////////////////////////////////////////////////

static int  cankoz_sender     (void *elem, void *privptr)
{
  kozdevinfo_t *dp = privptr;
  canqelem_t   *qe = elem;
  int           r;
  
    r = SendFrame(lines + dp->line, dp->kid, qe->prio, qe->dlc, qe->data);
    
    return r == CANHAL_OK? 0 : -1;
}

static int  cankoz_eq_cmp_func(void *elem, void *ptr)
{
  canqelem_t *a = elem;
  canqelem_t *b = ptr;
  
    if (b->dlc >= 0)
        return
            a->prio ==  b->prio  &&
            a->dlc  ==  b->dlc   &&
            (a->dlc ==  0  ||
             memcmp(a->data, b->data, a->dlc) == 0);
    else
        return
            a->prio ==  b->prio  &&
            a->dlc  >= -b->dlc   &&
             memcmp(a->data, b->data, -b->dlc) == 0;
}

static void tout_proc   (int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  kozdevinfo_t *dp = privptr;
  
    dp->tid = -1;
    sq_timeout(&(dp->q));
}

static int  tim_enqueuer(void *privptr, int usecs)
{
  kozdevinfo_t *dp = privptr;

    if (dp->tid >= 0)
    {
        sl_deq_tout(dp->tid);
        dp->tid = -1;
    }
    dp->tid = sl_enq_tout_after(dp->devid, NULL/*devptr*/, usecs, tout_proc, dp);
    return dp->tid >= 0? 0 : -1;
}

static void tim_dequeuer(void *privptr)
{
  kozdevinfo_t *dp = privptr;
  
    if (dp->tid >= 0)
    {
        sl_deq_tout(dp->tid);
        dp->tid = -1;
    }
}

static void cankoz_send_callback(void        *q_privptr,
                                 sq_eprops_t *e,
                                 sq_try_n_t   try_n)
{
  kozdevinfo_t *dp = q_privptr;
  canqelem_t   *qe = (canqelem_t *)e;

    qe->on_send(dp->devid, dp->devptr, try_n, qe->privptr);
}


//////////////////////////////////////////////////////////////////////

#define DECODE_AND_CHECK(errret)                                             \
    do {                                                                     \
        decode_handle(handle, &line, &kid);                                  \
        if (line < 0  ||  line >= countof(lines)  ||  lines[line].fd < 0  || \
            kid  < 0  ||  kid  >= countof(lines[line].devs))                 \
        {                                                                    \
            DoDriverLog(my_lyrid, 0,                                         \
                        "%s: invalid handle %d (=>%d:%d)",                   \
                        __FUNCTION__, handle, line, kid);                    \
            return errret;                                                   \
        }                                                                    \
        dp = lines[line].devs + kid;                                         \
        if (dp->devid == DEVID_NOT_IN_DRIVER)                                \
        {                                                                    \
            DoDriverLog(my_lyrid, 0,                                         \
                        "%s: attempt to use unregistered dev=%d:%d",         \
                        __FUNCTION__, line, kid);                            \
            return errret;                                                   \
        }                                                                    \
    } while (0)

/*### Queue management #############################################*/

static void        cankoz_q_clear     (int handle)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;

    DECODE_AND_CHECK();
    sq_clear(&(dp->q));
}

static sq_status_t cankoz_q_enqueue   (int handle, sq_enqcond_t how,
                                       int tries, int timeout_us,
                                       CanKozOnSendProc on_send, void *privptr,
                                       int model_dlc, int dlc, uint8 *data)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;
  canqelem_t    item;
  canqelem_t    model;

    DECODE_AND_CHECK(SQ_ERROR);

    if (dlc < 1  ||  dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;
    if (model_dlc > CANKOZ_MAXPKTBYTES  ||
        model_dlc > dlc)                       return SQ_ERROR;

    /* Filter-out  */
    if (dp->devcode_chk_ctr != 0  &&  data[0] != CANKOZ_DESC_GETDEVATTRS)
        return SQ_ERROR;

    /* Prepare item... */
    bzero(&item, sizeof(item));

    item.props.tries      = tries;
    item.props.timeout_us = timeout_us;
    item.props.callback   = (on_send == NULL)? NULL : cankoz_send_callback;

    item.on_send = on_send;
    item.privptr = privptr;
    item.prio    = CANKOZ_PRIO_UNICAST;
    item.dlc     = dlc;
    memcpy(item.data, data, dlc);

    /* ...and model, if required */
    if (model_dlc > 0)
    {
        model     = item;
        model.dlc = -model_dlc;
    }

    return sq_enq(&(dp->q), &(item.props), how, model_dlc > 0? &model : NULL);
}
static sq_status_t cankoz_q_enqueue_v (int handle, sq_enqcond_t how,
                                       int tries, int timeout_us,
                                       CanKozOnSendProc on_send, void *privptr,
                                       int model_dlc, int dlc, ...)
{
  va_list  ap;
  uint8    data[CANKOZ_MAXPKTBYTES];
  int      x;
  
    if (dlc < 1  ||  dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return cankoz_q_enqueue(handle, how,
                            tries, timeout_us,
                            on_send, privptr,
                            model_dlc, dlc, data);
}
static sq_status_t cankoz_q_enq       (int handle, sq_enqcond_t how,
                                       int dlc, uint8 *data)
{
    return cankoz_q_enqueue(handle, how,
                            SQ_TRIES_INF, 0,
                            NULL, NULL,
                            0, dlc, data);
}
static sq_status_t cankoz_q_enq_v     (int handle, sq_enqcond_t how,
                                       int dlc, ...)
{
  va_list  ap;
  uint8    data[CANKOZ_MAXPKTBYTES];
  int      x;
  
    if (dlc < 1  ||  dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return cankoz_q_enq(handle, how, dlc, data);
}
static sq_status_t cankoz_q_enq_ons   (int handle, sq_enqcond_t how,
                                       int dlc, uint8 *data)
{
    return cankoz_q_enqueue(handle, how,
                            SQ_TRIES_ONS, 0,
                            NULL, NULL,
                            0, dlc, data);
}
static sq_status_t cankoz_q_enq_ons_v (int handle, sq_enqcond_t how,
                                       int dlc, ...)
{
  va_list  ap;
  uint8    data[CANKOZ_MAXPKTBYTES];
  int      x;
  
    if (dlc < 1  ||  dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return cankoz_q_enq_ons(handle, how, dlc, data);
}

static sq_status_t cankoz_q_erase_and_send_next  (int handle,
                                                  int dlc, uint8 *data)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;
  canqelem_t    item;
  int           abs_dlc = abs(dlc);
  void         *fe;

    DECODE_AND_CHECK(SQ_ERROR);

    if (abs_dlc < 1  ||  abs_dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;

    /* Make a model for comparison */
    item.prio = CANKOZ_PRIO_UNICAST;
    item.dlc  = dlc;
    memcpy(item.data, data, abs_dlc);

    /* Check "whether we should really erase and send next packet" */
    fe = sq_access_e(&(dp->q), 0);
    /*!!! should be "if (fe == NULL  || ...)"*/
    if (fe != NULL  &&  cankoz_eq_cmp_func(fe, &item) == 0)
        return SQ_NOTFOUND;

    /* Erase... */
    sq_foreach(&(dp->q), SQ_ELEM_IS_EQUAL, &item, SQ_ERASE_FIRST,  NULL);
    /* ...and send next */
    sq_sendnext(&(dp->q));

    return SQ_FOUND;
}
static sq_status_t cankoz_q_erase_and_send_next_v(int handle,
                                                  int dlc, ...)
{
  va_list  ap;
  int      abs_dlc = abs(dlc);
  uint8    data[CANKOZ_MAXPKTBYTES];
  int      x;
  
    if (abs_dlc < 1  ||  abs_dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < abs_dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return cankoz_q_erase_and_send_next(handle, dlc, data);
}

typedef struct
{
    CanKozCheckerProc  checker;
    void              *privptr;
} foreach_info_t;
static int foreach_checker(void *elem, void *ptr)
{
  canqelem_t     *a      = elem;
  foreach_info_t *info_p = ptr;

    return info_p->checker(a->dlc, a->data, info_p->privptr);
}
static sq_status_t cankoz_q_foreach  (int handle,
                                      CanKozCheckerProc checker, void *privptr,
                                      sq_action_t action,
                                      int dlc, uint8 *data)
{
  int             line;
  int             kid;
  kozdevinfo_t   *dp;
  canqelem_t      item;
  int             abs_dlc = abs(dlc);
  foreach_info_t  info;

    DECODE_AND_CHECK(SQ_ERROR);

    if (checker == NULL)
    {
        if (abs_dlc < 1  ||  abs_dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;

        /* Make a model for comparison */
        item.prio = CANKOZ_PRIO_UNICAST;
        item.dlc  = dlc;
        memcpy(item.data, data, abs_dlc);
        return sq_foreach(&(dp->q), SQ_ELEM_IS_EQUAL, &item, action, NULL);
    }
    else
    {
        info.checker = checker;
        info.privptr = privptr;
        return sq_foreach(&(dp->q), foreach_checker,  &info, action, NULL);
    }
}
static sq_status_t cankoz_q_foreach_v(int handle,
                                      sq_action_t action,
                                      int dlc, ...)
{
  va_list  ap;
  int      abs_dlc = abs(dlc);
  uint8    data[CANKOZ_MAXPKTBYTES];
  int      x;
  
    if (abs_dlc < 1  ||  abs_dlc > CANKOZ_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < abs_dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return cankoz_q_foreach(handle, NULL, NULL, action, dlc, data);
}

/*##################################################################*/


static int   cankoz_init_lyr  (int lyrid)
{
  int  l;
  int  d;

    DoDriverLog(my_lyrid, 0, "%s(%d)!", __FUNCTION__, lyrid);
    my_lyrid = lyrid;
    bzero(lines, sizeof(lines));
    for (l = 0;  l < countof(lines);  l++)
    {
        lines[l].fd = -1;
        for (d = 0;  d < countof(lines[l].devs);  d++)
            lines[l].devs[d].devid = DEVID_NOT_IN_DRIVER;
    }

    return 0;
}

static void  cankoz_term_lyr  (void)
{
    /*!!!*/
}

static void  cankoz_disconnect(int devid)
{
  int           l;
  int           d;
  kozdevinfo_t *dp;
  int           was_found = 0;

    for (l = 0;  l < countof(lines);  l++)
        for (d = 0;  d < countof(lines[l].devs);  d++)
        {
            if (lines[l].devs[d].devid == devid)
            {
                was_found = 1;
                dp = lines[l].devs + d;
                ////fprintf(stderr, "%s(devid=%d) kid=%d,%d\n", __FUNCTION__, devid, l, d);

                sq_fini(&(dp->q));
                bzero(dp, sizeof(*dp));
                dp->devid = DEVID_NOT_IN_DRIVER;

                /* Check if some other devices are active on this line */
                for (d = 0;  d < countof(lines[l].devs);  d++)
                    if (lines[l].devs[d].devid > 0)
                        goto NEXT_KID;

                /* ...or was last "client" of this line? */
                /* Then release the line! */
                sl_del_fd(lines[l].fdhandle);
                canhal_close_line(lines[l].fd);
                lines[l].fd = -1;
            }
        NEXT_KID:;
        }

    if (!was_found)
        DoDriverLog(devid, 0, "%s: request to disconnect unknown devid=%d",
                    __FUNCTION__, devid);
}

static void cankoz_fd_p     (int devid, void *devptr,
                             sl_fdh_t fdhandle, int fd,  int mask,
                             void *privptr);

static int  cankoz_add   (int devid, void *devptr,
                          int businfocount, int businfo[],
                          int devcode,
                          CanKozOnIdProc  ffproc,
                          CanKozPktinProc inproc,
                          int queue_size,
                          int options)
{
  int           line;
  int           kid;
  lineinfo_t   *lp;
  kozdevinfo_t *dp;
  int           handle;
  int           x;
  int           r;
  char         *err;
  uint8         data[CANKOZ_MAXPKTBYTES];

    if (devid == DEVID_NOT_IN_DRIVER)
    {
        DoDriverLog(my_lyrid, 0, "%s: devid==DEVID_NOT_IN_DRIVER", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }

    if (businfocount != 2)
    {
        DoDriverLog(devid, 0, "%s: businfocount=%d, !=2",
                    __FUNCTION__, businfocount);
        return -CXRF_CFG_PROBL;
    }

    line = businfo[0];
    kid  = businfo[1];
    
    if (line < 0  ||  line >= NUMLINES)
    {
        DoDriverLog(devid, 0, "%s: line=%d, out_of[%d..%d]",
                    __FUNCTION__, line, 0, NUMLINES-1);
        return -CXRF_CFG_PROBL;
    }
    if (kid < 0  ||  kid >= DEVSPERLINE)
    {
        DoDriverLog(devid, 0, "%s: kid=%d, out_of[%d..%d]",
                    __FUNCTION__, kid,  0, DEVSPERLINE-1);
        return -CXRF_CFG_PROBL;
    }

    lp     = lines    + line;
    dp     = lp->devs + kid;
    handle = encode_handle(line, kid);

    if (lp->fd >= 0  &&  dp->devid != DEVID_NOT_IN_DRIVER)
    {
        DoDriverLog(devid, 0, "%s: device (%d,%d) is already in use",
                   __FUNCTION__, line, kid);
        ReturnDataSet(dp->devid, RETURNDATA_COUNT_PONG,
                      NULL, NULL, NULL, NULL, NULL, NULL);
        return -CXRF_CFG_PROBL;
    }

    for (x = 0;  x < countof(dp->devcodes);  x++) dp->devcodes[x] = -1;

    dp->line        = line;
    dp->kid         = kid;
    dp->devid       = devid;
    dp->devptr      = devptr;
    dp->devcodes[0] = devcode;
    dp->ffproc      = ffproc;
    dp->inproc      = inproc;
    dp->options     = options;
    
    dp->hw_ver = dp->sw_ver = dp->hw_code = 0;
    dp->ioregs.base = -1;

    r = sq_init(&(dp->q), NULL,
                queue_size, sizeof(canqelem_t),
                CANKOZ_DEFAULT_TIMEOUT, dp,
                cankoz_sender,
                cankoz_eq_cmp_func,
                tim_enqueuer,
                tim_dequeuer);
    if (r < 0)
    {
        DoDriverLog(devid, 0 | DRIVERLOG_ERRNO,
                    "%s: sq_init()", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }
    dp->tid = -1;
    
    if (lp->fd < 0)
    {
        /*!!! Obtain layer-info and psp-parse it */

        err = NULL;
        lp->fd = canhal_open_and_setup_line(line, /*!!!*/CANHAL_B125K, &err);
        if (lp->fd < 0)
        {
            DoDriverLog(my_lyrid, 0 | DRIVERLOG_ERRNO,
                        "%s: canhal_open_and_setup_line(): %s",
                        __FUNCTION__, err);
            sq_fini(&(dp->q));
            return -CXRF_DRV_PROBL;
        }
        lp->last_rd_r  = lp->last_wr_r = CANHAL_OK;
        lp->last_ex_id = 0;
        lp->fdhandle   = sl_add_fd(my_lyrid, NULL, lp->fd, SL_RD,
                                   cankoz_fd_p, lp);

        data[0] = CANKOZ_DESC_GETDEVATTRS;
        SendFrame(lp, 0, CANKOZ_PRIO_BROADCAST, 1, data);
    }

    cankoz_q_enqueue_v(handle, SQ_IF_NONEORFIRST,
                       SQ_TRIES_INF, CANKOZ_0xFF_TIMEOUT,
                       NULL, NULL,
                       0, 1, CANKOZ_DESC_GETDEVATTRS);

    return handle;
}

static int  cankoz_add_devcode(int handle, int devcode)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;
  int           x;

    DECODE_AND_CHECK(-1);

    for (x = 0;  x < countof(dp->devcodes);  x++)
        if (dp->devcodes[x] < 0)
        {
            dp->devcodes[x] = devcode;
            return 0;
        }

    DoDriverLog(dp->devid, 0, "%s: devcodes[%d] table overflow",
                __FUNCTION__, countof(dp->devcodes));
    return -1;
}

static int  cankoz_get_dev_ver(int handle, 
                               int *hw_ver_p, int *sw_ver_p,
                               int *hw_code_p)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;

    DECODE_AND_CHECK(-1);
    if (hw_ver_p  != NULL) *hw_ver_p  = dp->hw_ver;
    if (sw_ver_p  != NULL) *sw_ver_p  = dp->sw_ver;
    if (hw_code_p != NULL) *hw_code_p = dp->hw_code;
    return 0;
}


static void send_wrreg_cmds(kozdevinfo_t *dp)
{
  int    handle = encode_handle(dp->line, dp->kid);
  uint8  o_val  = (dp->ioregs.cur_val &~ dp->ioregs.req_msk) |
                  (dp->ioregs.req_val &  dp->ioregs.req_msk);
    
    if (cankoz_q_enqueue_v(handle, SQ_REPLACE_NOTFIRST,
                           SQ_TRIES_ONS, 0,
                           NULL/*!!!*/, NULL,
                           1, 2, CANKOZ_DESC_WRITEOUTREG, o_val) == SQ_NOTFOUND)
        cankoz_q_enq_v(handle, SQ_ALWAYS, 1, CANKOZ_DESC_READREGS);

    //dp->ioregs.req_msk = 0;
    dp->ioregs.pend    = 0; /*!!!???*/
}

static int  cankoz_has_regs(int handle, int chan_base)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;

    DECODE_AND_CHECK(-1);

    dp->ioregs.base = chan_base;

    return 0;
}

static void cankoz_regs_rw (int handle, int action, int chan, int32 *wvp)
{
  int           line;
  int           kid;
  kozdevinfo_t *dp;

  int           n;

  uint8         mask;

    DECODE_AND_CHECK();

    n = chan - dp->ioregs.base;
    if (dp->ioregs.base < 0  ||
        n < 0  ||  n >= CANKOZ_IOREGS_CHANCOUNT) return;

    /* Unsupported channel? */
    if ((n >= CANKOZ_IOREGS_OFS_IR_IEB0  &&  n <= CANKOZ_IOREGS_OFS_IR_IEB7)
        ||
        n == CANKOZ_IOREGS_OFS_IR_IE8B
        ||
        (n >= CANKOZ_IOREGS_OFS_RESERVED_REGS_27  &&  n <= CANKOZ_IOREGS_OFS_RESERVED_REGS_29))
    {
        ReturnInt32Datum(dp->devid, chan, 0, CXRF_UNSUPPORTED);
        return;
    }

    /* Read? */
    if (action == DRVA_READ  ||
        n == CANKOZ_IOREGS_OFS_INPR8B  ||
        (n >= CANKOZ_IOREGS_OFS_INPRB0  &&  n <= CANKOZ_IOREGS_OFS_INPRB7))
    {
        cankoz_q_enq_v(encode_handle(dp->line, dp->kid),
                       SQ_IF_ABSENT,
                       1, CANKOZ_DESC_READREGS);
    }
    /* No, some form of write */
    else
    {
        /* Decide, what to write... */
        if (n == CANKOZ_IOREGS_OFS_OUTR8B)
        {
            dp->ioregs.req_val = *wvp;
            dp->ioregs.req_msk = 0xFF;
        }
        else
        {
            mask = 1 << (n - CANKOZ_IOREGS_OFS_OUTRB0);
            if (*wvp != 0) dp->ioregs.req_val |=  mask;
            else           dp->ioregs.req_val &=~ mask;
            dp->ioregs.req_msk |= mask;
        }

        dp->ioregs.pend = 1;
        /* May we perform write right now? */
        if (dp->ioregs.rcvd)
        {
            send_wrreg_cmds(dp);
        }
        else
        {
            cankoz_q_enq_v(encode_handle(dp->line, dp->kid),
                           SQ_IF_ABSENT,
                           1, CANKOZ_DESC_READREGS);
        }
    }
}

static void cankoz_regs_f8 (kozdevinfo_t *dp, uint8 *can_data)
{
  int        regsaddrs [CANKOZ_IOREGS_CHANCOUNT];
  cxdtype_t  regsdtypes[CANKOZ_IOREGS_CHANCOUNT];
  int        regsnelems[CANKOZ_IOREGS_CHANCOUNT];
  int32      regsvals  [CANKOZ_IOREGS_CHANCOUNT];
  void      *regsvals_p[CANKOZ_IOREGS_CHANCOUNT];
  rflags_t   regsrflags[CANKOZ_IOREGS_CHANCOUNT];

  int        n;
#define SET_RET_INFO(chan_n, val)                 \
    do                                            \
    {                                             \
        regsaddrs [n] = dp->ioregs.base + chan_n; \
        regsdtypes[n] = CXDTYPE_INT32;            \
        regsnelems[n] = 1;                        \
        regsvals  [n] = (val);                    \
        regsvals_p[n] = regsvals + n;             \
        regsrflags[n] = 0;                        \
        n++;                                      \
    }                                             \
    while (0)

  int        x;

    /* Erase the requestor */
    cankoz_q_erase_and_send_next_v(encode_handle(dp->line, dp->kid),
                                   1, CANKOZ_DESC_READREGS);

    /* Extract data into a ready-for-ReturnDataSet format */
    dp->ioregs.cur_val = can_data[1];
    dp->ioregs.cur_inp = can_data[2];

    n = 0;

    SET_RET_INFO(CANKOZ_IOREGS_OFS_OUTR8B, dp->ioregs.cur_val);
    SET_RET_INFO(CANKOZ_IOREGS_OFS_INPR8B, dp->ioregs.cur_inp);

    for (x = 0;  x < 8;  x++)
    {
        SET_RET_INFO(CANKOZ_IOREGS_OFS_OUTRB0 + x, (dp->ioregs.cur_val & (1 << x)) != 0);
        SET_RET_INFO(CANKOZ_IOREGS_OFS_INPRB0 + x, (dp->ioregs.cur_inp & (1 << x)) != 0);
    }
    dp->ioregs.rcvd = 1;

    /* Do we have a pending write request? */
    if (dp->ioregs.pend != 0)
        send_wrreg_cmds(dp);

    /* And return requested data <<(only what was REALLY requested up to this point)>> */
    ReturnDataSet(dp->devid,
                  n,
                  regsaddrs, regsdtypes, regsnelems,
                  regsvals_p, regsrflags, NULL);
}


static void cankoz_fd_p     (int devid, void *devptr,
                             sl_fdh_t fdhandle, int fd,  int mask,
                             void *privptr)
{
  lineinfo_t   *lp = privptr;

  int           r;
  int           last_rd_r;
  int           can_id;
  int           can_dlc;
  uint8         can_data[CANKOZ_MAXPKTBYTES];

  int           errflg;
  char         *errstr;

  int           kid;
  int           prio;
  int           desc;
  kozdevinfo_t *dp;
  int           handle;

  int           id_devcode;
  int           id_hwver;
  int           id_swver;
  int           id_reason;
  int           is_a_reset;
  int           devcode_is_correct;
  int           x;

    /* Obtain a frame */
    r = canhal_recv_frame(fd, &can_id, &can_dlc, can_data);
    last_rd_r = lp->last_rd_r;
    lp->last_rd_r = r;
    handle = -1;

    /* Check result */
    if (r != CANHAL_OK)
    {
        if (r != last_rd_r)
        {
            errflg = 0;
            errstr = "";
            if      (r == CANHAL_ZERO)   errstr = ": ZERO";
            else if (r == CANHAL_BUSOFF) errstr = ": BUSOFF";
            else                         errflg = DRIVERLOG_ERRNO;
            
            DoDriverLog(my_lyrid, 0 | errflg, "%s()%s", __FUNCTION__, errstr);
        }
        return;
    }

    log_frame(my_lyrid, "RCVFRAME", can_id, can_dlc, can_data);

    /* An extended-id/RTR/other-non-kozak frame? */
    if (can_id != (can_id & 0x7FF))
    {
        if (can_id != lp->last_ex_id)
        {
            lp->last_ex_id = can_id;
            /*!!! DoDriverLog() it somehow? */
        }
        return;
    }

    /* Okay, that's a valid frame */
    decode_frameid(can_id, &kid, &prio);
    desc = can_dlc > 0? can_data[0] : -1;

    /* We don't want to deal with not-from-devices frames */
    if (prio != CANKOZ_PRIO_REPLY) return;

    /**/
    dp     = lp->devs + kid;
    handle = encode_handle(lp2line(lp), kid);

    /* Perform additional (common) actions for GETDEVATTRS */
    if (desc == CANKOZ_DESC_GETDEVATTRS)
    {
        id_devcode = can_dlc > 1? can_data[1] : -1;
        id_hwver   = can_dlc > 2? can_data[2] : -1;
        id_swver   = can_dlc > 3? can_data[3] : -1;
        id_reason  = can_dlc > 4? can_data[4] : -1;
        is_a_reset =
            id_reason == CANKOZ_IAMR_POWERON   ||
            id_reason == CANKOZ_IAMR_RESET     ||
            id_reason == CANKOZ_IAMR_WATCHDOG  ||
            id_reason == CANKOZ_IAMR_BUSOFF;

        DoDriverLog(dp->devid >= 0? dp->devid : my_lyrid, 0,
                    "GETDEVATTRS: ID=%d,%d, DevCode=%d:%s, HWver=%d, SWver=%d, Reason=%d:%s",
                    lp2line(lp), kid,
                    id_devcode,
                    id_devcode >= 0  &&  id_devcode < countof(cankoz_devtypes)  &&  cankoz_devtypes[id_devcode] != NULL?
                        cankoz_devtypes[id_devcode] : "???",
                    id_hwver,
                    id_swver,
                    id_reason,
                    id_reason  >= 0  &&  id_reason  < countof(cankoz_GETDEVATTRS_reasons)?
                        cankoz_GETDEVATTRS_reasons[id_reason] : "???");


        dp->hw_ver  = id_hwver;
        dp->sw_ver  = id_swver;
        dp->hw_code = id_devcode;

        /* Is it our device? */
        if (dp->devid != DEVID_NOT_IN_DRIVER)
        {
#if 1
            if (dp->devcodes[0] < 0)
                devcode_is_correct = 1;
            else
            {
                for (x = 0, devcode_is_correct = 0;
                     x < countof(dp->devcodes)  &&  dp->devcodes[x] >= 0;
                     x++)
                    if (dp->devcodes[x] == id_devcode) devcode_is_correct = 1;
            }

            if (devcode_is_correct)
            {
                if (dp->devcode_chk_ctr == 0)
                    /* Everything is OK? Just dequeue and continue */
                    cankoz_q_erase_and_send_next_v(handle, 1, CANKOZ_DESC_GETDEVATTRS);
                else
                {
                    /* Ignore such packets, because they are filtered-out
                       below from passing to ffproc();
                       but we NEED last 0xFF to be passed to ffproc() */
                    if (id_reason == CANKOZ_IAMR_WHOAREHERE) return;

                    dp->devcode_chk_ctr--;
                    /* Just do-nothing, leaving 0xFF in queue head,
                       to be resent CANKOZ_CONFIDENCE_TIMEOUT usecs later */
                    if (dp->devcode_chk_ctr != 0) return;
                    /* Okay, confidence restored */
                    DoDriverLog(dp->devid, 0,
                                "%s: devcode confidence restored after %d correct replies",
                                __FUNCTION__, DEVCODE_CHK_COUNT);
                    /* This will also cause sq_clear() below,
                       as well as DevState change */
                    is_a_reset = 1;
                }
            }
            else
            {
                if (dp->devcode_chk_ctr == 0) /* Log only upon FIRST wrong devcode in sequence */
                {
                    DoDriverLog(dp->devid, 0,
                                "%s: devcode=%d differs from registered %d%s; temporarily deactivating device",
                                __FUNCTION__, id_devcode,
                                dp->devcodes[0], dp->devcodes[1] >= 0? "(+)" : "");
                    SetDevState(dp->devid, DEVSTATE_NOTREADY, CXRF_WRONG_DEV, "DevCode mismatch");
                }
                sq_clear(&(dp->q));
                sq_pause(&(dp->q), CANKOZ_MISTRUST_TIMEOUT);
                cankoz_q_enqueue_v(handle, SQ_ALWAYS,
                                   SQ_TRIES_INF, CANKOZ_CONFIDENCE_TIMEOUT,
                                   NULL, NULL,
                                   0, 1, CANKOZ_DESC_GETDEVATTRS);
                dp->devcode_chk_ctr = DEVCODE_CHK_COUNT;
                return;
            }
#else
            cankoz_q_erase_and_send_next_v(handle, 1, CANKOZ_DESC_GETDEVATTRS);

            /* Is code correct? */
            if (dp->devcodes[0] >= 0)
            {
                for (x = 0;
                     x < countof(dp->devcodes)  &&  dp->devcodes[x] >= 0;
                     x++)
                    if (dp->devcodes[x] == id_devcode) goto DEVCODE_CHECK_PASSED;
                /* Failed... */
                /*!!! Should switch to non-OFFLINEing (trust/wrong) algorythm
                      from bigfile-0001.html@14.10.2013 */
                DoDriverLog(dp->devid, 0,
                            "%s: DevCode=%d differs from registered %d%s. Terminating device",
                            __FUNCTION__, id_devcode,
                            dp->devcodes[0], dp->devcodes[1] >= 0? "(+)" : "");
                SetDevState(dp->devid, DEVSTATE_OFFLINE, CXRF_WRONG_DEV, "DevCode mismatch");
                return;
            }
 DEVCODE_CHECK_PASSED:
            if (dp->devcode_chk_ctr != 0)
            {
                dp->devcode_chk_ctr--;
                if (dp->devcode_chk_ctr == 0)
                {
                    DoDriverLog(dp->devid, 0,
                                "%s: DevCode identity restored after %d checks",
                                __FUNCTION__, DEVCODE_CHK_COUNT);
                }
            }
#endif

            if (is_a_reset)
            {
                sq_clear(&(dp->q));
            }
            /* Should we perform re-request? */
            if (is_a_reset)
            {
                // Notify those who requested BEFORE devstate change
                if (dp->ffproc != NULL  &&  id_reason != CANKOZ_IAMR_WHOAREHERE  &&
                    (dp->options & CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET) != 0)
                    dp->ffproc(dp->devid, dp->devptr, CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET);

                dp->ioregs.rcvd    = 0;
                dp->ioregs.pend    = 0;
                dp->ioregs.req_msk = 0;
                SetDevState(dp->devid, DEVSTATE_NOTREADY,  0, NULL);
                SetDevState(dp->devid, DEVSTATE_OPERATING, 0, NULL);
            }

            // And an AFTER devstate change notification
            if (dp->ffproc != NULL  &&  id_reason != CANKOZ_IAMR_WHOAREHERE)
            {
                // a. A regular one
                if      (is_a_reset == 0  ||
                         (dp->options & 
                          (CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET | 
                           CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET)
                         ) == 0)
                    dp->ffproc(dp->devid, dp->devptr, is_a_reset);
                // b. Or a special for those who requested
                else if ((dp->options & CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET) != 0)
                    dp->ffproc(dp->devid, dp->devptr, CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET);
            }

        }
    }

    /* If this device isn't registered, we aren't interested in further processing */
    if (dp->devid == DEVID_NOT_IN_DRIVER) return;

    /* If in "wrong device" mode, shouldn't pass anything to driver */
    if (dp->devcode_chk_ctr != 0) return;

    /* I/O-regs operation? */
    if (desc == CANKOZ_DESC_READREGS  &&  dp->ioregs.base >= 0)
    {
        if (can_dlc < 3)
        {
            DoDriverLog(dp->devid, 0,
                        "DESC_READREGS: packet is too short - %d bytes",
                        can_dlc);
            return;
        }
        cankoz_regs_f8(dp, can_data);
    }

    /* Finally, call the processer... */
    if (dp->inproc != NULL)
        dp->inproc(dp->devid, dp->devptr, desc, can_dlc, can_data);
}

static void set_log_to(int onoff)
{
    log_frame_allowed = (onoff != 0);
}

static void send_0xff(void)
{
  int           line;
  uint8         data[CANKOZ_MAXPKTBYTES];

    for (line = 0;  line < NUMLINES;  line++)
        if (lines[line].fd >= 0)
        {
            data[0] = CANKOZ_DESC_GETDEVATTRS;
            SendFrame(lines + line, 0,
                      CANKOZ_PRIO_BROADCAST, 1, data);
        }
}

static int  get_dev_info(int devid,
                         int *line_p, int *kid_p, int *hw_code_p)
{
  int           handle;
  int           line;
  int           kid;
  kozdevinfo_t *dp;
  
    handle = devid2handle(devid);
    if (handle < 0) return handle;
    DECODE_AND_CHECK(-1);
    if (line_p    != NULL) *line_p    = line;
    if (kid_p     != NULL) *kid_p     = kid;
    if (hw_code_p != NULL) *hw_code_p = dp->hw_code;

    return 0;
}


static cankoz_vmt_t cankoz_vmt =
{
    cankoz_add,
    cankoz_add_devcode,
    cankoz_get_dev_ver,

    cankoz_q_clear,
    
    cankoz_q_enqueue,
    cankoz_q_enqueue_v,
    cankoz_q_enq,
    cankoz_q_enq_v,
    cankoz_q_enq_ons,
    cankoz_q_enq_ons_v,
    
    cankoz_q_erase_and_send_next,
    cankoz_q_erase_and_send_next_v,

    cankoz_q_foreach,
    cankoz_q_foreach_v,

    cankoz_has_regs,
    cankoz_regs_rw,

    set_log_to,
    send_0xff,
    get_dev_info,
};


DEFINE_CXSD_LAYER(CANLYR_NAME, "CANKOZ implementation via '" __CX_STRINGIZE(CANHAL_DESCR) "' HAL",
                  NULL, NULL,
                  CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                  cankoz_init_lyr, cankoz_term_lyr,
                  cankoz_disconnect,
                  &cankoz_vmt);
