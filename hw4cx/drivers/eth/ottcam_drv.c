#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pzframe_drv.h"

#include "misc_macros.h"
#include "misclib.h"
#include "sendqlib.h"

#include "drv_i/ottcam_drv_i.h"


enum
{
    OTTREQ_COMM = 0x636f6d6d, // "comm"
    OTTRPY_EXPO = 0x6578706f, // "expo"
    OTTRPY_NEXP = 0x6e656870, // "nexp"
    OTTRPY_ACKN = 0x61636b6e, // "ackn"
    OTTRPY_NACK = 0x6e61636b, // "nack"
    OTTRPY_DATA = 0x64617461, // "data"
};

enum // OTTREQ_COMM.cmd
{
    OTTCMD_SETMODE   = 0, // .par1=mode
    OTTCMD_NEW_FRAME = 1, //
    OTTCMD_SND_FRAME = 2, // .par1,.par2 - line range
    OTTCMD_SET_FSIZE = 3, // .par1=width, .par2=height
    OTTCMD_RESERVED4 = 4,
    OTTCMD_WRITEREG  = 5, // .par1=addr, .par2=value
    OTTCMD_READREG   = 6, // .par1=addr
    OTTCMD_RESET     = 7,
};

enum // OTTREQ_COMM:OTTCMD_SETMODE.par1
{
    OTTMODE_INACTIVITY  = 0,
    OTTMODE_SINGLE_SHOT = 1,
    OTTMODE_MONITOR     = 2, // Not implemented
    OTTMODE_SERIAL      = 3, // Not implemented
};

enum
{
    OTTPING_CMD = OTTCMD_READREG,
    OTTPING_REG = 0x7F,
    OTTPING_VAL = 0x2AA,          // == 0b0000001010101010
    ALIVE_USECS = 10*1000000,     // 10s keepalive ping period
};


typedef struct
{
    uint32  id;
    uint16  seq;
    uint16  cmd;
    uint16  par1;
    uint16  par2;
} ottcam_hdr_t;

typedef struct
{
    uint32  id;
    uint16  seq;
    uint16  line_n;
    uint8   data[0];
} ottcam_line_t;

enum {PAYLOAD_SIZE = (OTTCAM_MAX_W * 4 / 3)};
enum {DEFAULT_OTTCAM_PORT = 4001};

#define REG_0x07_VAL(sync_val)  (0x8 | ((sync_val) << 4) | (0x4 << 5))

typedef int16 OTTCAM_DATUM_T;
enum         {OTTCAM_DTYPE = CXDTYPE_INT16};

//////////////////////////////////////////////////////////////////////

enum
{
    OTTCAM_SENDQ_SIZE = 100,
    OTTCAM_SENDQ_TOUT = 100000, // 100ms
};
typedef struct
{
    sq_eprops_t       props;
    ottcam_hdr_t      hdr;
} ottqelem_t;

//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [OTTCAM_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},

    /* controls */
    [OTTCAM_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [OTTCAM_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [OTTCAM_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [OTTCAM_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},

    [OTTCAM_CHAN_X]             = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_Y]             = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_W]             = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_H]             = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_K]             = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_T]             = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_SYNC]          = {PZFRAME_CHTYPE_VALIDATE,    0},
    [OTTCAM_CHAN_RRQ_MSECS]     = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [OTTCAM_CHAN_MISS]          = {PZFRAME_CHTYPE_STATUS,      -1},
    [OTTCAM_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},

    [OTTCAM_CHAN_CUR_X]         = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_X},
    [OTTCAM_CHAN_CUR_Y]         = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_Y},
    [OTTCAM_CHAN_CUR_W]         = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_W},
    [OTTCAM_CHAN_CUR_H]         = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_H},
    [OTTCAM_CHAN_CUR_K]         = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_K},
    [OTTCAM_CHAN_CUR_T]         = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_T},
    [OTTCAM_CHAN_CUR_SYNC]      = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_SYNC},
    [OTTCAM_CHAN_CUR_RRQ_MSECS] = {PZFRAME_CHTYPE_STATUS,      OTTCAM_CHAN_RRQ_MSECS},
};

//--------------------------------------------------------------------

enum {RRQ_USECS = 100 * 1000};

enum
{
    STATE_READY = 0, STATE_INITIAL = 1, STATE_REGET = 2
};

