#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "fix_arpa_inet.h"

#include "misclib.h"
#include "memcasecmp.h"
#include "cxscheduler.h"
#include "fdiolib.h"

#include "cxsd_hw.h"
#include "cxsd_hwP.h"
#include "cxsd_frontend.h"
#include "cxsd_logger.h"

#include "starogate_proto.h"

static int            l_socket = -1;
static fdio_handle_t  l_handle = -1;

enum {RDS_MAX_COUNT = 20};
enum
{
    I_PHYS_COUNT = 2,
};

enum
{
    MONITOR_EVMASK =
        CXSD_HW_CHAN_EVMASK_UPDATE   |
        CXSD_HW_CHAN_EVMASK_RDSCHG
};

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;
    cxsd_cpntid_t  cpid;
    cxsd_gchnid_t  gcid;
    const char    *name;

    int            phys_count;
    int            phys_n_allocd;    // in PAIRS -- i.e., malloc(sizeof(double)*phys_n_allocd*2)
    double         imm_phys_rds[I_PHYS_COUNT * 2];
    double        *alc_phys_rds;
} moninfo_t;

typedef struct
{
    int              in_use;
    int              fd;
    fdio_handle_t    fhandle;

    uint32           ID;

    moninfo_t       *monitors_list;
    int              monitors_list_allocd;

    cxsd_gchnid_t   *periodics;
    int              periodics_allocd;
    int              periodics_used;
    int              periodics_needs_rebuild;
} clientrec_t;

static clientrec_t *clients_list        = NULL;
static int          clients_list_allocd = 0;

static inline int cp2cd(clientrec_t *cp)
{
    return cp - clients_list;
}

//--------------------------------------------------------------------

// GetMonSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Mon, moninfo_t,
                                 monitors, in_use, 0, 1,
                                 0, 100, 0,
                                 cp->, cp,
                                 clientrec_t *cp, clientrec_t *cp)

static void MonEvproc(int            uniq,
                      void          *privptr1,
                      cxsd_gchnid_t  gcid,
                      int            reason,
                      void          *privptr2);

static void RlsMonSlot(int id, clientrec_t *cp)
{
  moninfo_t *mp = AccessMonSlot(id, cp);

    mp->in_use = 0;

    CxsdHwDelChanEvproc(cp->ID, lint2ptr(cp2cd(cp)),
                        mp->gcid, MONITOR_EVMASK,
                        MonEvproc, lint2ptr(id));

    if (mp->cpid >= 0)
    {
        cp->periodics_used--;
        cp->periodics_needs_rebuild = 1;
    }

    safe_free(mp->name);
    safe_free(mp->alc_phys_rds);
}

//--------------------------------------------------------------------

// GetConnSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Conn, clientrec_t,
                                 clients, in_use, 0, 1,
                                 1, 10, 100,
                                 , , void)

static int del_mon(moninfo_t *mp, void *privptr)
{
  clientrec_t *cp  = privptr;

    RlsMonSlot(mp - cp->monitors_list, cp);
    return 0;
}
static void RlsConnSlot(int cd)
{
  clientrec_t *cp  = AccessConnSlot(cd);
  int          err = errno;        // To preserve errno
  
    if (cp->in_use == 0) return; /*!!! In fact, an internal error */
    
    cp->in_use = 0;

    if (cp->fhandle >= 0) fdio_deregister(cp->fhandle);
    if (cp->fd      >= 0) close          (cp->fd);
    ForeachMonSlot(del_mon, cp, cp);
    DestroyMonSlotArray(cp);
    
    cxsd_hw_do_cleanup  (cp->ID);
    CxsdHwDeleteClientID(cp->ID);

    errno = err;
}

//////////////////////////////////////////////////////////////////////

static int  send_or_close(int cd, const char *s)
{
  clientrec_t    *cp    = AccessConnSlot(cd);

    if (fdio_send(cp->fhandle, s, strlen(s)) < 0)
    {
        RlsConnSlot(cd);
        return -1;
    }

    return 0;
}

