#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fix_arpa_inet.h>

#include "cx_sysdeps.h"
#include "timeval_utils.h"

#include "cxsd_driver.h"

#include "sendqlib.h"

#include "drv_i/triadatv_um_drv_i.h"

//// Device specifications ///////////////////////////////////////////

enum {TRIADATV_PORT = 10001};

#define UM_CMD_GET_PARAMS "TX8101"


static int getxdigit(char ch)
{
    if      (ch >= '0'  &&  ch <= '9') return ch - '0';
    else if (ch >= 'A'  &&  ch <= 'F') return ch - 'A' + 10;
    else if (ch >= 'a'  &&  ch <= 'f') return ch - 'a' + 10;
    else                               return -1;
}

static int readxbyte(const char *cp)
{
  int  hi;
  int  lo;

    if ((hi = getxdigit(cp[0])) < 0  ||
        (lo = getxdigit(cp[1])) < 0) return -1;

    return (hi << 4) | lo;
}

static int readxword(const char *cp)
{
  int  hi;
  int  lo;

    if ((hi = readxbyte(cp    )) < 0  ||
        (lo = readxbyte(cp + 2)) < 0) return -1;

    return (hi << 8) | lo;
}

//////////////////////////////////////////////////////////////////////

enum
{
    UM_SENDQ_SIZE  = 100,
    UM_SENDQ_TOUT  = 100000, // 100ms
    UM_MAXPKTBYTES = 100,
};
typedef struct
{
    sq_eprops_t       props;
    int               len;
    char              str[UM_MAXPKTBYTES];
} umqelem_t;

//// Driver definitions //////////////////////////////////////////////

enum
{
    RECONNECT_DELAY  =  10*1000*1000, /* 10 seconds */
    HEARTBEAT_PERIOD =  15*1000*1000, /* 15 seconds, because after 20s of inactivity the device drops connection */
};

typedef struct
{
    int                  devid;
    int                  fd;
    fdio_handle_t        iohandle;
    sl_tid_t             rcn_tid;

    int                  is_ready;
    int                  is_reconnecting;
    int                  was_success;

    unsigned long        hostaddr;

    sq_q_t               q;
    sl_tid_t             sq_tid;
} privrec_t;

//////////////////////////////////////////////////////////////////////

static int  um_sender     (void *elem, void *devptr)
{
  privrec_t  *me = devptr;
  umqelem_t  *qe = elem;

    if (me->is_ready)
    {
        return fdio_send(me->iohandle, qe->str, strlen(qe->str)) < 0? -1 : 0;
    }
    else
    {
        errno = 0;
        return -1;
    }
}

static int  um_eq_cmp_func(void *elem, void *ptr)
{
  umqelem_t  *a = elem;
  umqelem_t  *b = ptr;

    if (b->len >= 0)
        return a->len ==  b->len  &&  memcmp(a->str, b->str,  a->len) == 0;
    else
        return a->len >= -b->len  &&  memcmp(a->str, b->str, -b->len) == 0;
}

static void tout_proc   (int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  privrec_t *me = devptr;
  
    me->sq_tid = -1;
    sq_timeout(&(me->q));
}

static int  tim_enqueuer(void *devptr, int usecs)
{
  privrec_t *me = devptr;

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
  privrec_t  *me = devptr;
  
    if (me->sq_tid >= 0)
    {
        sl_deq_tout(me->sq_tid);
        me->sq_tid = -1;
    }
}

static sq_status_t do_enqx(privrec_t    *me,
                           sq_enqcond_t  how,
                           int           tries,
                           int           timeout_us,
                           const char   *str,
                           int           model_len)
{
  int        len;
  umqelem_t  item;
  umqelem_t  model;

    len = strlen(str);
    if (len < 1  ||  len > UM_MAXPKTBYTES) return SQ_ERROR;
    if (model_len > UM_MAXPKTBYTES  ||
        model_len > len)                   return SQ_ERROR;

    /* Prepare item... */
    bzero(&item, sizeof(item));

    item.props.tries      = tries;
    item.props.timeout_us = timeout_us;

    item.len = len;
    memcpy(item.str, str, len);

    /* ...and model, if required */
    if (model_len > 0)
    {
        model     = item;
        model.len = -model_len;
    }

    return sq_enq(&(me->q), &(item.props), how, model_len > 0? &model : NULL);
}