typedef struct
{
    pzframe_drv_t     pz;
    int               devid;
    int               fd;
    int               is_sim;

    int32             cur_args[OTTCAM_NUMCHANS];
    int32             nxt_args[OTTCAM_NUMCHANS];
    int32             prv_args[OTTCAM_NUMCHANS];
    OTTCAM_DATUM_T    retdata[OTTCAM_MAX_W * OTTCAM_MAX_H];

    //// For simulation
    int               sim_x;
    int               sim_y;
    int               sim_vx;
    int               sim_vy;

    //// For true operation via pzframe_drv
    uint16            curframe;
    int               state;
    int               rqd_h;
    int               rest;
    int               rrq_rest;
    sl_tid_t          rrq_tid;
    int               smth_rcvd;
    uint8             is_rcvd[OTTCAM_MAX_H];

    ////
    sq_q_t            q;
    sl_tid_t          sq_tid;

    ////
    sl_tid_t          alv_tid;

    int                data_rqd;
    struct
    {
        int            addrs [OTTCAM_NUMCHANS];
        cxdtype_t      dtypes[OTTCAM_NUMCHANS];
        int            nelems[OTTCAM_NUMCHANS];
        void          *val_ps[OTTCAM_NUMCHANS];
        rflags_t       rflags[OTTCAM_NUMCHANS];
    }                  r;
} ottcam_privrec_t;

#define PDR2ME(pdr) ((ottcam_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static inline OTTCAM_DATUM_T * ACCESS_DATA_LINE(ottcam_privrec_t *me, int l)
{
    return me->retdata + l * OTTCAM_MAX_W;
}

static int GET_RRQ_USECS(ottcam_privrec_t *me)
{
  int  ret;

    ret = me->cur_args[OTTCAM_CHAN_RRQ_MSECS] * 1000;
    if (ret <= 0) ret = RRQ_USECS;

    return ret;
}

static inline void DropRrqTout(ottcam_privrec_t *me)
{
    if (me->rrq_tid >= 0)
    {
        sl_deq_tout(me->rrq_tid);
        me->rrq_tid = -1;
    }
}

static void SendReset(ottcam_privrec_t *me)
{
  ottcam_hdr_t      hdr;

    hdr.id   = htonl(OTTREQ_COMM);
    hdr.seq  = htons(me->curframe);
    hdr.cmd  = htons(OTTCMD_RESET);
    send(me->fd, &hdr, sizeof(hdr), 0);
}

//////////////////////////////////////////////////////////////////////

static int  ottcam_sender     (void *elem, void *devptr)
{
  ottcam_privrec_t *me = devptr;
  ottqelem_t       *qe = elem;
  ottcam_hdr_t      hdr;
  int               r;

    hdr.id   = htonl(qe->hdr.id);
    hdr.seq  = htons(qe->hdr.seq);
    hdr.cmd  = htons(qe->hdr.cmd);
    hdr.par1 = htons(qe->hdr.par1);
    hdr.par2 = htons(qe->hdr.par2);
    r = send(me->fd, &hdr, sizeof(hdr), 0);
    ////fprintf(stderr, "send=%d\n", r);
    return r > 0? 0 : -1;
}

static int  ottcam_eq_cmp_func(void *elem, void *ptr)
{
  ottcam_hdr_t *a = &(((ottqelem_t *)elem)->hdr);
  ottcam_hdr_t *b = &(((ottqelem_t *)ptr )->hdr);

    // We DON'T compare id, since only OTTREQ_COMM can be in queue
    if (a->seq != b->seq  ||  a->cmd != b->cmd) return 0;

    if (a->cmd  == OTTCMD_SETMODE    &&
        a->par1 == b->par1)                         return 1;
    if (a->cmd  == OTTCMD_NEW_FRAME)                return 1;
    if (a->cmd  == OTTCMD_SND_FRAME  &&
        a->par1 == b->par1  &&  a->par2 == b->par2) return 1;
    if (a->cmd  == OTTCMD_SET_FSIZE  &&
        a->par1 == b->par1  &&  a->par2 == b->par2) return 1;
    if (a->cmd  == OTTCMD_WRITEREG   &&
        a->par1 == b->par1  &&  a->par2 == b->par2) return 1;
    if (a->cmd  == OTTCMD_READREG    &&
        a->par1 == b->par1)                         return 1;
    if (a->cmd  == OTTCMD_RESET)                    return 1;

    return 0;
}

static void tout_proc   (int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  ottcam_privrec_t *me = devptr;
  
    me->sq_tid = -1;
    sq_timeout(&(me->q));
}

static int  tim_enqueuer(void *devptr, int usecs)
{
  ottcam_privrec_t *me = devptr;

////fprintf(stderr, "%s(%d)\n", __FUNCTION__, usecs);
    if (me->sq_tid >= 0)
    {
        sl_deq_tout(me->sq_tid);
        me->sq_tid = -1;
    }
    me->sq_tid = sl_enq_tout_after(me->devid, devptr, usecs, tout_proc, NULL);
    return me->sq_tid >= 0? 0 : -1;
}