static void StoreRDs(moninfo_t *mp, int phys_count, double *rds)
{
    if (phys_count > 0)
    {
        if (mp->alc_phys_rds != NULL  ||  phys_count > I_PHYS_COUNT)
        {
            if (GrowUnitsBuf(&(mp->alc_phys_rds), &(mp->phys_n_allocd),
                             phys_count, sizeof(double) * 2) < 0)
            {
                /*!!!Bark? */
                return;
            }
        }
        memcpy(mp->alc_phys_rds != NULL? mp->alc_phys_rds : mp->imm_phys_rds,
               rds, phys_count * sizeof(double) * 2);
    }
    mp->phys_count = phys_count;
}

static int SendMonitor(int cd, int mid)
{
  clientrec_t    *cp    = AccessConnSlot(cd);
  moninfo_t      *mp    = AccessMonSlot(mid, cp);
  cxsd_hw_chan_t *chn_p = cxsd_hw_channels + mp->gcid;
  void           *src   = chn_p->current_val;

  double          v;
  int             n;
  double         *rdp;

  char            buf[400];
  int             len;

  static const char prefix[] = "100 value ";
  static const char space [] = " ";
  static const char nl    [] = "\n";

    /* 1. Obtain a value */
    // A copy from cda_core.c::cda_dat_p_update_dataset()
    // 1a. Get a double
    switch (chn_p->current_dtype)
    {
        case CXDTYPE_INT32:  v = *((  int32*)src);     break;
        case CXDTYPE_UINT32: v = *(( uint32*)src);     break;
        case CXDTYPE_INT16:  v = *((  int16*)src);     break;
        case CXDTYPE_UINT16: v = *(( uint16*)src);     break;
        case CXDTYPE_DOUBLE: v = *((float64*)src);     break;
        case CXDTYPE_SINGLE: v = *((float32*)src);     break;
        case CXDTYPE_INT64:  v = *((  int64*)src);     break;
        case CXDTYPE_UINT64: v = *(( uint64*)src);     break;
        case CXDTYPE_INT8:   v = *((  int8 *)src);     break;
        default:             v = *(( uint8 *)src);     break;
    }
    // 1b. Perform RD-conversion
    rdp = mp->alc_phys_rds;
    if (rdp == NULL) rdp = mp->imm_phys_rds;
    n   = mp->phys_count;
    while (n > 0)
    {
        v = v / rdp[0] - rdp[1];
        rdp += 2;
        n--;
    }

    /* 2. Convert to string */
    // A copy from snprintf_dbl_trim()
    len = snprintf(buf, sizeof(buf)-1,  "%8.3f", v);
    if (len < 0  ||  (size_t)len > sizeof(buf)-1) buf[len = sizeof(buf)-1] = '\0';

    /* 3. Send */
    if (fdio_lock_send  (cp->fhandle) < 0  ||
        fdio_send       (cp->fhandle, prefix,   strlen(prefix))   < 0  ||
        fdio_send       (cp->fhandle, mp->name, strlen(mp->name)) < 0  ||
        fdio_send       (cp->fhandle, space,    1)                < 0  ||
        fdio_send       (cp->fhandle, buf,      len)              < 0  ||
        fdio_send       (cp->fhandle, nl,       strlen(nl))       < 0  ||
        fdio_unlock_send(cp->fhandle) < 0)
    {
        RlsConnSlot(cd);
        return -1;
    }

    return 0;
}

