#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
/*!!!*/#include <netinet/tcp.h>

#include "cx_sysdeps.h"
#include "misclib.h"

#include "remdrv_proto_v4.h"
#define __REMCXSD_DRIVER_V4_C
#include "remcxsd.h"
#include "remcxsdP.h"


//////////////////////////////////////////////////////////////////////

void remcxsd_report_to_fd (int fd,    int code,            const char *format, ...)
{
  struct {remdrv_pkt_header_t hdr;  int32 data[1000];} pkt;
  int      len;
  va_list  ap;

    va_start(ap, format);
    len = vsprintf((char *)(pkt.data), format, ap);
    va_end(ap);

    bzero(&pkt.hdr, sizeof(pkt.hdr));
    pkt.hdr.pktsize = sizeof(pkt.hdr) + len + 1;
    pkt.hdr.command = code;

    write(fd, &pkt, pkt.hdr.pktsize);
}

static int vReportThat    (int devid, int code, int level, const char *format, va_list ap)
{
  remcxsd_dev_t  *dev = remcxsd_devices + devid;
  struct {remdrv_pkt_header_t hdr;  int32 data[1000];} pkt;
  int        len;

    len = vsprintf((char *)(pkt.data), format, ap);

    /*!!! Should support _ERRNO flag too! */
    
    bzero(&pkt.hdr, sizeof(pkt.hdr));
    pkt.hdr.pktsize = sizeof(pkt.hdr) + len + 1;
    pkt.hdr.command = code;
    pkt.hdr.var.debugp.level = level &~ DRIVERLOG_ERRNO;

    ////fprintf(stderr, " vRT: %s\n", (char *)pkt.data);
    return fdio_send(dev->fhandle, &pkt, pkt.hdr.pktsize);
}

int  remcxsd_report_to_dev(int devid, int code, int level, const char *format, ...)
{
  va_list    ap;
  int        r;

    va_start(ap, format);
    r = vReportThat(devid, code, level, format, ap);
    va_end(ap);

    return r;
}

//////////////////////////////////////////////////////////////////////

void DoDriverLog (int devid, int level, const char *format, ...)
{
  va_list        ap;
  
    va_start(ap, format);
    vDoDriverLog(devid, level, format, ap);
    va_end(ap);
}

void vDoDriverLog(int devid, int level, const char *format, va_list ap)
{
  int             category = level & DRIVERLOG_C_mask;
  remcxsd_dev_t  *dev = remcxsd_devices + devid;

  static int  last_good_devid = -1;

    /* Check the devid first... */
    /* a. NOT_IN_DRIVER or a layer */
    if (devid == DEVID_NOT_IN_DRIVER  ||
        (devid < 0  &&  -devid < remcxsd_numlyrs))
    {
        devid = last_good_devid;
        dev    = remcxsd_devices + devid;

        if (devid < 0  ||  devid >= remcxsd_maxdevs  ||
            dev->in_use == 0)
//            (dev->in_use == 0 && (fprintf(stderr, "[%d].in_use=0\n", devid), 1)))
        {
            vfprintf(stderr, format, ap);
            fprintf(stderr, "\n");
            return;
        }
    }
    /* b. some strange/unknown one */
    if (devid < 0  ||  devid >= remcxsd_maxdevs  ||
        dev->in_use == 0)
    {
        remcxsd_debug("%s: message from unknown devid=%d: \"%s\"",
                      __FUNCTION__, devid, format);
    }
    /* c. okay */
    else
    {
        last_good_devid = devid;
        
        /* Check current level/mask settings */
        if (category != DRIVERLOG_C_DEFAULT  &&
            !DRIVERLOG_m_C_ISIN_CHECKMASK(category, dev->logmask))
            return;
        if ((level & DRIVERLOG_LEVEL_mask) > dev->loglevel)
            return;
        
        vReportThat(devid, REMDRV_C_DEBUGP, level, format, ap);
    }
}

