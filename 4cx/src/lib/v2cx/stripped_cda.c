#include <sys/types.h>
#include <sys/stat.h>

#include "v2cda_renames.h"


// Handles' encoding scheme

/*
 *
 *  Handle internal format:
 *      high-order 1 byte -- server id
 *      lower 3 bytes     -- channel id inside that server
 */

static inline cda_objhandle_t encode_chanhandle(cda_serverid_t sid, int objofs)
{
    return (sid << 24)  + objofs;
}

static inline void decode_objhandle(cda_objhandle_t handle,
                                    cda_serverid_t *sid_p, int *objofs_p)
{
    *sid_p    = (handle >> 24) & 0xFF;
    *objofs_p = handle & 0x00FFFFFF;
}

//

typedef struct
{
    int             in_use;
    char            srvrspec[200]; /*!!! Replace '200' with what? */
    cda_serverid_t  main_sid;
    int             sid_n;
    int             auxsidscount;
    cda_serverid_t *auxsidslist;
    physprops_t    *phprops;
    int             phpropscount;
    physinfodb_rec_t *global_physinfo_db;
} serverinfo_t;

enum
{
    V2_MAX_CONNS = 1000, // An arbitrary limit
    V2_ALLOC_INC = 2
};

static serverinfo_t **servers_list        = NULL;
static int            servers_list_allocd = 0;

// GetV2sidSlot()
GENERIC_SLOTARRAY_DEFINE_GROWFIXELEM(static, V2sid, serverinfo_t,
                                     servers, in_use, 0, 1,
                                     0, V2_ALLOC_INC, V2_MAX_CONNS,
                                     , , void)

static void RlsV2sidSlot(int sid)
{
  serverinfo_t *si = AccessV2sidSlot(sid);

    safe_free(si->auxsidslist);
    safe_free(si->phprops);
    /*!!! For now -- we do nothing */
}

/*
 *  _CdaCheckSid
 *      Check if the server id is valid and represents an active record
 */

int _CdaCheckSid(cda_serverid_t sid)
{
    if (sid >= 0                                   &&
        sid < (cda_serverid_t)servers_list_allocd  &&
        AccessV2sidSlot(sid)->in_use != 0)
        return 0;

    errno = EBADF;
    return -1;
}

//

cda_serverid_t  cda_new_server(const char *spec,
                               cda_eventp_t event_processer, void *privptr,
                               cda_conntype_t conntype)
{
  cda_serverid_t        id;
  serverinfo_t         *si;
  char                 *comma_p;

    if (spec == NULL) spec = "";

    /* Allocate a new connection slot */
    if ((int)(id = GetV2sidSlot()) < 0) return CDA_SERVERID_ERROR;
    si = AccessV2sidSlot(id);
    
    /* Initialize and fill in the info */
    bzero(si, sizeof(*si));

    strzcpy(si->srvrspec, spec, sizeof(si->srvrspec));
    if ((comma_p = strchr(si->srvrspec, ',')) != NULL)
        *comma_p++ = '\0';

    si->in_use          = 1;

    return id;
}

