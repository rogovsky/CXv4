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
#include "remdrv_proto_v4.h"


#ifndef MAY_USE_INT64 /*!!! isn't checked... */
  #define MAY_USE_INT64 1
#endif


typedef enum
{
    DRVLET_LITTLE_ENDIAN = 0,
    DRVLET_BIG_ENDIAN    = 1,
} drvlet_endianness_t;

typedef enum
{
    DRVLET_FORKED = 1,
    DRVLET_REMOTE = 2,
} drvlet_kind_t;

enum
{
    MAX_BUSINFOCOUNT = 20,            /* ==cxsd_dbP.h::CXSD_DB_MAX_BUSINFOCOUNT */
    RECONNECT_DELAY  =  10*1000*1000, /* 10 seconds */
#if 1
    HEARTBEAT_PERIOD =  60*1000*1000, /* 60 seconds = 1 minute (26.11.2018) */
#else
    HEARTBEAT_PERIOD = 300*1000*1000, /* 300 seconds = 5 minutes */
#endif
};

enum
{
    CFG_PROBL_RETRIES     = 2, /* How many times to ignore CFG_PROBL before giving up */
    CONFIG_TO_CHSTAT_SECS = 3, /* Maximum delay between REMDRV_C_CONFIG and REMDRV_C_CHSTAT/OFFLINE */
};


static drvlet_endianness_t local_e =
#if   BYTE_ORDER == LITTLE_ENDIAN
    DRVLET_LITTLE_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
    DRVLET_BIG_ENDIAN
#else
    -1
#error Sorry, the "BYTE_ORDER" is neither "LITTLE_ENDIAN" nor "BIG_ENDIAN"
#endif
    ;

//// Type definitions ////////////////////////////////////////////////

typedef uint16  (*l2r_i16_t)       (uint16  v);
typedef uint32  (*l2r_i32_t)       (uint32  v);
typedef uint64  (*l2r_i64_t)       (uint64  v);
typedef float32 (*l2r_f32_t)       (float32 v);
typedef float64 (*l2r_f64_t)       (float64 v);
typedef void    (*memcpy_l2r_i16_t)(uint16 *dst, uint16 *src, int count);
typedef void    (*memcpy_l2r_i32_t)(uint32 *dst, uint32 *src, int count);
typedef void    (*memcpy_l2r_i64_t)(uint64 *dst, uint64 *src, int count);

typedef uint16  (*r2l_i16_t)       (uint16  v);
typedef uint32  (*r2l_i32_t)       (uint32  v);
typedef uint64  (*r2l_i64_t)       (uint64  v);
typedef float32 (*r2l_f32_t)       (float32 v);
typedef float64 (*r2l_f64_t)       (float64 v);
typedef void    (*memcpy_r2l_i16_t)(uint16 *dst, uint16 *src, int count);
typedef void    (*memcpy_r2l_i32_t)(uint32 *dst, uint32 *src, int count);
typedef void    (*memcpy_r2l_i64_t)(uint64 *dst, uint64 *src, int count);

typedef struct
{
    drvlet_kind_t        kind;
    int                  is_ready;
    int                  is_reconnecting;
    int                  was_success;

    int                  fd;
    fdio_handle_t        iohandle;
    sl_tid_t             rcn_tid;

    int                  fail_count;
    struct timeval       config_timestamp;

    int                  last_loglevel;
    int                  last_logmask;

    int                  businfocount;
    int32                businfo[MAX_BUSINFOCOUNT];
    char                 typename[100];
    drvlet_endianness_t  e;
    unsigned long        hostaddr;
    int                  port;

    l2r_i16_t            l2r_i16;
    l2r_i32_t            l2r_i32;
    l2r_i64_t            l2r_i64;
    l2r_f32_t            l2r_f32;
    l2r_f64_t            l2r_f64;
    memcpy_l2r_i16_t     memcpy_l2r_i16;
    memcpy_l2r_i32_t     memcpy_l2r_i32;
    memcpy_l2r_i64_t     memcpy_l2r_i64;
    
    r2l_i16_t            r2l_i16;
    r2l_i32_t            r2l_i32;
    r2l_i64_t            r2l_i64;
    r2l_f32_t            r2l_f32;
    r2l_f64_t            r2l_f64;
    memcpy_r2l_i16_t     memcpy_r2l_i16;
    memcpy_r2l_i32_t     memcpy_r2l_i32;
    memcpy_r2l_i64_t     memcpy_r2l_i64;

    char                 drvletinfo[0];
} privrec_t;

//// Endian conversion routines //////////////////////////////////////

static uint16  noop_i16(uint16  v) {return v;}
static uint32  noop_i32(uint32  v) {return v;}
static uint64  noop_i64(uint64  v) {return v;}
static float32 noop_f32(float32 v) {return v;}
static float64 noop_f64(float64 v) {return v;}
static void    noop_memcpy_i16(uint16 *dst, uint16 *src, int count)
{
    memcpy(dst, src, count * sizeof(*dst));
}
static void    noop_memcpy_i32(uint32 *dst, uint32 *src, int count)
{
    memcpy(dst, src, count * sizeof(*dst));
}
static void    noop_memcpy_i64(uint64 *dst, uint64 *src, int count)
{
    memcpy(dst, src, count * sizeof(*dst));
}

static inline uint16 SwapBytes16(uint16 x)
{
    return
        (((x & 0x00FF) << 8)) |
        (((x & 0xFF00) >> 8) & 0xFF);
}

 /*!!! BUGGY-BUGGY!!! Side-effects */
