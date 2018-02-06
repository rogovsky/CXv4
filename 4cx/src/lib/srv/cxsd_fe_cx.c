#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <netinet/in.h>
#include "fix_arpa_inet.h"

#include "misclib.h"
#include "cxscheduler.h"
#include "fdiolib.h"

#include "cxsd_hw.h"
#include "cxsd_hwP.h"
#include "cxsd_frontend.h"
#include "cxsd_logger.h"

#include "cx_proto_v4.h"
#include "endianconv.h"


#define REQ_ALL_MONITORS 1


enum
{
    MAX_UNCONNECTED_SECONDS = 60,
};

enum
{
    GURU_TAKEOVER_HEARTBEAT_SECONDS = 10,
};


static  int            my_server_instance;

static  int            inet_serv_socket = -1;    /* Socket to listen AF_INET */
static  fdio_handle_t  inet_serv_handle = -1;

static  int            unix_serv_socket = -1;    /* Socket to listen AF_UNIX */
static  fdio_handle_t  unix_serv_handle = -1;

static  int            inet_guru_socket = -1;    /* Socket to listen AF_INET */
static  fdio_handle_t  inet_guru_handle = -1;

static  int            unix_guru_socket = -1;    /* Socket to listen AF_UNIX */
static  fdio_handle_t  unix_guru_handle = -1;

static  int            unix_info_socket = -1;    /* Socket to connect to "guru" */
static  fdio_handle_t  unix_info_handle = -1;

static  sl_tid_t       guru_takeover_hbt_tid = -1;


//////////////////////////////////////////////////////////////////////

enum {RDS_MAX_COUNT = 20};

//////////////////////////////////////////////////////////////////////

enum
{
    CS_OPEN_FRESH = 0,  // Just allocated
    CS_OPEN_ENDIANID,   // Waiting for CXT4_ENDIANID
    CS_OPEN_LOGIN,      // Waiting for CXT4_LOGIN

    CS_USABLE,          // The connection is full-operational if >=
    CS_READY,           // Is ready for I/O
};

enum
{
    MONITOR_EVMASK =
        CXSD_HW_CHAN_EVMASK_UPDATE   |
        CXSD_HW_CHAN_EVMASK_STATCHG  |
        CXSD_HW_CHAN_EVMASK_STRSCHG  |
        CXSD_HW_CHAN_EVMASK_RDSCHG   |
        CXSD_HW_CHAN_EVMASK_FRESHCHG |
        CXSD_HW_CHAN_EVMASK_QUANTCHG
};
typedef struct
{
    int            in_use;
    uint32         Seq;
    cxsd_gchnid_t  cpid;
    cxsd_gchnid_t  gcid;
    int32          param1;
    int32          param2;
    int            cond;
    union
    {
        int zzz;
    } info;

    int            modified;
    int            being_reqd; // Counting semaphore
} moninfo_t;

typedef struct
{
    int              in_use;
    int              state;

    int              fd;
    fdio_handle_t    fhandle;
    sl_tid_t         clp_tid;

    uint32           ID;
    time_t           when;
    uint32           ip;
    struct sockaddr  addr;

    int              endianness;
    char             progname[40];
    char             username[40];

    /* Data buffers */
    CxV4Header      *replybuf;
    size_t           replybufsize;

    moninfo_t       *monitors_list;
    int              monitors_list_allocd;

    cxsd_gchnid_t   *per_cycle_monitors;
    int              per_cycle_monitors_allocd;
    int              per_cycle_monitors_count;
    int              per_cycle_monitors_needs_rebuild;
    int              per_cycle_monitors_some_modified;
} v4clnt_t;


typedef struct
{
    int              in_use;

    int              fd;
    fdio_handle_t    fhandle;

    int              instance;
} advisor_t;

//////////////////////////////////////////////////////////////////////

enum
{
    V4_MAX_CONNS = 1000,  // An arbitrary limit
    V4_ALLOC_INC = 2,     // Must be >1 (to provide growth from 0 to 2)
};

enum
{
    ADVISORS_MAX_CONNS = 1000,  // An arbitrary limit
    ADVISORS_ALLOC_INC = 2,     //
};

static v4clnt_t  *cx4clnts_list        = NULL;
static int        cx4clnts_list_allocd = 0;

static advisor_t *advisors_list        = NULL;
static int        advisors_list_allocd = 0;

static CxsdDb     advisors_dbs[CX_MAX_SERVER+1] = {[0 ... CX_MAX_SERVER]=NULL};

static inline int cp2cd(v4clnt_t *cp)
{
    return cp - cx4clnts_list;
}

//////////////////////////////////////////////////////////////////////

static void MonEvproc(int            uniq,
                      void          *privptr1,
                      cxsd_gchnid_t  gcid,
                      int            reason,
                      void          *privptr2);

// GetMonSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Mon, moninfo_t,
                                 monitors, in_use, 0, 1,
                                 0, 100, 0,
                                 cp->, cp,
                                 v4clnt_t *cp, v4clnt_t *cp)

static void RlsMonSlot(int id, v4clnt_t *cp)
{
  moninfo_t *mp = AccessMonSlot(id, cp);

    CxsdHwDelChanEvproc(cp->ID, lint2ptr(cp2cd(cp)),
                        mp->gcid, MONITOR_EVMASK,
                        MonEvproc, lint2ptr(id));
    if (REQ_ALL_MONITORS  ||  mp->cond == CX_MON_COND_ON_CYCLE)
    {
        cp->per_cycle_monitors_count--;
        cp->per_cycle_monitors_needs_rebuild = 1;
    }

    mp->in_use = 0;
}

//--------------------------------------------------------------------

// GetV4connSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, V4conn, v4clnt_t,
                                 cx4clnts, in_use, 0, 1,
                                 1, V4_ALLOC_INC, V4_MAX_CONNS,
                                 , , void)

static void RlsV4connSlot(int cd)
{
  v4clnt_t  *cp  = AccessV4connSlot(cd);
  int        err = errno;        // To preserve errno
  
    if (cp->in_use == 0) return; /*!!! In fact, an internal error */
    
    cp->in_use = 0;

    cxsd_hw_do_cleanup  (cp->ID);
    CxsdHwDeleteClientID(cp->ID);

    DestroyMonSlotArray(cp); /* Note: we do NOT call RlsMonSlot() for each element only because this is done by cxsd_hw_do_cleanup() */
    safe_free(cp->per_cycle_monitors);

    ////////////
    if (cp->fhandle >= 0) fdio_deregister(cp->fhandle);
    if (cp->fd      >= 0) close          (cp->fd);
    if (cp->clp_tid >= 0) sl_deq_tout    (cp->clp_tid);
    safe_free(cp->replybuf);
    
    errno = err;
}

//--------------------------------------------------------------------

// GetAdvisorSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Advisor, advisor_t,
                                 advisors, in_use, 0, 1,
                                 0, ADVISORS_ALLOC_INC, ADVISORS_MAX_CONNS,
                                 , , void)

static void RlsAdvisorSlot(int jd)
{
  advisor_t *jp  = AccessAdvisorSlot(jd);
  int        err = errno;        // To preserve errno
  
    if (jp->in_use == 0) return; /*!!! In fact, an internal error */
    
    jp->in_use = 0;
    
    ////////////
    if (jp->fhandle >= 0) fdio_deregister(jp->fhandle);
    if (jp->fd      >= 0) close          (jp->fd);
    
    errno = err;
}

//////////////////////////////////////////////////////////////////////

static uint32 host_u32(v4clnt_t *cp, uint32 c_u32)
{
    if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN) return l2h_u32(c_u32);
    else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)    return b2h_u32(c_u32);
    else                                               return         c_u32;
}

static uint32 clnt_u32(v4clnt_t *cp, uint32 h_u32)
{
    if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN) return h2l_u32(h_u32);
    else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)    return h2b_u32(h_u32);
    else                                               return         h_u32;
}

static  int32 host_i32(v4clnt_t *cp,  int32 c_i32)
{
    if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN) return l2h_u32(c_i32);
    else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)    return b2h_u32(c_i32);
    else                                               return         c_i32;
}

static  int32 clnt_i32(v4clnt_t *cp,  int32 h_i32)
{
    if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN) return h2l_u32(h_i32);
    else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)    return h2b_u32(h_i32);
    else                                               return         h_i32;
}

static int sanitize_strzcpy(char *dst, size_t dst_size,
                            char *src, size_t src_size)
{
  int  ret = 0;
  int  x;
  
    for (x = 0;
         x < dst_size  &&  x < src_size;
         x++)
    {
        dst[x] = src[x];
        if (dst[x] == '\0') break;
        if (!isprint(dst[x]))
        {
            dst[x] = '?';
            ret = 1;
        }
    }
    dst[dst_size - 1] = '\0';

    return ret;
}

//////////////////////////////////////////////////////////////////////

static void InitReplyPacket (v4clnt_t *cp, int32 code, int32 Seq)
{
    bzero(cp->replybuf, sizeof(CxV4Header));

    cp->replybuf->Type = clnt_u32(cp, code);
    cp->replybuf->ID   = clnt_u32(cp, cp->ID);
    cp->replybuf->Seq  = Seq; /* Direct copy -- no need for (double) conversion */
}


static int  GrowReplyPacket (v4clnt_t *cp, size_t datasize)
{
  int r;

    r = GrowBuf((void *) &(cp->replybuf), &(cp->replybufsize),
                sizeof(CxV4Header) + datasize);
    if (r < 0) return r;

    return 0;
}


//////////////////////////////////////////////////////////////////////

enum
{
    GAC_NEWDB  = CXC_REQ_CMD('G', 'd', 'b'),  // new DB
    GAC_NMSP   = CXC_REQ_CMD('G', 'n', 's'),  // NameSpace
    GAC_DEVICE = CXC_REQ_CMD('G', 'd', 'v'),  // DeVice
    GAC_CPOINT = CXC_REQ_CMD('G', 'c', 'p'),  // CPoint
    GAC_CLEVEL = CXC_REQ_CMD('G', 'c', 'l'),  // CLevel
    GAC_EOD    = CXC_REQ_CMD('G', 'e', 'd'),  // End of Data
};

typedef struct
{
    uint32     OpCode;
    uint32     ByteSize;
} GuruChunk;

typedef struct
{
    GuruChunk  ck;
    int32      instance;
    int32      numdevs;
    int32      numchans;
    int32      cpnts_used;
    int32      clvls_used;
    int32      nsps_used;
    int32      strbuf_used;
    int32      reserved;
    char       strbuf[0];
} GuruDbInitChunk;

typedef struct
{
    int32      name_ofs;
    int32      devchan_n;
} GuruDbNspItem;
typedef struct
{
    GuruChunk      ck;
    int32          id;
    int32          typename_ofs;
    int32          chancount;
    int32          items_used;
    GuruDbNspItem  items[0];
} GuruDbNspChunk;

typedef struct
{
    GuruChunk  ck;
    int32      devid;
    int32      instname_ofs;
    int32      type_nsp_id;
    int32      chan_nsp_id;
    int32      chancount;
    int32      wauxcount;
} GuruDbDevChunk;

typedef struct
{
    GuruChunk  ck;
    int32      cpn;
    int32      devid;
    int32      ref_n;
    int32      reserved;
} GuruDbCpntChunk;

typedef struct
{
    int32      name_ofs;
    int32      type;
    int32      ref_n;
    int32      rsrvd;
} GuruDbClvlItem;
typedef struct
{
    GuruChunk       ck;
    int32           id;
    int32           items_used;
    GuruDbClvlItem  items[0];
} GuruDbClvlChunk;

// -------------------------------------------------------------------

static int  ConnectToGuru(void);
static void DestroyInfoSockets(int and_deregister);