void ReturnDataSet    (int devid,
                       int count,
                       int *addrs, cxdtype_t *dtypes, int *nelems,
                       void **values, rflags_t *rflags, cx_time_t *timestamps)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  size_t               pktsize;
  remdrv_pkt_header_t  hdr;

  enum                {SEGLEN_MAX = 100};
  int                  seglen;
  int32                ad2snd[SEGLEN_MAX];
  int32                ne2snd[SEGLEN_MAX];
  int32                rf2snd[SEGLEN_MAX];
  uint32               ts2snd[SEGLEN_MAX*3];
  int                  stage;
  int                  x;
  size_t               valuestotal = 0;  // Only to prevent "may be used uninitialized"

  size_t               size;
  size_t               padsize;

  static uint8         zeroes[8]; // Up to 7 bytes of padding

    CHECK_SANITY_OF_DEVID();

    if (count == RETURNDATA_COUNT_PONG)
    {
        pktsize = sizeof(hdr);
        bzero(&hdr, sizeof(hdr));
        hdr.pktsize      = pktsize;
        hdr.command      = REMDRV_C_PONG_Y;
        if (fdio_send(dev->fhandle, &hdr, pktsize) < 0)
            goto TERMINATE;
        return;
    }

    while (count > 0)
    {
        seglen = count;
        if (seglen > SEGLEN_MAX)
            seglen = SEGLEN_MAX;

        if (fdio_lock_send  (dev->fhandle) < 0)
            goto TERMINATE;
        for (stage = 0;  stage <= 1;  stage++)
        {
            if (stage != 0)
            {
                size =
                    sizeof(hdr)
                    +
                    (sizeof(ad2snd[0]) + sizeof(ne2snd[0]) + sizeof(rf2snd[0]))
                    * seglen
                    +
                    (timestamps == NULL? 0 : sizeof(ts2snd[0]) * 3)
                    * seglen
                    +
                    seglen /* dtypes */;
                padsize = REMDRV_PROTO_SIZE_CEIL(size) - size;

                bzero(&hdr, sizeof(hdr));
                hdr.pktsize = size + padsize + valuestotal /* valuestotal is already padded */;
                hdr.command = REMDRV_C_DATA;
                hdr.var.group.first = timestamps == NULL? 0 : 1; /* "first" is used as boolean, signifying presence(=1)/absence(=0) of timestamps */
                hdr.var.group.count = seglen;

                if (fdio_send(dev->fhandle, &hdr,   sizeof(hdr))                < 0  ||
                    fdio_send(dev->fhandle, ad2snd, seglen * sizeof(ad2snd[0])) < 0  ||
                    fdio_send(dev->fhandle, ne2snd, seglen * sizeof(ne2snd[0])) < 0  ||
                    fdio_send(dev->fhandle, rf2snd, seglen * sizeof(rf2snd[0])) < 0)
                    goto TERMINATE;
                if (timestamps != NULL  &&
                     /* Note: we don't use zeroes[] but send an extra int32 from ts2snd[], since ts2snd[]'s size is multiple of 8 */
                    fdio_send(dev->fhandle, ts2snd, seglen * sizeof(ts2snd[0]) * 3) < 0)
                    goto TERMINATE;
                if (fdio_send(dev->fhandle, dtypes, seglen)  < 0)
                    goto TERMINATE;
                if (padsize    != 0     &&
                    fdio_send(dev->fhandle, zeroes, padsize) < 0)
                    goto TERMINATE;
            }

            for (x = 0, valuestotal = 0;  x < seglen;  x++)
            {
                ad2snd[x] = addrs [x];
                ne2snd[x] = nelems[x];
                rf2snd[x] = rflags[x];
                if (timestamps != NULL)
                {
                    ts2snd[x * 3 + 0] = timestamps[x].nsec;
                    ts2snd[x * 3 + 1] = timestamps[x].sec /*!!! lo32()!!! */;
                    ts2snd[x * 3 + 2] = 0; /*!!! hi32() !!! */
                }

                size    = sizeof_cxdtype(dtypes[x]) * nelems[x];
                padsize = REMDRV_PROTO_SIZE_CEIL(size) - size;
                valuestotal += size + padsize;

                if (stage != 0)
                {
                    if (size    != 0  &&
                        fdio_send(dev->fhandle, values[x], size)    < 0)
                        goto TERMINATE;
                    if (padsize != 0  &&
                        fdio_send(dev->fhandle, zeroes,    padsize) < 0)
                        goto TERMINATE;
                        
                }
            }
        }
        if (fdio_unlock_send(dev->fhandle) < 0)
            goto TERMINATE;

        addrs  += seglen;
        dtypes += seglen;
        nelems += seglen;
        values += seglen;
        rflags += seglen;
        if (timestamps != NULL)
            timestamps += seglen;
        count -= seglen;
    }

    return;

 TERMINATE:
    FreeDevID(devid);
}

