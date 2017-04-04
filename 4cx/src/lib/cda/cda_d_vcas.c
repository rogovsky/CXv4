#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include "fix_arpa_inet.h"

#include "misc_macros.h"
#include "misclib.h"
#include "memcasecmp.h"
#include "cxscheduler.h"
#include "fdiolib.h"

#include "cda.h"
#include "cdaP.h"

#include "cda_d_vcas.h"


//////////////////////////////////////////////////////////////////////

enum { VCAS_DEFAULT_PORT = 20041 };

//////////////////////////////////////////////////////////////////////

enum {E_NOHOST = -1};
enum { EMULATED_BASECYCLESIZE = 1000000}; // 1s

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;

    cda_dataref_t  dataref; // "Backreference" to corr.entry in the global table
    cxdtype_t      dtype;
    int            nelems;

    char          *name;
} hwrinfo_t;

#if 0 /*!!! What was it?*/
typedef struct
{
    cda_hwcnref_t  hwr;
    const char    *name;
} hwrname_t;
#endif

typedef struct
{
    cda_srvconn_t  sid;
    int            fd;
    fdio_handle_t  iohandle;
    int            state;
    int            is_suffering;
    int            was_data_somewhen;
    sl_tid_t       rcn_tid;
    sl_tid_t       cyc_tid;

    hwrinfo_t     *hwrs_list;
    int            hwrs_list_allocd;
} cda_d_vcas_privrec_t;

enum
{
    HWR_MIN_VAL   = 1,
    HWR_MAX_COUNT = 1000000,  // An arbitrary limit
    HWR_ALLOC_INC = 100,
};


//--------------------------------------------------------------------

// GetHwrSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Hwr, hwrinfo_t,
                                 hwrs, in_use, 0, 1,
                                 HWR_MIN_VAL, HWR_ALLOC_INC, HWR_MAX_COUNT,
                                 me->, me,
                                 cda_d_vcas_privrec_t *me, cda_d_vcas_privrec_t *me)

static void RlsHwrSlot(cda_hwcnref_t hwr, cda_d_vcas_privrec_t *me)
{
  hwrinfo_t *hi = AccessHwrSlot(hwr, me);

    if (hi->in_use == 0) return; /*!!! In fact, an internal error */
    hi->in_use = 0;

    safe_free(hi->name);
}

//////////////////////////////////////////////////////////////////////

static int split_srvrspec(const char *spec,
                          char       *hostbuf, size_t hostbufsize,
                          int        *port_p)
{
  const char *colonp;
  char       *endptr;
  size_t      len;

    /* Treat missing spec as localhost */
    if (spec == NULL  ||  *spec == '\0') spec = "localhost";

    colonp = strchr(spec, ':');
    if (colonp != NULL)
    {
        if (colonp[1] == '\0') // An empty port -- just a ':' for separation?
            *port_p = VCAS_DEFAULT_PORT;
        else
        {
            *port_p = (int)(strtol(colonp + 1, &endptr, 10));
            if (endptr == colonp + 1  ||  *endptr != '\0')
            {
                cda_set_err("invalid server specification \"%s\"", spec);
                errno = EINVAL;
                return -1;
            }
        }
        len = colonp - spec;
    }
    else
    {
        *port_p = VCAS_DEFAULT_PORT;
        len = strlen(spec);
    }

    if (len > hostbufsize - 1) len = hostbufsize - 1;
    memcpy(hostbuf, spec, len);
    hostbuf[len] = '\0';

    if (hostbuf[0] == '\0') /* Local ":N" spec? */
        strzcpy(hostbuf, "localhost", hostbufsize);

    return 0;
}