static void tim_dequeuer(void *devptr)
{
  ottcam_privrec_t  *me = devptr;
  
    if (me->sq_tid >= 0)
    {
        sl_deq_tout(me->sq_tid);
        me->sq_tid = -1;
    }
}

static sq_status_t enq_pkt(ottcam_privrec_t *me, 
                           uint16           cmd,
                           uint16           par1,
                           uint16           par2)
{
  ottqelem_t  item;
  sq_status_t r;

    bzero(&item, sizeof(item));
    
    item.props.tries      = SQ_TRIES_INF;
    item.props.timeout_us = OTTCAM_SENDQ_TOUT;

    item.hdr.id   = OTTREQ_COMM;
    item.hdr.seq  = me->curframe;
    item.hdr.cmd  = cmd;
    item.hdr.par1 = par1;
    item.hdr.par2 = par2;

    r = sq_enq(&(me->q), &(item.props), SQ_ALWAYS, NULL);
////fprintf(stderr, "%s()=%d %d %d\n", __FUNCTION__, r, me->q.ring_used, me->q.tout_set);
    return r;
}

static sq_status_t esn_pkt(ottcam_privrec_t *me,
                           uint16            cmd,
                           uint16            par1,
                           uint16            par2)
{
  ottqelem_t  item;
  void       *fe;

    bzero(&item, sizeof(item));
    
    item.hdr.id   = OTTREQ_COMM;
    item.hdr.seq  = me->curframe;
    item.hdr.cmd  = cmd;
    item.hdr.par1 = par1;
    item.hdr.par2 = par2;

    /* Check "whether we should really erase and send next packet" */
    fe = sq_access_e(&(me->q), 0);
    /*!!! should be "if (fe == NULL  || ...)"*/
    if (fe != NULL  &&  ottcam_eq_cmp_func(fe, &item) == 0)
        return SQ_NOTFOUND;

    /* Erase... */
    sq_foreach(&(me->q), SQ_ELEM_IS_EQUAL, &item, SQ_ERASE_FIRST,  NULL);
    /* ...and send next */
    sq_sendnext(&(me->q));

    return SQ_FOUND;
}

//////////////////////////////////////////////////////////////////////

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int32 v)
{
  ottcam_privrec_t *me = PDR2ME(pdr);

    if      (n == OTTCAM_CHAN_K)
    {
        if (v < 16) v = 16;
        if (v > 64) v = 64;
    }
    else if (n == OTTCAM_CHAN_T)
    {
        if (v < 0)     v = 0;
        if (v > 65535) v = 65535;
    }
    else if (n == OTTCAM_CHAN_SYNC)
    {
        v = v != 0;
    }
    //!!! X,Y,W,H

    return v;
}