static int CreateGuruSockets(void)
{
  int                 r;                /* Result of system calls */
  struct sockaddr_in  iaddr;            /* Address to bind `inet_guru_socket' to */
  struct sockaddr_un  uaddr;            /* Address to bind `unix_guru_socket' to */
  int                 on     = 1;       /* "1" for REUSE_ADDR */
  char                unix_name[PATH_MAX]; /* Buffer to hold unix socket name */

    /* 1. INET/DGRAM socket */

    /* Create the socket */
    inet_guru_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (inet_guru_socket < 0)
        return -1;

    /* Bind it to the name */
    //setsockopt(inet_guru_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    iaddr.sin_family      = AF_INET;
    iaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iaddr.sin_port        = htons(CX_V4_INET_RESOLVER);
    r = bind(inet_guru_socket, (struct sockaddr *) &iaddr,
             sizeof(iaddr));
    if (r < 0)
        return -1;

    /* Mark it as non-blocking */
    set_fd_flags(inet_guru_socket, O_NONBLOCK, 1);

    /* 2. UNIX socket */

    /* Create the socket */
    unix_guru_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_guru_socket < 0)
        return -1;

    /* Bind it to the name */
    sprintf(unix_name, "%s", CX_V4_UNIX_RESOLVER); /*!!!snprintf()?*/
    unlink (unix_name);
    strcpy(uaddr.sun_path, unix_name);
    uaddr.sun_family = AF_UNIX;
    r = bind(unix_guru_socket, (struct sockaddr *) &uaddr,
             offsetof(struct sockaddr_un, sun_path) + strlen(uaddr.sun_path));
    if (r < 0)
        return -1;

    /* Mark it as listening and non-blocking */
    listen(unix_guru_socket, 5);
    set_fd_flags(unix_guru_socket, O_NONBLOCK, 1);
    
    /* Set it to be world-accessible (bind() is affected by umask in Linux) */
    chmod(unix_name, 0777);

    return 0;
}

static void DestroyGuruSockets(int and_deregister)
{
    if (inet_guru_socket >= 0)
    {
        if (and_deregister) fdio_deregister(inet_guru_handle), inet_guru_handle = -1;
        close(inet_guru_socket);
    }
    if (unix_guru_socket >= 0)
    {
        if (and_deregister) fdio_deregister(unix_guru_handle), unix_guru_handle = -1;
        close(unix_guru_socket);

        unlink (CX_V4_UNIX_RESOLVER);
    }
    
    inet_guru_socket = unix_guru_socket = -1;
}

static void ServeGuruRequest(int uniq, void *unsdptr,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr)
{
  CxV4Header *hdr  = inpkt;
  int         endianness;
  uint32      pkdtsz;
  int32       pktype;
  int         numrqs;          // # of chunks in request
  uint32      cln_ver;
  uint8      *ptr;
  CxV4Chunk  *req;
  int         reqcn;           // REQuest chunk # (loop ctl var)
  uint32      reqcode;         // REQuest opCODE
  size_t      reqcsize;        // REQuest chunk size in bytes
  size_t      reqlen;          // REQuest *data* LENgth in bytes
  size_t      req_total;

  int         srv;
  CxsdDb      db;
  int         res_res;
  int         devid;
  int         chann;

  struct
  {
      CxV4Header  hdr;
      uint8       data[CX_V4_MAX_UDP_DATASIZE];
  } result;
  size_t      result_DataSize;
  int         result_NumChunks;
  size_t      rpycsize;
  CxV4Chunk  *rpy;

#define BeginSearchReply()    \
    do {                      \
        result_DataSize  = 0; \
        result_NumChunks = 0; \
    } while (0)
#define SendSearchReply()                                                  \
    do {                                                                   \
        result.hdr = *hdr;                                                 \
        if (endianness == FDIO_LEN_LITTLE_ENDIAN)                          \
        {                                                                  \
            result.hdr.DataSize  = h2l_u32(result_DataSize);               \
            result.hdr.Type      = h2l_u32(CXT4_SEARCH_RESULT);            \
            result.hdr.NumChunks = h2l_u32(result_NumChunks);              \
        }                                                                  \
        else                                                               \
        {                                                                  \
            result.hdr.DataSize  = h2b_u32(result_DataSize);               \
            result.hdr.Type      = h2b_u32(CXT4_SEARCH_RESULT);            \
            result.hdr.NumChunks = h2b_u32(result_NumChunks);              \
        }                                                                  \
        fdio_reply(handle, &result, sizeof(result.hdr) + result_DataSize); \
    } while (0)

    if (reason != FDIO_R_DATA) return;

    ////fprintf(stderr, "%s inpktsize=%d\n", __FUNCTION__, inpktsize);

    if (inpktsize < sizeof(*hdr)) return;

    if      (l2h_u32(hdr->var1) == CXV4_VAR1_ENDIANNESS_SIG)
    {
        endianness = FDIO_LEN_LITTLE_ENDIAN;
        pkdtsz  = l2h_u32(hdr->DataSize);
        pktype  = l2h_u32(hdr->Type);
        numrqs  = l2h_u32(hdr->NumChunks);
        cln_ver = l2h_u32(hdr->var2);
    }
    else if (b2h_u32(hdr->var1) == CXV4_VAR1_ENDIANNESS_SIG)
    {
        endianness = FDIO_LEN_BIG_ENDIAN;
        pkdtsz  = b2h_u32(hdr->DataSize);
        pktype  = b2h_u32(hdr->Type);
        numrqs  = b2h_u32(hdr->NumChunks);
        cln_ver = b2h_u32(hdr->var2);
    }
    else
        return;

    /* Pakcet-wide checks */
    /* 1. Packet type */
    if (pktype != CXT4_SEARCH) return;
    /* 1.1. Version */
    if (!CX_VERSION_IS_COMPATIBLE(cln_ver, CX_V4_PROTO_VERSION)) return;
    /* 2. Packet size */
    /*    Note: no reason to check "inpktsize <= CX_V4_MAX_UDP_PKTSIZE",
                because that is not crucial */
    if (pkdtsz + sizeof(*hdr) != inpktsize)
        return;

    BeginSearchReply();

    for (reqcn = 0, ptr = hdr->data, req_total = sizeof(CxV4Header);
         reqcn < numrqs;
         reqcn++,   ptr += reqcsize, req_total += reqcsize)
    {
        /* Check if we MAY access chunk */
        if (req_total + sizeof(CxV4Chunk) > inpktsize)
        {
            return;
        }

        req = (void *)ptr;
        if (endianness == FDIO_LEN_LITTLE_ENDIAN)
        {
            reqcode  = l2h_u32(req->OpCode);
            reqcsize = l2h_u32(req->ByteSize);
        }
        else
        {
            reqcode  = l2h_u32(req->OpCode);
            reqcsize = l2h_u32(req->ByteSize);
        }
        reqlen   = reqcsize - sizeof(CxV4Chunk);

        /* Check: */
        /* I. Size validity */
        /* 1. Is reqcsize sane at all? */
        if (reqcsize < sizeof(*req) + 1)
        {
            return;
        }
        /* 2. Does this chunk have appropriate alignment? */
        if (reqcsize != CXV4_CHUNK_CEIL(reqcsize))
        {
            return;
        }
        /* 3. Okay, does this chunk still fit into packet? */
        if (req_total + reqcsize > inpktsize)
        {
            return;
        }
        /* II. Name */
        /* 1. Is there a meaningful string
              ("2" means non-empty, for at least 1 char besides NUL) */
        if (reqlen < 2) continue;
        /* 2. Force NUL-termination */
        req->data[reqlen-1] = '\0';

        if (reqcode != CXC_SEARCH) continue;

        ////fprintf(stderr, "Q: [%d]<%s>\n", reqlen, req->data);

        for (srv = 0;  srv <= CX_MAX_SERVER;  srv++)
        {
            db = (srv == my_server_instance)? cxsd_hw_cur_db : advisors_dbs[srv];
            if (db == NULL) continue;
            res_res = CxsdDbResolveName(db, req->data, &devid, &chann);
            if (res_res == CXSD_DB_RESOLVE_DEVCHN  ||
                res_res == CXSD_DB_RESOLVE_CPOINT
                /* Note:   CXSD_DB_RESOLVE_GLOBAL is NOT counted */)
            {
                rpycsize = sizeof(*rpy) + reqlen;
                /* Flush buffer if space is over */
                if (result_DataSize + rpycsize > sizeof(result.data))
                {
                    SendSearchReply ();
                    BeginSearchReply();
                }
                rpy = result.data + result_DataSize;

                *rpy = *req;
                if (endianness == FDIO_LEN_LITTLE_ENDIAN)
                {
                    rpy->OpCode    = h2l_u32(CXC_CVT_TO_RPY(reqcode));
                    rpy->ByteSize  = h2l_u32(rpycsize);
                    rpy->rs4       = h2l_u32(srv);
                }
                else
                {
                    rpy->OpCode    = h2b_u32(CXC_CVT_TO_RPY(reqcode));
                    rpy->ByteSize  = h2b_u32(rpycsize);
                    rpy->rs4       = h2b_u32(srv);
                }

                memcpy(rpy->data, req->data, reqlen);

                result_DataSize += rpycsize;
                result_NumChunks++;
                ////fprintf(stderr, "\"%s\" is at :%d\n", req->data, srv);

                goto NEXT_REQ;
            }
        }

 NEXT_REQ:;
    }

    if (result_NumChunks > 0) SendSearchReply();

    ////fprintf(stderr, "%s() size=%zd\n", __FUNCTION__, inpktsize);
    ////fdio_reply(handle, inpkt, inpktsize);
#undef BeginSearchReply
#undef SendSearchReply
}

static void ForgetAdvisorDb(int instance)
{
    CxsdDbDestroy(advisors_dbs[instance]);
    advisors_dbs[instance] = NULL;
}