void ReturnInt32Datum (int devid, int chan, int32 v, rflags_t rflags)
{
  static cxdtype_t  dt_int32    = CXDTYPE_INT32;
  static int        nels_1      = 1;

  void             *vp          = &v;

    ReturnDataSet(devid, 1,
                  &chan, &dt_int32, &nels_1,
                  &vp, &rflags, NULL);
}

void ReturnInt32DatumTimed(int devid, int chan, int32 v,
                           rflags_t rflags, cx_time_t timestamp)
{
  static cxdtype_t  dt_int32    = CXDTYPE_INT32;
  static int        nels_1      = 1;

  void             *vp          = &v;

    ReturnDataSet(devid, 1,
                  &chan, &dt_int32, &nels_1,
                  &vp, &rflags, &timestamp);
}

void SetChanRDs       (int devid,
                       int first, int count,
                       double phys_r, double phys_d)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  struct
  {
      remdrv_pkt_header_t            hdr;
      remdrv_data_set_rds_t          data;
  } pkt;

    CHECK_SANITY_OF_DEVID();

    bzero(&pkt, sizeof(pkt));
    pkt.hdr.pktsize = sizeof(pkt);
    pkt.hdr.command = REMDRV_C_RDS;
    pkt.hdr.var.group.first = first;
    pkt.hdr.var.group.count = count;
    pkt.data.phys_r         = phys_r;
    pkt.data.phys_d         = phys_d;
    if (fdio_send(dev->fhandle, &pkt, sizeof(pkt)) < 0)
        FreeDevID(devid);
}

void SetChanFreshAge  (int devid,
                       int first, int count,
                       cx_time_t fresh_age)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  struct
  {
      remdrv_pkt_header_t            hdr;
      remdrv_data_set_fresh_age_t    data;
  } pkt;

    CHECK_SANITY_OF_DEVID();

    bzero(&pkt, sizeof(pkt));
    pkt.hdr.pktsize = sizeof(pkt);
    pkt.hdr.command = REMDRV_C_FRHAGE;
    pkt.hdr.var.group.first = first;
    pkt.hdr.var.group.count = count;
    pkt.data.age_nsec       = fresh_age.nsec;
    pkt.data.age_sec_lo32   = fresh_age.sec; /*!!! lo32()!!! */
    pkt.data.age_sec_hi32   = 0; /*!!! hi32() !!! */
    if (fdio_send(dev->fhandle, &pkt, sizeof(pkt)) < 0)
        FreeDevID(devid);
}