static void  InitParams(pzframe_drv_t *pdr)
{   
  ottcam_privrec_t *me = PDR2ME(pdr);

  int               n;

    me->nxt_args[OTTCAM_CHAN_X]         = 0;
    me->nxt_args[OTTCAM_CHAN_Y]         = 0;
    me->nxt_args[OTTCAM_CHAN_W]         = OTTCAM_MAX_W;
    me->nxt_args[OTTCAM_CHAN_H]         = OTTCAM_MAX_H;
    me->nxt_args[OTTCAM_CHAN_K]         = 16;
    me->nxt_args[OTTCAM_CHAN_T]         = 30*100;
    me->nxt_args[OTTCAM_CHAN_SYNC]      = 0;
    me->nxt_args[OTTCAM_CHAN_RRQ_MSECS] = RRQ_USECS / 1000;

    for (n = 0;  n < countof(chinfo);  n++)
        if (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
            chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  ottcam_privrec_t *me = PDR2ME(pdr);

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Just for now -- force */
    me->cur_args[OTTCAM_CHAN_X]         = 0;
    me->cur_args[OTTCAM_CHAN_Y]         = 0;
    me->cur_args[OTTCAM_CHAN_W]         = OTTCAM_MAX_W;
    me->cur_args[OTTCAM_CHAN_H]         = OTTCAM_MAX_H;

    /* */
    me->curframe++;
    me->state     = STATE_INITIAL;
    me->rqd_h     = me->cur_args[OTTCAM_CHAN_H];
    me->rest      = me->rqd_h;
    me->smth_rcvd = 0;
    bzero(me->is_rcvd, sizeof(me->is_rcvd));

    sq_clear(&(me->q));
    enq_pkt(me, OTTCMD_RESET,     0,                   0);
    enq_pkt(me, OTTCMD_SETMODE,   OTTMODE_SINGLE_SHOT, 0);
    enq_pkt(me, OTTCMD_WRITEREG,  0x02, 0 + 4); // !!! +X|Y?
    enq_pkt(me, OTTCMD_WRITEREG,  0x01, 0 + 1); // !!! +X|Y?
    enq_pkt(me, OTTCMD_WRITEREG,  0x07, REG_0x07_VAL(me->cur_args[OTTCAM_CHAN_SYNC]));
    enq_pkt(me, OTTCMD_WRITEREG,  0x0B, me->cur_args[OTTCAM_CHAN_T]);
    enq_pkt(me, OTTCMD_WRITEREG,  0x0D, 0);
    enq_pkt(me, OTTCMD_WRITEREG,  0x35, me->cur_args[OTTCAM_CHAN_K]);
    enq_pkt(me, OTTCMD_WRITEREG,  0xAF, 0);
    enq_pkt(me, OTTCMD_WRITEREG,  0x47, 0x808);
    enq_pkt(me, OTTCMD_WRITEREG,  0x48, 0);
    enq_pkt(me, OTTCMD_WRITEREG,  0x70, 0x14);
    enq_pkt(me, OTTCMD_WRITEREG,  OTTPING_REG, OTTPING_VAL);
    enq_pkt(me, OTTCMD_SET_FSIZE, me->cur_args[OTTCAM_CHAN_W],
                                  me->cur_args[OTTCAM_CHAN_H]);
    enq_pkt(me, OTTCMD_NEW_FRAME, 0,    0);

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  ottcam_privrec_t *me = PDR2ME(pdr);

    // Set SYNC:=0 (i.e., start immediately)
    enq_pkt(me, OTTCMD_WRITEREG,  0x07, REG_0x07_VAL(0));

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  ottcam_privrec_t *me = PDR2ME(pdr);

    sq_clear(&(me->q));
    SendReset(me);
    me->state = STATE_READY;
    DropRrqTout(me);

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  ottcam_privrec_t *me = PDR2ME(pdr);

    /* In fact, at this point data is always ready (to some extent) */
    me->state = STATE_READY;
    DropRrqTout(me);

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  ottcam_privrec_t *me = PDR2ME(pdr);
  int               l;

  int               n;
  int               x;

    /* Wipe-out data in absent lines */
    if (me->rest != 0)
        for (l = 0;  l < OTTCAM_MAX_H;  l++)
            if (!me->is_rcvd[l])
                bzero(ACCESS_DATA_LINE(me, l),
                      sizeof(me->retdata[0]) * OTTCAM_MAX_W);

    me->cur_args[OTTCAM_CHAN_MISS] = me->rest;

    /**/
    n = 0;

    /* Scalar STATUS channels */
    for (x = 0;  x < countof(chinfo);  x++)
        if (chinfo[x].chtype == PZFRAME_CHTYPE_STATUS)
        {
            if (chinfo[x].refchn >= 0) // Copy CUR value from respective control one
                me->cur_args[x] = me->cur_args[chinfo[x].refchn];
            me->nxt_args[x] = me->cur_args[x]; // For them to stay in place upon copy cur_args[]=nxt_args[]
            me->r.addrs [n] = x;
            me->r.dtypes[n] = CXDTYPE_INT32;
            me->r.nelems[n] = 1;
            me->r.val_ps[n] = me->cur_args + x;
            me->r.rflags[n] = 0;
            n++;
        }

    /* Data, if was requested */
    if (me->data_rqd)
    {
        me->r.addrs [n] = OTTCAM_CHAN_DATA;
        me->r.dtypes[n] = OTTCAM_DTYPE;
        me->r.nelems[n] = me->cur_args[OTTCAM_CHAN_W] * me->cur_args[OTTCAM_CHAN_H];
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    /* ...and drop "requested" flags */
    me->data_rqd = 0;

    /* Store retbufs data */
    me->pz.retbufs.count      = n;
    me->pz.retbufs.addrs      = me->r.addrs;
    me->pz.retbufs.dtypes     = me->r.dtypes;
    me->pz.retbufs.nelems     = me->r.nelems;
    me->pz.retbufs.values     = me->r.val_ps;
    me->pz.retbufs.rflags     = me->r.rflags;
    me->pz.retbufs.timestamps = NULL;
}


static void rrq_timeout(int devid, void *devptr,
                        sl_tid_t tid,
                        void *privptr);

static void RequestMissingLines(ottcam_privrec_t *me)
{
  uint8 *b_p;
  uint8 *e_p;

    // !!! Start from Y, end at Y+H
    b_p = memchr(me->is_rcvd, 0, countof(me->is_rcvd));
    e_p = memchr(b_p,         1, countof(me->is_rcvd) - (b_p - me->is_rcvd));
    if (e_p == NULL) e_p = me->is_rcvd + countof(me->is_rcvd) - 1;

    sq_clear(&(me->q));
    enq_pkt(me, OTTCMD_SND_FRAME, b_p - me->is_rcvd, e_p - me->is_rcvd);
    fprintf(stderr, "%s [%d] %s(): %d,%d\n", strcurtime(), me->devid, __FUNCTION__, (int)(b_p - me->is_rcvd), (int)(e_p - me->is_rcvd));

    me->state     = STATE_REGET;
    me->rrq_rest  = e_p - b_p + 1;
    me->smth_rcvd = 0;

    DropRrqTout(me);
    me->rrq_tid = sl_enq_tout_after(me->devid, me, GET_RRQ_USECS(me), rrq_timeout, NULL);
}

static void rrq_timeout(int devid, void *devptr,
                        sl_tid_t tid,
                        void *privptr)
{
  ottcam_privrec_t    *me = (ottcam_privrec_t *) devptr;
    
    me->rrq_tid = -1;
    if (me->state == STATE_READY)
    {
        /*!!!?*/
        return;
    }

    /* Okay, have we received something at all? */
    if (me->smth_rcvd)
    {
        RequestMissingLines(me);
        return;
    }

    pzframe_drv_drdy_p(&(me->pz), 1, CXRF_IO_TIMEOUT);
}

static void DecodeRawToData(uint8 *src, OTTCAM_DATUM_T *dst)
{
  register uint8          *srcp = src;
  register OTTCAM_DATUM_T *dstp = dst;
  OTTCAM_DATUM_T           b0, b1, b2, b3;
  int                      x;

    for (x = OTTCAM_MAX_W / 3;  x > 0;  x--)
    {
        b3 = *srcp++;
        b2 = *srcp++;
        b1 = *srcp++;
        b0 = *srcp++;

        *dstp++ = ((b0     )     ) | ((b1 & 3 ) << 8);
        *dstp++ = ((b1 >> 2) & 63) | ((b2 & 15) << 6);
        *dstp++ = ((b2 >> 4) & 15) | ((b3 & 63) << 4);
    }
}

static void ottcam_fd_p(int devid, void *devptr,
                        sl_fdh_t fdhandle, int fd, int mask,
                        void *privptr)
{
  ottcam_privrec_t    *me = (ottcam_privrec_t *) devptr;
  uint8                packet[2000];
  ottcam_hdr_t         hdr;
  int                  frame_n;
  int                  line_n;
  int                  r;
  int                  repcount;
  int                  was_good;
  int                  usecs = 0;

    for (repcount = me->rest + 100, was_good = 0;
         repcount > 0;
         repcount--,                was_good = 1)
    {
        errno = 0;
        r = recv(fd, packet, sizeof(packet), 0);
        if (r <= 0)
        {
            if (ERRNO_WOULDBLOCK())
            {
                /* Okay, have just emptied the buffer */
                /* Sure?  Have we read at least one packet? */
                if (was_good == 0)
                {
                    DoDriverLog(devid, DRIVERLOG_ERR,
                                "%s() called w/o data!", __FUNCTION__);
                    SetDevState(devid, DEVSTATE_OFFLINE, CXRF_DRV_PROBL, "fd_p() no data");
                    return;
                }
                goto NORMAL_EXIT;
            }
            DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "%s: recv()=%d", __FUNCTION__, r);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_DRV_PROBL, "recv() error");
            return;
        }

        if (me->state == STATE_READY) goto CONTINUE_TO_NEXT_PACKET;

        /* Should we also check that the packet is of right size? */

        memcpy(&hdr, packet, sizeof(hdr));
        hdr.id   = ntohl(hdr.id);
        hdr.seq  = ntohs(hdr.seq);
        hdr.cmd  = ntohs(hdr.cmd);
        hdr.par1 = ntohs(hdr.par1);
        hdr.par2 = ntohs(hdr.par2);

        ////fprintf(stderr, "hdr.id=%08x seq=%d cmd=%d par1=%d par2=%d\n", hdr.id, hdr.seq, hdr.cmd, hdr.par1, hdr.par2);
        switch (hdr.id)
        {
            case OTTRPY_EXPO:
                enq_pkt(me, OTTCMD_SND_FRAME, 0, OTTCAM_MAX_H - 1);
                break;

            case OTTRPY_NEXP:
                fprintf(stderr, "%s [%d] NEXP(seq=%d,cur=%d): cmd=%d par1=%d par2=%d\n",
                        strcurtime(), devid, hdr.seq, me->curframe, hdr.cmd, hdr.par1, hdr.par2);
                pzframe_drv_drdy_p(&(me->pz), 1, CXRF_CAMAC_NO_Q);
                break;

            case OTTRPY_ACKN:
                esn_pkt(me, hdr.cmd, hdr.par1, hdr.par2);

                /*!!! Why this "==STATE_INITIAL" check is here?
                  A rudiment from tsycamlib/tsycamv_drv (old CX days),
                  which had only 1 timeout? */
                if (me->state == STATE_INITIAL  &&  // In STATE_REGET there's another timeout
                    hdr.cmd   == OTTPING_CMD    &&
                    hdr.par1  == OTTPING_REG    &&
                    //(fprintf(stderr, "%s %s(%d)/PING par2=0x%x\n", strcurtime(), __FUNCTION__, devid, hdr.par2)  ||  1)  &&
                    hdr.par2  != OTTPING_VAL)
                {
                    /* Ouch... The camera has been reset! */
                    pzframe_drv_drdy_p(&(me->pz), 1, CXRF_REM_C_PROBL);
                }

                break;

            case OTTRPY_NACK:
                DoDriverLog(devid, 0, "NACK(seq=%d,cur=%d): cmd=%d par1=%d par2=%d",
                            hdr.seq, me->curframe, hdr.cmd, hdr.par1, hdr.par2);
                esn_pkt(me, hdr.cmd, hdr.par1, hdr.par2);
                break;
            
            case OTTRPY_DATA:
                frame_n = packet[5] + ((unsigned int)packet[4]) * 256;
                line_n  = packet[7] + ((unsigned int)packet[6]) * 256;
                ////fprintf(stderr, "DATA frame=%d line=%d rest=%d\n", frame_n, line_n, me->rest);

                /* Check frame # */
                if (frame_n != me->curframe)
                    goto CONTINUE_TO_NEXT_PACKET;

                /* Check line # */
                if (line_n >= OTTCAM_MAX_H)
                {
                    DoDriverLog(devid, 0 | DRIVERLOG_C_PKTINFO, "line=%d >= h=%d", line_n, OTTCAM_MAX_H);
                    goto CONTINUE_TO_NEXT_PACKET;
                }

                if (me->is_rcvd[line_n]                   ||
                    line_n < me->cur_args[OTTCAM_CHAN_Y]  ||
                    line_n > me->cur_args[OTTCAM_CHAN_Y] +
                             me->cur_args[OTTCAM_CHAN_H] - 1)
                {
                    goto CONTINUE_TO_NEXT_PACKET;
                }

                /* Okay, a valuable data line */

                /* Should we start a timer? */
                /* Note: it could be better to request timeout upon EXPO,
                         but upon receive of first packet is a more general
                         approach (which would work in other cases, without
                         EXPO -- like tsycam). */
                if (me->smth_rcvd == 0  &&  me->state != STATE_REGET)
                    usecs = GET_RRQ_USECS(me);
                me->smth_rcvd = 1;

                DecodeRawToData(packet + 8, ACCESS_DATA_LINE(me, line_n));

                me->is_rcvd[line_n] = 1;
                me->rest--;
                if (me->rest == 0)
                {
                    DropRrqTout(me);
                    pzframe_drv_drdy_p(&(me->pz), 1, 0);
                    return;
                }

                if (me->state == STATE_REGET)
                {
                    me->rrq_rest--;
                    if (me->rrq_rest == 0)
                    {
                        RequestMissingLines(me);
                        return;
                    }
                }

            default:
                ;
        }

 CONTINUE_TO_NEXT_PACKET:;
    }

 NORMAL_EXIT:;
    if (usecs > 0)
    {
        if (me->rrq_tid >= 0)
            sl_deq_tout(me->rrq_tid);
        me->rrq_tid = sl_enq_tout_after(devid, devptr, usecs, rrq_timeout, NULL);
    }
}