static void InteractWithAdvisor(int uniq, void *unsdptr,
                                fdio_handle_t handle, int reason,
                                void *inpkt, size_t inpktsize,
                                void *privptr)
{
  int                 jd = ptr2lint(privptr);
  advisor_t          *jp = AccessAdvisorSlot(jd);
  GuruChunk          *cp = inpkt;

  GuruDbInitChunk    *dbr;
  GuruDbNspChunk     *nsr;
  GuruDbDevChunk     *dvr;
  GuruDbCpntChunk    *cpr;
  GuruDbClvlChunk    *cvr;

  CxsdDb              new_db;
  size_t              size;
  int                 x;

  int                 nsp_id;
  CxsdDbDcNsp_t      *nsp;

  int                 devid;
  CxsdDbDevLine_t    *dev_p;

  int                 cpn;
  CxsdDbCpntInfo_t   *cpnt_data_p;

  int                 clvl_id;
  CxsdDbClvlInfo_t   *clvl;

#define ADV_FATAL(code, format, params...)                    \
    do {                                                      \
        logline(LOGF_SYSTEM, LOGL_ERR,                        \
                "%s(:%d): "format, __FUNCTION__, jp->instance, ##params);   \
        if (jp->instance >= 0) ForgetAdvisorDb(jp->instance); \
        RlsAdvisorSlot(jd);                                   \
        return;                                               \
    } while (0)

    ////fprintf(stderr, "%s(:%d) reason=%d size=%zd\n", __FUNCTION__, jp->instance, reason, inpktsize);
    switch (reason)
    {
        case FDIO_R_DATA:
            new_db = (jp->instance >= 0)? advisors_dbs[jp->instance] : NULL;
            /* Attempt to add to a finalized db? */
            if (new_db != NULL  &&
                new_db->is_readonly)
            {
                logline(LOGF_SYSTEM, LOGL_ERR,
                        "%s() instance=%d, is_readonly, new DATA OpCode=0x%08x",
                        __FUNCTION__, jp->instance, cp->OpCode);
                return;
            }
            /* A fresh connection and not a NEWDB? */
            if (jp->instance < 0  &&  cp->OpCode != GAC_NEWDB)
                ADV_FATAL(0, "instance<0 and OpCode=0x%08x, !=NEWDB(0x%08x)",
                          cp->OpCode, GAC_NEWDB);

            if (cp->OpCode == GAC_NEWDB)
            {
                dbr = inpkt;
                if (inpktsize < sizeof(*dbr))
                    ADV_FATAL(0, "GAC_NEWDB: inpktsize<sizeof(GuruDbInitChunk)");

                /* Check for protocol errors */
                if (jp->instance >= 0)
                    ADV_FATAL(0, "instance>=0 and OpCode=NEWDB");
                if (dbr->instance == my_server_instance)
                    ADV_FATAL(0, "instance=%d ==my_server_instance -- forbidding",
                              jp->instance);

                /* ...and for logical errors */
                if (dbr->instance < 0  ||  dbr->instance > CX_MAX_SERVER)
                    ADV_FATAL(0, "NEWDB: instance=%d is out of range [0-%d]",
                            dbr->instance, CX_MAX_SERVER);
                if (advisors_dbs[dbr->instance] != NULL)
                {
                    /*!!!Any check?*/
                    logline(LOGF_SYSTEM, LOGL_WARNING,
                            "WARNING: instance=%d, already registered",
                            dbr->instance);
                }

                ForgetAdvisorDb(dbr->instance);

                size = sizeof(*new_db);
                if ((new_db = malloc(size)) == NULL)
                    ADV_FATAL(0, "malloc(db)==NULL");
                bzero(new_db, size);
                advisors_dbs[jp->instance = dbr->instance] = new_db;

                if (dbr->strbuf_used > 0)
                {
                    if ((new_db->strbuf = malloc(dbr->strbuf_used)) == NULL)
                        ADV_FATAL(0, "malloc(strbuf)");
                    new_db->strbuf_used = new_db->strbuf_allocd = dbr->strbuf_used;
                    memcpy(new_db->strbuf, dbr->strbuf, new_db->strbuf_used);
                }

                if (dbr->nsps_used > 0)
                {
                    size = sizeof(*(new_db->nsps_list)) * dbr->nsps_used;
                    if ((new_db->nsps_list = malloc(size)) == NULL)
                        ADV_FATAL(0, "malloc(nsps_list)");
                    bzero(new_db->nsps_list, size);
                    new_db->nsps_used = new_db->nsps_allocd = dbr->nsps_used;
                }

                if (dbr->numdevs > 0)
                {
                    size = sizeof(*(new_db->devlist)) * dbr->numdevs;
                    if ((new_db->devlist = malloc(size)) == NULL)
                        ADV_FATAL(0, "malloc(devlist)");
                    bzero(new_db->devlist, size);
                    new_db->numdevs = dbr->numdevs;
                }
                /*!!! Dubious!  Should it be in db-copy at all?*/
                new_db->numchans = dbr->numchans;

                if (dbr->cpnts_used > 0)
                {
                    size = sizeof(*(new_db->cpnts)) * dbr->cpnts_used;
                    if ((new_db->cpnts = malloc(size)) == NULL)
                        ADV_FATAL(0, "malloc(cpnts)");
                    bzero(new_db->cpnts, size);
                    new_db->cpnts_used = dbr->cpnts_used;
                }

                if (dbr->clvls_used > 0)
                {
                    size = sizeof(*(new_db->clvls_list)) * dbr->clvls_used;
                    if ((new_db->clvls_list = malloc(size)) == NULL)
                        ADV_FATAL(0, "malloc(clvls_list)");
                    bzero(new_db->clvls_list, size);
                    new_db->clvls_used = new_db->clvls_allocd = dbr->clvls_used;
                }
            }
            else if (cp->OpCode == GAC_NMSP)
            {
                nsr = inpkt;
                nsp_id = nsr->id;
                if (nsp_id < 0  ||  nsp_id >= new_db->nsps_used)
                    ADV_FATAL(0, "GAC_DEVICE: devid=%d, out_of[0-%d)",
                              nsp_id, new_db->nsps_used);
                if (nsr->items_used < 0) nsr->items_used = 0; /*!!!*/

                if (new_db->nsps_list[nsp_id] != NULL)
                {
                    /*!!!*/
                    safe_free(new_db->nsps_list[nsp_id]);
                    new_db->nsps_list[nsp_id] = NULL;
                }

                size = sizeof(*nsp) + sizeof(nsp->items[0]) * nsr->items_used;
                if ((nsp = new_db->nsps_list[nsp_id] = malloc(size)) == NULL)
                    ADV_FATAL(0, "malloc(nsp#%d)", nsp_id);
                bzero(nsp, size);
                nsp->typename_ofs = nsr->typename_ofs;
                nsp->chancount    = nsr->chancount;
                nsp->items_used   = nsp->items_allocd =
                                    nsr->items_used;
                for (x = 0;  x < nsp->items_used;  x++)
                {
                    nsp->items[x].name_ofs  = nsr->items[x].name_ofs;
                    nsp->items[x].devchan_n = nsr->items[x].devchan_n;
                }
            }
            else if (cp->OpCode == GAC_DEVICE)
            {
                dvr = inpkt;
                if (inpktsize != sizeof(*dvr))
                    ADV_FATAL(0, "GAC_DEVICE: inpktsize<sizeof(GuruDbDevChunk)");
                devid = dvr->devid;
                if (devid < 0  ||  devid >= new_db->numdevs)
                    ADV_FATAL(0, "GAC_DEVICE: devid=%d, out_of[0-%d)",
                              devid, new_db->numdevs);

                dev_p = new_db->devlist + devid;
                dev_p->instname_ofs = dvr->instname_ofs;
                dev_p->type_nsp_id  = dvr->type_nsp_id;
                dev_p->chan_nsp_id  = dvr->chan_nsp_id;
                dev_p->chancount    = dvr->chancount;
                dev_p->wauxcount    = dvr->wauxcount;
            }
            else if (cp->OpCode == GAC_CPOINT)
            {
                cpr = inpkt;
                cpn = cpr->cpn;
                if (cpn < 0  ||  cpn > new_db->cpnts_used)
                    ADV_FATAL(0, "GAC_CPOINT: cpn=%d, out_of[0-%d)",
                              cpn, new_db->cpnts_used);

                cpnt_data_p = new_db->cpnts + cpn;
                cpnt_data_p->devid = cpr->devid;
                cpnt_data_p->ref_n = cpr->ref_n;
            }
            else if (cp->OpCode == GAC_CLEVEL)
            {
                cvr = inpkt;
                clvl_id = cvr->id;
                if (clvl_id < 0  ||  clvl_id >= new_db->clvls_used)
                    ADV_FATAL(0, "GAC_CLEVEL: clvl_id=%d, out_of[0-%d)",
                              clvl_id, new_db->clvls_used);
                if (cvr->items_used < 0) cvr->items_used = 0; /*!!!*/

                if (new_db->clvls_list[clvl_id] != NULL)
                {
                    /*!!!*/
                    safe_free(new_db->clvls_list[clvl_id]);
                    new_db->clvls_list[clvl_id] = NULL;
                }

                size = sizeof(*clvl) + sizeof(clvl->items[0]) * cvr->items_used;
                if ((clvl = new_db->clvls_list[clvl_id] = malloc(size)) == NULL)
                    ADV_FATAL(0, "malloc(clvl#%d", clvl_id);
                bzero(clvl, size);
                clvl->items_used = clvl->items_allocd =
                                   cvr->items_used;
                for (x = 0;  x < clvl->items_used;  x++)
                {
                    clvl->items[x].name_ofs = cvr->items[x].name_ofs;
                    clvl->items[x].type     = cvr->items[x].type;
                    clvl->items[x].ref_n    = cvr->items[x].ref_n;
                }
            }
            else if (cp->OpCode == GAC_EOD)
            {
                new_db->is_readonly = 1;
                logline(LOGF_SYSTEM, LOGL_NOTICE,
                        "GURU got db from :%d ", jp->instance);
            }
            else
            {
                logline(LOGF_SYSTEM, LOGL_ERR,
                        "%s(), instance=%d unknown OpCode=0x%08x",
                        __FUNCTION__, jp->instance, cp->OpCode);
            }
            break;
      
        case FDIO_R_CLOSED:
        default:
            fprintf(stderr, "%s(:%d) CLOSING: %s fd=%d errno=%d/%s\n", __FUNCTION__, jp->instance, fdio_strreason(reason), jp->fd, errno, strerror(errno));
            if (jp->instance >= 0)
                ForgetAdvisorDb(jp->instance);
            RlsAdvisorSlot(jd);
            break;
    }
}
static void AcceptGuruConnection(int uniq, void *unsdptr,
                                 fdio_handle_t handle, int reason,
                                 void *inpkt, size_t inpktsize,
                                 void *privptr)
{
  int                 s;

  int                 jd;
  advisor_t          *jp;
  fdio_handle_t       fhandle;

    ////fprintf(stderr, "%s()\n", __FUNCTION__);
    s = fdio_accept(handle, NULL, NULL);

    /* Is anything wrong? */
    if (s < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): fdio_accept()",
                __FUNCTION__);
        return;
    }

    /* Instantly switch it to non-blocking mode */
    if (set_fd_flags(s, O_NONBLOCK, 1) < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): set_fd_flags(O_NONBLOCK)",
                __FUNCTION__);
        close(s);
        return;
    }

    /* Get an advisor slot... */
    if ((jd = GetAdvisorSlot()) < 0)
    {
        /*!!! log this? */
        close(s);
        return;
    }

    /* ...and fill it with data */
    jp = AccessAdvisorSlot(jd);
    jp->fd       = s;
    jp->instance = -1;
    
    /* Okay, let's obtain an fdio slot... */
    jp->fhandle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                   s, FDIO_STREAM,
                                   InteractWithAdvisor, lint2ptr(jd),
                                   1000000/*!!!*/,
                                   sizeof(GuruChunk),
                                   FDIO_OFFSET_OF(GuruChunk, ByteSize),
                                   FDIO_SIZEOF   (GuruChunk, ByteSize),
                                   BYTE_ORDER == LITTLE_ENDIAN? FDIO_LEN_LITTLE_ENDIAN
                                                              : FDIO_LEN_BIG_ENDIAN,
                                   1, 0);
    if (jp->fhandle < 0)
    {
        /*!!! log this? */
        RlsAdvisorSlot(jd);
        return;
    }
}

static int TryToBecomeGuru(void)
{
    if (CreateGuruSockets() != 0)
        goto DO_CLEANUP;

    inet_guru_handle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                        inet_guru_socket,     FDIO_DGRAM,
                                        ServeGuruRequest,     NULL,
                                        CX_V4_MAX_UDP_PKTSIZE,
                                        sizeof(CxV4Header), 0, 0,
                                        FDIO_LEN_LITTLE_ENDIAN,
                                        0, 0);
    if (inet_guru_handle < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_EMERG,
                "%s::%s: fdio_register_fd(inet_guru_socket)",
                __FILE__, __FUNCTION__);
        goto DO_CLEANUP;
    }
    unix_guru_handle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                        unix_guru_socket,     FDIO_LISTEN,
                                        AcceptGuruConnection, NULL,
                                        0, 0, 0, 0,
                                        BYTE_ORDER == LITTLE_ENDIAN? FDIO_LEN_LITTLE_ENDIAN
                                                                   : FDIO_LEN_BIG_ENDIAN,
                                        1, 0);
    if (unix_guru_handle < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_EMERG,
                "%s::%s: fdio_register_fd(unix_guru_socket)",
                __FILE__, __FUNCTION__);
        goto DO_CLEANUP;
    }

    return 0;

 DO_CLEANUP:
    DestroyGuruSockets(0);
    return -1;
}

