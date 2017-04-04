#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/param.h> // For PATH_MAX, at least on ARM@MOXA

#include "misclib.h"

#include "cxsd_driver.h"
#include "piv485_lyr.h"

#ifndef SERIALHAL_FILE_H
  #error The "SERIALHAL_FILE_H" macro is undefined
#else
  #include SERIALHAL_FILE_H
#endif

#ifndef SERIALLYR_NAME
  #error The "SERIALLYR_NAME" macro is undefined
#endif /* SERIALLYR_NAME */


enum {PIV485_MAXPKTBYTES = 15};

enum
{
    PIV485_START = 0xAA,
    PIV485_STOP  = 0xAB,
    PIV485_SHIFT = 0xAC
};

enum {PIV485_PKTBUFSIZE = 1 + (PIV485_MAXPKTBYTES + 1) * 2 + 1};

enum
{
    PIV485_DEFAULT_TIMEOUT =  1 *  100 * 1000, // 100 ms
};

typedef struct
{
    sq_eprops_t       props;

    Piv485OnSendProc  on_send;
    void             *privptr;

    int               dlc;  /* 'dlc' MUST be signed (instead of size_t) in order to permit partial-comparison (triggered by negative dlc) */
    uint8             data[PIV485_MAXPKTBYTES];
} pivqelem_t;

enum
{
    NUMLINES    = 8,
    DEVSPERLINE = 256,
};

typedef struct
{
    /* Driver properties */
    int              line;    // For back-referencing when passed just dp
    int              kid;     // The same
    int              devid;
    void            *devptr;
    Piv485PktinProc  inproc;
    Piv485SndMdProc  mdproc;

    /* Life state */
    int              last_cmd;

    /* Support */
    sq_q_t           q;
    sl_tid_t         tid;
} pivdevinfo_t;

typedef struct
{
    int           fd;
    sl_fdh_t      fdhandle;
    sl_tid_t      tid;
    sq_port_t     port;

    int           last_kid;

    uint8         inbuf[PIV485_PKTBUFSIZE];
    size_t        inbufused;

    pivdevinfo_t  devs[DEVSPERLINE];
} lineinfo_t;


static int         my_lyrid;

static lineinfo_t  lines[NUMLINES];

static inline int  encode_handle(int line, int kid)
{
    return (line << 16)  |  kid;
}

