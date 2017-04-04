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

#include "drv_i/ottucc_drv_i.h"


enum
{
    UCCREQ_COMM = 0x6d6d6f63, // "comm"
    UCCRPY_ACKN = 0x6e6b6361, // "ackn"
    UCCRPY_EXPO = 0x6f707865, // "expo"
    UCCRPY_TOUT = 0x74756f74, // "tout"
    UCCRPY_BUSY = 0x79737562, // "busy"
    UCCRPY_ERRV = 0x56727265, // "errV"
    UCCRPY_ERRM = 0x4d727265, // "errM"
    UCCRPY_ERRC = 0x43727265, // "errC"
    UCCRPY_ERRS = 0x53727265, // "errS"
};

typedef int16 OTTUCC_DATUM_T;
enum         {OTTUCC_DTYPE = CXDTYPE_INT16};

//////////////////////////////////////////////////////////////////////

typedef struct
{
    uint32  id;
    uint16  frame_n;
    uint16  cmd_type;
    uint16  cmd_val;
} ottucc_req_t;

enum
{
    OTTUCC_SENDQ_SIZE = 100,
    OTTUCC_SENDQ_TOUT = 100000, // 100ms
};
typedef struct
{
    sq_eprops_t       props;
    ottucc_req_t      req;
} uccqelem_t;

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
}

//////////////////////////////////////////////////////////////////////

static int  ottcam_sender     (void *elem, void *devptr)
{
  ottcam_privrec_t *me = devptr;
  uccqelem_t       *qe = elem;
//  ottcam_hdr_t      hdr;
  int               r;

//    hdr.id   = htonl(qe->hdr.id);
//    hdr.seq  = htons(qe->hdr.seq);
//    hdr.cmd  = htons(qe->hdr.cmd);
//    hdr.par1 = htons(qe->hdr.par1);
//    hdr.par2 = htons(qe->hdr.par2);
    r = send(me->fd, &hdr, sizeof(hdr), 0);
    ////fprintf(stderr, "send=%d\n", r);
    return r > 0? 0 : -1;
}

static int  ottcam_eq_cmp_func(void *elem, void *ptr)
{
  ottucc_req_t *a = &(((uccqelem_t *)elem)->req);
  ottucc_req_t *b = &(((uccqelem_t *)ptr )->req);

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


static int ottucc_init_d(int devid, void *devptr, 
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  ottucc_privrec_t   *me = (ottucc_privrec_t *) devptr;

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
        cam_addr.sin_port   = htons(DEFAULT_OTTUCC_PORT);
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
        bsize = 1500 * OTTUCC_MAX_H;
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
                         OTTUCC_CHAN_SHOT,
                         OTTUCC_CHAN_ISTART,
                         OTTUCC_CHAN_WAITTIME,
                         OTTUCC_CHAN_STOP,
                         OTTUCC_CHAN_ELAPSED,
                         StartMeasurements, TrggrMeasurements,
                         AbortMeasurements, ReadMeasurements,
                         PrepareRetbufs);

        InitParams(&(me->pz));

        /* Init queue */
        r = sq_init(&(me->q), NULL,
                    OTTUCC_SENDQ_SIZE, sizeof(ottqelem_t),
                    OTTUCC_SENDQ_TOUT, me,
                    ottucc_sender,
                    ottucc_eq_cmp_func,
                    tim_enqueuer,
                    tim_dequeuer);
        if (r < 0)
        {
            DoDriverLog(me->devid, 0 | DRIVERLOG_ERRNO, "%s: sq_init()", __FUNCTION__);
            return -CXRF_DRV_PROBL;
        }

        sl_add_fd(me->devid, devptr, fd, SL_RD, ottucc_fd_p, NULL);

        me->alv_tid = sl_enq_tout_after(me->devid, devptr, ALIVE_USECS, ottucc_alv, NULL);

        return DEVSTATE_OPERATING;
    }
}

static void ottucc_term_d(int devid, void *devptr)
{
  ottucc_privrec_t *me = (ottucc_privrec_t *) devptr;

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

static void ottucc_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  ottucc_privrec_t *me = (ottucc_privrec_t *)devptr;

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
                /* No individual channels in ottucc */
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


DEFINE_CXSD_DRIVER(ottucc,  "Ottmar's Camera v2, microcontroller version",
                   NULL, NULL,
                   sizeof(ottucc_privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   ottucc_init_d, ottucc_term_d, ottucc_rw_p);