static void SendDbToGuru(void)
{
  GuruDbInitChunk dbi;
  GuruDbNspChunk  nsi;
  GuruDbNspItem   nii;
  GuruDbDevChunk  dvi;
  GuruDbCpntChunk cpi;
  GuruDbClvlChunk cvi;
  GuruDbClvlItem  cii;
  GuruChunk       eod;

  int             nsn;
  int             nin;
  int             devid;
  int             cpn;
  int             cvn;
  int             cin;

    /* Create new db (send parameters) */
    bzero(&dbi, sizeof(dbi));
    dbi.ck.OpCode   = GAC_NEWDB;
    dbi.ck.ByteSize = sizeof(dbi) + cxsd_hw_cur_db->strbuf_used;
    dbi.instance    = my_server_instance;
    dbi.numdevs     = cxsd_hw_cur_db->numdevs;
    dbi.numchans    = cxsd_hw_cur_db->numchans;
    dbi.cpnts_used  = cxsd_hw_cur_db->cpnts_used;
    dbi.clvls_used  = cxsd_hw_cur_db->clvls_used;
    dbi.nsps_used   = cxsd_hw_cur_db->nsps_used;
    dbi.strbuf_used = cxsd_hw_cur_db->strbuf_used;
    if (fdio_send(unix_info_handle, &dbi, sizeof(dbi)) < 0  ||
        /* ...plus strbuf */
        (cxsd_hw_cur_db->strbuf_used > 0  &&
         fdio_send(unix_info_handle,
                   cxsd_hw_cur_db->strbuf,
                   cxsd_hw_cur_db->strbuf_used) < 0)) goto FAILURE;

    /* Namespaces */
    for (nsn = 0;  nsn < cxsd_hw_cur_db->nsps_used;  nsn++)
    {
        bzero(&nsi, sizeof(nsi));
        nsi.ck.OpCode   = GAC_NMSP;
        nsi.ck.ByteSize = sizeof(nsi) +
                          sizeof(nii) * cxsd_hw_cur_db->nsps_list[nsn]->items_used;
        nsi.id           = nsn;
        nsi.typename_ofs = cxsd_hw_cur_db->nsps_list[nsn]->typename_ofs;
        nsi.chancount    = cxsd_hw_cur_db->nsps_list[nsn]->chancount;
        nsi.items_used   = cxsd_hw_cur_db->nsps_list[nsn]->items_used;
        if (fdio_send(unix_info_handle, &nsi, sizeof(nsi)) < 0) goto FAILURE;

        for (nin = 0;  nin < cxsd_hw_cur_db->nsps_list[nsn]->items_used;  nin++)
        {
            bzero(&nii, sizeof(nii));
            nii.name_ofs  = cxsd_hw_cur_db->nsps_list[nsn]->items[nin].name_ofs;
            nii.devchan_n = cxsd_hw_cur_db->nsps_list[nsn]->items[nin].devchan_n;
            if (fdio_send(unix_info_handle, &nii, sizeof(nii)) < 0) goto FAILURE;
        }
    }
    
    /* Devices */
    for (devid = 0;  devid < cxsd_hw_cur_db->numdevs;  devid++)
    {
        bzero(&dvi, sizeof(dvi));
        dvi.ck.OpCode    = GAC_DEVICE;
        dvi.ck.ByteSize  = sizeof(dvi);
        dvi.devid        = devid;
        dvi.instname_ofs = cxsd_hw_cur_db->devlist[devid].instname_ofs;
        dvi.type_nsp_id  = cxsd_hw_cur_db->devlist[devid].type_nsp_id;
        dvi.chan_nsp_id  = cxsd_hw_cur_db->devlist[devid].chan_nsp_id;
        dvi.chancount    = cxsd_hw_cur_db->devlist[devid].chancount;
        dvi.wauxcount    = cxsd_hw_cur_db->devlist[devid].wauxcount;
        if (fdio_send(unix_info_handle, &dvi, sizeof(dvi)) < 0) goto FAILURE;
    }

    /* Cpoints */
    for (cpn = 0;  cpn < cxsd_hw_cur_db->cpnts_used;  cpn++)
    {
        bzero(&cpi, sizeof(cpi));
        cpi.ck.OpCode   = GAC_CPOINT;
        cpi.ck.ByteSize = sizeof(cpi);
        cpi.cpn         = cpn;
        cpi.devid       = cxsd_hw_cur_db->cpnts[cpn].devid;
        cpi.ref_n       = cxsd_hw_cur_db->cpnts[cpn].ref_n;
        if (fdio_send(unix_info_handle, &cpi, sizeof(cpi)) < 0) goto FAILURE;
    }

    /* Clevels */
    for (cvn = 0;  cvn < cxsd_hw_cur_db->clvls_used;  cvn++)
    {
        bzero(&cvi, sizeof(cvi));
        cvi.ck.OpCode   = GAC_CLEVEL;
        cvi.ck.ByteSize = sizeof(cvi) +
                          sizeof(cii) * cxsd_hw_cur_db->clvls_list[cvn]->items_used;
        cvi.id          = cvn;
        cvi.items_used  = cxsd_hw_cur_db->clvls_list[cvn]->items_used;
        if (fdio_send(unix_info_handle, &cvi, sizeof(cvi)) < 0) goto FAILURE;

        for (cin = 0;  cin < cxsd_hw_cur_db->clvls_list[cvn]->items_used;  cin++)
        {
            bzero(&cii, sizeof(cii));
            cii.name_ofs = cxsd_hw_cur_db->clvls_list[cvn]->items[cin].name_ofs;
            cii.type     = cxsd_hw_cur_db->clvls_list[cvn]->items[cin].type;
            cii.ref_n    = cxsd_hw_cur_db->clvls_list[cvn]->items[cin].ref_n;
            if (fdio_send(unix_info_handle, &cii, sizeof(cii)) < 0) goto FAILURE;
        }
    }
    
    /* Finish */
    bzero(&eod, sizeof(eod));
    eod.OpCode   = GAC_EOD;
    eod.ByteSize = sizeof(eod);
    if (fdio_send(unix_info_handle, &eod, sizeof(eod)) < 0) goto FAILURE;

    return;

 FAILURE:
    DestroyInfoSockets(1);
}

static void ServeInfoRequest(int uniq, void *unsdptr,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr)
{
    fprintf(stderr, "%s() reason=%d size=%zd\n", __FUNCTION__, reason, inpktsize);

    switch (reason)
    {
        case FDIO_R_DATA:
            break;
      
        case FDIO_R_CONNECTED:
            logline(LOGF_SYSTEM, LOGL_NOTICE, "Connected to guru");
            SendDbToGuru();
            break;

        case FDIO_R_CONNERR:
            DestroyInfoSockets(1);
            break;

        case FDIO_R_CLOSED:
        case FDIO_R_IOERR:
        default:
            DestroyInfoSockets(1);
            ////fprintf(stderr, "%s inet_guru_socket=%d unix_info_socket=%d\n", __FUNCTION__, inet_guru_socket, unix_info_socket);
            if (TryToBecomeGuru() >= 0)
                logline(LOGF_SYSTEM, LOGL_NOTICE, "Took over GURU");
            else
                ConnectToGuru();
            break;
    }
}

static int  ConnectToGuru(void)
{
  struct sockaddr_un  udst;
  int                 r;

    unix_info_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_info_socket < 0)
        return -1;
    set_fd_flags(unix_info_socket, O_NONBLOCK, 1);

    udst.sun_family = AF_UNIX;
    strzcpy(udst.sun_path, CX_V4_UNIX_RESOLVER, sizeof(udst.sun_path));
    r = connect(unix_info_socket, (struct sockaddr *) &udst,
                offsetof(struct sockaddr_un, sun_path) + strlen(udst.sun_path));
    if (r < 0)
    {
        close(unix_info_socket); unix_info_socket = -1;
        return -1;
    }

    unix_info_handle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                        unix_info_socket, FDIO_CONNECTING,
                                        ServeInfoRequest, NULL,
                                        1000000/*!!!*/,
                                        sizeof(GuruChunk),
                                        FDIO_OFFSET_OF(GuruChunk, ByteSize),
                                        FDIO_SIZEOF   (GuruChunk, ByteSize),
                                        BYTE_ORDER == LITTLE_ENDIAN? FDIO_LEN_LITTLE_ENDIAN
                                                                   : FDIO_LEN_BIG_ENDIAN,
                                        1, 0);
    if (unix_info_handle < 0)
    {
        close(unix_info_socket); unix_info_socket = -1;
        return -1;
    }

    return 0;
}

static void DestroyInfoSockets(int and_deregister)
{
    if (unix_info_socket >= 0)
    {
        if (and_deregister) fdio_deregister(unix_info_handle), unix_info_handle = -1;
        close(unix_info_socket);
    }

    unix_info_socket = -1;
}

static void GuruTakeoverHbtProc(int uniq, void *privptr1,
                                sl_tid_t tid, void *privptr2)
{
    guru_takeover_hbt_tid = -1;
    sl_enq_tout_after(uniq, privptr1,
                      GURU_TAKEOVER_HEARTBEAT_SECONDS * 1000000,
                      GuruTakeoverHbtProc, privptr2);

    ////fprintf(stderr, "%s inet_guru_socket=%d unix_info_socket=%d\n", __FUNCTION__, inet_guru_socket, unix_info_socket);
    if (inet_guru_socket < 0  &&  unix_info_socket < 0)
    {
        if (TryToBecomeGuru() >= 0)
            logline(LOGF_SYSTEM, LOGL_NOTICE, "Took over GURU");
        else
            ConnectToGuru();
    }
}


//////////////////////////////////////////////////////////////////////

/*
 *  CreateServSockets
 *          Creates two sockets which are to be listened, binds them
 *      to addresses and marks as "listening".
 *
 *  Effect:
 *      inet_serv_socket  the socket is bound to INADDR_ANY.CX_V4_INET_PORT+instance
 *      unix_serv_socket  the socket is bound to CX_V4_UNIX_ADDR_FMT(instance)
 *
 *  Note:
 *      `inet_serv_socket' should be bound first, since it will fail if
 *      INET port is busy. Otherwise the following will happen if
 *      the daemon is started twice: it unlink()s CX_UNIX_ADDR, bind()s
 *      this name to its own socket, and then fails to bind() inet_serv_socket.
 *      So, it will just leave a working copy without local entry.
 */

#define Failure(msg) \
    do {             \
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_EMERG, "%s::%s: %s", __FILE__, __FUNCTION__, msg); return -1; \
        return -1;                                                                                           \
    } while (0)

static int CreateServSockets(void)
{
  int                 r;                /* Result of system calls */
  struct sockaddr_in  iaddr;            /* Address to bind `inet_serv_socket' to */
  struct sockaddr_un  uaddr;            /* Address to bind `unix_serv_socket' to */
  int                 on     = 1;       /* "1" for REUSE_ADDR */
  char                unix_name[PATH_MAX]; /* Buffer to hold unix socket name */

    /* 1. INET socket */

    /* Create the socket */
    inet_serv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (inet_serv_socket < 0)
        Failure("socket(inet_serv_socket)");

    /* Bind it to the name */
    setsockopt(inet_serv_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    iaddr.sin_family      = AF_INET;
    iaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iaddr.sin_port        = htons(CX_V4_INET_PORT + my_server_instance);
    r = bind(inet_serv_socket, (struct sockaddr *) &iaddr,
             sizeof(iaddr));
    if (r < 0)
        Failure("bind(inet_serv_socket)");

    /* Mark it as listening and non-blocking */
    listen(inet_serv_socket, 5);
    set_fd_flags(inet_serv_socket, O_NONBLOCK, 1);

    
    /* 2. UNIX socket */

    /* Create the socket */
    unix_serv_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_serv_socket < 0)
        Failure("socket(unix_serv_socket)");

    /* Bind it to the name */
    sprintf(unix_name, CX_V4_UNIX_ADDR_FMT, my_server_instance); /*!!!snprintf()?*/
    unlink (unix_name);
    strcpy(uaddr.sun_path, unix_name);
    uaddr.sun_family = AF_UNIX;
    r = bind(unix_serv_socket, (struct sockaddr *) &uaddr,
             offsetof(struct sockaddr_un, sun_path) + strlen(uaddr.sun_path));
    if (r < 0)
        Failure("bind(unix_serv_socket)");

    /* Mark it as listening and non-blocking */
    listen(unix_serv_socket, 5);
    set_fd_flags(unix_serv_socket, O_NONBLOCK, 1);
    
    /* Set it to be world-accessible (bind() is affected by umask in Linux) */
    chmod(unix_name, 0777);

    return 0;
}

static void DestroyServSockets(int and_deregister)
{
  char                unix_name[PATH_MAX]; /* Buffer to hold unix socket name */
  
    if (inet_serv_socket >= 0)
    {
        if (and_deregister) fdio_deregister(inet_serv_handle), inet_serv_handle = -1;
        close(inet_serv_socket);
    }
    if (unix_serv_socket >= 0)
    {
        if (and_deregister) fdio_deregister(unix_serv_handle), unix_serv_handle = -1;
        close(unix_serv_socket);

        sprintf(unix_name, CX_V4_UNIX_ADDR_FMT, my_server_instance); /*!!!snprintf()?*/
        unlink (unix_name);
    }
    
    inet_serv_socket = unix_serv_socket = -1;
    
}

//////////////////////////////////////////////////////////////////////

enum
{
    ERR_CLOSED = -1,
    ERR_KILLED = -2,
    ERR_IN2BIG = -3,
    ERR_HSHAKE = -4,
    ERR_TIMOUT = -5,
    ERR_INTRNL = -6,
};