static inline void decode_handle(int handle, int *line_p, int *kid_p)
{
    *line_p = handle >> 16;
    *kid_p  = handle & 255;
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

//--------------------------------------------------------------------

static void errreport(void *opaqueptr,
                      const char *format, ...)
{
  int            devid = ptr2lint(opaqueptr);

  va_list        ap;
  
    va_start(ap, format);
    vDoDriverLog(devid, 0 | DRIVERLOG_ERRNO, format, ap);
    va_end(ap);
}

static int  piv485_sender     (void *elem, void *privptr)
{
  pivdevinfo_t *dp = privptr;
  pivqelem_t   *qe = elem;
  lineinfo_t   *lp = lines + dp->line;

  uint8         databuf[PIV485_PKTBUFSIZE];
  int           len;
  uint8         csum;
  int           x;
  uint8         c;

  int           r;

  char          bufimage[PIV485_PKTBUFSIZE*3];
  char         *bip;

    /* 0. Do we suffer from yet-unopened serial-device? */
    if (lp->fd < 0) return -1;

    /**/
    if (dp->mdproc != NULL)
        dp->mdproc(dp->devid, dp->devptr, &(qe->dlc), qe->data);

    /**/
    bip = bufimage;
    *bip = '\0';
    for (x = 0;  x < qe->dlc;  x++)
        bip += sprintf(bip, "%s%02x",
                       x == 0? "":",", qe->data[x]);

    /* I. Prepare data in "PIV485"-encoded representation */
    len  = 0;
    csum = 0;

    /* 1. The START byte */
    databuf[len++] = PIV485_START;
    /* 2. The address */
    csum ^= (databuf[len++] = dp->kid);
    /* 3. Packet data, including trailing checksum */
    for (x = 0;  x <= qe->dlc /* '==' - csum */;  x++)
    {
        if (x < qe->dlc)
        {
            c = qe->data[x];
            csum ^= c;
        }
        else
            c = csum;

        if (c >= PIV485_START  &&  c <= PIV485_SHIFT)
        {
            databuf[len++] = PIV485_SHIFT;
            databuf[len++] = c - PIV485_START;
        }
        else
            databuf[len++] = c;
    }
    /* 4. The STOP byte */
    databuf[len++] = PIV485_STOP;

    /**/
    bip += sprintf(bip, " -> ");
    for (x = 0;  x < len;  x++)
    {
        bip += sprintf(bip, "%s%02x", x == 0? "" : ",", databuf[x]);
    }
    DoDriverLog(dp->devid, 0 | DRIVERLOG_C_PKTDUMP, "SEND %d:%s",
                dp->kid, bufimage);

    /* II. Send data to port altogether */
    r = n_write(lp->fd, databuf, len);

    /* III. Could we send the whole packet? */
    if (r == len)
    {
        lp->last_kid = dp->kid;
        dp->last_cmd = qe->data[0];
//if (1 && dp->last_cmd == 1) fprintf(stderr, "%s   sender: kid=%d %s\n", strcurtime_msc(), lp->last_kid, bufimage);
//fprintf(stderr, "%s sender: kid=%d %s\n", strcurtime_msc(), lp->last_kid, bufimage);
        return 0;
    }

    /* Unfortunately, no */
    /*!!! Re-open?*/
    lp->last_kid = -1;
    dp->last_cmd = -1;

    return -1;
}

static int  piv485_eq_cmp_func(void *elem, void *ptr)
{
  pivqelem_t *a = elem;
  pivqelem_t *b = ptr;
  
    if (b->dlc >= 0)
        return
            a->dlc  ==  b->dlc   &&
            (a->dlc ==  0  ||
             memcmp(a->data, b->data, a->dlc) == 0);
    else
        return
            a->dlc  >= -b->dlc   &&
             memcmp(a->data, b->data, -b->dlc) == 0;
}


static void port_tout_proc(int devid, void *devptr,
                           sl_tid_t tid,
                           void *privptr)
{
  int           line = ptr2lint(privptr);
  lineinfo_t   *lp   = lines + line;

    lp->tid = -1;
    sq_port_tout(&(lp->port));
}

static int  port_tim_enqueuer(void *privptr, int usecs)
{
  int           line = ptr2lint(privptr);
  lineinfo_t   *lp   = lines + line;

    if (lp->tid >= 0)
    {
        sl_deq_tout(lp->tid);
        lp->tid = -1;
    }
    lp->tid = sl_enq_tout_after(my_lyrid, NULL, usecs, port_tout_proc, lint2ptr(line));
    return lp->tid >= 0? 0 : -1;
}

static void port_tim_dequeuer(void *privptr)
{
  int           line = ptr2lint(privptr);
  lineinfo_t   *lp   = lines + line;

    if (lp->tid >= 0)
    {
        sl_deq_tout(lp->tid);
        lp->tid = -1;
    }
}

static void piv485_tout_proc(int devid, void *devptr,
                             sl_tid_t tid,
                             void *privptr)
{
  int           handle = ptr2lint(privptr);
  int           line;
  int           kid;
  pivdevinfo_t *dp;

    DECODE_AND_CHECK();

    dp->tid = -1;
    sq_timeout(&(dp->q));
}

static int  tim_enqueuer(void *privptr, int usecs)
{
  pivdevinfo_t *dp = privptr;

    if (dp->tid >= 0)
    {
        sl_deq_tout(dp->tid);
        dp->tid = -1;
    }
    dp->tid = sl_enq_tout_after(dp->devid, dp->devptr, usecs, piv485_tout_proc,
                                lint2ptr(encode_handle(dp->line, dp->kid)));
    return dp->tid >= 0? 0 : -1;
}

static void tim_dequeuer(void *privptr)
{
  pivdevinfo_t *dp = privptr;

    if (dp->tid >= 0)
    {
        sl_deq_tout(dp->tid);
        dp->tid = -1;
    }
}

static void piv485_send_callback(void        *q_privptr,
                                 sq_eprops_t *e,
                                 sq_try_n_t   try_n)
{
  pivdevinfo_t *dp = q_privptr;
  pivqelem_t   *qe = (pivqelem_t *)e;

    qe->on_send(dp->devid, dp->devptr, try_n, qe->privptr);
}

//--------------------------------------------------------------------

static int piv485_init_lyr(int lyrid)
{
  int  l;
  int  d;

    DoDriverLog(my_lyrid, 0, "%s(%d)!", __FUNCTION__, lyrid);
    my_lyrid = lyrid;
    bzero(lines, sizeof(lines));
    for (l = 0;  l < countof(lines);  l++)
    {
        lines[l].fd  = -1;
        lines[l].tid = -1;
        sq_port_init(&(lines[l].port),
                     lint2ptr(l), port_tim_enqueuer, port_tim_dequeuer);

        for (d = 0;  d < countof(lines[l].devs);  d++)
            lines[l].devs[d].devid = DEVID_NOT_IN_DRIVER;
    }

    return 0;
}

static void  piv485_term_lyr  (void)
{
    /*!!!*/
}

static void piv485_disconnect(int devid)
{
  int           l;
  int           d;
  pivdevinfo_t *dp;

    for (l = 0;  l < countof(lines);  l++)
        for (d = 0;  d < countof(lines[l].devs);  d++)
            if (lines[l].devs[d].devid == devid)
            {
                dp = lines[l].devs + d;

                /* Release slot */
                sq_fini(&(dp->q));
                bzero(dp, sizeof(*dp));
                dp->devid = DEVID_NOT_IN_DRIVER;

                /* Check if it is the current line's "owner" */
                if (lines[l].last_kid == d)
                    lines[l].last_kid = -1;

                /* Check if some other devices are active on this line */
                for (d = 0;  d < countof(lines[l].devs);  d++)
                    if (lines[l].devs[d].devid >= 0)
                        return;

                /* ...or was last "client" of this line? */
                /* Then release the line! */
                sl_del_fd(lines[l].fdhandle);
                lines[l].fdhandle = -1;
                close(lines[l].fd);
                lines[l].fd = -1;
                /* Note: we do NOT dequeue the port timeout, for it to remain active in case of subsequent port re-use */
                //if (lines[l].tid >= 0) sl_deq_tout(lines[l].tid);
                //lines[l].tid = -1;
                return;
            }

    DoDriverLog(devid, DRIVERLOG_WARNING,
                "%s: request to disconnect unknown devid=%d",
                __FUNCTION__, devid);
}

static void piv485_fd_p(int devid, void *devptr,
                        sl_fdh_t fdhandle, int fd, int mask,
                        void *privptr);

static int  piv485_add(int devid, void *devptr,
                       int businfocount, int businfo[],
                       Piv485PktinProc inproc,
                       Piv485SndMdProc mdproc,
                       int queue_size)
{
  int           line;
  int           kid;

  lineinfo_t   *lp;
  pivdevinfo_t *dp;
  int           handle;
  int           r;

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
                    __FUNCTION__, line, 0, NUMLINES);
        return -CXRF_CFG_PROBL;
    }
    if (kid < 0  ||  kid >= DEVSPERLINE)
    {
        DoDriverLog(devid, 0, "%s: kid=%d, out_of[%d..%d]",
                    __FUNCTION__, line, 0, DEVSPERLINE);
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

    dp->line     = line;
    dp->kid      = kid;
    dp->devid    = devid;
    dp->devptr   = devptr;
    dp->inproc   = inproc;
    dp->mdproc   = mdproc;
    dp->last_cmd = -1;

    r = sq_init(&(dp->q), &(lp->port),
                queue_size, sizeof(pivqelem_t),
                PIV485_DEFAULT_TIMEOUT, dp,
                piv485_sender,
                piv485_eq_cmp_func,
                tim_enqueuer,
                tim_dequeuer);
    if (r < 0)
    {
        DoDriverLog(devid, DRIVERLOG_ERR | DRIVERLOG_ERRNO,
                    "%s: sq_init()", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }
    dp->tid = -1;

    if (lp->fd < 0)
    {
        /*!!! Obtain layer-info and psp-parse it */

        lp->fd = serialhal_opendev(line, B9600,
                                   errreport, lint2ptr(devid));
        if (lp->fd < 0)
        {
            DoDriverLog(devid, DRIVERLOG_ERR | DRIVERLOG_ERRNO,
                        "%s: serialhal_opendev(%d)", __FUNCTION__, line);
            sq_fini(&(dp->q));
            return -CXRF_DRV_PROBL;
        }
        lp->tid = -1;

        lp->fdhandle = sl_add_fd(my_lyrid, NULL, lp->fd, SL_RD, piv485_fd_p, lint2ptr(line));
        lp->last_kid = -1;
    }

    return handle;
}

static void DoReset(lineinfo_t *lp)
{
  pivdevinfo_t *dp;

    if (lp->last_kid >= 0) lp->devs[lp->last_kid].last_cmd = -1;

    lp->last_kid  = -1;
    lp->inbufused = 0;
}

static sq_status_t piv485_q_erase_and_send_next  (int handle,
                                                  int dlc, uint8 *data);

static void piv485_fd_p(int unused_devid, void *unused_devptr,
                        sl_fdh_t fdhandle, int fd, int mask,
                        void *privptr)
{
  int           line = ptr2lint(privptr);
  lineinfo_t   *lp   = lines + line;
  pivdevinfo_t *dp;
  int           devid;

  int           r;

  uint8         data[PIV485_PKTBUFSIZE];
  int           x;
  int           dlc;
  uint8         csum;
  uint8         cmd;

  char          bufimage[PIV485_PKTBUFSIZE*3];
  char         *bip;

    /*!!! The device-address is in the packet, 0th byte!!! */
    if (lp->last_kid >= 0)
    {
        dp    = lines[line].devs + lp->last_kid;
        devid = dp->devid;
    }
    else
    {
        dp    = NULL;
        devid = my_lyrid;
    }

    /* Read 1 byte */
    r = uintr_read(fd,
                   lp->inbuf + lp->inbufused,
                   1);
    if (r <= 0) /*!!! Should we REALLY close/re-open?  */
    {
        DoDriverLog(unused_devid, 0 | DRIVERLOG_ERRNO, "%s() r=%d errno=%d", __FUNCTION__, r, errno);
        /*!!! Should also (schedule?) re-open */
        sl_del_fd(lp->fdhandle); lp->fdhandle = -1;
        close(fd);
        lp->fd = -1;
        return;
    }

    /* Isn't it a STOP byte? */
    if (lp->inbuf[lp->inbufused] != PIV485_STOP)
    {
        lp->inbufused++;
        /* Is there still space in input buffer? */
        if (lp->inbufused < sizeof(lp->inbuf)) return;

        DoDriverLog(my_lyrid, DRIVERLOG_ALERT,
                    "%s: input buffer overflow; resetting",
                    __FUNCTION__);
        DoReset(lp);

        return;
    }

    /* Okay, the whole packet has arrived */
    /* Let's decode and check it */
    for (x = 0, dlc = 0, csum = 0;
         x < lp->inbufused;
         x++,   dlc++)
    {
        if (lp->inbuf[x] == PIV485_SHIFT)
        {
            x++;
            if (x == lp->inbufused)
            {
                DoDriverLog(devid, DRIVERLOG_WARNING,
                            "%s: malformed packet (SHIFT,STOP); last_kid=%d, devid=%d",
                            __FUNCTION__, lp->last_kid, devid);
                DoReset(lp);
                return;
            }
            data[dlc] = lp->inbuf[x] + PIV485_START;
        }
        else
            data[dlc] = lp->inbuf[x];
    }

    if (csum != 0)
    {
        DoDriverLog(devid, DRIVERLOG_WARNING,
                    "%s: malformed packet (csum!=0); last_kid=%d, devid=%d",
                    __FUNCTION__, lp->last_kid, devid);
        DoReset(lp);
        return;
    }

    /* Select target */
    dp    = lines[line].devs + data[0];
    devid = dp->devid;
    if (devid < 0)
    {
        dp    = NULL;
        devid = my_lyrid;
    }

    /* Log */
    bip = bufimage;
    *bip = '\0';
    for (x = 1;  x < dlc - 1;  x++)
        bip += sprintf(bip, "%s%02x",
                       x == 1? "":",", data[x]);
    bip += sprintf(bip, " <- ");
    for (x = 0;  x < lp->inbufused + 1;  x++)
    {
        bip += sprintf(bip, "%s%02x", x == 0? "" : ",", lp->inbuf[x]);
    }
    DoDriverLog(devid, 0 | DRIVERLOG_C_PKTDUMP, "RECV %d:%02x:%s",
                data[0], dp != NULL? dp->last_cmd : -1, bufimage);

//if (1  &&  dp != NULL  &&  dp->last_cmd == 1) fprintf(stderr, "%s GET_INFO: last_kid=%d;%d %s\n", strcurtime_msc(), lp->last_kid, data[0], bufimage);
//fprintf(stderr, "%s   fd_p: last_kid=%d;%d %s\n", strcurtime_msc(), lp->last_kid, data[0], bufimage);

    /* Reset input buffer to readiness */
    lp->last_kid  = -1;
    lp->inbufused = 0;

    if (dp != NULL)
    {
        cmd = dp->last_cmd;
        dp->last_cmd  = -1;

        /* (send next, if present) */
        piv485_q_erase_and_send_next(encode_handle(dp->line, dp->kid), -1, &cmd);
        /* Finally, dispatch the packet */
        if (dp->inproc != NULL)
            dp->inproc(dp->devid, dp->devptr,
                       cmd, dlc - 2, data + 1);
    }
}


static void        piv485_q_clear    (int handle)
{
  int           line;
  int           kid;
  pivdevinfo_t *dp;
  lineinfo_t   *lp;

    DECODE_AND_CHECK();
    sq_clear(&(dp->q));
    /*!!! any other actions -- like clearing "last-sent-cmd"? And/or "schedule next"? */
    lp = lines + line;
    if (lp->last_kid == kid)
        lp->last_kid = -1;
}

static sq_status_t piv485_q_enqueue  (int handle, sq_enqcond_t how,
                                      int tries, int timeout_us,
                                      Piv485OnSendProc on_send, void *privptr,
                                      int model_dlc, int dlc, uint8 *data)
{
  int           line;
  int           kid;
  pivdevinfo_t *dp;
  pivqelem_t    item;
  pivqelem_t    model;

    DECODE_AND_CHECK(SQ_ERROR);

    if (dlc < 1  ||  dlc > PIV485_MAXPKTBYTES) return SQ_ERROR;
    if (model_dlc > PIV485_MAXPKTBYTES  ||
        model_dlc > dlc)                       return SQ_ERROR;

    /* Prepare item... */
    bzero(&item, sizeof(item));

    item.props.tries      = tries;
    item.props.timeout_us = timeout_us;
    item.props.callback   = (on_send == NULL)? NULL : piv485_send_callback;

    item.on_send = on_send;
    item.privptr = privptr;
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
static sq_status_t piv485_q_enqueue_v(int handle, sq_enqcond_t how,
                                      int tries, int timeout_us,
                                      Piv485OnSendProc on_send, void *privptr,
                                      int model_dlc, int dlc, ...)
{
  va_list  ap;
  uint8    data[PIV485_MAXPKTBYTES];
  int      x;

    if (dlc < 1  ||  dlc > PIV485_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return piv485_q_enqueue(handle, how,
                            tries, timeout_us,
                            on_send, privptr,
                            model_dlc, dlc, data);
}
static sq_status_t piv485_q_enq      (int handle, sq_enqcond_t how,
                                      int dlc, uint8 *data)
{
    return piv485_q_enqueue(handle, how,
                            SQ_TRIES_INF, 0,
                            NULL, NULL,
                            0, dlc, data);
}
static sq_status_t piv485_q_enq_v    (int handle, sq_enqcond_t how,
                                      int dlc, ...)
{
  va_list  ap;
  uint8    data[PIV485_MAXPKTBYTES];
  int      x;

    if (dlc < 1  ||  dlc > PIV485_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return piv485_q_enqueue(handle, how,
                            SQ_TRIES_INF, 0,
                            NULL, NULL,
                            0, dlc, data);
}

static sq_status_t piv485_q_erase_and_send_next  (int handle,
                                                  int dlc, uint8 *data)
{
  int           line;
  int           kid;
  pivdevinfo_t *dp;
  pivqelem_t    item;
  int           abs_dlc = abs(dlc);
  void         *fe;

    DECODE_AND_CHECK(SQ_ERROR);

    if (abs_dlc < 1  ||  abs_dlc > PIV485_MAXPKTBYTES) return SQ_ERROR;

    /* Make a model for comparison */
    item.dlc  = dlc;
    memcpy(item.data, data, abs_dlc);

    /* Check "whether we should really erase and send next packet" */
    fe = sq_access_e(&(dp->q), 0);
    /*!!! should be "if (fe == NULL  || ...)"*/
    if (fe != NULL  &&  piv485_eq_cmp_func(fe, &item) == 0)
        return SQ_NOTFOUND;

    /* Erase... */
    sq_foreach(&(dp->q), SQ_ELEM_IS_EQUAL, &item, SQ_ERASE_FIRST,  NULL);
    /* ...and send next */
    sq_sendnext(&(dp->q));

    return SQ_FOUND;
}
static sq_status_t piv485_q_erase_and_send_next_v(int handle,
                                                  int dlc, ...)
{
  va_list  ap;
  int      abs_dlc = abs(dlc);
  uint8    data[PIV485_MAXPKTBYTES];
  int      x;
  
    if (abs_dlc < 1  ||  abs_dlc > PIV485_MAXPKTBYTES) return SQ_ERROR;
    va_start(ap, dlc);
    for (x = 0;  x < abs_dlc;  x++) data[x] = va_arg(ap, int);
    va_end(ap);
    return piv485_q_erase_and_send_next(handle, dlc, data);
}

typedef struct
{
    Piv485PktCmpFunc  do_cmp;
    void             *privptr;
} cmp_rec_t;
static int piv485_foreach_cmp_func(void *elem, void *ptr)
{
  pivqelem_t *a  = elem;
  cmp_rec_t  *rp = ptr;

    return rp->do_cmp(a->dlc, a->data, rp->privptr);
}
static sq_status_t piv485_q_foreach(int handle,
                                    Piv485PktCmpFunc do_cmp, void *privptr,
                                    sq_action_t action)
{
  int           line;
  int           kid;
  pivdevinfo_t *dp;
  cmp_rec_t     rec;

    DECODE_AND_CHECK(SQ_ERROR);
    if (do_cmp == NULL) return SQ_ERROR;

    rec.do_cmp  = do_cmp;
    rec.privptr = privptr;
    return sq_foreach(&(dp->q),
                      piv485_foreach_cmp_func, &rec, action, NULL);
}

/*##################################################################*/

static piv485_vmt_t piv485_vmt =
{
    piv485_add,

    piv485_q_clear,
    piv485_q_enqueue,
    piv485_q_enqueue_v,
    piv485_q_enq,
    piv485_q_enq_v,
    piv485_q_erase_and_send_next,
    piv485_q_erase_and_send_next_v,
    piv485_q_foreach,
};


DEFINE_CXSD_LAYER(SERIALLYR_NAME, "PIV485 implementation via '" __CX_STRINGIZE(SERIALHAL_DESCR) "' HAL",
                  NULL, NULL,
                  PIV485_LYR_NAME, PIV485_LYR_VERSION,
                  piv485_init_lyr, piv485_term_lyr,
                  piv485_disconnect,
                  &piv485_vmt);