static void ottcam_alv(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr)
{
  ottcam_privrec_t   *me = (ottcam_privrec_t *) devptr;
  ottcam_hdr_t        hdr;
  int                 r;

    me->alv_tid = sl_enq_tout_after(me->devid, devptr, ALIVE_USECS, ottcam_alv, NULL);

    if (1)
    {
        hdr.id   = htonl(OTTREQ_COMM);
        hdr.seq  = htons(me->curframe);
        hdr.cmd  = htons(OTTPING_CMD);
        hdr.par1 = htons(OTTPING_REG);
        r = send(me->fd, &hdr, sizeof(hdr), 0);
    }
}

//////////////////////////////////////////////////////////////////////

enum
{
    SIMSQ_W = 60,
    SIMSQ_H = 50,
    SIM_PERIOD = 200*1000 // 200ms
};

static void sim_heartbeat(int devid, void *devptr,
                          sl_tid_t tid,
                          void *privptr)
{
  ottcam_privrec_t *me = (ottcam_privrec_t *) devptr;
  int               x;
  int               y;
  OTTCAM_DATUM_T   *p;
  pzframe_drv_t    *pdr;

    me->sq_tid = -1;

    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));
  
    me->cur_args[OTTCAM_CHAN_X]    = 0;
    me->cur_args[OTTCAM_CHAN_Y]    = 0;
    me->cur_args[OTTCAM_CHAN_W]    = OTTCAM_MAX_W;
    me->cur_args[OTTCAM_CHAN_H]    = OTTCAM_MAX_H;
    me->cur_args[OTTCAM_CHAN_MISS] = 0;

    me->sim_x += me->sim_vx;
    me->sim_y += me->sim_vy;

    if (me->sim_x < 0)
    {
        me->sim_x  = -me->sim_x;
        me->sim_vx = -me->sim_vx;
    }
    if (me->sim_y < 0)
    {
        me->sim_y  = -me->sim_y;
        me->sim_vy = -me->sim_vy;
    }
    if (me->sim_x > OTTCAM_MAX_W-SIMSQ_W)
    {
        me->sim_x  = (OTTCAM_MAX_W-SIMSQ_W) - (me->sim_x - (OTTCAM_MAX_W-SIMSQ_W));
        me->sim_vx = -me->sim_vx;
    }
    if (me->sim_y > OTTCAM_MAX_H-SIMSQ_H)
    {
        me->sim_y  = (OTTCAM_MAX_H-SIMSQ_H) - (me->sim_y - (OTTCAM_MAX_H-SIMSQ_H));
        me->sim_vy = -me->sim_vy;
    }

    bzero(me->retdata, sizeof(me->retdata));
    for (y = 0;  y < SIMSQ_H;  y++)
        for (x = 0,  p = me->retdata + (me->sim_y + y) * OTTCAM_MAX_W + me->sim_x;
             x < SIMSQ_W;
             x++,    p++)
            *p = (me->cur_args[OTTCAM_CHAN_T] +
                  me->cur_args[OTTCAM_CHAN_K] +
                  x * 4 + y * 4) & 1023;