static inline uint32 SwapBytes32(uint32 x)
{
    return
        (((x & 0x000000FF) << 24)) |
        (((x & 0x0000FF00) <<  8)) |
        (((x & 0x00FF0000) >>  8)) |
        (((x & 0xFF000000) >> 24) & 0xFF);
}
static inline uint64 SwapBytes64(uint64 x)
{
    return
        (((x & 0x00000000000000FF) << 56)) |
        (((x & 0x000000000000FF00) << 40)) |
        (((x & 0x0000000000FF0000) << 24)) |
        (((x & 0x00000000FF000000) <<  8)) |
        (((x & 0x000000FF00000000) >>  8)) |
        (((x & 0x0000FF0000000000) >> 24)) |
        (((x & 0x00FF000000000000) >> 40)) |
        (((x & 0xFF00000000000000) >> 56) & 0xFF);
}

static uint16 swab_i16(uint16 v) {return SwapBytes16(v);}
static uint32 swab_i32(uint32 v) {return SwapBytes32(v);}
static uint64 swab_i64(uint64 v) {return SwapBytes64(v);}
static void  swab_memcpy_i16(uint16 *dst, uint16 *src, int count)
{
    while (count > 0) {*dst++ = SwapBytes16(*src++); count--;}
}
static void  swab_memcpy_i32(uint32 *dst, uint32 *src, int count)
{
    while (count > 0) {*dst++ = SwapBytes32(*src++); count--;}
}
static void  swab_memcpy_i64(uint64 *dst, uint64 *src, int count)
{
    while (count > 0) {*dst++ = SwapBytes64(*src++); count--;}
}

typedef union {uint32 u32; float32 f32;} uif32_t;
typedef union {uint64 u64; float64 f64;} uif64_t;

static float32 swab_f32(float32 v)
{
  uif32_t  s32;
  uif32_t  d32;

    s32.f32 = v;
    d32.u32 = SwapBytes32(s32.u32);
    return d32.f32;
}

static float64 swab_f64(float64 v)
{
  uif64_t  s64;
  uif64_t  d64;

    s64.f64 = v;
    d64.u64 = SwapBytes64(s64.u64);
    return d64.f64;
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

static void CleanupEVRT(int devid, privrec_t *me)
{
    CleanupFDIO(devid, me);
    if (me->rcn_tid >= 0)
    {
        sl_deq_tout(me->rcn_tid);
        me->rcn_tid = -1;
    }
    RegisterDevPtr(devid, NULL); // In fact, an unneeded step
    safe_free(me);
}


static void CommitSuicide(int devid, privrec_t *me,
                          const char *function, const char *reason)
{
    DoDriverLog(devid, DRIVERLOG_CRIT, "%s: FATAL: %s", function, reason);
    CleanupEVRT(devid, me);
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
                    "Successfully %sstarted the drivelet",
                    me->was_success? "re-" : "");
}

//////////////////////////////////////////////////////////////////////
static int  InitiateStart     (int devid, privrec_t *me);
static void InitializeDrivelet(int devid, privrec_t *me);
static void ProcessInData     (int devid, privrec_t *me,
                               remdrv_pkt_header_t *pkt, size_t inpktsize);
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
    /* Is it a forked-drivelet?  Then no way... */
    if (me->kind == DRVLET_FORKED)
    {
        CommitSuicide(devid, me, __FUNCTION__, "reconnect is impossible for local fork()-drivelets");
        return;
    }

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
            InitializeDrivelet(devid, me);
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
                                    REMDRV_MAXPKTSIZE,
                                    sizeof(remdrv_pkt_header_t),
                                    FDIO_OFFSET_OF(remdrv_pkt_header_t, pktsize),
                                    FDIO_SIZEOF   (remdrv_pkt_header_t, pktsize),
                                    me->e == DRVLET_LITTLE_ENDIAN? FDIO_LEN_LITTLE_ENDIAN
                                                                 : FDIO_LEN_BIG_ENDIAN,
                                    1,
                                    0);
    if (me->iohandle < 0)
    {
        DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO,
                    "%s: FATAL: fdio_register_fd()", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }

    return 0;
}

/* Our local copy of remdrvlet_report() -- for failed-to-exec, with shrunk 'data' buffer */
static void remdrvlet_report(int fd, int code, const char *format, ...)
{
  struct {remdrv_pkt_header_t hdr;  int32 data[100];} pkt;
  int      len;
  va_list  ap;

    va_start(ap, format);
    len = vsnprintf((char *)(pkt.data), sizeof(pkt.data), format, ap);
    va_end(ap);

    bzero(&pkt.hdr, sizeof(pkt.hdr));
    pkt.hdr.pktsize = sizeof(pkt.hdr) + len + 1;
    pkt.hdr.command = code;

    write(fd, &pkt, pkt.hdr.pktsize);
}