static void DisconnectClient(v4clnt_t *cp, int err, uint32 code)
{
  const char *reason_s;
  const char *errdescr_s;
  CxV4Header  hdr;
  char        from[100];
    
    /* Log the case... */
    errdescr_s = "";
    if      (err == 0)          reason_s = "disconnect by";
    else if (err == ERR_CLOSED) reason_s = "conn-socket closed by";
    else if (err == ERR_KILLED) reason_s = "killed";
    else if (err == ERR_IN2BIG) reason_s = "overlong packet from";
    else if (err == ERR_HSHAKE) reason_s = "handshake error, dropping";
    else if (err == ERR_TIMOUT) reason_s = "handshake timeout, dropping";
    else if (err == ERR_INTRNL) reason_s = "internal error, disconnecting";
    else if (err == EOVERFLOW)  reason_s = "send-buffer overflow, disconnecting";
    else                        reason_s = "abnormally disconnected",
                                errdescr_s = strerror(err);
  
    if (cp->ip == 0)
        strcpy(from, "-");
    else
        sprintf(from, "%d.%d.%d.%d",
                      (cp->ip >> 24) & 255,
                      (cp->ip >> 16) & 255,
                      (cp->ip >>  8) & 255,
                      (cp->ip >>  0) & 255);

    logline(LOGF_ACCESS, LOGL_NOTICE, "[%d] %s %s@%s:%s%s%s",
            cp2cd(cp),
            reason_s,
            cp->username[0] != '\0'? cp->username : "UNKNOWN",
            0                      ? ""           : from,
            cp->progname[0] != '\0'? cp->progname : "UNKNOWN",
            errdescr_s[0]   != '\0'? ": " : "",
            errdescr_s);

    /* Should try to tell him something? */
    if (code != 0)
    {
        bzero(&hdr, sizeof(hdr));
        if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN)
            hdr.Type = h2l_u32(code);
        else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)
            hdr.Type = h2b_u32(code);
        else
            hdr.Type = code;

        fdio_send(cp->fhandle, &hdr, sizeof(hdr));
    }

    /* Do "preventive" closes */
    fdio_deregister_flush(cp->fhandle, 10/*!!!Should make an enum-constant*/);
    cp->fhandle = -1;
    cp->fd      = -1;

    RlsV4connSlot(cp2cd(cp));
}

// Should live in cxsd_hw.c
static cxsd_gchnid_t cpid2gcid(cxsd_cpntid_t  cpid)
{
  CxsdDbCpntInfo_t *cp;
  int               id;

    if (cpid < 0) return -1;
    while ((cpid & CXSD_DB_CPOINT_DIFF_MASK) != 0)
    {
        id = cpid & CXSD_DB_CHN_CPT_IDN_MASK;
        if (id < 0  ||  id >= cxsd_hw_cur_db->cpnts_used) return -1;
        cp  = cxsd_hw_cur_db->cpnts + id;
        cpid = cp->ref_n;
        if ((cpid & CXSD_DB_CPOINT_DIFF_MASK) == 0)
            cpid += cxsd_hw_devices[cp->devid].first;
    }
    /* Note: below this point "cpid" is actually a "gcid",
             since it had been unwound to a globalchan in the loop above. */
    if (cpid < 0  ||  cpid >= cxsd_hw_numchans) return -1;
    return cpid;
}

#define PUT_ERROR_CHUNK_REPLY(errcode)                             \
    do {                                                           \
        rpycsize = CXV4_CHUNK_CEIL(sizeof(CxV4Chunk));             \
        if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)    \
            {/*!!!*/}                                              \
        rpy = (void*)(cp->replybuf->data + replydatasize);         \
        bzero(rpy, sizeof(*rpy));                                  \
        rpy->OpCode   = clnt_u32(cp, CXC_CVT_TO_RPY(OpCode));      \
        rpy->ByteSize = clnt_u32(cp, rpycsize);                    \
        rpy->param1   = req->param1;                               \
        rpy->param2   = req->param2;                               \
                                                                   \
        replydatasize += rpycsize;                                 \
        rpycn++;                                                   \
    } while (0)

typedef size_t (put_reply_chunk_t)(v4clnt_t *cp, size_t replydatasize,
                                   int param1, int param2,
                                   cxsd_cpntid_t cpid, uint32 Seq,
                                   int info_int);

static int  SendAReply(v4clnt_t *cp, moninfo_t *mp,
                       put_reply_chunk_t chunk_maker,
                       int info_int)
{
  int        rpycn;           // RePlY chunk # (total # at end)
  size_t     rpycsize;        // RePlY chunk size in bytes
  size_t     replydatasize;

    InitReplyPacket(cp, CXT4_DATA_MONITOR, mp->Seq);
    rpycn         = 0;
    replydatasize = 0;

    rpycsize = chunk_maker(cp, replydatasize,
                           mp->param1, mp->param2,
                           mp->cpid,
                           mp->Seq,
                           info_int);
    if (rpycsize == 0)
#if 1
        return -1;
#else
        PUT_ERROR_CHUNK_REPLY(0);
#endif
    else
    {
        replydatasize += rpycsize;
        rpycn++;
    }
    cp->replybuf->DataSize  = clnt_u32(cp, replydatasize);
    cp->replybuf->NumChunks = clnt_u32(cp, rpycn);
    
    if (fdio_send(cp->fhandle, cp->replybuf, sizeof(CxV4Header) + replydatasize) != 0)
    {
        DisconnectClient(cp, errno, 0);
        return -1;
    }

    return 0;
}


/*
      PutDataChunkReply()
          Returns size of chunk, 0 upon error.
 */
static size_t PutDataChunkReply(v4clnt_t *cp, size_t replydatasize,
                                int param1, int param2,
                                cxsd_cpntid_t cpid, uint32 Seq,
                                int info_int)
{
  size_t     rpycsize;        // RePlY chunk size in bytes

  cxsd_gchnid_t          gcid;
  cxsd_hw_chan_t        *chn_p;
  cx_time_t             *tp;
  CxV4ResultChunk       *rslt;
  size_t     datasize;

    gcid = cpid2gcid(cpid);
    if (gcid <= 0  ||  gcid >= cxsd_hw_numchans) return 0;
    chn_p = cxsd_hw_channels + gcid;
    tp    = &(chn_p->timestamp);

#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
    datasize = chn_p->current_usize * chn_p->current_nelems;
#else
    datasize = chn_p->usize * chn_p->current_nelems;
#endif
    rpycsize = CXV4_CHUNK_CEIL(sizeof(*rslt) + datasize);
    if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)
        {/*!!!*/}

    rslt = (void*)(cp->replybuf->data + replydatasize);
    bzero(rslt, sizeof(*rslt));
    rslt->ck.OpCode   = clnt_u32(cp, info_int);
    rslt->ck.ByteSize = clnt_u32(cp, rpycsize);
    rslt->ck.param1   = param1;
    rslt->ck.param2   = param2;
    rslt->hwid               = clnt_u32(cp, cpid);
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
    rslt->dtype              = clnt_u32(cp, chn_p->current_dtype);
#else
    rslt->dtype              = clnt_u32(cp, chn_p->dtype);
#endif
    rslt->nelems             = clnt_u32(cp, chn_p->current_nelems);
    rslt->rflags             = clnt_u32(cp, chn_p->rflags);
    rslt->timestamp_nsec     = clnt_u32(cp, tp->nsec);
    rslt->timestamp_sec_lo32 = clnt_u32(cp, tp->sec);
    rslt->timestamp_sec_hi32 = clnt_u32(cp, 0); /*!!!*/
    rslt->Seq                = Seq;

    /*!!! do conversion */
    if (datasize != 0) memcpy(rslt->data, chn_p->current_val, datasize);
////fprintf(stderr, "%s() gcid=%d code=%08x (CUR=%08x,NEW=%08x) ts=%ld\n", __FUNCTION__, gcid, info_int, CXC_CURVAL, CXC_NEWVAL, cxsd_hw_channels[gcid].timestamp.sec);

    return rpycsize;
}

static size_t PutStrsChunkReply(v4clnt_t *cp, size_t replydatasize,
                                int param1, int param2,
                                cxsd_cpntid_t cpid, uint32 Seq,
                                int info_int)
{
  size_t                 rpycsize;        // RePlY chunk size in bytes
  CxV4StrsChunk         *strd;

  cxsd_gchnid_t          dummy_gcid;
  char                  *strings[8];
  int                    n;
  size_t                 strsize;
  size_t                 ofs;
  size_t                 size1;

    if (CxsdHwGetCpnProps(cpid, &dummy_gcid,
                          NULL, NULL, 0,
                          strings + 0, strings + 1,
                          strings + 2, strings + 3,
                          strings + 4, strings + 5,
                          strings + 6, strings + 7) < 0) return 0;

    /* Count size */
    for (n = 0, strsize = 0;
         n < countof(strings);
         n++)
        if (strings[n] != NULL) strsize += strlen(strings[n]) + 1;

    rpycsize = CXV4_CHUNK_CEIL(sizeof(*strd) + strsize);
    if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)
        /*!!!*/return 0;

    strd = (void*)(cp->replybuf->data + replydatasize);
    bzero(strd, sizeof(*strd));
    strd->ck.OpCode   = clnt_u32(cp, CXC_CVT_TO_RPY(CXC_STRS));
    strd->ck.ByteSize = clnt_u32(cp, rpycsize);
    strd->ck.param1   = param1;
    strd->ck.param2   = param2;
    for (n = 0, ofs = 0;
         n < countof(strings);
         n++)
    {
        if (strings[n] != NULL)
        {
            strd->offsets[n] = clnt_i32(cp, ofs);
            size1 = strlen(strings[n]) + 1;
            memcpy(strd->data + ofs, strings[n], size1);
            ofs += size1;
        }
        else
            strd->offsets[n] = clnt_i32(cp, -1);
    }

    return rpycsize;
}

static size_t PutRDsChunkReply (v4clnt_t *cp, size_t replydatasize,
                                int param1, int param2,
                                cxsd_cpntid_t cpid, uint32 Seq,
                                int info_int)
{
  size_t                 rpycsize;        // RePlY chunk size in bytes
  CxV4RDsChunk          *rdsl;

  cxsd_gchnid_t          dummy_gcid;
  int                    phys_count;
  double                 rds_buf[RDS_MAX_COUNT*2];
  int                    x;
  float64               *f64p;

    if (CxsdHwGetCpnProps(cpid, &dummy_gcid,
                          &phys_count, rds_buf, RDS_MAX_COUNT,
                          NULL, NULL, NULL, NULL,
                          NULL, NULL, NULL, NULL) < 0) return 0;
    rpycsize = CXV4_CHUNK_CEIL(sizeof(*rdsl) + sizeof(float64) * phys_count * 2);
    if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)
        /*!!!*/return 0;

    rdsl = (void*)(cp->replybuf->data + replydatasize);
    bzero(rdsl, sizeof(*rdsl));
    rdsl->ck.OpCode   = clnt_u32(cp, CXC_CVT_TO_RPY(CXC_RDS));
    rdsl->ck.ByteSize = clnt_u32(cp, rpycsize);
    rdsl->ck.param1   = param1;
    rdsl->ck.param2   = param2;
    rdsl->phys_count  = clnt_i32(cp, phys_count);

    for (x = 0, f64p = rdsl->data;  x < phys_count * 2;  x++, f64p++)
    {
        /*!!! do conversion */
        *f64p = rds_buf[x];
    }

    return rpycsize;
}

static size_t PutFrAgChunkReply(v4clnt_t *cp, size_t replydatasize,
                                int param1, int param2,
                                cxsd_cpntid_t cpid, uint32 Seq,
                                int info_int)
{
  size_t                 rpycsize;        // RePlY chunk size in bytes
  CxV4FreshAgeChunk     *frsh;
  cxsd_hw_chan_t        *chn_p;
  cxsd_gchnid_t          gcid;

    gcid = cpid2gcid(cpid);
    if (gcid <= 0  ||  gcid >= cxsd_hw_numchans) return 0;
    chn_p = cxsd_hw_channels + gcid;

    rpycsize = CXV4_CHUNK_CEIL(sizeof(*frsh));
    if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)
        {/*!!!*/}

    frsh = (void*)(cp->replybuf->data + replydatasize);
    bzero(frsh, sizeof(*frsh));
    frsh->ck.OpCode   = clnt_u32(cp, CXC_CVT_TO_RPY(CXC_FRH_AGE));
    frsh->ck.ByteSize = clnt_u32(cp, rpycsize);
    frsh->ck.param1   = param1;
    frsh->ck.param2   = param2;
    frsh->fresh_age_nsec     = clnt_u32(cp, chn_p->fresh_age.nsec);
    frsh->fresh_age_sec_lo32 = clnt_u32(cp, chn_p->fresh_age.sec);
    frsh->fresh_age_sec_hi32 = clnt_u32(cp, 0); /*!!!*/

    return rpycsize;
}