#if 1 /*!!! No simulation for now... */
    me->data_rqd = 1;
    pdr = &(me->pz);
    PrepareRetbufs(pdr, 0);
    ReturnDataSet(devid,
                  pdr->retbufs.count,
                  pdr->retbufs.addrs,  pdr->retbufs.dtypes, pdr->retbufs.nelems,
                  pdr->retbufs.values, pdr->retbufs.rflags, pdr->retbufs.timestamps);
//    ReturnBigc(devid, 0, me->cur_args, OTTCAM_NUM_PARAMS,
//               me->retdata, sizeof(me->retdata), OTTCAM_DATAUNITS,
//               0);
#endif
    
    me->sq_tid = sl_enq_tout_after(devid, devptr, SIM_PERIOD, sim_heartbeat, NULL);
}

//////////////////////////////////////////////////////////////////////

static int ottcam_init_d(int devid, void *devptr, 
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  ottcam_privrec_t   *me = (ottcam_privrec_t *) devptr;

  unsigned long       host_ip;
  struct hostent     *hp;
  struct sockaddr_in  cam_addr;
  int                 fd;
  size_t              bsize;                     // Parameter for setsockopt
  int                 r;

    me->devid   = devid;
    me->fd      = -1;
    me->sq_tid  = -1;
    me->rrq_tid = -1;

    if (auxinfo == NULL  ||  *auxinfo == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s: either CCD-camera hostname/IP, or '-' should be specified in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    if (auxinfo[0] == '-')
    {
        me->is_sim = 1;
        
        InitParams(&(me->pz));

        me->sim_x  = 300;
        me->sim_y  = 200;
        me->sim_vx = SIMSQ_W/2;
        me->sim_vy = SIMSQ_H/2;

        me->sq_tid = sl_enq_tout_after(devid, devptr, SIM_PERIOD, sim_heartbeat, NULL);

        return DEVSTATE_OPERATING;
    }
    else
    {
        /* Find out IP of the host */
        
        /* First, is it in dot notation (aaa.bbb.ccc.ddd)? */
        host_ip = inet_addr(auxinfo);
        
        /* No, should do a hostname lookup */
        if (host_ip == INADDR_NONE)
        {
            hp = gethostbyname(auxinfo);
            /* No such host?! */
            if (hp == NULL)
            {
                DoDriverLog(devid, 0, "%s: unable to resolve name \"%s\"",
                            __FUNCTION__, auxinfo);
                return -CXRF_CFG_PROBL;
            }
            
            memcpy(&host_ip, hp->h_addr, hp->h_length);
        }
        
        /* Record camera's ip:port */
        cam_addr.sin_family = AF_INET;
        cam_addr.sin_port   = htons(DEFAULT_OTTCAM_PORT);
        memcpy(&(cam_addr.sin_addr), &host_ip, sizeof(host_ip));

        /* Create a UDP socket */
        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "%s: socket()", __FUNCTION__);
            return -CXRF_DRV_PROBL;
        }
        me->fd = fd;
        /* Turn it into nonblocking mode */
        set_fd_flags(fd, O_NONBLOCK, 1);
        /* And set RCV buffer size */
        bsize = 1500 * OTTCAM_MAX_H;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *) &bsize, sizeof(bsize));
        /* Specify the other endpoint */
        r = connect(fd, &cam_addr, sizeof(cam_addr));
        if (r != 0)
        {
            DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "%s: connect()", __FUNCTION__);
            return -CXRF_DRV_PROBL;
        }

        /* Init pzframe */
        pzframe_drv_init(&(me->pz), devid,
                         OTTCAM_CHAN_SHOT,
                         OTTCAM_CHAN_ISTART,
                         OTTCAM_CHAN_WAITTIME,
                         OTTCAM_CHAN_STOP,
                         OTTCAM_CHAN_ELAPSED,
                         StartMeasurements, TrggrMeasurements,
                         AbortMeasurements, ReadMeasurements,
                         PrepareRetbufs);

        InitParams(&(me->pz));

        /* Init queue */
        r = sq_init(&(me->q), NULL,
                    OTTCAM_SENDQ_SIZE, sizeof(ottqelem_t),
                    OTTCAM_SENDQ_TOUT, me,
                    ottcam_sender,
                    ottcam_eq_cmp_func,
                    tim_enqueuer,
                    tim_dequeuer);
        if (r < 0)
        {
            DoDriverLog(me->devid, 0 | DRIVERLOG_ERRNO, "%s: sq_init()", __FUNCTION__);
            return -CXRF_DRV_PROBL;
        }

        sl_add_fd(me->devid, devptr, fd, SL_RD, ottcam_fd_p, NULL);

        me->alv_tid = sl_enq_tout_after(me->devid, devptr, ALIVE_USECS, ottcam_alv, NULL);

        return DEVSTATE_OPERATING;
    }
}