static int  InitiateStart     (int devid, privrec_t *me)
{
  int                 fdtype;

  int                 pair[2];
  const char         *drvletdir;
  char                path[PATH_MAX];
  char               *args[2];

  struct sockaddr_in  idst;
  socklen_t           addrlen;
  int                 r;
  int                 on    = 1;

    if (me->kind == DRVLET_FORKED)
    {
        fdtype = FDIO_STREAM;

        /* a. Create a pair of interconnected sockets */
        /* Note: [0] is master's, [1] is drivelet's */
        r = socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
        if (r != 0)
        {
            DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO, "socketpair()");
            return -CXRF_DRV_PROBL;
        }

        /* b. Prepare the name for exec */
        drvletdir = GetLayerInfo("remdrv_fork_drvlets_dir", -1);
        if (drvletdir == NULL)
            drvletdir = "lib/server/drivers"; /*!!!DIRSTRUCT*/
        check_snprintf(path, sizeof(path), "%s/%s.drvlet",
                       drvletdir, me->typename);
        
        /* c. Fork into separate process */
        r = fork();
        switch (r)
        {
            case  0:  /* Child */
                /* Clone connection fd into stdin/stdout. !!!Don't touch stderr!!! */
                dup2 (pair[1], 0);  fcntl(0, F_SETFD, 0);
                dup2 (pair[1], 1);  fcntl(1, F_SETFD, 0);
                close(pair[1]); // Close original descriptor...
                close(pair[0]); // ...and our copy of master's one

                /* Exec the required program */
                args[0] = path;
                args[1] = NULL;
                r = execv(path, args);

                /* Failed to exec? */
                remdrvlet_report(1, REMDRV_C_RRUNDP, "can't execv(\"%s\"): %s",
                                 path, strerror(errno));
                _exit(0);
                /* No need for "break" here */

            case -1:  /* Failed to fork */
                DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO, "fork()");
                close(pair[0]);
                close(pair[1]);
                return -CXRF_DRV_PROBL;

            default:  /* Parent */
                me->fd = pair[0];
                close(pair[1]); // Close drivelet's end
        }
    }
    else
    {
        fdtype = FDIO_CONNECTING;

        /* Create a socket */
        me->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (me->fd < 0)
        {
            DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO,
                        "%s: FATAL: socket()", __FUNCTION__);
            return -CXRF_DRV_PROBL;
        }
    
        idst.sin_family = AF_INET;
        idst.sin_port   = htons(me->port);
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
    }

    /* Register with fdio */
    r = RegisterWithFDIO(devid, me, fdtype);
    if (r < 0)
    {
        close(me->fd); me->fd = -1;
        return r;
    }

    /* Okay -- the connection will be completed later */
    return 0;
}

static void InitializeDrivelet(int devid, privrec_t *me)
{
  struct
  {
      remdrv_pkt_header_t  hdr;
      int32                businfo[MAX_BUSINFOCOUNT];
  }            pkt;
  
  size_t       pktsize;

  struct
  {
      remdrv_pkt_header_t  hdr;
  }            dbg;

  static char  newline[] = "\n";

    /* Prepare the CONFIG packet */
    pktsize = sizeof(pkt.hdr)                  +
              sizeof(int32) * me->businfocount +
              strlen(me->drvletinfo) + 1;

    pkt.hdr.pktsize                  = me->l2r_i32(pktsize);
    pkt.hdr.command                  = me->l2r_i32(REMDRV_C_CONFIG);
    pkt.hdr.var.config.businfocount  = me->l2r_i32(me->businfocount);
    pkt.hdr.var.config.proto_version = me->l2r_i32(REMDRV_PROTO_VERSION);
    if (me->businfocount > 0)
        me->memcpy_l2r_i32(pkt.businfo, me->businfo, me->businfocount);

    GetDevLogPrms(devid, &(me->last_loglevel), &(me->last_logmask));
    dbg.hdr.pktsize              = me->l2r_i32(sizeof(dbg));
    dbg.hdr.command              = me->l2r_i32(REMDRV_C_SETDBG);
    dbg.hdr.var.setdbg.verblevel = me->l2r_i32(me->last_loglevel);
    dbg.hdr.var.setdbg.mask      = me->l2r_i32(me->last_logmask);

    ////DoDriverLog(devid, 0, "%s(): pktsize=%zd", __FUNCTION__, pktsize);
    /* Send driver name, CONFIG and SETDBG */
    if (fdio_send(me->iohandle, me->typename,   strlen(me->typename)) < 0
        ||
        fdio_send(me->iohandle, newline,        strlen(newline))      < 0
        ||
        fdio_send(me->iohandle, &pkt,           sizeof(pkt.hdr) + sizeof(int32) * me->businfocount) < 0
        ||
        fdio_send(me->iohandle, me->drvletinfo, strlen(me->drvletinfo) + 1) < 0
        ||
        fdio_send(me->iohandle, &dbg,           sizeof(dbg))          < 0)
        goto FAILURE;
    gettimeofday(&(me->config_timestamp), NULL);

    /* Proclaim that we are ready */
    ReportConnect(devid, me);
    me->is_ready        = 1;
    me->is_reconnecting = 0;
    me->was_success     = 1;
    SetDevState(devid, DEVSTATE_OPERATING, 0, NULL);

    return;
    
 FAILURE:
    DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO, __FUNCTION__);
    ScheduleReconnect(devid, me, "drivelet initialization failure");
}