static int initiate_connect(cda_d_vcas_privrec_t *me,
                            int         uniq,     void *privptr1,
                            fdio_ntfr_t notifier, void *privptr2)
{
  char                host[256];
  int                 srv_port;

  struct hostent     *hp;
  in_addr_t           spechost;

  struct sockaddr_in  idst;
  struct sockaddr    *serv_addr;
  int                 addrlen;
  int                 r;
  int                 on    = 1;
  size_t              bsize = 2048; // An arbitrary value

    /* I. Preparations: resolve name */
    if (split_srvrspec(cda_dat_p_srvrn_of_sid(me->sid),
                       host, sizeof(host),
                       &srv_port) < 0) return -1;

    /* Note:
     All the code from this point and up to actual connect()
     could be moved to asynchronous processing if there was a way to perform
     resolving asynchronously
     */

    /* Find out IP of the specified host */
    /* First, is it in dot notation (aaa.bbb.ccc.ddd)? */
    spechost = inet_addr(host);
    /* No, should do a hostname lookup */
    if (spechost == INADDR_NONE)
    {
        hp = gethostbyname(host);
        /* No such host?! */
        if (hp == NULL)
        {
            errno = E_NOHOST;
            return -1;
        }

        memcpy(&spechost, hp->h_addr, hp->h_length);
    }

    /* Set addrlen::servaddr to host:port */
    idst.sin_family = AF_INET;
    idst.sin_port   = htons(srv_port);
    memcpy(&idst.sin_addr, &spechost, sizeof(spechost));
    serv_addr = (struct sockaddr *) &idst;
    addrlen   = sizeof(idst);

    /* II. Create socket */
    me->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (me->fd < 0)
        return -1;
    /* ...and tune it */
    /* For TCP, turn on keepalives */
    r = setsockopt(me->fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    /* Set Close-on-exec:=1 */
    fcntl(me->fd, F_SETFD, 1);
    /* Set buffers */
    setsockopt(me->fd, SOL_SOCKET, SO_SNDBUF, (char *) &bsize, sizeof(bsize));
    setsockopt(me->fd, SOL_SOCKET, SO_RCVBUF, (char *) &bsize, sizeof(bsize));

    /* III. Initiate connect() */
    set_fd_flags(me->fd, O_NONBLOCK, 1);
    r = connect(me->fd, serv_addr, addrlen);
    if (r < 0  &&  errno != EINPROGRESS)
    {
        close(me->fd); me->fd = -1;
        return -1;
    }

    /* Register with fdiolib */
    me->iohandle = fdio_register_fd(uniq,     privptr1,
                                    me->fd,   FDIO_STRING_CONN,
                                    notifier, privptr2,
                                    1024,
                                    0,
                                    0,
                                    0,
                                    FDIO_LEN_LITTLE_ENDIAN,
                                    1, 0);
    if (me->iohandle < 0)
    {
        close(me->fd); me->fd = -1;
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void FailureProc(cda_d_vcas_privrec_t *me, int reason);

static int SubscribeToOneChannel(hwrinfo_t *hi, void *privptr)
{
  cda_d_vcas_privrec_t *me = privptr;

  static const char *prefix = "name:";
  static const char *suffix = "|method:subscr\n";

    if (fdio_lock_send  (me->iohandle) < 0                              ||
        fdio_send       (me->iohandle, prefix,   strlen(prefix))   < 0  ||
        fdio_send       (me->iohandle, hi->name, strlen(hi->name)) < 0  ||
        fdio_send       (me->iohandle, suffix,   strlen(suffix))   < 0  ||
        fdio_unlock_send(me->iohandle) < 0)
        return 1;
    else
        return 0;
}

static int determine_name_type(const char *name,
                               char srvrspec[], size_t srvrspec_size,
                               char **channame_p)
{
  int         srv_len;
  const char *colon_p;
  const char *p;
  char       *vp;

    /* Defaults */
    srv_len = 0;
    *channame_p = name;

    /* Doesn't start with alphanumeric (or just ":...")? */
    if (*name != ':'  &&  !isalnum(*name)) goto DO_RET;

    /* Is there a colon at all? */
    colon_p = strchr(name, ':');
    if (colon_p == NULL) goto DO_RET;

    /* Check than EVERYTHING preceding ':' can constitute a hostname */
    for (p = name;  p < colon_p;  p++)
        if (*p == '\0'  ||
            (!isalnum(*p)  &&  *p != '.')  &&  *p != '-') goto DO_RET;
    /* Okay, skip ':' */
    p++;
    /* ':' should be followed by a digit */
    if (*p == '\0'  ||  !isdigit(*p)) goto DO_RET;
    /* Okay, skip digits... */
    while (*p != '\0'  &&  isdigit(*p)) p++;
    //
    if (*p != '.') goto DO_RET;

    ////////////////////
    srv_len = p - name;
    *channame_p = p + 1;

 DO_RET:
    if (srv_len > 0)
    {
        if (srv_len > srvrspec_size - 1)
            srv_len = srvrspec_size - 1;
        memcpy(srvrspec, name, srv_len);
    }
    srvrspec[srv_len] = '\0';
    for (vp = srvrspec;  *vp != '\0';  vp++) *vp = tolower(*vp);

    return srv_len != 0;
}

static int  cda_d_vcas_new_chan(cda_dataref_t ref, const char *name,
                                int options,
                                cxdtype_t dtype, int nelems)
{
  cda_d_vcas_privrec_t *me;

  char                  srvrspec[200];
  const char           *channame;

  cda_hwcnref_t         hwr;
  hwrinfo_t            *hi;

  char                 *p;

fprintf(stderr, "CCC %s(%s)\n", __FUNCTION__, name);

    if (!determine_name_type(name, srvrspec, sizeof(srvrspec), &channame))
    {
        errno = EINVAL;
        return CDA_DAT_P_ERROR;
    }

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(vcas), srvrspec);
    if (me == NULL) return CDA_DAT_P_ERROR;

    /* Alloc and fill a slot */
    hwr = GetHwrSlot(me);
    if (hwr < 0) return CDA_DAT_P_ERROR;
    hi = AccessHwrSlot(hwr, me);
    // Name
    if ((hi->name = strdup(channame)) == NULL)
    {
        RlsHwrSlot(hwr, me);
        return CDA_DAT_P_ERROR;
    }
    for (p = hi->name; *p != '\0';  p++)
        if (*p == '.') *p = '/';
    // Other data
    hi->dataref = ref;
    hi->dtype   = dtype;
    hi->nelems  = nelems;
    cda_dat_p_set_hwr  (ref, hwr);
    cda_dat_p_set_ready(ref, 1);

    /* Subscribe to channel if connection is already established */
    if (me->state == CDA_DAT_P_OPERATING)
        if (SubscribeToOneChannel(hi, me) != 0)
            FailureProc(me, FDIO_R_IOERR);

    return CDA_DAT_P_NOTREADY;
}

static int  cda_d_vcas_snd_data(void *pdt_privptr, cda_hwcnref_t hwr,
                                cxdtype_t dtype, int nelems, void *value)
{
  cda_d_vcas_privrec_t *me = pdt_privptr;
  hwrinfo_t            *hi = AccessHwrSlot(hwr, me);
  char                  buf[400];
  void                 *data;
  size_t                datalen;

  static const char *s1 = "name:";
  static const char *s2 = "||method:set|val:";
  static const char *s3 = "\n";

fprintf(stderr, "%s(hwr=%d, dtype=%d nelems=%d)\n", __FUNCTION__, hwr, dtype, nelems);
    /* A safety net */
    if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        cda_dat_p_report(-1,
                         "%s(me=%p): attempt to write to illegal hwr=%d (hwrs_list_allocd=%d)\n",
                         __FUNCTION__, me, hwr, me->hwrs_list_allocd);
        return CDA_PROCESS_SEVERE_ERR;
    }

    if (nelems != 1  &&  dtype != CXDTYPE_TEXT) return CDA_PROCESS_SEVERE_ERR;

    data    = NULL;
    datalen = 0; /* That's just to make gcc happy */
    switch (dtype)
    {
        case CXDTYPE_INT32:  sprintf(buf, "%d", *((  int32*)value)); break;
        case CXDTYPE_UINT32: sprintf(buf, "%u", *(( uint32*)value)); break;
        case CXDTYPE_INT16:  sprintf(buf, "%d", *((  int16*)value)); break;
        case CXDTYPE_UINT16: sprintf(buf, "%u", *(( uint16*)value)); break;
        case CXDTYPE_DOUBLE: sprintf(buf, "%f", *((float64*)value)); break;
        case CXDTYPE_SINGLE: sprintf(buf, "%f", *((float32*)value)); break;
        case CXDTYPE_INT8:   sprintf(buf, "%d", *((  int8 *)value)); break;
        case CXDTYPE_UINT8:  sprintf(buf, "%u", *(( uint8 *)value)); break;
        case CXDTYPE_TEXT:   data = value; datalen = nelems;         break;
        default: return CDA_PROCESS_SEVERE_ERR;
    }
    if (data == NULL)
    {
        data    = buf;
        datalen = strlen(data);
    }

    if (fdio_lock_send  (me->iohandle) < 0                              ||
        fdio_send       (me->iohandle, s1,       strlen(s1))       < 0  ||
        fdio_send       (me->iohandle, hi->name, strlen(hi->name)) < 0  ||
        fdio_send       (me->iohandle, s2,       strlen(s2))       < 0  ||
        fdio_send       (me->iohandle, data,     datalen)          < 0  ||
        fdio_send       (me->iohandle, s3,       strlen(s3))       < 0  ||
        fdio_unlock_send(me->iohandle) < 0)
    {
        FailureProc(me, FDIO_R_IOERR);
        return CDA_PROCESS_ERR;
    }

    return CDA_PROCESS_DONE;
}

//////////////////////////////////////////////////////////////////////

static int DefunctOneRef(hwrinfo_t *hi, void *privptr)
{
  cda_d_vcas_privrec_t *me = privptr;

    cda_dat_p_defunct_dataset(me->sid, 1, &(hi->dataref));
    return 0;
}
static void MarkAllAsDefunct(cda_d_vcas_privrec_t *me)
{
if(0)fprintf(stderr, "%s: %s\n", strcurtime(), __FUNCTION__);
    if (me->was_data_somewhen)
    {
        ForeachHwrSlot(DefunctOneRef, me, me);
        cda_dat_p_update_server_cycle(me->sid);
    }
}

//////////////////////////////////////////////////////////////////////

static void ReconnectProc(int uniq, void *unsdptr,
                          sl_tid_t tid, void *privptr);

typedef struct
{
    const char *p;
    int         len;
} nameinfo_t;

static int CompareName(hwrinfo_t *hi, void *privptr)
{
  nameinfo_t *info_p = privptr;

    return cx_strmemcmp(hi->name, info_p->p, info_p->len) == 0;
}
static int isdigit_str(const char *s, int len)
{
    for (; len > 0; s++, len--)
        if (!isdigit(*s)) return 0;
    return 1;
}
static int digits2int (const char *s, int len)
{
  int  ret;

    for (ret = 0;  len > 0;  s++, len--)
        ret = (ret * 10) + (*s - '0');
    return ret;
}
static void ProcessInData(cda_d_vcas_privrec_t *me,
                          void *inpkt, size_t inpktsize)
{
  char          *p;
  char          *colon_p;
  char          *val_p;
  char          *pipe_p;
  int            keylen;
  int            vallen;

  char          *namstr_p;
  char          *valstr_p;
  char          *timstr_p;
  char          *dscstr_p;
  char          *unistr_p;
  int            namstr_len;
  int            valstr_len;
  /*int          timstr_len -- not needed*/
  int            dscstr_len;
  int            unistr_len;

  nameinfo_t     info;
  cda_hwcnref_t  hwr;
  hwrinfo_t     *hi;

  void          *value_p;
  int            nelems;
  rflags_t       rflags;
  cx_time_t      timestamp;
  cx_time_t     *timestamp_p;
  struct tm      brk_time;

  int8         i8;
  uint8        u8;
  int16        i16;
  uint16       u16;
  int32        i32;
  uint32       u32;
  int64        i64;
  uint64       u64;
  float        f32;
  double       f64;

  char        *endptr;

  static const char *not_found = " not found";

fprintf(stderr, "<%s>\n", (char*)inpkt);
    for (p = inpkt;  *p != '\0'  &&  isspace(*p);  p++);
    if (*p == '\0') return;

    for (namstr_p = valstr_p = timstr_p = dscstr_p = unistr_p = NULL;
         *p != '\0';
        )
    {
        colon_p = strchr(p, ':');
        if (colon_p == NULL)
        {
            /* A syntax error */
            return;
        }
        keylen = colon_p - p;

        val_p = colon_p + 1;

        pipe_p = strchr(val_p, '|');
        if (pipe_p == NULL) pipe_p = val_p + strlen(val_p); // ==inpkt+inpktsize
        vallen = pipe_p - val_p;

        if      (cx_strmemcasecmp("name", p, keylen) == 0  ||
                 cx_strmemcasecmp("n",    p, keylen) == 0)
        {
            namstr_p   = val_p;
            namstr_len = vallen;
        }
        else if (cx_strmemcasecmp("val",   p, keylen) == 0  ||
                 cx_strmemcasecmp("value", p, keylen) == 0  ||
                 cx_strmemcasecmp("v",     p, keylen) == 0)
        {
            valstr_p   = val_p;
            valstr_len = vallen;
        }
        else if (cx_strmemcasecmp("time", p, keylen) == 0)
        {
            if (
                vallen >= strlen("02.04.2015 18_05_06.731")  &&
                isdigit_str(val_p +  0, 2)  &&  // Day
                isdigit_str(val_p +  3, 2)  &&  // Month
                isdigit_str(val_p +  6, 4)  &&  // Year
                isdigit_str(val_p + 11, 2)  &&  // Hour
                isdigit_str(val_p + 14, 2)  &&  // Minutes
                isdigit_str(val_p + 17, 2)  &&  // Seconds
                isdigit_str(val_p + 20, 3)      // Milliseconds
               )
            {
                timstr_p   = val_p;
            }
        }
        else if (cx_strmemcasecmp("descr", p, keylen) == 0)
        {
            dscstr_p   = val_p;
            dscstr_len = vallen;
        }
        else if (cx_strmemcasecmp("units", p, keylen) == 0)
        {
            unistr_p   = val_p;
            unistr_len = vallen;
        }

        p = pipe_p;
        if (*p == '|') p++;
    }

    /* Okay, what had we got? */

    /* Missing "name" or "val" -- no use */
    if (namstr_p == NULL  ||  valstr_p == NULL) return;

    /* Find hwr */
    info.p   = namstr_p;
    info.len = namstr_len;
    hwr = ForeachHwrSlot(CompareName, &info, me);
    if (hwr < 0) return;
    hi = AccessHwrSlot(hwr, me);

    /* Get timestamp */
    if (timstr_p != NULL)
    {
        bzero(&brk_time, sizeof(brk_time));
        brk_time.tm_mday = digits2int(timstr_p +  0, 2);
        brk_time.tm_mon  = digits2int(timstr_p +  3, 2) - 1     /* number of months since January, in the range 0 to 11*/;
        brk_time.tm_year = digits2int(timstr_p +  6, 4) - 1900; /* number of years since 1900 */
        brk_time.tm_hour = digits2int(timstr_p + 11, 2);
        brk_time.tm_min  = digits2int(timstr_p + 14, 2);
        brk_time.tm_sec  = digits2int(timstr_p + 17, 2);
        timestamp.sec    = mktime(&brk_time);
        timestamp.nsec   = digits2int(timstr_p + 20, 3) * 1000000;
        timestamp_p = &timestamp;
        //fprintf(stderr, "TS %ld %d.%d.%d %d:%d:%d\n", timestamp.sec, brk_time.tm_mday, brk_time.tm_mon, brk_time.tm_year, brk_time.tm_hour, brk_time.tm_min, brk_time.tm_sec);
    }
    else
        timestamp_p = NULL;

    /**/
    if      (cx_strmemcasecmp("error", valstr_p, valstr_len) == 0)
    {
//            fprintf(stderr, "CH(%s) NOTFOUND\n", hi->name);
        /* Some error -- probably "not found" */
        if (dscstr_p != NULL  &&  dscstr_len >= strlen(not_found)  &&
            memcmp(not_found,
                   dscstr_p + dscstr_len - strlen(not_found),
                   strlen(not_found)) == 0)
        {
            fprintf(stderr, "CH(%s) NOTFOUND\n", hi->name);
            cda_dat_p_set_notfound(hi->dataref);
            return;
        }

        return;
    }
    else if (cx_strmemcasecmp("none",  valstr_p, valstr_len) == 0)
    {
        /* Nothing to do if value never read */
        return;
    }
    else
    {
        /* Okay -- try to decode, depending on dtype */
        nelems = 1;
        switch (hi->dtype)
        {
            case CXDTYPE_INT32:
                i32 = strtol(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &i32;
                break;

            case CXDTYPE_UINT32:
                u32 = strtoul(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &u32;
                break;

            case CXDTYPE_INT16:
                u16 = strtol(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &i16;
                break;

            case CXDTYPE_UINT16:
                u16 = strtoul(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &u16;
                break;

            case CXDTYPE_DOUBLE:
                f64 = strtod(valstr_p, &endptr);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &f64;
                break;

            case CXDTYPE_SINGLE:
                f32 = strtof(valstr_p, &endptr);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &f32;
                break;

#if 1
            case CXDTYPE_INT64:
                i64 = strtol(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &i64;
                break;

            case CXDTYPE_UINT64:
                u64 = strtoull(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &u64;
                break;
#endif

            case CXDTYPE_INT8:
                i8  = strtol(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &i8;
                break;

            case CXDTYPE_UINT8:
                u8  = strtoul(valstr_p, &endptr, 0);
                if (endptr != (valstr_p + valstr_len)) goto BAD_VAL_SYNTAX;
                value_p = &u8;
                break;

            case CXDTYPE_TEXT:
                value_p = valstr_p;
                nelems  = valstr_len;
                break;

            default: return; /* Unsupported type */
        }

        if (dscstr_p != NULL  ||  unistr_p != NULL)
        {
            /* Yes, we modify inpkt buffer, but that's OK (as of 10.04.2015) */
            if (dscstr_p != NULL) dscstr_p[dscstr_len] = '\0';
            if (unistr_p != NULL) unistr_p[unistr_len] = '\0';
            cda_dat_p_set_strings(hi->dataref,
                                  NULL/*ident*/,
                                  NULL/*label*/,
                                  NULL/*tip*/,
                                  dscstr_p/*comment*/,
                                  NULL/*geoinfo*/,
                                  NULL/*rsrvd6*/,
                                  unistr_p/*units*/,
                                  NULL/*dpyfmt*/);
        }

        cda_dat_p_update_dataset(me->sid, 1, &(hi->dataref),
                                 &value_p, &(hi->dtype), &nelems,
                                 &rflags, timestamp_p, 1);
    }

    return;

 BAD_VAL_SYNTAX:
    return;
}

static void SuccessProc(cda_d_vcas_privrec_t *me)
{
    if (me->is_suffering)
    {
        cda_dat_p_report(me->sid, "connected.");
        me->is_suffering = 0;
    }

    if (ForeachHwrSlot(SubscribeToOneChannel, me, me) >= 0)
    {
        FailureProc(me, FDIO_R_IOERR);
        return;
    }

    sl_deq_tout(me->rcn_tid); me->rcn_tid = -1;

    cda_dat_p_set_server_state(me->sid, me->state = CDA_DAT_P_OPERATING);
}

static void FailureProc(cda_d_vcas_privrec_t *me, int reason)
{
  int                 ec = errno;
  int                 us;
  enum {MAX_US_X10 = 2*1000*1000*1000};

    us = 1*1000*1000;
    if (ec == E_NOHOST)
    {
        if (us > MAX_US_X10 / 10)
            us = MAX_US_X10;
        else
            us *= 10;
    }

    if (!me->is_suffering)
    {
        cda_dat_p_report(me->sid, "%s: %s; will reconnect.",
                         fdio_strreason(reason),
                         ec == E_NOHOST? "Unknown host" : strerror(ec));
        me->is_suffering = 1;
    }

    /* Notify cda */
    cda_dat_p_set_server_state(me->sid, me->state = CDA_DAT_P_NOTREADY);

    /* Forget old connection */
    if (me->iohandle >= 0) fdio_deregister(me->iohandle);
    me->iohandle = -1;
    if (me->fd       >= 0) close(me->fd);
    me->fd       = -1;

    MarkAllAsDefunct(me);

    /* And organize a timeout in a second... */
    if (me->rcn_tid >= 0) sl_deq_tout(me->rcn_tid);
    me->rcn_tid = sl_enq_tout_after(cda_dat_p_suniq_of_sid(me->sid), NULL, /*!!!uniq*/
                                    us, ReconnectProc, me);
}

static void ProcessFdioEvent(int uniq, void *unsdptr,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr)
{
  cda_d_vcas_privrec_t *me = privptr;

    switch (reason)
    {
        case FDIO_R_DATA:
            ProcessInData(me, inpkt, inpktsize);
            break;

        case FDIO_R_CONNECTED:
            SuccessProc(me);
            break;

        case FDIO_R_CONNERR:
        case FDIO_R_CLOSED:
        case FDIO_R_IOERR:
            FailureProc(me, reason);
            break;

            /*!!!
             Note: following errors look "fatal",
             but for now we just try to reconnect.
             */
        case FDIO_R_PROTOERR:
        case FDIO_R_INPKT2BIG:
        case FDIO_R_ENOMEM:
            FailureProc(me, reason);
            break;
    }
}

static void ReconnectProc(int uniq, void *unsdptr,
                          sl_tid_t tid, void *privptr)
{
  cda_d_vcas_privrec_t *me = privptr;

    me->rcn_tid = -1;

    if (initiate_connect(me,
                         cda_dat_p_suniq_of_sid(me->sid), NULL,
                         ProcessFdioEvent,                me) < 0)
        FailureProc(me, FDIO_R_CONNERR);
}

static void CycleProc(int uniq, void *unsdptr,
                      sl_tid_t tid, void *privptr)
{
  cda_d_vcas_privrec_t *me = privptr;

    me->cyc_tid = -1;

    if (me->state == CDA_DAT_P_OPERATING)
        cda_dat_p_update_server_cycle(me->sid);

    me->cyc_tid = sl_enq_tout_after(cda_dat_p_suniq_of_sid(me->sid), NULL,
                                    EMULATED_BASECYCLESIZE,
                                    CycleProc, me);
}

static int  cda_d_vcas_new_srv (cda_srvconn_t  sid, void *pdt_privptr,
                                int            uniq,
                                const char    *srvrspec,
                                const char    *argv0)
{
  cda_d_vcas_privrec_t *me = pdt_privptr;
  int                   r;
  int                   ec;

fprintf(stderr, "ZZZ %s(%s)\n", __FUNCTION__, srvrspec);
    me->sid     = sid;
    /* Just precaution, for case if cda_core would someday call del_srv() on undercreated srvs */
    me->fd       = -1;
    me->iohandle = -1;
    me->state   = CDA_DAT_P_NOTREADY;
    me->rcn_tid = -1;
    me->cyc_tid = -1;

    r = initiate_connect(me,
                         cda_dat_p_suniq_of_sid(me->sid), NULL,
                         ProcessFdioEvent,                me);
    if (r < 0)
    {
        if (!IS_A_TEMPORARY_CONNECT_ERROR()  &&  errno != 0)
        {
            ec = errno;
            cda_set_err("initiate_connect()/\"%s\": %s",
                        cda_dat_p_srvrn_of_sid(me->sid), strerror(ec));
            return CDA_DAT_P_ERROR;
        }
        
        FailureProc(me, FDIO_R_CONNERR);
    }

    me->cyc_tid = sl_enq_tout_after(cda_dat_p_suniq_of_sid(me->sid), NULL,
                                    EMULATED_BASECYCLESIZE,
                                    CycleProc, me);

    return me->state;
}

static void cda_d_vcas_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_vcas_privrec_t *me = pdt_privptr;
  
    if (me->fd >= 0)
        close(me->fd);
    sl_deq_tout(me->rcn_tid); me->rcn_tid = -1;
    sl_deq_tout(me->cyc_tid); me->cyc_tid = -1;
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_DAT_PLUGIN(vcas, "VCAS data-access plugin",
                      NULL, NULL,
                      sizeof(cda_d_vcas_privrec_t),
                      '.', ':', '@',
                      cda_d_vcas_new_chan, NULL,
                      cda_d_vcas_snd_data, NULL,
                      cda_d_vcas_new_srv,  cda_d_vcas_del_srv);