static sq_status_t esn_pkt(privrec_t *me,
                           const char *model, int model_len)
{
  umqelem_t  item;
  int        abs_len = abs(model_len);
  void      *fe;

    if (abs_len == 0) abs_len = -(model_len = -strlen(model));
    if (abs_len < 1  ||  abs_len > UM_MAXPKTBYTES) return SQ_ERROR;

    /* Make a model for comparison */
    item.len = model_len;
    memcpy(item.str, model, abs_len);

    /* Check "whether we should really erase and send next packet" */
    fe = sq_access_e(&(me->q), 0);
    if (fe != NULL  &&  um_eq_cmp_func(fe, &item) == 0)
        return SQ_NOTFOUND;

    /* Erase... */
    sq_foreach(&(me->q), SQ_ELEM_IS_EQUAL, &item, SQ_ERASE_FIRST,  NULL);
    /* ...and send next */
    sq_sendnext(&(me->q));

    return SQ_FOUND;
}

//// ... /////////////////////////////////////////////////////////////

static void CleanupFDIO(int devid, privrec_t *me)
{
    if (me->iohandle >= 0)
    {
        fdio_deregister(me->iohandle);
        me->iohandle = -1;
    }
    if (me->fd     >= 0)
    {
        close(me->fd);
        me->fd       = -1;
    }
}


static void CommitSuicide(int devid, privrec_t *me,
                          const char *function, const char *reason)
{
    DoDriverLog(devid, DRIVERLOG_CRIT, "%s: FATAL: %s", function, reason);
    SetDevState(devid, DEVSTATE_OFFLINE, CXRF_DRV_PROBL, reason);
}

static void ReportConnfail(int devid, privrec_t *me, const char *function)
{
    if (!me->is_reconnecting)
        DoDriverLog(devid, DRIVERLOG_NOTICE | DRIVERLOG_ERRNO,
                    "%s: connect()", function);
}

static void ReportConnect (int devid, privrec_t *me)
{
    if (me->is_reconnecting)
        DoDriverLog(devid, DRIVERLOG_NOTICE,
                    "Successfully %sconnected",
                    me->was_success? "re-" : "");
}

//////////////////////////////////////////////////////////////////////
static int  InitiateStart     (int devid, privrec_t *me);
static void InitializeRemote  (int devid, privrec_t *me);
static void ProcessInData     (int devid, privrec_t *me,
                               const char *pkt, size_t inpktsize);
//////////////////////////////////////////////////////////////////////

static void TryToReconnect(int devid, void *devptr,
                           sl_tid_t tid,
                           void *privptr)
{
  privrec_t  *me  = (privrec_t *)devptr;
  int         r;
  
    me->rcn_tid = -1;
    r = InitiateStart(devid, me);
    if (r < 0)
        CommitSuicide(devid, me, NULL, NULL);
}
static void ScheduleReconnect(int devid, privrec_t *me, const char *reason)
{
    /* Perform cleanup and notify server of our problem */
    CleanupFDIO(devid, me);
    SetDevState(devid, DEVSTATE_NOTREADY, CXRF_REM_C_PROBL, reason);

    /* And do schedule reconnect */
    me->is_ready        = 0;
    me->is_reconnecting = 1;
    if (me->rcn_tid >= 0) sl_deq_tout(me->rcn_tid);
    me->rcn_tid         = sl_enq_tout_after(devid, me, RECONNECT_DELAY,
                                            TryToReconnect, NULL);
}