static void HeartBeat(int devid, void *devptr,
                      sl_tid_t tid,
                      void *privptr)
{
  privrec_t           *me = (privrec_t *) devptr;
  remdrv_pkt_header_t  hdr;

    if (me->is_ready)
    {
        bzero(&hdr, sizeof(hdr));
        hdr.pktsize = me->l2r_i32(sizeof(hdr));
        hdr.command = me->l2r_i32(REMDRV_C_PING_R);

        if (fdio_send(me->iohandle, &hdr, sizeof(hdr)) < 0)
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

//// Driver's methods ////////////////////////////////////////////////

static int  remdrv_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t          *me;
  int                 r;
  int                 x;
  
  const char         *typename;

  const char         *p   = auxinfo;
  const char         *endp;

  char                ec;
  char                hostname[100];
  size_t              hostnamelen;
  const char         *drvletinfo;
  size_t              drvletinfolen1;
  struct hostent     *hp;
  int                 port;

    typename = GetDevTypename(devid);

    /* Check if we aren't run directly */
    if (typename == NULL  ||  *typename == '\0')
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "%s: this driver isn't intended to be run directly, only as TYPENAME/remdrv",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }


    /* I. Split auxinfo into endianness:host and optional drvletinfo */
    /* ("e:host[ drvletinfo]") */

    /* 0. Is there anything at all? */
    if (auxinfo == NULL  ||  auxinfo[0] == '\0')
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "%s: auxinfo is empty...", __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    /* 1. Endianness */
    ec = toupper(*p++);
    if ((ec != 'L'  &&  ec != 'B')  ||  *p++ != ':')
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "%s: auxinfo should start with either 'l:' or 'b:'",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    /* 2. Host */
    endp = p;
    while (*endp != '\0'  &&  !isspace(*endp)  &&  *endp != ':') endp++;
    hostnamelen = endp - p;
    if (hostnamelen == 0)
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "%s: HOST is expected after '%c:'",
                    __FUNCTION__, ec);
        return -CXRF_CFG_PROBL;
    }
    if (hostnamelen > sizeof(hostname) - 1)
    {
        DoDriverLog(devid, DRIVERLOG_WARNING,
                    "%s: HOST is %zu characters long, >%zu",
                    __FUNCTION__, hostnamelen, sizeof(hostname) - 1);
        hostnamelen = sizeof(hostname) - 1;
    }
    memcpy(hostname, p, hostnamelen);
    hostname[hostnamelen] = '\0';
    /* [:PORT] */
    if (*endp == ':')
    {
        p = endp + 1;
        port = strtol(p, &endp, 10);
        if (endp == p  ||
            (*endp != '\0'  &&  !isspace(*endp)))
        {
            DoDriverLog(devid, DRIVERLOG_ERR, "invalid :PORT spec");
            return -CXRF_CFG_PROBL;
        }
    }
    else
        port = REMDRV_DEFAULT_PORT;
    ////fprintf(stderr, "hostname[%d]=<%s>:%d\n", hostnamelen, hostname, port);

    /* 3. Drvletinfo */
    p = endp;
    while (*p != '\0'  &&  isspace(*p)) p++; // Skip whitespace
    drvletinfo = p;
    drvletinfolen1 = strlen(drvletinfo) + 1;
    ////fprintf(stderr, "drvletinfo[%d-1]=<%s>\n", drvletinfolen1, drvletinfo);

    /* (4). Businfo */
    if (businfocount > countof(me->businfo))
    {
        DoDriverLog(devid, DRIVERLOG_WARNING,
                    "%s: BUSINFO is too long, will use first %d elements",
                    __FUNCTION__, countof(me->businfo));

        businfocount = countof(me->businfo);
    }
    
    /* (5). Typename */
    if (strlen(typename) > sizeof(me->typename) - 1)
        DoDriverLog(devid, DRIVERLOG_WARNING,
                    "%s: TYPENAME is too long, will use first %zu characters",
                    __FUNCTION__, sizeof(me->typename) - 1);

    /* II. Allocate privrec and store everything there */
    me = malloc(sizeof(privrec_t) + drvletinfolen1);
    if (me == NULL) return -CXRF_DRV_PROBL;
    bzero(me, sizeof(*me) + drvletinfolen1);
    me->fd       = -1;
    me->iohandle = -1;
    me->rcn_tid  = -1;

    me->businfocount = businfocount;
    for (x = 0;  x < businfocount;  x++) me->businfo[x] = businfo[x];
    strzcpy(me->typename,    typename,   sizeof(me->typename));
    me->e            = (ec == 'L')? DRVLET_LITTLE_ENDIAN : DRVLET_BIG_ENDIAN;
    memcpy (me->drvletinfo,  drvletinfo, drvletinfolen1);

    if (me->e == local_e)
    {
        me->l2r_i16        = noop_i16;
        me->l2r_i32        = noop_i32;
        me->l2r_i64        = noop_i64;
        me->l2r_f32        = noop_f32;
        me->l2r_f64        = noop_f64;
        me->memcpy_l2r_i16 = noop_memcpy_i16;
        me->memcpy_l2r_i32 = noop_memcpy_i32;
        me->memcpy_l2r_i64 = noop_memcpy_i64;
        
        me->r2l_i16        = noop_i16;
        me->r2l_i32        = noop_i32;
        me->r2l_i64        = noop_i64;
        me->r2l_f32        = noop_f32;
        me->r2l_f64        = noop_f64;
        me->memcpy_r2l_i16 = noop_memcpy_i16;
        me->memcpy_r2l_i32 = noop_memcpy_i32;
        me->memcpy_r2l_i64 = noop_memcpy_i64;
    }
    else
    {
        me->l2r_i16        = swab_i16;
        me->l2r_i32        = swab_i32;
        me->l2r_i64        = swab_i64;
        me->l2r_f32        = swab_f32;
        me->l2r_f64        = swab_f64;
        me->memcpy_l2r_i16 = swab_memcpy_i16;
        me->memcpy_l2r_i32 = swab_memcpy_i32;
        me->memcpy_l2r_i64 = swab_memcpy_i64;
        
        me->r2l_i16        = swab_i16;
        me->r2l_i32        = swab_i32;
        me->r2l_i64        = swab_i64;
        me->r2l_f32        = swab_f32;
        me->r2l_f64        = swab_f64;
        me->memcpy_r2l_i16 = swab_memcpy_i16;
        me->memcpy_r2l_i32 = swab_memcpy_i32;
        me->memcpy_r2l_i64 = swab_memcpy_i64;
    }
    
    RegisterDevPtr(devid, me);

    /* III. Find out IP of the host */

    /* What kind of driver should we start? */
    if (strcmp(hostname, ".") == 0)
    {
        /* A local forked-out drivelet -- just remember this fact */
        me->kind = DRVLET_FORKED;
    }
    else
    {
        /* A host */
        me->kind = DRVLET_REMOTE;

        /*First, is it in dot notation (aaa.bbb.ccc.ddd)? */
        me->hostaddr = inet_addr(hostname);
    
        /* No, should do a hostname lookup */
        if (me->hostaddr == INADDR_NONE)
        {
            hp = gethostbyname(hostname);
            /* No such host?! */
            if (hp == NULL)
            {
                DoDriverLog(devid, DRIVERLOG_ERR, "gethostbyname(\"%s\"): %s",
                            hostname, hstrerror(h_errno));
                CleanupEVRT(devid, me);
                return -(CXRF_REM_C_PROBL|CXRF_CFG_PROBL);
            }
    
            memcpy(&(me->hostaddr), hp->h_addr, hp->h_length);
        }
    }
    me->port = port;

    /* IV. Try to start */
    r = InitiateStart(devid, me);
    if (r < 0)
    {
        CleanupEVRT(devid, me);
        return r;
    }

    HeartBeat(devid, me, -1, NULL);

    return DEVSTATE_NOTREADY;
}