static void MonEvproc(int            uniq,
                      void          *privptr1,
                      cxsd_gchnid_t  gcid,
                      int            reason,
                      void          *privptr2)
{
  int          cd = ptr2lint(privptr1);
  int          id = ptr2lint(privptr2);
  clientrec_t *cp = AccessConnSlot(cd);
  moninfo_t   *mp = AccessMonSlot(id, cp);

  cxsd_gchnid_t  dummy_gcid;
  int            phys_count;
  double         rds_buf[RDS_MAX_COUNT*2];

    if      (reason == CXSD_HW_CHAN_R_UPDATE)
    {
        SendMonitor(cd, id);
    }
    else if (reason == CXSD_HW_CHAN_R_RDSCHG)
    {
        if (CxsdHwGetCpnProps(mp->cpid, &dummy_gcid,
                              &phys_count, rds_buf, RDS_MAX_COUNT,
                              NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL) < 0) return;
        StoreRDs(mp, phys_count, rds_buf);
    }
}

static int CxsdHwIsChanValReady(int gcid)
{
  cxsd_hw_chan_t        *chn_p = cxsd_hw_channels + gcid;

    if (gcid <= 0  ||  gcid >= cxsd_hw_numchans) return -1;

    return
        chn_p->is_internal
        ||
        (chn_p->timestamp.sec != CX_TIME_SEC_NEVER_READ
         &&
         (chn_p->rw  ||
          (chn_p->is_autoupdated       &&
           chn_p->fresh_age.sec  == 0  &&
           chn_p->fresh_age.nsec == 0)
         )
        );
}