static size_t PutQuantChunkReply(v4clnt_t *cp, size_t replydatasize,
                                int param1, int param2,
                                cxsd_cpntid_t cpid, uint32 Seq,
                                int info_int)
{
  size_t                 rpycsize;        // RePlY chunk size in bytes
  CxV4QuantChunk        *qunt;
  cxsd_hw_chan_t        *chn_p;
  cxsd_gchnid_t          gcid;

  size_t                 q_dsize;

    gcid = cpid2gcid(cpid);
    if (gcid <= 0  ||  gcid >= cxsd_hw_numchans) return 0;
    chn_p = cxsd_hw_channels + gcid;

    q_dsize = sizeof_cxdtype(chn_p->q_dtype);
    if (q_dsize > sizeof(qunt->q_data))
    {
        logline(LOGF_HARDWARE, LOGL_ERR,
                "BUG BUG BUG %s(): cpid=%d/gcid=%d q_dtype=%d, sizeof_cxdtype()=%zd, >sizeof(q_data)=%zd",
                __FUNCTION__, cpid, gcid,
                chn_p->q_dtype, q_dsize, sizeof(qunt->q_data));
        return 0;
    }

    rpycsize = CXV4_CHUNK_CEIL(sizeof(*qunt));
    if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)
        {/*!!!*/}

    qunt = (void*)(cp->replybuf->data + replydatasize);
    bzero(qunt, sizeof(*qunt));
    qunt->ck.OpCode   = clnt_u32(cp, CXC_CVT_TO_RPY(CXC_QUANT));
    qunt->ck.ByteSize = clnt_u32(cp, rpycsize);
    qunt->ck.param1   = param1;
    qunt->ck.param2   = param2;
    qunt->q_dtype     = clnt_u32(cp, chn_p->q_dtype);
    /*!!! do conversion, in a CORRECT way (q_dsize bytes */
    memcpy(qunt->q_data, &(chn_p->q), sizeof(qunt->q_data));

    return rpycsize;
}

static void MonEvproc(int            uniq,
                      void          *privptr1,
                      cxsd_gchnid_t  gcid,
                      int            reason,
                      void          *privptr2)
{
  int        cd = ptr2lint(privptr1);
  int        id = ptr2lint(privptr2);
  v4clnt_t  *cp = AccessV4connSlot(cd);
  moninfo_t *mp = AccessMonSlot(id, cp);

  int        rpycn;           // RePlY chunk # (total # at end)
  size_t     rpycsize;        // RePlY chunk size in bytes
  size_t     replydatasize;

    if      (reason == CXSD_HW_CHAN_R_UPDATE)
    {
        if      (mp->cond == CX_MON_COND_ON_UPDATE)
        {
            if (SendAReply(cp, mp, PutDataChunkReply, CXC_NEWVAL) != 0) return;
            if (mp->being_reqd == 0)
            {
                mp->being_reqd++;
                CxsdHwDoIO(cp->ID, DRVA_READ | CXSD_HW_DRVA_IGNORE_UPD_CYCLE_FLAG,
                           1, &(mp->gcid),
                           NULL, NULL, NULL);
                mp->being_reqd--;
            }
        }
        else if (mp->cond == CX_MON_COND_ON_CYCLE)
        {
            mp->modified = 1;
            cp->per_cycle_monitors_some_modified = 1;
        }
    }
    else if (reason == CXSD_HW_CHAN_R_STATCHG)
    {
        SendAReply(cp, mp, PutDataChunkReply, CXC_CURVAL);
    }
    else if (reason == CXSD_HW_CHAN_R_STRSCHG)
    {
        SendAReply(cp, mp, PutStrsChunkReply, 0);
    }
    else if (reason == CXSD_HW_CHAN_R_RDSCHG)
    {
        SendAReply(cp, mp, PutRDsChunkReply, 0);
    }
    else if (reason == CXSD_HW_CHAN_R_FRESHCHG)
    {
        SendAReply(cp, mp, PutFrAgChunkReply, 0);
    }
    else if (reason == CXSD_HW_CHAN_R_QUANTCHG)
    {
        SendAReply(cp, mp, PutQuantChunkReply, 0);
    }
}

static int mon_eq_checker(moninfo_t *mp, void *privptr)
{
  moninfo_t *model = privptr;

    return
      mp->cpid   == model->cpid    &&
      mp->param1 == model->param1  &&
      mp->param2 == model->param2  &&
      mp->cond   == model->cond;
}

static int SetMonitor(v4clnt_t *cp, CxV4MonitorChunk *monr, uint32 Seq)
{
  moninfo_t  info;
  int        id;
  moninfo_t *mp;

    info.cpid   = host_i32(cp, monr->cpid);
    info.gcid   = cpid2gcid(info.cpid);
    info.param1 = monr->ck.param1;
    info.param2 = monr->ck.param2;
    info.cond   = (host_u32(cp, monr->dtype_and_cond) & 0x0000FF00) >> 8;
    if (info.gcid < 0)
        return -CXT4_EINVAL;

    /* a. Check validity of parameters */
    if (info.cond != CX_MON_COND_ON_UPDATE  &&
        info.cond != CX_MON_COND_ON_CYCLE)
        return -CXT4_EINVAL;

    /* b. Than check if it is already monitored */
    if (ForeachMonSlot(mon_eq_checker, &info, cp) >= 0) return -1;

    id = GetMonSlot(cp);
    if (id < 0) return -CXT4_ENOMEM;
    mp = AccessMonSlot(id, cp);

    /*  */
    if (REQ_ALL_MONITORS  ||  info.cond == CX_MON_COND_ON_CYCLE)
    {
        if (cp->per_cycle_monitors_allocd <= cp->per_cycle_monitors_count  &&
            GrowUnitsBuf(&(cp->per_cycle_monitors),
                         &(cp->per_cycle_monitors_allocd),
                         cp->per_cycle_monitors_allocd + 100,
                         sizeof(*(cp->per_cycle_monitors))) < 0)
            goto DO_CLEANUP;
        cp->per_cycle_monitors_count++;
        cp->per_cycle_monitors_needs_rebuild = 1;
    }

    /* Fill */
    mp->Seq    = Seq;
    mp->cpid   = info.cpid;
    mp->gcid   = info.gcid;
    mp->param1 = info.param1;
    mp->param2 = info.param2;
    mp->cond   = info.cond;

    /*  */
    if (CxsdHwAddChanEvproc(cp->ID, lint2ptr(cp2cd(cp)),
                            mp->gcid, MONITOR_EVMASK,
                            MonEvproc, lint2ptr(id)) < 0)
    {
        goto DO_CLEANUP;
    }

    /* Request initial read IMMEDIATELY, not delaying to "on cycle" */
    if (mp->cond == CX_MON_COND_ON_UPDATE)
    {
        mp->being_reqd++;
        CxsdHwDoIO(cp->ID, DRVA_READ | CXSD_HW_DRVA_IGNORE_UPD_CYCLE_FLAG,
                   1, &(mp->gcid),
                   NULL, NULL, NULL);
        mp->being_reqd--;
    }

    return 0;

 DO_CLEANUP:
    RlsMonSlot(id, cp);
    return -CXT4_ENOMEM;
}

static int DelMonitor(v4clnt_t *cp, CxV4MonitorChunk *monr, uint32 Seq)
{
  moninfo_t  info;
  int        id;

    info.cpid   = host_i32(cp, monr->cpid);
    info.param1 = monr->ck.param1;
    info.param2 = monr->ck.param2;
    info.cond   = (host_u32(cp, monr->dtype_and_cond) & 0x0000FF00) >> 8;

    /* a. Check validity of parameters */
    if (info.cond != CX_MON_COND_ON_UPDATE  &&
        info.cond != CX_MON_COND_ON_CYCLE)
        return -CXT4_EINVAL;

    /* b. Than check if it is monitored */
    id = ForeachMonSlot(mon_eq_checker, &info, cp);
    if (id < 0) return -1;

    /* Okay, stop monitoring */
    RlsMonSlot(id, cp);

    return 0;
}