static void remdrv_term_d(int devid, void *devptr)
{
  privrec_t  *me = (privrec_t *)devptr;

    if (me == NULL) return; // Terminated before malloc()
    CleanupEVRT(devid, me);
}

static void ProcessInData     (int devid, privrec_t *me,
                               remdrv_pkt_header_t *pkt, size_t inpktsize)
{
  uint32                         command = me->r2l_i32(pkt->command);
  size_t                         expsize;

  remdrv_data_set_rds_t         *rds_p;
  remdrv_data_set_fresh_age_t   *fra_p;
  remdrv_data_set_quant_t       *qnt_p;
  remdrv_data_set_range_t       *rng_p;
  remdrv_data_set_return_type_t *rtt_p;

  int                            cur_loglevel;
  int                            cur_logmask;
  struct
  {
      remdrv_pkt_header_t  hdr;
  }                              dbg;


  int32                          state_to_set;
  int32                          rflags_to_set;
  struct timeval                 now;

  int                  count;
  enum                {SEGLEN_MAX = 100};
  int                  seglen;
  int                  ad2ret[SEGLEN_MAX];
  int                  ne2ret[SEGLEN_MAX];
  void                *vp2ret[SEGLEN_MAX];
  rflags_t             rf2ret[SEGLEN_MAX];
  cx_time_t            ts2ret[SEGLEN_MAX];
  int                  x;

  uint32              *ad_ptr;
  uint32              *ne_ptr;
  uint32              *rf_ptr;
  uint32              *ts_ptr;
  uint8               *dt_ptr;
  int                  data_ofs;
  uint8               *data_ptr;
  size_t               unitsize;

  cxdtype_t            q_dtype;
  size_t               q_dsize;
  CxAnyVal_t           q_val;

  cxdtype_t            range_dtype;
  size_t               range_dsize;
  CxAnyVal_t           range_val[2];

    ////fprintf(stderr, "inpktsize=%d command=0x%08x\n", inpktsize, command);
    switch (command)
    {
        case REMDRV_C_DEBUGP:
            DoDriverLog (devid, me->r2l_i32(pkt->var.debugp.level) &~ DRIVERLOG_ERRNO,
                         "DEBUGP: %s", (char *)(pkt->data));

            GetDevLogPrms(devid, &cur_loglevel, &cur_logmask);
            if (cur_loglevel != me->last_loglevel  ||
                cur_logmask  != me->last_logmask)
            {
                dbg.hdr.pktsize              = me->l2r_i32(sizeof(dbg));
                dbg.hdr.command              = me->l2r_i32(REMDRV_C_SETDBG);
                dbg.hdr.var.setdbg.verblevel = me->l2r_i32(me->last_loglevel);
                dbg.hdr.var.setdbg.mask      = me->l2r_i32(me->last_logmask);

                if (fdio_send(me->iohandle, &dbg, sizeof(dbg)) < 0)
                {
                    DoDriverLog(devid, DRIVERLOG_ERR | DRIVERLOG_ERRNO, "send-SETDBG-upon-DEBUGP failure");
                    ScheduleReconnect(devid, me, "send-SETDBG-upon-DEBUGP failure");
                    return;
                }
            }

            break;

        case REMDRV_C_CRAZY:
            DoDriverLog(devid, DRIVERLOG_CRIT, "CRAZY -- maybe %s<->drivelet version incompatibility",
                        __FILE__);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "CRAZY");
            break;
        
        case REMDRV_C_RRUNDP:
            DoDriverLog(devid, DRIVERLOG_CRIT, "RRUNDP: %s", (char *)(pkt->data));
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "RRUNDP");
            break;
        
        case REMDRV_C_CHSTAT:
            state_to_set  = me->r2l_i32(pkt->var.chstat.state);
            rflags_to_set = me->r2l_i32(pkt->var.chstat.rflags_to_set);

            gettimeofday(&now, NULL); now.tv_sec -= CONFIG_TO_CHSTAT_SECS;
            if (state_to_set  == DEVSTATE_OFFLINE   &&
                rflags_to_set == CXRF_CFG_PROBL     &&
                me->fail_count < CFG_PROBL_RETRIES  &&
                !TV_IS_AFTER(now, me->config_timestamp))
            {
                me->fail_count++;
                DoDriverLog(devid, 0, "ERROR: CXRF_CFG_PROBL; probably a temporary error, will restart");
                ScheduleReconnect(devid, me, "CXRF_CFG_PROBL; probably a temporary error");
                return;
            }
            
            me->fail_count = 0;

            SetDevState(devid, state_to_set, rflags_to_set,
                        inpktsize > sizeof(*pkt)? (char *)(pkt->data) : NULL);
            break;

        case REMDRV_C_REREQD:
            ReRequestDevData(devid);
            break;

        case REMDRV_C_PONG_Y:
            /* We don't need to do anything upon PONG */
            break;

        case REMDRV_C_DATA:
            me->fail_count = 0;
            count = me->r2l_i32(pkt->var.group.count);
            if (count <= 0  ||  count > 10000 /* Arbitrary limit */)
            {
                /* Display a warning (where?)? */
                return;
            }
            // !!! Check inpktsize
            ////fprintf(stderr, "\tcount=%d\n", count);

            ad_ptr = pkt->data;
            ne_ptr = pkt->data + count;
            rf_ptr = pkt->data + count * 2;
            data_ofs = (sizeof(*ad_ptr) + sizeof(*ne_ptr) + sizeof(*rf_ptr))
                * count;
            if (me->r2l_i32(pkt->var.group.first) != 0)
            {
                ts_ptr = rf_ptr + count;
                dt_ptr = (void *)(ts_ptr + count * 3);
                data_ofs += sizeof(*ts_ptr) * 3 * count + count;
            }
            else
            {
                ts_ptr = NULL;
                dt_ptr = (void *)(rf_ptr + count);
                data_ofs += count;
            }
            data_ofs = REMDRV_PROTO_SIZE_CEIL(data_ofs);
            data_ptr = ((uint8*)(pkt->data)) + data_ofs;
            ////fprintf(stderr, "\tdata_ofs=%d ad_ptr=%p ne_ptr=%p rf_ptr=%p ts_ptr=%p dt_ptr=%p\n", data_ofs, ad_ptr, ne_ptr, rf_ptr, ts_ptr, dt_ptr);

            /* Convert vectors all at once, not by seglen pieces later */
            if (me->e != local_e)
            {
                me->memcpy_r2l_i32(ad_ptr, ad_ptr, count);
                me->memcpy_r2l_i32(ne_ptr, ne_ptr, count);
                me->memcpy_r2l_i32(rf_ptr, rf_ptr, count);
                if (ts_ptr != NULL)
                    me->memcpy_r2l_i32(ts_ptr, ts_ptr, count * 3);
            }

            while (count > 0)
            {
                seglen = count;
                if (seglen > SEGLEN_MAX)
                    seglen = SEGLEN_MAX;

                for (x = 0;  x < seglen;  x++)
                {
                    ad2ret[x] = ad_ptr[x];
                    ne2ret[x] = ne_ptr[x];
                    rf2ret[x] = rf_ptr[x];
                    /*!!! Check parameters (at least, data size...) */
                    ////fprintf(stderr, "\t[%d] ad=%x ne=%x rf=%d dt=%d\n", x, ad2ret[x], ne2ret[x], rf2ret[x], dt_ptr[x]);
                    if (ts_ptr != NULL)
                    {
                        ts2ret[x].nsec = ts_ptr[x * 3 + 0];
                        ts2ret[x].sec  = ts_ptr[x * 3 + 1] /*!!! + ((uint64)(ts_ptr[x * 3 + 2]))<<32*/;
                    }

                    unitsize = sizeof_cxdtype(dt_ptr[x]);
                    if (me->e != local_e  &&  ne2ret[x] != 0)
                    {
                        if      (unitsize == 2)
                            me->memcpy_r2l_i16((void*)data_ptr, (void*)data_ptr, ne2ret[x]);
                        else if (unitsize == 4)
                            me->memcpy_r2l_i32((void*)data_ptr, (void*)data_ptr, ne2ret[x]);
                        else if (unitsize == 8)
                            me->memcpy_r2l_i64((void*)data_ptr, (void*)data_ptr, ne2ret[x]);
                    }
                    vp2ret[x] = data_ptr;
                    data_ptr += REMDRV_PROTO_SIZE_CEIL(unitsize * ne2ret[x]);
                }
                ReturnDataSet(devid, seglen,
                              ad2ret, dt_ptr, ne2ret,
                              vp2ret, rf2ret, ts_ptr != NULL? ts2ret : NULL);

                ad_ptr += seglen;
                ne_ptr += seglen;
                rf_ptr += seglen;
                if (ts_ptr != NULL)
                    ts_ptr += seglen * 3;
                dt_ptr += seglen;
                count -= seglen;
            }
            break;

        case REMDRV_C_RDS:
            me->fail_count = 0;
            expsize = sizeof(*pkt) + sizeof(*rds_p);
            if (inpktsize < expsize)
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_RDS inpktsize=%zd, less than %zd",
                            inpktsize, expsize);
                return;
            }
            rds_p = (void *)(pkt->data);
            SetChanRDs(devid,
                       me->r2l_i32(pkt->var.group.first),
                       me->r2l_i32(pkt->var.group.count),
                       me->r2l_f64(rds_p->phys_r), me->r2l_f64(rds_p->phys_d));
            break;

        case REMDRV_C_FRHAGE:
            me->fail_count = 0;
            expsize = sizeof(*pkt) + sizeof(*fra_p);
            if (inpktsize < expsize)
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_FRHAGE inpktsize=%zd, less than %zd",
                            inpktsize, expsize);
                return;
            }
            fra_p = (void *)(pkt->data);
            SetChanFreshAge(devid,
                            me->r2l_i32(pkt->var.group.first),
                            me->r2l_i32(pkt->var.group.count),
                            (cx_time_t){
                                me->r2l_i32(fra_p->age_sec_lo32) /*!!!+hi32<<32*/,
                                me->r2l_i32(fra_p->age_nsec)
                            });
            break;

        case REMDRV_C_QUANT:
            me->fail_count = 0;
            expsize = sizeof(*pkt) + sizeof(*qnt_p);
            if (inpktsize < expsize)
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_QUANT inpktsize=%zd, less than %zd",
                            inpktsize, expsize);
                return;
            }
            qnt_p = (void *)(pkt->data);
            q_dtype = me->r2l_i32(qnt_p->q_dtype);
            q_dsize = sizeof_cxdtype(q_dtype);
            if (q_dsize > sizeof(qnt_p->q_data))
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_QUANT (first=%d,count=%d): q_dtype=%d, sizeof_cxdtype()=%zd, >sizeof(q_data)=%zd",
                            me->r2l_i32(pkt->var.group.first),
                            me->r2l_i32(pkt->var.group.count),
                            q_dtype, q_dsize, sizeof(qnt_p->q_data));
            }
            bzero(&q_val, sizeof(q_val));
            if      (q_dsize == 2)
                me->memcpy_r2l_i16((void*)&q_val, (void*)(qnt_p->q_data), 1);
            else if (q_dsize == 4)
                me->memcpy_r2l_i32((void*)&q_val, (void*)(qnt_p->q_data), 1);
            else if (q_dsize == 8)
                me->memcpy_r2l_i64((void*)&q_val, (void*)(qnt_p->q_data), 1);
            else
                memcpy(&q_val, qnt_p->q_data, q_dsize);
            SetChanQuant(devid,
                         me->r2l_i32(pkt->var.group.first),
                         me->r2l_i32(pkt->var.group.count),
                         q_val, q_dtype);
            break;

        case REMDRV_C_RANGE:
            me->fail_count = 0;
            expsize = sizeof(*pkt) + sizeof(*rng_p);
            if (inpktsize < expsize)
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_RANGE inpktsize=%zd, less than %zd",
                            inpktsize, expsize);
                return;
            }
            rng_p = (void *)(pkt->data);
            range_dtype = me->r2l_i32(rng_p->range_dtype);
            range_dsize = sizeof_cxdtype(range_dtype);
            if (range_dsize > sizeof(rng_p->range_min))
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_RANGE (first=%d,count=%d): range_dtype=%d, sizeof_cxdtype()=%zd, >sizeof(range_min)=%zd",
                            me->r2l_i32(pkt->var.group.first),
                            me->r2l_i32(pkt->var.group.count),
                            range_dtype, range_dsize, sizeof(rng_p->range_min));
            }
            bzero(range_val, sizeof(range_val));
            if      (range_dsize == 2)
            {
                me->memcpy_r2l_i16((void*)(range_val + 0), (void*)(rng_p->range_min), 1);
                me->memcpy_r2l_i16((void*)(range_val + 1), (void*)(rng_p->range_max), 1);
            }
            else if (range_dsize == 4)
            {
                me->memcpy_r2l_i32((void*)(range_val + 0), (void*)(rng_p->range_min), 1);
                me->memcpy_r2l_i32((void*)(range_val + 1), (void*)(rng_p->range_max), 1);
            }
            else if (range_dsize == 8)
            {
                me->memcpy_r2l_i64((void*)(range_val + 0), (void*)(rng_p->range_min), 1);
                me->memcpy_r2l_i64((void*)(range_val + 1), (void*)(rng_p->range_max), 1);
            }
            else
            {
                memcpy(range_val + 0, rng_p->range_min, range_dsize);
                memcpy(range_val + 1, rng_p->range_max, range_dsize);
            }
            SetChanRange(devid,
                         me->r2l_i32(pkt->var.group.first),
                         me->r2l_i32(pkt->var.group.count),
                         range_val[0], range_val[1], range_dtype);
            break;

        case REMDRV_C_RTTYPE:
            me->fail_count = 0;
            expsize = sizeof(*pkt) + sizeof(*rtt_p);
            if (inpktsize < expsize)
            {
                DoDriverLog(devid, DRIVERLOG_ERR,
                            "REMDRV_C_RTTYPE inpktsize=%zd, less than %zd",
                            inpktsize, expsize);
                return;
            }
            rtt_p = (void *)(pkt->data);
            SetChanReturnType(devid,
                              me->r2l_i32(pkt->var.group.first),
                              me->r2l_i32(pkt->var.group.count),
                              me->r2l_i32(rtt_p->return_type));
            break;

        default:;
    }
}