static int mon_eq_checker(moninfo_t *mp, void *privptr)
{
    return strcasecmp(mp->name, privptr) == 0;
}
static void InteractWithClient(int uniq, void *unsdptr,
                               fdio_handle_t handle, int reason,
                               void *inpkt, size_t inpktsize,
                               void *privptr)
{
  int            cd  = ptr2lint(privptr);
  clientrec_t   *cp  = AccessConnSlot(cd);

  const char    *p;
  const char    *nxt;
  size_t         len;

  int            mid;
  moninfo_t     *mp;

  char           namebuf[200];
  double         val;
  void          *vp;

  cxsd_cpntid_t  cpid;
  cxsd_gchnid_t  gcid;
  int            phys_count;
  double         rds_buf[RDS_MAX_COUNT*2];

  int            n;
  double        *rdp;

  /*static*/ cxdtype_t  dt_double = CXDTYPE_DOUBLE;
  /*static*/ int        nels_1    = 1;

  static const char   unrecognized_cmd_msg[] = "500 unrecognized command\n";
  static const char   channame_rqd_msg    [] = "501 channel name required\n";
  static const char   value_rqd_msg       [] = "501 value-to-write required\n";
  static const char   channel_notfound_msg[] = "404 channel not found\n";
  static const char   non_scalar_msg      [] = "406 channel is not scalar\n";
  static const char   non_numeric_msg     [] = "406 channel is not numeric\n";
  static const char   mon_alloc_err_msg   [] = "500 GetMonSlot() failure\n";
  static const char   perbuf_grow_err_msg [] = "500 GrowUnitsBuf(periodics) failure\n";
  static const char   add_chan_err_msg    [] = "500 CxsdHwAddChanEvproc() failure\n";
  static const char   monitor_set_msg     [] = "200 monitor set\n";
  static const char   monitor_absent_msg  [] = "422 monitor absent\n";
  static const char   monitor_del_msg     [] = "200 monitor deleted\n";
  static const char   write_done_msg      [] = "200 write done\n";

    if (reason != FDIO_R_DATA)
    {
        RlsConnSlot(cd);
        return;
    }

    /* Skip leading spaces */
    for (p = inpkt;  *p != '\0'  &&  isspace(*p);  p++);
    if (*p == '\0') return;

    /* Extract the command */
    nxt = strchr(p, ' ');
    if (nxt == NULL)
        nxt = p + strlen(p);
    len = nxt - p;

    /* Select what to do */
    if      (cx_strmemcasecmp(STAROGATE_MONITOR_S, p, len) == 0)
    {
        p = nxt;
        while (*p != '\0'  &&  isspace(*p)) p++;
        if (*p == '\0')
        {
            send_or_close(cd, channame_rqd_msg);
            return;
        }

        /* Does this name exist? */
        if ((cpid = CxsdHwResolveChan(p, &gcid,
                                      &phys_count, rds_buf, RDS_MAX_COUNT,
                                      NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL)) < 0)
        {
            send_or_close(cd, channel_notfound_msg);
            return;
        }
        /* Is type compatible with scalar double? */
        if (cxsd_hw_channels[gcid].max_nelems != 1)
        {
            send_or_close(cd, non_scalar_msg);
            return;
        }
        /* ...and NO, variants (CXDTYPE_UKNNOWN-type channels) are not supported */
        if (reprof_cxdtype(cxsd_hw_channels[gcid].dtype) != CXDTYPE_REPR_INT  &&
            reprof_cxdtype(cxsd_hw_channels[gcid].dtype) != CXDTYPE_REPR_FLOAT)
        {
            send_or_close(cd, non_numeric_msg);
            return;
        }


        /* Okay: isn't it already monitored? */
        mid = ForeachMonSlot(mon_eq_checker, p, cp);
        if (mid < 0)
        {
            mid = GetMonSlot(cp);
            if (mid < 0)
            {
                send_or_close(cd, mon_alloc_err_msg);
                return;
            }
            mp  = AccessMonSlot(mid, cp);
            mp->cpid = -1; // Prevent RlsMonSlot() from "periodics_used--" if called before "periodics_used++"
            if ((mp->name = strdup(p)) == NULL)
            {
                RlsMonSlot(mid, cp);
                send_or_close(cd, mon_alloc_err_msg);
                return;
            }
            /* Add a cell to periodics[] */
            if (cp->periodics_allocd <= cp->periodics_used  &&
                GrowUnitsBuf(&(cp->periodics),
                &(cp->periodics_allocd),
                cp->periodics_allocd + 100,
                sizeof(*(cp->periodics))) < 0)
            {
                RlsMonSlot(mid, cp);
                send_or_close(cd, perbuf_grow_err_msg);
                return;
            }

            /* Fill */
            mp->cpid = cpid;
            mp->gcid = gcid;
            StoreRDs(mp, phys_count, rds_buf);

            /* Subscribe to updates */
            if (CxsdHwAddChanEvproc(cp->ID, lint2ptr(cd),
                                    mp->gcid, MONITOR_EVMASK,
                                    MonEvproc, lint2ptr(mid)) < 0)
            {
                RlsMonSlot(mid, cp);
                send_or_close(cd, add_chan_err_msg);
                return;
            }

        }

        if (send_or_close(cd, monitor_set_msg) != 0) return;

        /* Send current value if it is a write channel and data is ready */
        /*!!! Wouldn't it be good to have a *cxsd_hw* function to determine appropriateness? */
        if (CxsdHwIsChanValReady(gcid) > 0)
            SendMonitor(cd, mid);
    }
    else if (cx_strmemcasecmp(STAROGATE_IGNORE_S, p, len) == 0)
    {
        p = nxt;
        while (*p != '\0'  &&  isspace(*p)) p++;
        if (*p == '\0')
        {
            send_or_close(cd, channame_rqd_msg);
            return;
        }

        /* Does this name exist? */
        if ((cpid = CxsdHwResolveChan(p, &gcid,
                                      &phys_count, rds_buf, RDS_MAX_COUNT,
                                      NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL)) < 0)
        {
            send_or_close(cd, channel_notfound_msg);
            return;
        }

        mid = ForeachMonSlot(mon_eq_checker, p, cp);
        if (mid < 0)
        {
            send_or_close(cd, monitor_absent_msg);
            return;
        }
        RlsMonSlot(mid, cp);

        send_or_close(cd, monitor_del_msg);
    }
    else if (cx_strmemcasecmp(STAROGATE_WRITE_S,  p, len) == 0)
    {
        /* Get channel name */
        p = nxt;
        while (*p != '\0'  &&  isspace(*p)) p++;
        if (*p == '\0')
        {
            send_or_close(cd, channame_rqd_msg);
            return;
        }

        nxt = strchr(p, ' ');
        if (nxt == NULL)
        {
            send_or_close(cd, value_rqd_msg);
            return;
        }
        len = nxt - p;
        if (len > sizeof(namebuf) - 1)
            len = sizeof(namebuf) - 1;
        memcpy(namebuf, p, len); namebuf[len] = '\0';

        /* And value */
        p = nxt;
        while (*p != '\0'  &&  isspace(*p)) p++;
        val = strtod(p, &nxt);
        if (nxt == p  ||  *nxt != '\0')
        {
            send_or_close(cd, value_rqd_msg);
            return;
        }

        /* Does this name exist? */
        if ((cpid = CxsdHwResolveChan(namebuf, &gcid,
                                      &phys_count, rds_buf, RDS_MAX_COUNT,
                                      NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL)) < 0)
        {
            send_or_close(cd, channel_notfound_msg);
            return;
        }
        /* Note: we do NOT check for channel type to be compatible
                 with CXDTYPE_DOUBLE, but lay it on cxsd_hw */

        /* Perform RD-conversion */
        n   = phys_count;
        rdp = rds_buf + (n-1)*2;
        while (n > 0)
        {
            val = (val + rdp[1]) * rdp[0];
            rdp -= 2;
            n--;
        }

        /* Do write */
        fprintf(stderr, "write (%d)%d %8.3f\n", dt_double, nels_1, val);
        vp = &val;
        CxsdHwDoIO(cp->ID, DRVA_WRITE, 1, &gcid, &dt_double, &nels_1, &vp);

        send_or_close(cd, write_done_msg);
    }
    else
    {
        send_or_close(cd, unrecognized_cmd_msg);
    }
}