void SetChanQuant     (int devid,
                       int first, int count,
                       CxAnyVal_t q, cxdtype_t q_dtype)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  struct
  {
      remdrv_pkt_header_t            hdr;
      remdrv_data_set_quant_t        data;
  } pkt;
  size_t               q_size = sizeof_cxdtype(q_dtype);

    CHECK_SANITY_OF_DEVID();
    CHECK_LOG_CORRECT(q_size > sizeof(pkt.data.q_data),
                      return,
                      "(%d,%d) q_dtype=%d, sizeof_cxdtype()=%zd, >sizeof(q_data)=%zd",
                      first, count,
                      (int)q_dtype, q_size, sizeof(pkt.data.q_data));

    bzero(&pkt, sizeof(pkt));
    pkt.hdr.pktsize = sizeof(pkt);
    pkt.hdr.command = REMDRV_C_QUANT;
    pkt.hdr.var.group.first = first;
    pkt.hdr.var.group.count = count;
    pkt.data.q_dtype        = q_dtype;
    memcpy(pkt.data.q_data,  &q, q_size);
    if (fdio_send(dev->fhandle, &pkt, sizeof(pkt)) < 0)
        FreeDevID(devid);
}

void SetChanRange     (int devid,
                       int first, int count,
                       CxAnyVal_t minv, CxAnyVal_t maxv, cxdtype_t range_dtype)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  struct
  {
      remdrv_pkt_header_t            hdr;
      remdrv_data_set_range_t        data;
  } pkt;
  size_t               r_size = sizeof_cxdtype(range_dtype);

    CHECK_SANITY_OF_DEVID();
    CHECK_LOG_CORRECT(r_size > sizeof(pkt.data.range_min),
                      return,
                      "(%d,%d) range_dtype=%d, sizeof_cxdtype()=%zd, >sizeof(range_min)=%zd",
                      first, count,
                      (int)range_dtype, r_size, sizeof(pkt.data.range_min));

    bzero(&pkt, sizeof(pkt));
    pkt.hdr.pktsize = sizeof(pkt);
    pkt.hdr.command = REMDRV_C_RANGE;
    pkt.hdr.var.group.first = first;
    pkt.hdr.var.group.count = count;
    pkt.data.range_dtype    = range_dtype;
    memcpy(pkt.data.range_min, &minv, r_size);
    memcpy(pkt.data.range_max, &maxv, r_size);
    if (fdio_send(dev->fhandle, &pkt, sizeof(pkt)) < 0)
        FreeDevID(devid);
}

void SetChanReturnType(int devid,
                       int first, int count,
                       int return_type)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  struct
  {
      remdrv_pkt_header_t            hdr;
      remdrv_data_set_return_type_t  data;
  } pkt;

    CHECK_SANITY_OF_DEVID();

    bzero(&pkt, sizeof(pkt));
    pkt.hdr.pktsize = sizeof(pkt);
    pkt.hdr.command = REMDRV_C_RTTYPE;
    pkt.hdr.var.group.first = first;
    pkt.hdr.var.group.count = count;
    pkt.data.return_type    = return_type;
    if (fdio_send(dev->fhandle, &pkt, sizeof(pkt)) < 0)
        FreeDevID(devid);
}

void  SetDevState     (int devid, int state,
                       rflags_t rflags_to_set, const char *description)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  remdrv_pkt_header_t  pkt;
  int                  nagle_off = 1;
  size_t               dlen;

    CHECK_SANITY_OF_DEVID();

    /* Turn off Nagle algorithm, for packet to be sent immediately,
       instead of being discarded upon subsequent close(). */
    if (state == DEVSTATE_OFFLINE)
        setsockopt(dev->s, SOL_TCP, TCP_NODELAY, &nagle_off, sizeof(nagle_off));

    if (description != NULL  &&  description[0] != '\0')
        dlen = strlen(description) + 1;
    else
        dlen = 0;

    bzero(&pkt, sizeof(pkt));
    pkt.pktsize = sizeof(pkt) + dlen;
    pkt.command = REMDRV_C_CHSTAT;
    pkt.var.chstat.state         = state;
    pkt.var.chstat.rflags_to_set = rflags_to_set;
    if ( fdio_send(dev->fhandle, &pkt, sizeof(pkt)) < 0  ||
        (dlen != 0  &&
         fdio_send(dev->fhandle, description, dlen) < 0))
        FreeDevID(devid);

    if (state == DEVSTATE_OFFLINE) FreeDevID(devid);
}