static void ServeIORequest(v4clnt_t *cp, CxV4Header *hdr, size_t inpktsize)
{
  int        numrqs;          // # of chunks in request
  uint8     *ptr;
  CxV4Chunk *req;
  CxV4Chunk *rpy;
  int        reqcn;           // REQuest chunk # (loop ctl var)
  int        rpycn;           // RePlY chunk # (total # at end)
  size_t     reqcsize;        // REQuest chunk size in bytes
  size_t     rpycsize;        // RePlY chunk size in bytes
  size_t     reqlen;          // REQuest *data* LENgth in bytes
  size_t     replydatasize;
  uint32     OpCode;
  int        peekropc;        // cxc_PEEK Reply OPCode

  CxV4WriteChunk *wrrq;
  CxV4ReadChunk  *rdrq;
  cxsd_cpntid_t   cpid;
  cxsd_gchnid_t   gcid;
  cxdtype_t       dtype;
  int             nelems;
  void           *values;

  CxV4CpointPropsChunk  *prps;
  CxV4MonitorChunk      *monr;

    InitReplyPacket(cp, CXT4_DATA_IO, hdr->Seq);
    numrqs = host_u32(cp, hdr->NumChunks);

    /* Walk through the request */
    for (reqcn = 0, ptr = hdr->data, rpycn = 0, replydatasize = 0;
         reqcn < numrqs;
         reqcn++,   ptr += reqcsize)
    {
        req = (void *)ptr;
        OpCode   = host_u32(cp, req->OpCode);
        reqcsize = host_u32(cp, req->ByteSize);
        reqlen   = reqcsize - sizeof(CxV4Chunk);
        /*!!! Any checks? */

        switch (OpCode)
        {
            case CXC_PEEK:
                rdrq   = (void*)req;
                cpid   = host_i32(cp, rdrq->hwid);
                gcid   = cpid2gcid(cpid);
                ////fprintf(stderr, "PEEK %d\n", gcid);
                /* Send NEWVAL reply instead of CURVAL if
                   either an rw or _devstate channel */
                peekropc = gcid >= 0  &&  gcid < cxsd_hw_numchans  &&
#if 0
                           (cxsd_hw_channels[gcid].rw  ||
                            cxsd_hw_channels[gcid].is_internal
#else
                           (cxsd_hw_channels[gcid].is_internal  ||
                            cxsd_hw_channels[gcid].timestamp.sec !=
                                CX_TIME_SEC_NEVER_READ
                            &&
                            (cxsd_hw_channels[gcid].rw  ||
                             (cxsd_hw_channels[gcid].is_autoupdated       &&
                              cxsd_hw_channels[gcid].fresh_age.sec  == 0  &&
                              cxsd_hw_channels[gcid].fresh_age.nsec == 0)
                            )
  #if 0
                           cxsd_hw_channels[gcid].rw  ||
                            ||
                            (cxsd_hw_channels[gcid].is_autoupdated       &&
                             cxsd_hw_channels[gcid].fresh_age.sec  == 0  &&
                             cxsd_hw_channels[gcid].fresh_age.nsec == 0  &&
                             cxsd_hw_channels[gcid].timestamp.sec  != CX_TIME_SEC_NEVER_READ
                            )
  #endif

#endif
                                                              )? CXC_NEWVAL
                                                               : CXC_CURVAL;
                rpycsize = PutDataChunkReply(cp, replydatasize,
                                             req->param1, req->param2,
                                             gcid,
                                             hdr->Seq,
                                             peekropc);
                if (rpycsize == 0)
                    PUT_ERROR_CHUNK_REPLY(0);
                else
                {
                    replydatasize += rpycsize;
                    rpycn++;
                }
                break;

            case CXC_RQRDC:
                rdrq   = (void*)req;
                cpid   = host_i32(cp, rdrq->hwid);
                gcid   = cpid2gcid(cpid);
                if (gcid < 0)
                    PUT_ERROR_CHUNK_REPLY(CXT4_EINVAL);
                else
                    CxsdHwDoIO(cp->ID, DRVA_READ, 1, &gcid, NULL, NULL, NULL);
                break;

            case CXC_RQWRC:
                wrrq   = (void*)req;
                cpid   = host_i32(cp, wrrq->hwid);
                dtype  = host_i32(cp, wrrq->dtype);
                nelems = host_i32(cp, wrrq->nelems);
                values = wrrq->data;

                gcid   = cpid2gcid(cpid);
                if (gcid < 0)
                {
                    PUT_ERROR_CHUNK_REPLY(CXT4_EINVAL);
                    goto NEXT_CHUNK;
                }

                /*!!! At this point we can perform access control */

                /* Perform checks */
                /*!!!dtype*/
                if (nelems < 0  ||  nelems > 10000000/*!!!arbitrary value*/)
                {
                    PUT_ERROR_CHUNK_REPLY(CXT4_EINVAL);
                    goto NEXT_CHUNK;
                }

                /* Convert data */
                /*!!! "if (cp->endianness != MY_ENDIANNESS  &&  nelems > 0) {do_convert at wrrq->data}"  */

                CxsdHwDoIO(cp->ID, DRVA_WRITE, 1, &gcid, &dtype, &nelems, &values);
                break;

            case CXC_RESOLVE:
                if (req->data[reqlen - 1] == '\0'  &&
                    (cpid =
                     CxsdHwResolveChan(req->data, &gcid,
                                       NULL, NULL, 0,
                                       NULL, NULL, NULL, NULL,
                                       NULL, NULL, NULL, NULL)
                    ) >= 0)
                {
                    /*!!! Note: should put parameters-chunk FIRST,
                     for its data to be already cached in cxlib
                     upon arrival of RESOLVE-rpy chunk */

                    rpycsize = PutStrsChunkReply(cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }
                    rpycsize = PutRDsChunkReply (cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }
                    rpycsize = PutFrAgChunkReply(cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }
                    rpycsize = PutQuantChunkReply(cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }

                    rpycsize = CXV4_CHUNK_CEIL(sizeof(CxV4CpointPropsChunk));
                    if (GrowReplyPacket(cp, replydatasize + rpycsize) != 0)
                        {/*!!!*/}
                    rpy = (void*)(cp->replybuf->data + replydatasize);
                    bzero(rpy, rpycsize);
                    rpy->OpCode   = clnt_u32(cp, CXC_CVT_TO_RPY(OpCode));
                    rpy->ByteSize = clnt_u32(cp, rpycsize);
                    rpy->param1   = req->param1;
                    rpy->param2   = req->param2;

                    prps = (void*)(rpy);
                    prps->cpid   = clnt_u32(cp, cpid);
                    prps->hwid   = clnt_u32(cp, gcid);
                    prps->rw     = clnt_u32(cp, cxsd_hw_channels[gcid].rw);
                    prps->dtype  = clnt_u32(cp, cxsd_hw_channels[gcid].dtype);
                    prps->nelems = clnt_u32(cp, cxsd_hw_channels[gcid].max_nelems);

                    replydatasize += rpycsize;
                    rpycn++;
                }
                else
                {
                    PUT_ERROR_CHUNK_REPLY(CXT4_EINVAL);
                }
                break;

            case CXC_SETMON:
                monr = (void*)req;
                cpid = monr->cpid;
                ////fprintf(stderr, "SETMON %d\n", monr->cpid);
                if (SetMonitor(cp, monr, hdr->Seq) == 0)
                {
                    rpycsize = PutRDsChunkReply (cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }
                    rpycsize = PutFrAgChunkReply(cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }
                    rpycsize = PutQuantChunkReply(cp, replydatasize,
                                                 req->param1, req->param2,
                                                 cpid, hdr->Seq, 0);
                    if (rpycsize != 0)
                    {
                        replydatasize += rpycsize;
                        rpycn++;
                    }
                }
                else
                {
                    /*!!! USE RETURN CODE!!!*/
                }
                break;

            case CXC_DELMON:
                monr = (void*)req;
                DelMonitor(cp, monr, hdr->Seq); /*!!! USE RETURN CODE!!!*/
                break;

            default:
                PUT_ERROR_CHUNK_REPLY(CXT4_EBADRQC);
        }

 NEXT_CHUNK:;
    }

    cp->replybuf->DataSize  = clnt_u32(cp, replydatasize);
    cp->replybuf->NumChunks = clnt_u32(cp, rpycn);

    if (fdio_send(cp->fhandle, cp->replybuf, sizeof(CxV4Header) + replydatasize) != 0)
        DisconnectClient(cp, errno, 0);
}

static void HandleClientRequest(v4clnt_t *cp, CxV4Header *hdr, size_t inpktsize)
{
  struct
  {
      CxV4Header    hdr;
  } pkt;

    switch (host_u32(cp, hdr->Type))
    {
        case CXT4_LOGOUT:
            DisconnectClient(cp, 0, 0);
            break;

        case CXT4_PING:
            break;

        case CXT4_PONG:
            break;

        case CXT4_DATA_IO:
            ServeIORequest(cp, hdr, inpktsize);
            break;

        default:
            bzero(&pkt, sizeof(pkt));
            pkt.hdr.Type = clnt_u32(cp, CXT4_EBADRQC);
            pkt.hdr.ID   = clnt_u32(cp, cp->ID);
            pkt.hdr.Seq  = hdr->Seq;  /* Direct copy -- no need for (double) conversion */
            pkt.hdr.var1 = hdr->Type; /* The same */
            if (fdio_send(cp->fhandle, &pkt, sizeof(pkt)) < 0)
            {
                DisconnectClient(cp, fdio_lasterr(cp->fhandle)/*!!!errno?*/, 0);
                return;
            }
    }
}

static void HandleClientConnect(v4clnt_t *cp, CxV4Header *hdr, size_t inpktsize)
{
  uint32          Type;
  uint32          exp_Type     = 0;
  const char     *exp_TypeName = NULL;
  uint32          cln_ver;
  CxV4LoginChunk *lr = (CxV4LoginChunk *)hdr->data;
  const char     *from;
  
  struct
  {
      CxV4Header    hdr;
  } pkt;
  
    Type = hdr->Type;
    if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN) Type = l2h_u32(Type);
    else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)    Type = b2h_u32(Type);

    switch (cp->state)
    {
        case CS_OPEN_ENDIANID:
            exp_Type     =                CXT4_ENDIANID;
            exp_TypeName = __CX_STRINGIZE(CXT4_ENDIANID);
            break;

        case CS_OPEN_LOGIN:
            exp_Type     =                CXT4_LOGIN;
            exp_TypeName = __CX_STRINGIZE(CXT4_LOGIN);
            break;
    }

    if (Type != exp_Type)
    {
        logline(LOGF_ACCESS, LOGL_ERR, "%s::%s: packet.Type mismatch: received 0x%08x instead of %s=0x%08x",
                __FILE__, __FUNCTION__, Type, exp_TypeName, exp_Type);
        DisconnectClient(cp, ERR_HSHAKE, CXT4_EINVAL);
        return;
    }
    
    switch (cp->state)
    {
        case CS_OPEN_ENDIANID:
            /* First, determine endianness... */
            if      (l2h_u32(hdr->var1) == CXV4_VAR1_ENDIANNESS_SIG)
                cp->endianness = FDIO_LEN_LITTLE_ENDIAN;
            else if (b2h_u32(hdr->var1) == CXV4_VAR1_ENDIANNESS_SIG)
                cp->endianness = FDIO_LEN_BIG_ENDIAN;
            else
            {
                logline(LOGF_ACCESS, LOGL_ERR, "%s::%s: unrecognizable endianness-signature 0x%08x",
                        __FILE__, __FUNCTION__, hdr->var1);
                DisconnectClient(cp, ERR_HSHAKE, CXT4_EINVAL);
                return;
            }

            /* ...and update fdiolib's info */
            fdio_set_len_endian(cp->fhandle, cp->endianness);
            fdio_set_maxpktsize(cp->fhandle, CX_V4_MAX_PKTSIZE);

            /* Next, check version */
            cln_ver = host_u32(cp, hdr->var2);
            if (!CX_VERSION_IS_COMPATIBLE(cln_ver, CX_V4_PROTO_VERSION))
            {
                logline(LOGF_ACCESS, LOGL_ERR, "%s::%s: client's protocol version %d.%d(%d) is incompatible with our %d.%d",
                        __FILE__, __FUNCTION__,
                        CX_MAJOR_OF(cln_ver), CX_MINOR_OF(cln_ver), cln_ver,
                        CX_V4_PROTO_VERSION_MAJOR, CX_V4_PROTO_VERSION_MINOR);
                DisconnectClient(cp, ERR_HSHAKE, CXT4_DIFFPROTO);
                return;
            }

            /* Okay, let's command it to send login info... */
            bzero(&pkt, sizeof(pkt));
            pkt.hdr.Type = clnt_u32(cp, CXT4_LOGIN);
            pkt.hdr.ID   = clnt_u32(cp, cp->ID);
            pkt.hdr.Seq  = hdr->Seq; /* Direct copy -- no need for (double) conversion */
            pkt.hdr.var2 = clnt_u32(cp, CX_V4_PROTO_VERSION);
            if (fdio_send(cp->fhandle, &pkt, sizeof(pkt)) < 0)
            {
                DisconnectClient(cp, fdio_lasterr(cp->fhandle)/*!!!errno?*/, 0);
                return;
            }

            /* ...and promote state */
            cp->state = CS_OPEN_LOGIN;
            break;

        case CS_OPEN_LOGIN:
            /* Is packet size right? */
            if (inpktsize != sizeof(CxV4Header) + sizeof(CxV4LoginChunk))
            {
                logline(LOGF_ACCESS, LOGL_ERR, "%s::%s: inpktsize=%zu, while %zu expected",
                        __FILE__, __FUNCTION__,
                        inpktsize,
                        sizeof(CxV4Header) + sizeof(CxV4LoginChunk));
                DisconnectClient(cp, ERR_HSHAKE, CXT4_EINVAL);
                return;
            }

            sl_deq_tout(cp->clp_tid);
            cp->clp_tid = -1;

            /* Remember login info */
            sanitize_strzcpy(cp->progname, sizeof(cp->progname),
                             lr->progname, sizeof(lr->progname));
            sanitize_strzcpy(cp->username, sizeof(cp->username),
                             lr->username, sizeof(lr->username));

            /* Make client "logged-in" */
            bzero(&pkt, sizeof(pkt));
            pkt.hdr.Type = clnt_u32(cp, CXT4_ACCESS_GRANTED);
            pkt.hdr.ID   = clnt_u32(cp, cp->ID);
            pkt.hdr.Seq  = hdr->Seq; /* Direct copy -- no need for (double) conversion */
            pkt.hdr.var2 = clnt_u32(cp, CX_V4_PROTO_VERSION);
            if (fdio_send(cp->fhandle, &pkt, sizeof(pkt)) < 0)
            {
                DisconnectClient(cp, fdio_lasterr(cp->fhandle)/*!!!errno?*/, 0);
                return;
            }


            if (cp->ip == 0) from = "-";
            else
            {
                from = inet_ntoa(((struct sockaddr_in *)&(cp->addr))->sin_addr);
            }

            logline(LOGF_ACCESS, LOGL_NOTICE, "%s [%d]:fd=%d:h=%d %s@%s:%s (uid=%d, pid,ppid=%d,%d)",
                    "connect",
                    cp2cd(cp), cp->fd, cp->fhandle,
                    cp->username[0] != '\0'? cp->username : "UNKNOWN",
                    0                      ? ""           : from,
                    cp->progname[0] != '\0'? cp->progname : "UNKNOWN",
                    clnt_u32(cp, lr->ck.rs3),
                    clnt_u32(cp, lr->ck.rs1),
                    clnt_u32(cp, lr->ck.rs2));

            cp->state = CS_READY;
            break;
    }
}