static void AcceptConnection(int uniq, void *unsdptr,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr)
{
  int                 s;
  struct sockaddr     addr;
  socklen_t           addrlen;

  int                 cd;
  clientrec_t        *cp;

  int                 on = 1;

  static const char   getslot_err_msg[] = "500 GetConnSlot() failure\n";
  static const char   fdioreg_err_msg[] = "500 fdio_register_fd() failure\n";
  static const char   success_grt_msg[] = "220 Hi\n";

    if (reason != FDIO_R_ACCEPT) return;

    /* First, accept() the connection */
    addrlen = sizeof(addr);
    s = fdio_accept(handle, &addr, &addrlen);
    /* Is anything wrong? */
    if (s < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): fdio_accept()", __FUNCTION__);
        return;
    }

    /* Instantly switch it to non-blocking mode */
    if (set_fd_flags(s, O_NONBLOCK, 1) < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): set_fd_flags(O_NONBLOCK)",
                __FUNCTION__);
        /* Note: we do NOT want to send anything to client, because
                 that could block */
        close(s);
        return;
    }

    /* Tune it: */
    // Turn on TCP keepalives
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    // Set Close-on-exec:=1
    fcntl(s, F_SETFD, 1);

    /* Get a connection slot... */
    if ((cd = GetConnSlot()) < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): GetConnSlot() failure",
                __FUNCTION__);
        write(s, getslot_err_msg, strlen(getslot_err_msg));
        close(s);
        return;
    }

    /* ...and fill it with data */
    cp = AccessConnSlot(cd);
    cp->fd    = s;
    cp->ID    = CxsdHwCreateClientID();

    /* Okay, let's obtain an fdio slot... */
    cp->fhandle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                   s, FDIO_STRING,
                                   InteractWithClient, lint2ptr(cd),
                                   1000,
                                   0,
                                   0,
                                   0,
                                   FDIO_LEN_LITTLE_ENDIAN,
                                   1, 0);
    if (cp->fhandle < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): fdio_register_fd() failure",
                __FUNCTION__);
        write(s, fdioreg_err_msg, strlen(fdioreg_err_msg));
        RlsConnSlot(cd);
        return;
    }

    send_or_close(cd, success_grt_msg);
}