cda_serverid_t  cda_add_auxsid(cda_serverid_t  sid, const char *auxspec,
                               cda_eventp_t event_processer, void *privptr)
{
  serverinfo_t   *si = AccessV2sidSlot(sid);
  int             x;
  cda_serverid_t  auxsid;
  cda_serverid_t *tmpsidslist;

  physinfodb_rec_t *rec;

    if (_CdaCheckSid(sid) != 0) return CDA_SERVERID_ERROR;

    /* Step 1. The same as base? */
    if (strcasecmp(si->srvrspec, auxspec) == 0)
        return sid;

    /* Step 2. If not, try to find among already present auxsids */
    for (x = 0;  x < si->auxsidscount;  x++)
        if (_CdaCheckSid(si->auxsidslist[x]) == 0  &&
            strcasecmp(AccessV2sidSlot(si->auxsidslist[x])->srvrspec, auxspec) == 0)
            return si->auxsidslist[x];

    /* Step 3. None found?  Okay, make a new connection... */
    auxsid = cda_new_server(auxspec, event_processer, privptr, CDA_REGULAR);
    if (auxsid == CDA_SERVERID_ERROR) return auxsid;

    si = AccessV2sidSlot(sid); /*!!! Re-cache value, since it could have changed because of realloc() in cda_new_server() */

    /* ...and remember the server in "aux servers list"... */
    tmpsidslist = safe_realloc(si->auxsidslist,
                               sizeof (cda_serverid_t) * (si->auxsidscount + 1));
    if (tmpsidslist == NULL)
    {
        fprintf(stderr, "%s: safe_realloc(auxsidslist): %s",
                __FUNCTION__, strerror(errno));
    }
    else
    {
        si->auxsidslist = tmpsidslist;
        tmpsidslist[si->auxsidscount] = auxsid;
        si->auxsidscount++;
    }

    /* ...plus record its position in that list locally... */
    /* Note:
           we save "si->auxsidscount", without a "-1", for the value to
           be instantly usable as a cause_conn_n (where 0 is the main server,
           and aux ones count from 1).
    */
    AccessV2sidSlot(auxsid)->sid_n    = si->auxsidscount;
    /* ...and "link" to parent server too */
    AccessV2sidSlot(auxsid)->main_sid = sid;

    /* Try to obtain physinfo from database */
    for (rec = si->global_physinfo_db;
         rec != NULL  &&  rec->srv != NULL;
         rec++)
        if (strcasecmp(auxspec, rec->srv) == 0)
        {
            cda_set_physinfo(auxsid, rec->info, rec->count);
        }

    return auxsid;
}

int             cda_set_physinfo(cda_serverid_t sid, physprops_t *info, int count)
{
  serverinfo_t *si = AccessV2sidSlot(sid);

    if (_CdaCheckSid(sid) != 0) return -1;

    if (count > 0)
    {
        if ((si->phprops = malloc(count * sizeof(*info))) == NULL) return -1;
        si->phpropscount = count;

        memcpy(si->phprops, info, count * sizeof(*info));
    }

    return 0;
}

int stripped_cda_register_physinfo_dbase(cda_serverid_t sid,
                                         physinfodb_rec_t *db)
{
  serverinfo_t *si = AccessV2sidSlot(sid);

    if (_CdaCheckSid(sid) != 0) return -1;

    si->global_physinfo_db = db;

    return 0;
}

//

int             cda_status_lastn  (cda_serverid_t  sid)
{
  serverinfo_t *si = AccessV2sidSlot(sid);
  
    if (_CdaCheckSid(sid) != 0) return -1;

    return si->auxsidscount;
}

const char     *cda_status_srvname(cda_serverid_t  sid, int n)
{
  serverinfo_t *si = AccessV2sidSlot(sid);
  
    if (_CdaCheckSid(sid) != 0) return NULL;
    
    if (n < 0  ||  n > si->auxsidscount)
    {
        errno = ENOENT;
        return NULL;
    }
    else if (n != 0)
        return cda_status_srvname(si->auxsidslist[n - 1], 0);
    else
        return si->srvrspec;
}

int             cda_srcof_physchan(cda_physchanhandle_t  chanh,   const char **name_p, int *n_p)
{
  cda_serverid_t  sid;
  int             chanofs;

    /* Decode and check address */
    decode_objhandle(chanh, &sid, &chanofs);
    if (_CdaCheckSid(sid) != 0) return -1;

    if (name_p != NULL) *name_p = cda_status_srvname(sid, 0);
    if (n_p    != NULL) *n_p    = chanofs;

    return 1;
}

int             cda_srcof_formula (excmd_t              *formula, const char **name_p, int *n_p)
{
    return -1;
}

int             cda_cnsof_physchan(cda_serverid_t  sid,
                                   uint8 *conns_u, int conns_u_size,
                                   cda_physchanhandle_t  chanh)
{
    return 0;
}

int             cda_cnsof_formula (cda_serverid_t  sid,
                                   uint8 *conns_u, int conns_u_size,
                                   excmd_t              *formula)
{
    return 0;
}