static void InteractWithClient(int uniq, void *unsdptr,
                               fdio_handle_t handle, int reason,
                               void *inpkt, size_t inpktsize,
                               void *privptr)
{
  int         cd   = ptr2lint(privptr);
  v4clnt_t   *cp   = AccessV4connSlot(cd);
  CxV4Header *hdr  = inpkt;

    switch (reason)
    {
        case FDIO_R_DATA:
            if (cp->state > CS_USABLE) HandleClientRequest(cp, hdr, inpktsize);
            else                       HandleClientConnect(cp, hdr, inpktsize);
            break;
            
        case FDIO_R_CLOSED:
            DisconnectClient(cp, ERR_CLOSED,           0);
            break;

        case FDIO_R_IOERR:
            DisconnectClient(cp, fdio_lasterr(handle), 0);
            break;

        case FDIO_R_PROTOERR:
            /* In fact, as of 10.09.2009, FDIO_R_PROTOERR is unused by fdiolib */
            DisconnectClient(cp, fdio_lasterr(handle), CXT4_EBADRQC);
            break;

        case FDIO_R_INPKT2BIG:
            DisconnectClient(cp, ERR_IN2BIG,           CXT4_EPKT2BIG);
            break;

        case FDIO_R_ENOMEM:
            DisconnectClient(cp, ENOMEM,               CXT4_ENOMEM);
            break;

        default:
            logline(LOGF_ACCESS, LOGL_ERR, "%s::%s: unknown fdiolib reason %d",
                         __FILE__, __FUNCTION__, reason);
            DisconnectClient(cp, ERR_INTRNL,           CXT4_EINTERNAL);
    }
}

//////////////////////////////////////////////////////////////////////

static void WipeUnconnectedTimed(int uniq, void *unsdptr, sl_tid_t tid, void *privptr)
{
  int       cd = ptr2lint(privptr);
  v4clnt_t *cp = AccessV4connSlot(cd);

    cp->clp_tid = -1;
    DisconnectClient(cp, ERR_TIMOUT, CXT4_TIMEOUT);
}

static void RefuseConnection(int s, int cause)
{
  CxV4Header  hdr;
    
    bzero(&hdr, sizeof(hdr));
    hdr.Type = cause;
    hdr.var2 = CX_V4_PROTO_VERSION;
    write(s, &hdr, sizeof(hdr));
}

static void AcceptCXv4Connection(int uniq, void *unsdptr,
                                 fdio_handle_t handle, int reason,
                                 void *inpkt, size_t inpktsize,
                                 void *privptr)
{
  int                 s;
  struct sockaddr     addr;
  socklen_t           addrlen;

  int                 cd;
  v4clnt_t           *cp;
  fdio_handle_t       fhandle;
  
    if (reason != FDIO_R_ACCEPT)
    {
        logline(LOGF_SYSTEM, LOGL_ERR, "%s::%s(handle=%d): reason=%d, !=FDIO_R_ACCEPT=%d",
                __FILE__, __FUNCTION__, handle, reason, FDIO_R_ACCEPT);
        return;
    }

    /* First, accept() the connection */
    addrlen = sizeof(addr);
    s = fdio_accept(handle, &addr, &addrlen);

    /* Is anything wrong? */
    if (s < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): fdio_accept(%s)",
                __FUNCTION__,
                handle == unix_serv_handle? "unix_serv_handle" : "inet_serv_handle");
        return;
    }

    /* Instantly switch it to non-blocking mode */
    if (set_fd_flags(s, O_NONBLOCK, 1) < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_ERR,
                "%s(): set_fd_flags(O_NONBLOCK)",
                __FUNCTION__);
        close(s);
        return;
    }

    /* Is it allowed to log in? */
    if (0)
    {
        RefuseConnection(s, CXT4_ACCESS_DENIED);
        close(s);
        return;
    }

    /* Get a connection slot... */
    if ((cd = GetV4connSlot()) < 0)
    {
        RefuseConnection(s, CXT4_MANY_CLIENTS);
        close(s);
        return;
    }

    /* ...and fill it with data */
    cp = AccessV4connSlot(cd);
    cp->state = CS_OPEN_ENDIANID;
    cp->fd    = s;
    cp->ID    = CxsdHwCreateClientID();
    cp->when  = time(NULL);
    cp->ip    = handle == unix_serv_handle ?  0
                                           :  ntohl(inet_addr(inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr)));
    cp->addr  = addr;
////fprintf(stderr, "%s() s=%d ID=%08x\n", __FUNCTION__, s, cp->ID);

    cp->replybufsize = sizeof(CxV4Header);
    cp->replybuf     = malloc(cp->replybufsize);
    if (cp->replybuf == NULL)
    {
        RefuseConnection(s, CXT4_ENOMEM);
        RlsV4connSlot(cd);
        return;
    }
    
    /* Okay, let's obtain an fdio slot... */
    cp->fhandle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                   s, FDIO_STREAM,
                                   InteractWithClient, lint2ptr(cd),
                                   sizeof(CxV4Header),
                                   sizeof(CxV4Header),
                                   FDIO_OFFSET_OF(CxV4Header, DataSize),
                                   FDIO_SIZEOF   (CxV4Header, DataSize),
                                   FDIO_LEN_LITTLE_ENDIAN,
                                   1, sizeof(CxV4Header));
    if (cp->fhandle < 0)
    {
        RefuseConnection(s, CXT4_MANY_CLIENTS);
        RlsV4connSlot(cd);
        return;
    }
    fdio_set_maxsbuf(cp->fhandle, CX_V4_MAX_PKTSIZE * 10);

    cp->clp_tid = sl_enq_tout_after(0, NULL, /*!!!uniq*/
                                    MAX_UNCONNECTED_SECONDS * 1000000,
                                    WipeUnconnectedTimed, lint2ptr(cd));
    if (cp->clp_tid < 0)
    {
        RefuseConnection(s, CXT4_MANY_CLIENTS);
        RlsV4connSlot(cd);
        return;
    }
}

//////////////////////////////////////////////////////////////////////

#define CHECK_R(msg)                                                  \
    do {                                                              \
        if (r != 0)                                                   \
        {                                                             \
            logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_EMERG, "%s::%s: %s", \
                    __FILE__, __FUNCTION__, msg);                     \
            return -1;                                                \
        }                                                             \
    } while (0)

static int  cx_init_f (int server_instance)
{
  int  r;
    
    my_server_instance = server_instance;
  
    if (CreateServSockets() != 0)
    {
        DestroyServSockets(0);
        return -1;
    }

    inet_serv_handle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                        inet_serv_socket,      FDIO_LISTEN,
                                        AcceptCXv4Connection, NULL,
                                        0, 0, 0, 0, FDIO_LEN_LITTLE_ENDIAN, 0, 0);
    r = inet_serv_handle < 0;
    CHECK_R("fdio_register_fd(inet_serv_socket)");
    unix_serv_handle = fdio_register_fd(0, NULL, /*!!!uniq*/
                                        unix_serv_socket,      FDIO_LISTEN,
                                        AcceptCXv4Connection, NULL,
                                        0, 0, 0, 0, FDIO_LEN_LITTLE_ENDIAN, 0, 0);
    r = unix_serv_handle < 0;
    CHECK_R("fdio_register_fd(unix_serv_socket)");

    if (TryToBecomeGuru() >= 0)
        logline(LOGF_SYSTEM,              LOGL_NOTICE, "became GURU");
    else
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_NOTICE, "GURU unsuccessful");
        if (ConnectToGuru() >= 0)
            logline(LOGF_SYSTEM,              LOGL_NOTICE, "connecting to guru");
        else
            logline(LOGF_SYSTEM | LOGF_ERRNO, LOGL_NOTICE, "guru-connect unsuccessful");
            
    }
    sl_enq_tout_after(0/*!!!uniq*/, NULL,
                      GURU_TAKEOVER_HEARTBEAT_SECONDS * 1000000,
                      GuruTakeoverHbtProc, NULL);
    
    return 0;
}

static int TermAdvisorIterator(advisor_t *jp, void *privptr __attribute__((unused)))
{
  int  jd = jp - advisors_list; // "jp2jd()"

    if (jp->instance >= 0) ForgetAdvisorDb(jp->instance);
    RlsAdvisorSlot(jd);

    return 0;
}
static int TermV4connIterator (v4clnt_t  *cp, void *privptr __attribute__((unused)))
{
    DisconnectClient(cp, ERR_KILLED, CXT4_DISCONNECT);

    return 0;
}
static void cx_term_f (void)
{
    sl_deq_tout(guru_takeover_hbt_tid); guru_takeover_hbt_tid = -1;
    DestroyServSockets(1);
    DestroyGuruSockets(1); 
    DestroyInfoSockets(1);
    ForeachAdvisorSlot(TermAdvisorIterator, NULL); DestroyAdvisorSlotArray();
    ForeachV4connSlot (TermV4connIterator,  NULL); DestroyV4connSlotArray ();
}

static int IfIsPerCycleMonRecordIt(moninfo_t *mp, void *privptr)
{
  cxsd_gchnid_t **wp_p = privptr;

    if (REQ_ALL_MONITORS  ||  mp->cond == CX_MON_COND_ON_CYCLE)
    {
        **wp_p = mp->gcid;
        (*wp_p)++;
    }

    return 0;
}
static int RequestSubscription(v4clnt_t *cp, void *privptr __attribute__((unused)))
{
  cxsd_gchnid_t *wp;

    if (cp->per_cycle_monitors_count == 0) return 0;

    if (cp->per_cycle_monitors_needs_rebuild)
    {
        wp = cp->per_cycle_monitors;
        ForeachMonSlot(IfIsPerCycleMonRecordIt, &wp, cp);
        cp->per_cycle_monitors_needs_rebuild = 0;
    }

    CxsdHwDoIO(cp->ID, DRVA_READ,
               cp->per_cycle_monitors_count, cp->per_cycle_monitors,
               NULL, NULL, NULL);

    return 0;
}
static void cx_begin_c(void)
{
    ForeachV4connSlot(RequestSubscription, NULL);
}

static int IfMonModifiedPutIt(moninfo_t *mp, void *privptr)
{
  v4clnt_t  *cp = privptr;
  size_t     rpycsize;        // RePlY chunk size in bytes

    if (!(mp->modified)) return 0;

    mp->modified = 0;

    rpycsize = PutDataChunkReply(cp, cp->replybuf->DataSize,
                                 mp->param1, mp->param2,
                                 mp->gcid,
                                 mp->Seq, CXC_NEWVAL);
    if (rpycsize != 0)
    {
        cp->replybuf->DataSize += rpycsize;
        cp->replybuf->NumChunks++;
    }

    return 0;
}
static int SendEndC(v4clnt_t *cp, void *privptr __attribute__((unused)))
{
  CxV4Header  hdr;
  int         r;

  size_t      replydatasize;

    /* Don't mess with not-yet-handshaken clients */
    if (cp->state < CS_USABLE) return 0;

    if (cp->per_cycle_monitors_some_modified)
    {
        InitReplyPacket(cp, CXT4_DATA_IO, 0);

        ForeachMonSlot(IfMonModifiedPutIt, cp, cp);

        replydatasize = cp->replybuf->DataSize;
        cp->replybuf->DataSize  = clnt_u32(cp, cp->replybuf->DataSize);
        cp->replybuf->NumChunks = clnt_u32(cp, cp->replybuf->NumChunks);

        if (fdio_send(cp->fhandle, cp->replybuf, sizeof(CxV4Header) + replydatasize) != 0)
        {
            DisconnectClient(cp, errno, 0);
            return 0;
        }
    }
    
    bzero(&hdr, sizeof(hdr));
    if      (cp->endianness == FDIO_LEN_LITTLE_ENDIAN)
        hdr.Type = h2l_u32(CXT4_END_OF_CYCLE);
    else if (cp->endianness == FDIO_LEN_BIG_ENDIAN)
        hdr.Type = h2b_u32(CXT4_END_OF_CYCLE);
    else
        hdr.Type = CXT4_END_OF_CYCLE;

    r = fdio_send(cp->fhandle, &hdr, sizeof(hdr));
    if (r != 0)
        DisconnectClient(cp, errno, 0);

    return 0;
}
static void cx_end_c  (void)
{
    ForeachV4connSlot(SendEndC, NULL);
}


static int  cx_init_m(void);
static void cx_term_m(void);

DEFINE_CXSD_FRONTEND(cx, "CX frontend",
                     cx_init_m, cx_term_m,
                     cx_init_f, cx_term_f,
                     cx_begin_c, cx_end_c);

static int  cx_init_m(void)
{
    return CxsdRegisterFrontend(&(CXSD_FRONTEND_MODREC_NAME(cx)));
}

static void cx_term_m(void)
{
    CxsdDeregisterFrontend(&(CXSD_FRONTEND_MODREC_NAME(cx)));
}