static void ProcessIO(int devid, void *devptr,
                      fdio_handle_t iohandle __attribute__((unused)), int reason,
                      void *inpkt, size_t inpktsize,
                      void *privptr __attribute__((unused)))
{
  privrec_t           *me  = (privrec_t *)devptr;

    switch (reason)
    {
        case FDIO_R_DATA:
            ProcessInData     (devid, me, inpkt, inpktsize);
            break;
            
        case FDIO_R_CONNECTED:
            InitializeRemote(devid, me);
            break;

        case FDIO_R_CONNERR:
            ReportConnfail(devid, me, __FUNCTION__);
            ScheduleReconnect(devid, me, "connection failure");
            break;

        case FDIO_R_CLOSED:
        case FDIO_R_IOERR:
            DoDriverLog(devid, DRIVERLOG_NOTICE | DRIVERLOG_ERRNO,
                        "%s: ERROR", __FUNCTION__);
            ScheduleReconnect(devid, me,
                              reason == FDIO_R_CLOSED? "connection closed"
                                                     : "I/O error");
            break;

        case FDIO_R_PROTOERR:
            CommitSuicide(devid, me, __FUNCTION__, "protocol error");
            break;

        case FDIO_R_INPKT2BIG:
            CommitSuicide(devid, me, __FUNCTION__, "received packet is too big");
            break;

        case FDIO_R_ENOMEM:
            CommitSuicide(devid, me, __FUNCTION__, "out of memory");
            break;

        default:
            DoDriverLog(devid, DRIVERLOG_CRIT,
                        "%s: unknown fdiolib reason %d",
                         __FUNCTION__, reason);
            CommitSuicide(devid, me, __FUNCTION__, "unknown fdiolib reason");
    }
}


static int  RegisterWithFDIO  (int devid, privrec_t *me, int fdtype)
{
    me->iohandle = fdio_register_fd(devid, me, me->fd, fdtype,
                                    ProcessIO, NULL,
                                    1024,
                                    0,
                                    0,
                                    0,
                                    FDIO_LEN_LITTLE_ENDIAN,
                                    1, 0);
    if (me->iohandle < 0)
    {
        DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO,
                    "%s: FATAL: fdio_register_fd()", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }

    return 0;
}

static int  InitiateStart     (int devid, privrec_t *me)
{
  struct sockaddr_in  idst;
  socklen_t           addrlen;
  int                 r;
  int                 on    = 1;

    /* Create a socket */
    me->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (me->fd < 0)
    {
        DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO,
                    "%s: FATAL: socket()", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }

    idst.sin_family = AF_INET;
    idst.sin_port   = htons(TRIADATV_PORT);
    memcpy(&idst.sin_addr, &(me->hostaddr), sizeof(me->hostaddr));
    addrlen = sizeof(idst);

    /* Turn on TCP keepalives, ... */
    r = setsockopt(me->fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    /* ...switch to non-blocking mode... */
    r = set_fd_flags(me->fd, O_NONBLOCK, 1);
    /* ...and initiate connection */
    r = connect(me->fd, (struct sockaddr *)&idst, addrlen);
    if (r < 0)
    {
        if (errno == EINPROGRESS)
            ;
        else if (IS_A_TEMPORARY_CONNECT_ERROR())
        {
            ReportConnfail(devid, me, __FUNCTION__);
            ScheduleReconnect(devid, me, "connection failure");
            return 0;
        }
        else
        {
            DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO,
                        "%s: FATAL: connect()", __FUNCTION__);
            close(me->fd); me->fd = -1;
            return -CXRF_DRV_PROBL;
        }
    }

    /* Register with fdio */
    r = RegisterWithFDIO(devid, me, FDIO_STRING_CONN);
    if (r < 0)
    {
        close(me->fd); me->fd = -1;
        return r;
    }

    /* Okay -- the connection will be completed later */
    return 0;
}

static void InitializeRemote  (int devid, privrec_t *me)
{
    sq_clear(&(me->q));

    if (do_enqx(me, SQ_IF_ABSENT, SQ_TRIES_INF, UM_SENDQ_TOUT,
                UM_CMD_GET_PARAMS "\r\n", -1) == SQ_ERROR) goto FAILURE;

    /* Proclaim that we are ready */
    ReportConnect(devid, me);
    me->is_ready        = 1;
    me->is_reconnecting = 0;
    me->was_success     = 1;
    SetDevState(devid, DEVSTATE_OPERATING, 0, NULL);

    return;
    
 FAILURE:
    DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO, __FUNCTION__);
    ScheduleReconnect(devid, me, "remote initialization failure");
}