static void remdrv_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me = (privrec_t *)devptr;

  size_t               pktsize;
  remdrv_pkt_header_t  hdr;

  enum                {SEGLEN_MAX = 100};
  int                  seglen;
  int32                ad2snd[SEGLEN_MAX];
  int32                ne2snd[SEGLEN_MAX];
  int                  stage;
  int                  x;
  size_t               valuestotal;

  size_t               size;
  size_t               padsize;
  size_t               unitsize;
  int                  nels;

  uint8               *src;
  uint16               b16;
  uint32               b32;
  uint64               b64;

  static uint8         zeroes[8]; // Up to 7 bytes of padding

    if (action == DRVA_WRITE)
    {
        while (count > 0)
        {
            seglen = count;
            if (seglen > SEGLEN_MAX)
                seglen = SEGLEN_MAX;

            if (fdio_lock_send  (me->iohandle) < 0)
                goto FAILURE;
            for (stage = 0;  stage <= 1;  stage++)
            {
                if (stage != 0)
                {
                    size =
                        sizeof(hdr)
                        +
                        (sizeof(ad2snd[0]) + sizeof(ne2snd[0]))
                        * seglen
                        +
                        seglen /* dtypes */;
                    padsize = REMDRV_PROTO_SIZE_CEIL(size) - size;

                    bzero(&hdr, sizeof(hdr));
                    hdr.pktsize         = me->l2r_i32(size + padsize + valuestotal /* valuestotal is already padded */);
                    hdr.command         = me->l2r_i32(REMDRV_C_WRITE);
                    hdr.var.group.count = me->l2r_i32(seglen);

                    if (fdio_send(me->iohandle, &hdr,   sizeof(hdr))                < 0  ||
                        fdio_send(me->iohandle, ad2snd, seglen * sizeof(ad2snd[0])) < 0  ||
                        fdio_send(me->iohandle, ne2snd, seglen * sizeof(ne2snd[0])) < 0  ||
                        fdio_send(me->iohandle, dtypes, seglen)                     < 0)
                        goto FAILURE;
                    if (padsize    != 0     &&
                        fdio_send(me->iohandle, zeroes, padsize) < 0)
                        goto FAILURE;
                }
                
                for (x = 0, valuestotal = 0;  x < seglen;  x++)
                {
                    unitsize = sizeof_cxdtype(dtypes[x]);
                    nels     = nelems[x];
                    size     = unitsize * nels;
                    padsize  = REMDRV_PROTO_SIZE_CEIL(size) - size;
                    valuestotal += size + padsize;
                    
                    if (stage != 0  &&  size != 0)
                    {
#if 1
                        src = values[x];
                        /// send data element-by-element
                        if      (unitsize == 1  ||  me->e == local_e)
                        {
                            if (fdio_send(me->iohandle, src, size) < 0)
                                goto FAILURE;
                        }
                        else if (unitsize == 2)
                        {
                            while (nels > 0)
                            {
                                b16 = me->l2r_i16(*((uint16*)src));
                                src += unitsize;
                                if (fdio_send(me->iohandle, &b16, unitsize) < 0)
                                    goto FAILURE;
                                nels--;
                            }
                        }
                        else if (unitsize == 4)
                        {
                            while (nels > 0)
                            {
                                b32 = me->l2r_i32(*((uint32*)src));
                                src += unitsize;
                                if (fdio_send(me->iohandle, &b32, unitsize) < 0)
                                    goto FAILURE;
                                nels--;
                            }
                        }
                        else if (unitsize == 8)
                        {
                            while (nels > 0)
                            {
                                b64 = me->l2r_i64(*((uint64*)src));
                                src += unitsize;
                                if (fdio_send(me->iohandle, &b64, unitsize) < 0)
                                    goto FAILURE;
                                nels--;
                            }
                        }
                        else
                        {
                            /* We don't support other elem sizes */
                            if (fdio_send(me->iohandle, src, size) < 0)
                                goto FAILURE;
                        }
#else
                        if (size    != 0  &&
                            fdio_send(me->iohandle, values[x], size)    < 0)
                            goto FAILURE;
#endif
                        if (padsize != 0  &&
                            fdio_send(me->iohandle, zeroes,    padsize) < 0)
                            goto FAILURE;
                        
                    }
                    else
                    {
                        ad2snd[x] = me->l2r_i32(addrs [x]);
                        ne2snd[x] = me->l2r_i32(nelems[x]);
                    }
                }
            }
            if (fdio_unlock_send(me->iohandle) < 0)
                goto FAILURE;

            addrs  += seglen;
            dtypes += seglen;
            nelems += seglen;
            values += seglen;
            count  -= seglen;
        }
    }
    else /*       DRVA_READ */
    {
        while (count > 0)
        {
            seglen = count;
            if (seglen > SEGLEN_MAX)
                seglen = SEGLEN_MAX;

            pktsize = sizeof(hdr) + sizeof(ad2snd[0]) * seglen;
            bzero(&hdr, sizeof(hdr));
            hdr.pktsize         = me->l2r_i32(pktsize);
            hdr.command         = me->l2r_i32(REMDRV_C_READ);
            hdr.var.group.count = me->l2r_i32(seglen);

            for (x = 0;  x < seglen;  x++)
                ad2snd[x] = me->l2r_i32(addrs[x]);

            if (fdio_send(me->iohandle, &hdr,   sizeof(hdr))                < 0  ||
                fdio_send(me->iohandle, ad2snd, sizeof(ad2snd[0]) * seglen) < 0)
                /*!!! 4-byte padding for odd segsize (also count in pktsize)? */
                goto FAILURE;

            addrs += seglen;
            count -= seglen;
        }
    }

    return;
    
 FAILURE:
    DoDriverLog(devid, DRIVERLOG_CRIT | DRIVERLOG_ERRNO, __FUNCTION__);
    ScheduleReconnect(devid, me,
                      action == DRVA_READ ? "send READ failure"
                                          : "send WRITE failure");
}


DEFINE_CXSD_DRIVER(remdrv, "Remote driver adapter",
                   NULL, NULL,
                   0, NULL,
                   0, 1000 /* Seems to be a fair limit */,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   remdrv_init_d, remdrv_term_d, remdrv_rw_p);
