#ifndef __CXLIB_H
#define __CXLIB_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <unistd.h>

#include "cx.h"


/*==== Some additional error codes -- all are negative =============*/
#define CEINTERNAL     (-1)   /* Internal cxlib problem or data corrupt */
#define CENOHOST       (-2)   /* Unknown host */
#define CEUNKNOWN      (-3)   /* Unknown response error code */
#define CETIMEOUT      (-4)   /* Handshake timed out */
#define CEMANYCLIENTS  (-5)   /* Too many clients */
#define CEACCESS       (-6)   /* Access to server denied */
#define CEBADRQC       (-7)   /* Bad request code */
#define CEPKT2BIGSRV   (-8)   /* Packet is too big for server */
#define CEINVCHAN      (-9)   /* Invalid channel number */
#define CESRVNOMEM     (-10)  /* Server ran out of memory */
#define CEMCONN        (-11)  /* Too many connections */
#define CERUNNING      (-12)  /* Request was successfully sent */
#define CECLOSED       (-13)  /* Connection is closed */
#define CEBADC         (-14)  /* Bad connection number */
#define CEBUSY         (-15)  /* Connection is busy */
#define CESOCKCLOSED   (-16)  /* Connection socket is closed by server */
#define CEKILLED       (-17)  /* Connection is killed by server */
#define CERCV2BIG      (-18)  /* Received packet is too big */
#define CESUBSSET      (-19)  /* Set request in subscription */
#define CEINVAL        (-20)  /* Invalid argument */
#define CEREQ2BIG      (-21)  /* Request packet will be too big */
#define CEFORKERR      (-22)  /* One of the forks has error code set */
#define CESMALLBUF     (-23)  /* Client-supplied buffer is too small */
#define CEINVCONN      (-24)  /* Invalid connection type */
#define CESRVINTERNAL  (-25)  /* Internal server error */
#define CEWRONGUSAGE   (-26)  /* Wrong usage */
#define CEDIFFPROTO    (-27)  /* Server speaks different CX-protocol version */
#define CESRVSTIMEOUT  (-28)  /* Server-side handshake timeout */

/*==== Cx Asynchronous call Reasons ================================*/
/* Note: when adding/changing codes, the
   lib/cxlib/cxlib_utils.c::_cx_carlist[] must be updated coherently. */
enum {
         CAR_CONNECT = 0,     /* Connection succeeded */
         CAR_CONNFAIL,        /* Connection failed */
         CAR_ERRCLOSE,        /* Connection was closed on error */
         CAR_CYCLE,           /* Server cycle notification had arrived */
         CAR_RSRVD4,
         CAR_MUSTER,          /* List of channels had probably changed; may re-try resolving */
         
         CAR_NEWDATA = 100,   /* Data chunk had arrived */
         CAR_RSLV_RESULT,
         CAR_FRESH_AGE,
         CAR_STRS,
         CAR_RDS,
         CAR_QUANT,
         CAR_RANGE,

         CAR_ECHO = 200,      /* Echo packet */
         CAR_KILLED,          /* Connection was killed by server */

         CAR_SRCH_RESULT = 500 /* Result of broadcast search for channel by name */
     };


char *cx_strerror(int errnum);
void  cx_perror  (const char *s);
void  cx_perror2 (const char *s, const char *argv0);

char *cx_strreason(int reason);

char *cx_strrflag_short(int shift);
char *cx_strrflag_long (int shift);

int   cx_parse_chanref(const char *spec,
                       char *srvnamebuf, size_t srvnamebufsize,
                       chanaddr_t  *chan_n,
                       char **channame_p);

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int        hwid;
    int        param1;
    int        param2;
    int        is_update;
    cxdtype_t  dtype;
    int        nelems;
    rflags_t   rflags;
    cx_time_t  timestamp;
    void      *data;
} cx_newval_info_t;

typedef struct
{
    int        hwid;
    int        param1;
    int        param2;
    // Info props
    int        rw;     // 0 -- readonly, 1 -- read/write
    cxdtype_t  dtype;  // cxdtype_t, with 3 high bytes of 0s
    int        nelems; // Max # of units; ==1 for scalar channels
} cx_rslv_info_t;

typedef struct
{
    int        hwid;
    int        param1;
    int        param2;
    cx_time_t  fresh_age;
} cx_fresh_age_info_t;

typedef struct
{
    int        param1;
    int        param2;
    const char*strings[8];
} cx_strs_info_t;

typedef struct
{
    int        hwid;
    int        param1;
    int        param2;
    int        phys_count;
    double    *rds;
} cx_rds_info_t;

typedef struct
{
    int        hwid;
    int        param1;
    int        param2;
    cxdtype_t  q_dtype;
    CxAnyVal_t q;
} cx_quant_info_t;

typedef struct
{
    int        hwid;
    int        param1;
    int        param2;
    cxdtype_t  range_dtype;
    CxAnyVal_t range[2];
} cx_range_info_t;

typedef struct
{
    int        param1;
    int        param2;
    const char*name;

    const char*srv_addr;
    int        srv_n;
} cx_srch_info_t;


typedef void (*cx_notifier_t)(int uniq, void *privptr1,
                              int cd, int reason, const void *info,
                              void *privptr2);

int  cx_open  (int            uniq,     void *privptr1,
               const char    *spec,     int flags,
               const char    *argv0,    const char *username,
               cx_notifier_t  notifier, void *privptr2);
int  cx_close (int  cd);


int  cx_begin (int cd);
int  cx_run   (int cd);


int  cx_rslv  (int cd, const char *name,      int  param1,  int  param2);

int  cx_setmon(int cd, int count, int *hwids, int *param1s, int *param2s,
               int on_update);
int  cx_delmon(int cd, int count, int *hwids, int *param1s, int *param2s,
               int on_update);
int  cx_rd_cur(int cd, int count, int *hwids, int *param1s, int *param2s);
int  cx_rq_rd (int cd, int count, int *hwids, int *param1s, int *param2s);
int  cx_rq_wr (int cd, int count, int *hwids, int *param1s, int *param2s,
               cxdtype_t *dtypes, int *nelems, void **values);


int  cx_seeker(int            uniq,     void *privptr1,
               const char    *argv0,    const char *username,
               cx_notifier_t  notifier, void *privptr2);

int  cx_srch  (int cd, const char *name,      int  param1,  int  param2);


void cx_do_cleanup(int uniq);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXLIB_H */