// channels

cda_physchanhandle_t cda_add_physchan(cda_serverid_t sid, int physchan)
{
    return encode_chanhandle(sid, physchan);
}

int  cda_setphyschanval(cda_physchanhandle_t  chanh, double  v)
{
    return -1;
}

int  cda_getphyschanvnr(cda_physchanhandle_t  chanh, double *vp, int32  *rp, tag_t *tag_p, rflags_t *rflags_p)
{
    return -1;
}

int  cda_getphyschan_rd(cda_physchanhandle_t  chanh, double *rp, double *dp)
{
  cda_serverid_t  sid;
  int             chanofs;
  serverinfo_t   *si;
  double          r, d;
  int             i;
  physprops_t    *pp;

    /* Decode and check address */
    decode_objhandle(chanh, &sid, &chanofs);
    if (_CdaCheckSid(sid) != 0) return -1;

    si = AccessV2sidSlot(sid);

    r = 1.0;
    d = 0.0;
    for (i = 0, pp = si->phprops;  i < si->phpropscount;  i++, pp++)
        if (pp->n == chanofs)
        {
            r = pp->r;
            d = pp->d;
            break;
        }
    if (rp != NULL) *rp = r;
    if (dp != NULL) *dp = d;

    return 0;
}

int  cda_getphyschan_q (cda_physchanhandle_t  chanh, double *qp)
{
    return -1;
}

int  stripped_cda_getphyschan_q_int(cda_physchanhandle_t  chanh, int *qp)
{
  cda_serverid_t  sid;
  int             chanofs;
  serverinfo_t   *si;
  int             q;
  int             i;
  physprops_t    *pp;

    /* Decode and check address */
    decode_objhandle(chanh, &sid, &chanofs);
    if (_CdaCheckSid(sid) != 0) return -1;

    si = AccessV2sidSlot(sid);

    q = 1;
    for (i = 0, pp = si->phprops;  i < si->phpropscount;  i++, pp++)
        if (pp->n == chanofs)
        {
            q = pp->q;
            break;
        }
    if (qp != NULL) *qp = q;

    return 0;
}


// formulae

excmd_t *cda_register_formula(cda_serverid_t defsid, excmd_t *formula, int options)
{
  excmd_t              *ret = NULL;
  int                   ncmds;
  excmd_t              *src;
  excmd_t              *dst;

    if (_CdaCheckSid(defsid) != 0) return NULL;

    if (formula == NULL) return NULL;

    /* Find out the size of this formula */
    for (ncmds = 0, src = formula;
         (src->cmd & OP_code) != OP_RET;
         ncmds++, src++);
    ncmds++; /* OP_RET */

    /* Allocate space for its clone */
    ret = (excmd_t *)malloc(ncmds * sizeof(excmd_t));

    if (ret == NULL) return NULL;

    /* And perform cloning */
    for (src = formula, dst = ret;
         (src->cmd & OP_code) != OP_RET;
         src++, dst++)
    {
        *dst = *src;
        if (src->cmd == OP_GETP_I  ||  src->cmd == OP_SETP_I)
        {
            dst->arg.handle = cda_add_physchan(defsid, src->arg.chan_id);
        }
    }
    *dst = *src; /* OP_RET */

    return ret;
}

excmd_t *cda_HACK_findnext_f (excmd_t *formula)
{
  excmd_t *cp;
    
    if (formula == NULL) return NULL;

    for (cp = formula;  ;  cp++)
    {
        if ((cp->cmd & OP_code) == OP_RETSEP) return cp + 1;
        if ((cp->cmd & OP_code) == OP_RET)    break;
    }

    return NULL;
}

int  cda_execformula(excmd_t *formula,
                     int options, double userval,
                     double *result_p, tag_t *tag_p, rflags_t *rflags_p,
                     int32 *rv_p, int *rv_useful_p,
                     cda_localreginfo_t *localreginfo)
{
    return -1;
}

// "errors"

char *cda_lasterr(void)
{
    return __FUNCTION__;
}