static void ottcam_term_d(int devid, void *devptr)
{
  ottcam_privrec_t *me = (ottcam_privrec_t *) devptr;

    if (me->devid == 0) return; // For non-initialized devices

    if (me->is_sim)
    {
        if (me->sq_tid >= 0) 
            sl_deq_tout(me->sq_tid);
        me->sq_tid = -1;
    }
    else
    {
        SendReset(me);
        if (me->alv_tid >= 0)
            sl_deq_tout(me->alv_tid);
        me->alv_tid = -1;
        pzframe_drv_term(&(me->pz));
        sq_fini(&(me->q));
        /*!!! fd, fdio?*/
    }
}

static void ottcam_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  ottcam_privrec_t *me = (ottcam_privrec_t *)devptr;

  int               n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int               chn;   // channel
  int               ct;
  int32             val;   // Value

    if (me->is_sim) return;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if (chn < 0  ||  chn >= countof(chinfo)  ||
            (ct = chinfo[chn].chtype) == PZFRAME_CHTYPE_UNSUPPORTED)
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else if (ct == PZFRAME_CHTYPE_BIGC)
        {
            me->data_rqd = 1;

            pzframe_drv_req_mes(&(me->pz));
        }
        else if (ct == PZFRAME_CHTYPE_MARKER)
        {/* Ignore */}
        else
        {
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

            if      (ct == PZFRAME_CHTYPE_STATUS  ||  ct == PZFRAME_CHTYPE_AUTOUPDATED)
            {
                ReturnInt32Datum(devid, chn, me->cur_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_VALIDATE)
            {
                if (action == DRVA_WRITE)
                    me->nxt_args[chn] = ValidateParam(&(me->pz), chn, val);
                ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_INDIVIDUAL)
            {
                /* No individual channels in ottcam */
            }
            else /*  ct == PZFRAME_CHTYPE_PZFRAME_STD, which also returns UPSUPPORTED for unknown */
                ////fprintf(stderr, "PZFRAME_STD(%d,%d)\n", chn, val),
                pzframe_drv_rw_p  (&(me->pz), chn,
                                   action == DRVA_WRITE? val : 0,
                                   action);
        }

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(ottcam,  "Ottmar's Camera",
                   NULL, NULL,
                   sizeof(ottcam_privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   ottcam_init_d, ottcam_term_d, ottcam_rw_p);