void  ReRequestDevData(int devid)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;
  remdrv_pkt_header_t  pkt;
  int                  r;

    CHECK_SANITY_OF_DEVID();
  
    bzero(&pkt, sizeof(pkt));
    pkt.pktsize = sizeof(pkt);
    pkt.command = REMDRV_C_REREQD;
    r = fdio_send(dev->fhandle, &pkt, pkt.pktsize);
    if (r < 0) FreeDevID(devid);
}

void       * GetLayerVMT   (int devid)
{
  remcxsd_dev_t       *dev = remcxsd_devices + devid;

    CHECK_SANITY_OF_DEVID(NULL);

    if (dev->lyrid == 0)
    {
        DoDriverLog(devid, DRIVERLOG_ERR,
                "%s(devid=%d/active=%d): request for layer-VMT from non-layered device",
                __FUNCTION__, devid, active_devid);
        return NULL;
    }

    return remcxsd_layers[-dev->lyrid]->vmtlink;
}

//////////////////////////////////////////////////////////////////////

void ProcessPacket(int devid, void *devptr,
                   fdio_handle_t handle, int reason,
                   void *inpkt,  size_t inpktsize,
                   void *privptr)
{
  remcxsd_dev_t       *dev   = remcxsd_devices + devid;
  remdrv_pkt_header_t *hdr   = (remdrv_pkt_header_t *)inpkt;
  remdrv_pkt_header_t  crzbuf;
  int                  s_devid;
  int                  x;
  const char          *auxinfo;
  int                  state;

  char                 bus_id_str[countof(dev->businfo) * (12+2)];
  char                *bis_p;

  int                  count;
  size_t               values_ofs;
  int32               *nelems_base;
  cxdtype_t           *dtypes_base;
  uint8               *values_ptr;
  int                  ofs;
  enum                {SEGLEN_MAX = 100};
  int                  seglen;
  int                  addrs2io [SEGLEN_MAX];
  int                  nelems2wr[SEGLEN_MAX];
  void                *values2wr[SEGLEN_MAX];

    ////fprintf(stderr, "%s(%d): %s, p=%p size=%d, code=%08x\n", __FUNCTION__, handle, fdio_strreason(reason), inpkt, inpktsize, reason==FDIO_R_DATA?hdr->command:0);
  
    switch (reason)
    {
        case FDIO_R_DATA:
            break;

        case FDIO_R_INPKT2BIG:
            remcxsd_report_to_dev(devid, REMDRV_C_RRUNDP, 0,
                                  "fdiolib: received packet is too big");
            FreeDevID(devid);
            return;

        case FDIO_R_ENOMEM:
            remcxsd_report_to_dev(devid, REMDRV_C_RRUNDP, 0,
                                  "fdiolib: out of memory");
            FreeDevID(devid);
            return;
        
        case FDIO_R_CLOSED:
        case FDIO_R_IOERR:
        default:
            remcxsd_debug("%s(handle=%d, dev=%d): %s, errno=%d",
                          __FUNCTION__, handle, devid, fdio_strreason(reason), fdio_lasterr(handle));
            FreeDevID(devid);
            return;
    }

    if (hdr == NULL)
    {
        /*!!! ???*/

        return;
    }

    ////fprintf(stderr, "command=%08x\n", hdev->command);
    switch (hdr->command)
    {
        case REMDRV_C_CONFIG:
            ////DoDriverLog(devid, 0, "[%d] %s() inpktsize=%zd", devid, __FUNCTION__, inpktsize);
            if(0){
                int  z;
                fprintf(stderr, "[%d] dump ", devid);
                for (z = 0;  z < inpktsize;  z++)
                    fprintf(stderr, "%s%02x", z==0?"":" ", ((uint8*)inpkt)[z]);
                fprintf(stderr, "\n");
            }
            /* Check version compatibility */
            if (!CX_VERSION_IS_COMPATIBLE(hdr->var.config.proto_version,
                                          REMDRV_PROTO_VERSION))
            {
                bzero(&crzbuf, sizeof(crzbuf));
                crzbuf.pktsize = sizeof(remdrv_pkt_header_t);
                crzbuf.command = REMDRV_C_CRAZY;
                n_write(fdio_fd_of(handle), &crzbuf, crzbuf.pktsize);

                FreeDevID(devid);
                return;
            }

            if (dev->inited)
            {
                DoDriverLog(devid, 0, "CONFIG command to an already configured driver");
                return;
            }

            dev->businfocount = hdr->var.config.businfocount;
            if (dev->businfocount > countof(dev->businfo))
                dev->businfocount = countof(dev->businfo);
            for (x = 0;  x < dev->businfocount; x++)
                dev->businfo[x] = hdr->data[x];

            /* Note: PACKET'S provided businfocount is used in calcs */
            auxinfo = hdr->pktsize > sizeof(remdrv_pkt_header_t) + sizeof(int32) * hdr->var.config.businfocount?
                      (char *)(hdr->data) + sizeof(int32) * hdr->var.config.businfocount : NULL;
            ////DoDriverLog(devid, 0, "inpkt=%p auxinfo=%p", inpkt, auxinfo);

            if (dev->businfocount < dev->metric->min_businfo_n ||
                dev->businfocount > dev->metric->max_businfo_n)
            {
                DoDriverLog(devid, 0, "businfocount=%d, out_of[%d,%d]",
                            dev->businfocount,
                            dev->metric->min_businfo_n, dev->metric->max_businfo_n);
                SetDevState(devid, DEVSTATE_OFFLINE, CXRF_CFG_PROBL, "businfocount out of range");
                return;
            }

            /* Allocate privrec */
            if (dev->metric->privrecsize != 0)
            {
                if (devptr != NULL) // Static allocation
                    dev->devptr = devptr;
                else
                {
                    dev->devptr = malloc(dev->metric->privrecsize);
                    if (dev->devptr == NULL)
                    {
                        remcxsd_report_to_dev(devid, REMDRV_C_RRUNDP, 0,
                                              "out of memory when allocating privrec");
                        FreeDevID(devid);
                        return;
                    }
                }
                bzero(dev->devptr, dev->metric->privrecsize);
                if (dev->metric->paramtable != NULL)
                {
                    if (psp_parse(auxinfo, NULL,
                                  dev->devptr,
                                  '=', " \t", "",
                                  dev->metric->paramtable) != PSP_R_OK)
                    {
                        DoDriverLog(devid, 0, "psp_parse(auxinfo)@remsrv: %s (auxinfo=\"%s\")", psp_error(), auxinfo);
                        SetDevState(devid, DEVSTATE_OFFLINE, CXRF_CFG_PROBL, "auxinfo parsing error");
                        return;
                    }
                }
            }

            /* Initialize driver */
            {
                if (dev->businfocount == 0)
                    sprintf(bus_id_str, "-");
                else
                    for (bis_p = bus_id_str, x = 0;  x < dev->businfocount;  x++)
                        bis_p += sprintf(bis_p, "%s%d", x > 0? "," : "", dev->businfo[x]);

                remcxsd_debug("[%d] CONFIG: pktsize=%d, businfo=%s, auxinfo=\"%s\"",
                              devid, hdr->pktsize, bus_id_str, auxinfo);
                //fprintf(stderr, "(");
                //for (x = 0; x < hdr->pktsize - sizeof(remdrv_pkt_header_t); x++)
                //    fprintf(stderr, "%s%02X", x == 0? "": " ", ((char *)(hdr->data))[x]);
                //fprintf(stderr, ")\n");
            }
            state = DEVSTATE_OPERATING;
            ENTER_DRIVER_S(devid, s_devid);
            if (dev->metric->init_dev != NULL)
                state = (dev->metric->init_dev)
                    (devid, dev->devptr,
                     dev->businfocount, dev->businfo,
                     auxinfo);
            dev->inited = 1;
            LEAVE_DRIVER_S(s_devid);

            if (state < 0)
            {
                SetDevState(devid, DEVSTATE_OFFLINE,
                            state == -1? 0 : -state, "init_dev failure");
            }
            else
            {
                SetDevState(devid, state, 0, NULL);
            }

            break;

        case REMDRV_C_READ:
            count = hdr->var.group.count;
            if (count <= 0  ||  count > 10000 /* Arbitrary limit */)
            {
                /* Display a warning (where?)? */
                return;
            }
            // !!! Check inpktsize
            if (dev->metric->do_rw == NULL) return;

            for (ofs = 0;
                 count > 0  &&  dev->in_use;
                 count -= seglen, ofs += seglen)
            {
                seglen = count;
                if (seglen > SEGLEN_MAX)
                    seglen = SEGLEN_MAX;
                for (x = 0;  x < seglen;  x++)
                    addrs2io [x] = hdr->data  [ofs + x];

                ENTER_DRIVER_S(devid, s_devid);
                dev->metric->do_rw(devid, dev->devptr,
                                   DRVA_READ,
                                   seglen, addrs2io,
                                   NULL, NULL, NULL);
                LEAVE_DRIVER_S(s_devid);
            }
                
            break;

        case REMDRV_C_WRITE:
            count = hdr->var.group.count;
            if (count <= 0  ||  count > 10000 /* Arbitrary limit */)
            {
                /* Display a warning (where?)? */
                return;
            }
            // !!! Check inpktsize
            if (dev->metric->do_rw == NULL) return;

            /* ADDRS(int32),NELEMS(int32),DTYPES(uint8),[padding-to-8],values */
            values_ofs  = REMDRV_PROTO_SIZE_CEIL((count * 2 /*ADDRS,NELEMS*/)
                                                 * sizeof(int32)
                                                 +
                                                 count /*DTYPES*/);
            nelems_base =           hdr->data + count;
            dtypes_base =   (void*)(hdr->data + count * 2);
            values_ptr  = ((uint8*)(hdr->data)) + values_ofs;
            for (ofs = 0;
                 count > 0  &&  dev->in_use;
                 count -= seglen, ofs += seglen)
            {
                seglen = count;
                if (seglen > SEGLEN_MAX)
                    seglen = SEGLEN_MAX;
                for (x = 0;  x < seglen;  x++)
                {
                    addrs2io [x] = hdr->data  [ofs + x];
                    nelems2wr[x] = nelems_base[ofs + x];
                    values2wr[x] = values_ptr;
                    values_ptr += REMDRV_PROTO_SIZE_CEIL(sizeof_cxdtype(dtypes_base[ofs+x]) *
                                                         nelems2wr[x]);
                    /*!!! Should check if values_ptr>(inpkt+pktsize) */
                }

                ENTER_DRIVER_S(devid, s_devid);
                dev->metric->do_rw(devid, dev->devptr,
                                   DRVA_WRITE,
                                   seglen, addrs2io,
                                   dtypes_base + ofs, nelems2wr, values2wr);
                LEAVE_DRIVER_S(s_devid);
            }
                
            break;

        case REMDRV_C_SETDBG:
            dev->loglevel = hdr->var.setdbg.verblevel;
            dev->logmask  = hdr->var.setdbg.mask;
            break;

        case REMDRV_C_PING_R:
            hdr->command = REMDRV_C_PONG_Y;
            if (fdio_send(handle, inpkt, inpktsize) < 0) FreeDevID(devid);
            break;

        default:
            DoDriverLog(devid, 0, "%s: unknown command 0x%x",
                        __FUNCTION__, hdr->command);
            {
              int32 *p;
                for (x = 0, p = inpkt;  x < 8;  x++, p++)
                    fprintf(stderr, "%08x ", *p);
                fprintf(stderr, "\n");
            }
    }
}