static void HeartBeat(int devid, void *devptr,
                      sl_tid_t tid,
                      void *privptr)
{
  privrec_t           *me = (privrec_t *) devptr;
  const char          *crlf = "\r\n";

    if (me->is_ready)
    {
        if (fdio_send(me->iohandle, crlf, 2) < 0)
        {
            DoDriverLog(devid, 0 | DRIVERLOG_ERRNO,
                        "%s: send(PING) failure; terminating and scheduling restart",
                        __FUNCTION__);
            ScheduleReconnect(devid, me, "PING failure");
            goto RE_ENQ;
        }
    }

 RE_ENQ:
    sl_enq_tout_after(devid, devptr, HEARTBEAT_PERIOD, HeartBeat, NULL);
}

static int triadatv_um_init_d(int devid, void *devptr,
                              int businfocount, int businfo[],
                              const char *auxinfo)
{
  privrec_t           *me = (privrec_t *) devptr;
  int                  r;
  struct hostent      *hp;

    me->devid    = devid;
    me->fd       = -1;
    me->iohandle = -1;
    me->rcn_tid  = -1;

    SetChanRDs(devid, TRIADATV_UM_CHAN_KP,    1,  10.0, 0);
    SetChanRDs(devid, TRIADATV_UM_CHAN_PHASE, 1,  10.0, 0);
    SetChanRDs(devid, TRIADATV_UM_CHAN_I_n_base,
                      TRIADATV_UM_CHAN_I_n_count, 10.0, 0);

    /* Find out IP of the host */
    
    /* First, is it in dot notation (aaa.bbb.ccc.ddd)? */
    me->hostaddr = inet_addr(auxinfo);
    
    /* No, should do a hostname lookup */
    if (me->hostaddr == INADDR_NONE)
    {
        hp = gethostbyname(auxinfo);
        /* No such host?! */
        if (hp == NULL)
        {
            DoDriverLog(devid, 0, "%s: unable to resolve name \"%s\"",
                        __FUNCTION__, auxinfo);
            return -CXRF_CFG_PROBL;
        }
        
        memcpy(&me->hostaddr, hp->h_addr, hp->h_length);
    }

    /* Init queue */
    r = sq_init(&(me->q), NULL,
                UM_SENDQ_SIZE, sizeof(umqelem_t),
                UM_SENDQ_TOUT, me,
                um_sender,
                um_eq_cmp_func,
                tim_enqueuer,
                tim_dequeuer);
    if (r < 0)  return -CXRF_DRV_PROBL;

    /* Try to start */
    r = InitiateStart(devid, me);
    if (r < 0)
        return r;

    HeartBeat(devid, me, -1, NULL);

    return DEVSTATE_NOTREADY;
}

static void triadatv_um_term_d(int devid, void *devptr)
{
  privrec_t  *me = (privrec_t *)devptr;

    // SendTerm(me)?
    sq_fini(&(me->q));
}

