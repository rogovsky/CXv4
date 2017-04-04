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

#include "drv_i/triadatv_bku6_drv_i.h"

//// Device specifications ///////////////////////////////////////////

enum {TRIADATV_PORT = 10001};

//// Driver definitions //////////////////////////////////////////////

enum
{
    RECONNECT_DELAY  =  10*1000*1000, /* 10 seconds */
    HEARTBEAT_PERIOD = 300*1000*1000, /* 300 seconds = 5 minutes */
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
} privrec_t;

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

static int triadatv_bku6_init_d(int devid, void *devptr,
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

    /* Try to start */
    r = InitiateStart(devid, me);
    if (r < 0)
        return r;

    HeartBeat(devid, me, -1, NULL);

    return DEVSTATE_NOTREADY;
}

static void triadatv_bku6_term_d(int devid, void *devptr)
{
  privrec_t  *me = (privrec_t *)devptr;

    // SendTerm(me)?
}

static void ProcessInData     (int devid, privrec_t *me,
                               const char *pkt, size_t inpktsize)
{
//fprintf(stderr, "<%s>\n", pkt);
}

static void triadatv_bku6_rw_p(int devid, void *devptr,
                               int action,
                               int count, int *addrs,
                               cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me = (privrec_t *)devptr;
  int         n;     // channel N in values[] (loop ctl var)
  int         chn;   // channel indeX
  int32       val;   // Value

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



 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(triadatv_bku6, "Triada-TV BKU-6 driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   triadatv_bku6_init_d, triadatv_bku6_term_d, triadatv_bku6_rw_p);