//////////////////////////////////////////////////////////////////////

#define Failure(msg) \
    do {             \
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_EMERG, "%s::%s: %s", __FILE__, __FUNCTION__, msg); return -1; \
        goto CLEANUP;                                                                                        \
                             \
    } while (0)
static int  starogate_init_f (int server_instance)
{
  int                 r;                /* Result of system calls */
  struct sockaddr_in  iaddr;            /* Address to bind `l_socket' to */
  int                 on     = 1;       /* "1" for REUSE_ADDR */

    /* Create the socket */
    l_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (l_socket < 0)
        Failure("socket()");

    /* Bind it to the name */
    setsockopt(l_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    iaddr.sin_family      = AF_INET;
    iaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iaddr.sin_port        = htons(STAROGATE_PORT);
    r = bind(l_socket, (struct sockaddr *) &iaddr,
             sizeof(iaddr));
    if (r < 0)
        Failure("bind()");

    /* Mark it as listening and non-blocking */
    listen(l_socket, 5);
    set_fd_flags(l_socket, O_NONBLOCK, 1);

    /* Register with fdiolib */
    l_handle = fdio_register_fd(0, NULL,
                                l_socket, FDIO_LISTEN,
                                AcceptConnection, NULL,
                                0, 0, 0, 0, FDIO_LEN_LITTLE_ENDIAN, 0, 0);
    if (l_handle < 0)
        Failure("fdio_register_fd()");

    return 0;

 CLEANUP:
    if (l_handle >= 0)
    {
        fdio_deregister(l_handle);
        l_handle = -1;
    }
    if (l_socket >= 0)
    {
        close(l_socket);
        l_socket = -1;
    }
    return -1;
}

static void starogate_term_f (void)
{
    if (l_handle >= 0)
    {
        fdio_deregister(l_handle);
        l_handle = -1;
    }
    if (l_socket >= 0)
    {
        close(l_socket);
        l_socket = -1;
    }
}

static int RecordAMonitor(moninfo_t *mp, void *privptr)
{
  cxsd_gchnid_t **wp_p = privptr;

    **wp_p = mp->gcid;
    (*wp_p)++;

    return 0;
}
static int RequestSubscription(clientrec_t *cp, void *privptr __attribute__((unused)))
{
  cxsd_gchnid_t *wp;

    if (cp->periodics_used == 0) return 0;

    if (cp->periodics_needs_rebuild)
    {
        wp = cp->periodics;
        ForeachMonSlot(RecordAMonitor, &wp, cp);
        cp->periodics_needs_rebuild = 0;
    }

    CxsdHwDoIO(cp->ID, DRVA_READ,
               cp->periodics_used, cp->periodics,
               NULL, NULL, NULL);

    return 0;
}
static void starogate_begin_c(void)
{
    ForeachConnSlot(RequestSubscription, NULL);
}

static void starogate_end_c  (void)
{
}


static int  starogate_init_m(void);
static void starogate_term_m(void);

DEFINE_CXSD_FRONTEND(starogate, "StaroGate frontend (implementation of 'starogate' protocol for CXv4)",
                     starogate_init_m, starogate_term_m,
                     starogate_init_f, starogate_term_f,
                     starogate_begin_c, starogate_end_c);

static int  starogate_init_m(void)
{
    return CxsdRegisterFrontend(&(CXSD_FRONTEND_MODREC_NAME(starogate)));
}

static void starogate_term_m(void)
{
    CxsdDeregisterFrontend     (&(CXSD_FRONTEND_MODREC_NAME(starogate)));
}