static void ProcessInData     (int devid, privrec_t *me,
                               const char *pkt, size_t inpktsize)
{
  enum      {BUNCH = TRIADATV_UM_CHAN_RD_count};
  int        val;
  int        x;
  int        count;
  int        num_Is;
  int        dataaddrs [BUNCH];
  cxdtype_t  datadtypes[BUNCH];
  int        datanelems[BUNCH];
  int32      datavals  [BUNCH];
  void      *datavals_p[BUNCH];
  rflags_t   datarflags[BUNCH];
  

////fprintf(stderr, "%d <%s>\n", devid, pkt);

    if      (memcmp(pkt, UM_CMD_GET_PARAMS, strlen(UM_CMD_GET_PARAMS)) == 0)
    {
        esn_pkt(me, UM_CMD_GET_PARAMS, 0);

        count = 0;
        bzero(datarflags, sizeof(datarflags));

        dataaddrs[count] = TRIADATV_UM_CHAN_PROTO_VER;
        if ((datavals[count] = readxbyte(pkt + 4 + 2*1))  < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_UM_TYPE;
        if ((datavals[count] = readxbyte(pkt + 4 + 2*2))  < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_UM_NUMBER;
        if ((datavals[count] = readxbyte(pkt + 4 + 2*3))  < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_SERIAL;
        if ((datavals[count] = readxword(pkt + 4 + 2*4))  < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_P_OUT;
        if ((datavals[count] = readxword(pkt + 4 + 2*6))  < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_P_IN;
        if ((datavals[count] = readxword(pkt + 4 + 2*10)) < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_P_REFL;
        if ((datavals[count] = readxword(pkt + 4 + 2*12)) < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_T1;
        if ((datavals[count] = readxbyte(pkt + 4 + 2*14)) < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_T2;
        if ((datavals[count] = readxbyte(pkt + 4 + 2*15)) < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_KP;
        if ((datavals[count] = readxword(pkt + 4 + 2*35)) < 0) return;
        count++;

        dataaddrs[count] = TRIADATV_UM_CHAN_PHASE;
        if ((datavals[count] = readxword(pkt + 4 + 2*37)) < 0) return;
        if (datavals[count] > 32767)
            datavals[count] -= 65536;
        count++;

        if ((val             = readxbyte(pkt + 4 + 2*39)) < 0) return;
        dataaddrs[count] = TRIADATV_UM_CHAN_FAULT_PN;
        datavals [count] = val & 0x1F;
        count++;
        dataaddrs[count] = TRIADATV_UM_CHAN_IS_FAULT;
        datavals [count] = (val >> 6) & 1;
        count++;
        dataaddrs[count] = TRIADATV_UM_CHAN_DISP_PROC;
        datavals [count] = (val >> 5) & 1;
        count++;
        dataaddrs[count] = TRIADATV_UM_CHAN_DISP_STATE;
        datavals [count] = (val >> 7) & 1;
        count++;
        
        if ((val             = readxbyte(pkt + 4 + 2*8))  < 0) return;
        for (x = 0;  x < TRIADATV_UM_CHAN_STATUS12_n_count;  x++)
        {
            dataaddrs[count] = TRIADATV_UM_CHAN_STATUS12_n_base + x;
            datavals [count] = (val >> x) & 1;
            count++;
        }

        dataaddrs[count] = TRIADATV_UM_CHAN_NUM_CURRENTS;
        if ((datavals[count] = readxword(pkt + 4 + 2*37)) < 0) return;
        datavals[count] &= 0x1F; num_Is = datavals[count];
        count++;

        for (x = 0;  x < TRIADATV_UM_CHAN_I_n_count;  x++)
        {
            dataaddrs[count] = TRIADATV_UM_CHAN_I_n_base + x;
            if (x < num_Is)
            {
                if ((datavals[count] = readxbyte(pkt + 4 + 2*(16+x))) < 0)
                {
                    datavals  [count] = 0;
                    datarflags[count] = CXRF_CAMAC_NO_Q;
                }
            }
            else
            {
                datavals  [count] = 0;
                datarflags[count] = CXRF_UNSUPPORTED;
            }
            count++;
        }

        for (x = 0;  x < count;  x++)
        {
            datadtypes[x] = CXDTYPE_INT32;
            datanelems[x] = 1;
            datavals_p[x] = datavals + x;
        }
        ReturnDataSet(devid,
                      count,
                      dataaddrs,  datadtypes, datanelems,
                      datavals_p, datarflags, NULL);
    }
}

static void triadatv_um_rw_p(int devid, void *devptr,
                             int action,
                             int count, int *addrs,
                             cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me = (privrec_t *)devptr;
  int         n;     // channel N in values[] (loop ctl var)
  int         chn;   // channel indeX
  int32       val;   // Value

    if (me->is_ready == 0) return;

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


        if (chn >= TRIADATV_UM_CHAN_STATS_first  &&
            chn <= TRIADATV_UM_CHAN_STATS_last)
        {
            if (do_enqx(me, SQ_IF_ABSENT, SQ_TRIES_INF, UM_SENDQ_TOUT,
                        UM_CMD_GET_PARAMS "\r\n", -1) == SQ_ERROR) goto FAILURE;
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }

    return;

 FAILURE:
    DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO, __FUNCTION__);
    ScheduleReconnect(devid, me, "enq() failure");
}

DEFINE_CXSD_DRIVER(triadatv_um, "Triada-TV BKU-6 driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   triadatv_um_init_d, triadatv_um_term_d, triadatv_um_rw_p);
