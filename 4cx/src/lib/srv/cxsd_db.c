#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "memcasecmp.h"

#include "cxsd_db.h"
#include "cxsd_dbP.h"


CxsdDb  CxsdDbCreate (void)
{
  CxsdDb db;

  enum {RESERVED_CHANCOUNT = 100};  // Reserve first 100 channels
  static CxsdDbDevLine_t zero_dev =
  {
      .is_builtin   = 1,
      
      .instname_ofs = -1,
      .typename_ofs = -1,
      .drvname_ofs  = -1,
      .lyrname_ofs  = -1,

      .changrpcount = 1,
      .businfocount = 0,

      .type_nsp_id  = 0,
      .chancount    = RESERVED_CHANCOUNT,
      .wauxcount    = RESERVED_CHANCOUNT + CXSD_DB_AUX_CHANCOUNT,

      .changroups   =
      {
          {
              .count      = RESERVED_CHANCOUNT,  // Reserve first 100 channels
              .rw         = 0,
              .dtype      = CXDTYPE_INT32,       // Let them be simplest -- int32
              .max_nelems = 1,
          }
      },
      .businfo      = {},

      .options_ofs  = -1,
      .auxinfo_ofs  = -1,
  };

    db = malloc(sizeof(*db));
    if (db == NULL) return NULL;

    bzero(db, sizeof(*db));
    db->devlist = malloc(sizeof(zero_dev));
    if (db->devlist == NULL)
    {
        free(db);
        return NULL;
    }
    db->devlist[0] = zero_dev;
    db->numdevs    = 1;
    db->numchans   = db->devlist[0].wauxcount;

    return db;
}

int     CxsdDbDestroy(CxsdDb db)
{
  int  n;

    if (db == NULL) return 0;

    if (db->devlist != NULL)
        free(db->devlist);

    if (db->liolist != NULL)
        free(db->liolist);

    safe_free(db->cpnts);

    if (db->clvls_list != NULL)
    {
        for (n = 0;  n < db->clvls_used;  n++)
            safe_free(db->clvls_list[n]);
        free(db->clvls_list);
    }

    if (db->nsps_list != NULL)
    {
        for (n = 0;  n < db->nsps_used;  n++)
            safe_free(db->nsps_list[n]);
        free(db->nsps_list);
    }
    
    safe_free(db->strbuf);


    free(db);

    return 0;
}

int     CxsdDbAddDev (CxsdDb db, CxsdDbDevLine_t   *dev_p,
                      const char *instname, const char *typename,
                      const char *drvname,  const char *lyrname,
                      const char *options,  const char *auxinfo)
{
  CxsdDbDevLine_t *new_devlist;
  int              instname_ofs = -1;
  int              typename_ofs = -1;
  int              drvname_ofs  = -1;
  int              lyrname_ofs  = -1;
  int              options_ofs  = -1;
  int              auxinfo_ofs  = -1;
  int              g;

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    /* Check for duplicate .instname */
    if (CxsdDbFindDevice(db, instname, -1) >= 0)
    {
        errno = EEXIST;
        return -1;
    }

    /* Treat empty strings as unset */
    if (instname != NULL  &&  instname[0] == '\0') instname = NULL;
    if (typename != NULL  &&  typename[0] == '\0') typename = NULL;
    if (drvname  != NULL  &&  drvname [0] == '\0') drvname  = NULL;
    if (lyrname  != NULL  &&  lyrname [0] == '\0') lyrname  = NULL;
    if (options  != NULL  &&  options [0] == '\0') options  = NULL;
    if (auxinfo  != NULL  &&  auxinfo [0] == '\0') auxinfo  = NULL;

    /* Register strings if specified */
    if (instname != NULL  &&
        (instname_ofs = CxsdDbAddStr(db, instname)) < 0) return -1;
    if (typename != NULL  &&
        (typename_ofs = CxsdDbAddStr(db, typename)) < 0) return -1;
    if (drvname  != NULL  &&
        (drvname_ofs  = CxsdDbAddStr(db, drvname))  < 0) return -1;
    if (lyrname  != NULL  &&
        (lyrname_ofs = CxsdDbAddStr (db, lyrname))  < 0) return -1;
    if (options != NULL  &&  
        (options_ofs = CxsdDbAddStr(db, options)) < 0) return -1;
    if (auxinfo != NULL  &&
        (auxinfo_ofs = CxsdDbAddStr(db, auxinfo)) < 0) return -1;

    new_devlist = safe_realloc(db->devlist,
                               sizeof(*new_devlist) * (db->numdevs + 1));
    if (new_devlist == NULL) return -1;

    db->devlist = new_devlist;
    db->devlist[db->numdevs] = *dev_p;
    db->devlist[db->numdevs].instname_ofs = instname_ofs;
    db->devlist[db->numdevs].typename_ofs = typename_ofs;
    db->devlist[db->numdevs].drvname_ofs  = drvname_ofs;
    db->devlist[db->numdevs].lyrname_ofs  = lyrname_ofs;
    db->devlist[db->numdevs].options_ofs  = options_ofs;
    db->devlist[db->numdevs].auxinfo_ofs  = auxinfo_ofs;
    db->numdevs++;

    dev_p = db->devlist + db->numdevs - 1;
    for (g = 0, dev_p->chancount = 0;
         g < dev_p->changrpcount;
         g++)
        dev_p->chancount += dev_p->changroups[g].count;
    dev_p->wauxcount = dev_p->chancount + CXSD_DB_AUX_CHANCOUNT;
    db->numchans += dev_p->wauxcount;
    
    return 0;
}

int     CxsdDbAddLyrI(CxsdDb db,
                      const char *lyrname, int bus_n,
                      const char *lyrinfo)
{
  CxsdDbLayerinfo_t *new_liolist;
  int                lyrname_ofs = -1;
  int                lyrinfo_ofs = -1;

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

#if 1
    if (lyrname != NULL  &&  lyrname[0] == '\0') lyrname = NULL;
    if (lyrinfo != NULL  &&  lyrinfo[0] == '\0') lyrinfo = NULL;

    if (lyrname != NULL  &&
        (lyrname_ofs = CxsdDbAddStr(db, lyrname)) < 0) return -1;
    if (lyrinfo != NULL  &&
        (lyrinfo_ofs = CxsdDbAddStr(db, lyrinfo)) < 0) return -1;

    new_liolist = safe_realloc(db->liolist,
                               sizeof(*new_liolist) * (db->numlios + 1));
    if (new_liolist == NULL) return -1;

    db->liolist = new_liolist;
    bzero(db->liolist + db->numlios, sizeof(db->liolist[db->numlios]));
    db->liolist[db->numlios].lyrname_ofs = lyrname_ofs;
    db->liolist[db->numlios].bus_n       = bus_n;
    db->liolist[db->numlios].lyrinfo_ofs = lyrinfo_ofs;
    db->numlios++;
#endif
    
    return 0;
}

int     CxsdDbAddMem (CxsdDb db, const char        *mem, size_t len)
{
  int     retval;
  size_t  seglen;
  int     ofs;
  char   *newbuf;
  size_t  shortage;
  size_t  increment;

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    if (mem == NULL  ||  *mem == '\0') return 0;

    /* Try to find an already-registered string */
    for (ofs = 0;  ofs < db->strbuf_used;  ofs += seglen + 1)
    {
        seglen = strlen(db->strbuf + ofs);
        if (len == seglen  &&
            /* Note case-SENSITIVE comparison:
               strings in different case should be distinct */
            memcmp(mem, db->strbuf + ofs, len) == 0)
            return ofs;
    }

    if (db->strbuf_used + len + 1 > db->strbuf_allocd)
    {
        shortage = (db->strbuf_used + len + 1) - db->strbuf_allocd;
        if (db->strbuf_used == 0) shortage++;  // For empty at offset==0

        increment = (shortage + 1023) &~ 1023U; // Grow to a next multiple of 1kB

        newbuf = safe_realloc(db->strbuf, db->strbuf_allocd + increment);
        if (newbuf == NULL) return -1;
        db->strbuf         = newbuf;
        db->strbuf_allocd += increment;

        /* Grow from 0?  Add an empty "string" for offset==0 */
        if (db->strbuf_used == 0)
        {
            db->strbuf[0] = '\0';
            db->strbuf_used++;
        }
    }

    retval = db->strbuf_used;
    memcpy(db->strbuf + retval, mem, len); db->strbuf[retval + len] = '\0';
    db->strbuf_used += len + 1;

    return retval;
}

int     CxsdDbAddStr (CxsdDb db, const char *str)
{
    return CxsdDbAddMem(db, str, strlen(str));
}

const char
       *CxsdDbGetStr (CxsdDb db, int ofs)
{
    if (ofs <= 0  ||  ofs > db->strbuf_used - 1) return NULL;
    return db->strbuf + ofs;
}

int     CxsdDbAddNsp (CxsdDb db, CxsdDbDcNsp_t     *nsp)
{
  int             retval;
  CxsdDbDcNsp_t **new_nsps_list;
  CxsdDbDcNsp_t  *nsp0;

  enum {ALLOC_INC = 10};  // An estimation -- a typical config rarely contains more than 10 distinct types

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    /* Following code is a rudimental re-implementation of
       a GROWFIXELEM-type SLOTARRAY */

    /* Grow if there's no free slot atop of used cells */
    if (db->nsps_used >= db->nsps_allocd)
    {
        /* Grow... */
        new_nsps_list = safe_realloc(db->nsps_list,
                                     sizeof(*new_nsps_list) * (db->nsps_allocd + ALLOC_INC));
        if (new_nsps_list == NULL) return -1;
        bzero(new_nsps_list + db->nsps_allocd, sizeof(*new_nsps_list) * ALLOC_INC);
        /* ...and record it */
        db->nsps_list    = new_nsps_list;
        db->nsps_allocd += ALLOC_INC;

        /* Grow from 0?  Add an empty one for id=0 */
        if (db->nsps_used == 0)
        {
            nsp0 = malloc(sizeof(*nsp0));
            if (nsp0 == NULL)
            {
                free(db->nsps_list); db->nsps_list = NULL;
                db->nsps_allocd = 0;
                return -1;
            }
            bzero(nsp0, sizeof(*nsp0));
            db->nsps_list[db->nsps_used++] = nsp0;
        }
    }

    /* Record */
    retval = db->nsps_used;
    db->nsps_list[db->nsps_used++] = nsp;

    return retval;
}

const CxsdDbDcNsp_t
       *CxsdDbGetNsp (CxsdDb db, int nsp_id)
{
    if (nsp_id <= 0  ||  nsp_id >= db->nsps_allocd) return NULL;
    return db->nsps_list[nsp_id];
}

int     CxsdDbFindNsp(CxsdDb db, const char *typename)
{
  int         n;
  const char *nsp_typename;

    if (typename == NULL  ||  typename[0] == '\0') return -1;

    for (n = 0;  n < db->nsps_used;  n++)
        if (db->nsps_list[n] != NULL  &&
            (nsp_typename = CxsdDbGetStr(db, db->nsps_list[n]->typename_ofs)) 
                             != NULL  &&
            strcasecmp(typename, nsp_typename) == 0)
            return n;

    return -1;
}

int     CxsdDbNspAddL(CxsdDb db, CxsdDbDcNsp_t **nsp_p, const char *name)
{
  int             retval;
  int             ofs;
  CxsdDbDcNsp_t  *new_nsp;

  enum {ALLOC_INC = 20};

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    if (name == NULL  ||  *name == '\0') return -1;

    ofs = CxsdDbAddStr(db, name);
    if (ofs < 0) return -1;

    if ((*nsp_p)->items_used >= (*nsp_p)->items_allocd)
    {
        /* Grow... */
        new_nsp = safe_realloc(*nsp_p,
                               sizeof(*new_nsp)
                               +
                               (sizeof(new_nsp->items[0])
                                *
                                ((*nsp_p)->items_allocd + ALLOC_INC)
                               )
                              );
        if (new_nsp == NULL) return -1;
        bzero(new_nsp->items + new_nsp->items_allocd,
              sizeof(new_nsp->items[0]) * ALLOC_INC);
        /* ...and record it */
        *nsp_p = new_nsp;
        (*nsp_p)->items_allocd += ALLOC_INC;

        /* Note: no use to reserve id=0, since these ids aren't used in long term */
    }

    /* Record */
    retval = (*nsp_p)->items_used;
    (*nsp_p)->items[(*nsp_p)->items_used++].name_ofs = ofs;

    return retval;
}

int     CxsdDbNspSrch(CxsdDb db, CxsdDbDcNsp_t  *nsp,   const char *name)
{
  int  n;

    if (name == NULL  ||  *name == '\0') return -1;

    for (n = 0;  n < nsp->items_used;  n++)
    {
        if (nsp->items[n].name_ofs > 0  &&
            strcasecmp(name, db->strbuf + nsp->items[n].name_ofs) == 0)
            return n;
    }

    return -1;
}


int     CxsdDbAddCpnt(CxsdDb db, CxsdDbCpntInfo_t cpnt_data)
{
  int               retval;
  CxsdDbCpntInfo_t *new_cpnts;

  enum {ALLOC_INC = 100};

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    /* Grow if there's no free slot atop of used cells */
    if (db->cpnts_used >= db->cpnts_allocd)
    {
        /* Grow... */
        new_cpnts = safe_realloc(db->cpnts,
                                 sizeof(*new_cpnts) * (db->cpnts_allocd + ALLOC_INC));
        if (new_cpnts == NULL) return -1;
        bzero(new_cpnts + db->cpnts_allocd, sizeof(*new_cpnts) * ALLOC_INC);
        /* ...and record it */
        db->cpnts         = new_cpnts;
        db->cpnts_allocd += ALLOC_INC;

        /* Grow from 0?  Add an empty one for id=0 */
        if (db->cpnts_used == 0) db->cpnts_used++;
    }

    /* Record */
    retval = db->cpnts_used;
    db->cpnts[db->cpnts_used++] = cpnt_data;

    return retval;
}

enum {ROOT_CLVL_ID = 1};
static int  DoAddClvl     (CxsdDb db, int init_only)
{
  int                retval;
  CxsdDbClvlInfo_t **new_clvls_list;
  CxsdDbClvlInfo_t  *rsrv_clvl;
  CxsdDbClvlInfo_t  *root_clvl;
  CxsdDbClvlInfo_t  *clvl;

  enum {ALLOC_INC = 10};
  enum {START_ITEMS_COUNT = 10};

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    /* Grow if there's no free slot atop of used cells */
    if (db->clvls_used >= db->clvls_allocd)
    {
        /* Grow... */
        new_clvls_list = safe_realloc(db->clvls_list,
                                      sizeof(*new_clvls_list) * (db->clvls_allocd + ALLOC_INC));
        if (new_clvls_list == NULL) return -1;
        bzero(new_clvls_list + db->clvls_allocd, sizeof(*new_clvls_list) * ALLOC_INC);
        /* ...and record it */
        db->clvls_list    = new_clvls_list;
        db->clvls_allocd += ALLOC_INC;

        /* Grow from 0?  Add an empty one for id=0 and root for id=1 */
        if (db->clvls_used == 0)
        {
            rsrv_clvl = malloc(sizeof(*rsrv_clvl));
            root_clvl = malloc(sizeof(*root_clvl) +
                               sizeof(root_clvl->items[0]) * START_ITEMS_COUNT);
            if (rsrv_clvl == NULL  ||  root_clvl == NULL)
            {
                safe_free(rsrv_clvl);
                safe_free(root_clvl);
                free(db->clvls_list); db->clvls_list = NULL;
                db->clvls_allocd = 0;
                return -1;
            }
            bzero(rsrv_clvl, sizeof(*rsrv_clvl));
            bzero(root_clvl, sizeof(*root_clvl) +
                             sizeof(root_clvl->items[0]) * START_ITEMS_COUNT);
            root_clvl->items_allocd = START_ITEMS_COUNT;
            db->clvls_list[db->clvls_used++] = rsrv_clvl;
            db->clvls_list[db->clvls_used++] = root_clvl;
        }
    }

    if (init_only) return 0;

    /* Alloc */
    clvl = malloc(sizeof(*clvl) + sizeof(clvl->items[0]) * START_ITEMS_COUNT);
    if (clvl == NULL) return -1;
    bzero(clvl,   sizeof(*clvl) + sizeof(clvl->items[0]) * START_ITEMS_COUNT);
    clvl->items_allocd = START_ITEMS_COUNT;

    /* Record */
    retval = db->clvls_used;
    db->clvls_list[db->clvls_used++] = clvl;

    return retval;
}
int     CxsdDbAddClvl     (CxsdDb db)
{
    return DoAddClvl(db, 0);
}

int     CxsdDbClvlAddItem (CxsdDb db, int clvl_id, CxsdDbClvlItem_t item)
{
  int               retval;
  CxsdDbClvlInfo_t *clvl_p;

  enum {ALLOC_INC = 20};

    if (db->is_readonly)
    {
        errno = EROFS;
        return -1;
    }

    if (item.name_ofs <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (clvl_id <= 0)
    {
        clvl_id = ROOT_CLVL_ID;
        if (db->clvls_used <= clvl_id)
            if (DoAddClvl(db, 1) < 0) return 0;
    }

    if (clvl_id >= db->clvls_used) return -1;

    clvl_p = db->clvls_list[clvl_id];
    if (clvl_p->items_used >= clvl_p->items_allocd)
    {
        clvl_p = realloc(clvl_p,
                         sizeof(*clvl_p) +
                         sizeof(clvl_p->items[0]) * (clvl_p->items_used + ALLOC_INC));
        if (clvl_p == NULL) return -1;
        bzero(clvl_p->items + clvl_p->items_used, 
              sizeof(clvl_p->items[0]) * ALLOC_INC);
        db->clvls_list[clvl_id] = clvl_p;
    }

    /* Record */
    retval = clvl_p->items_used;
    clvl_p->items[clvl_p->items_used++] = item;

    return retval;
}

int     CxsdDbClvlFindItem(CxsdDb db, int clvl_id,
                           const char *name, size_t namelen,
                           CxsdDbClvlItem_t *item_return)
{
  int               n;
  CxsdDbClvlInfo_t *clvl_p;
  CxsdDbClvlItem_t *item_p;

    if (clvl_id <= 0)
    {
        clvl_id = ROOT_CLVL_ID;
        if (db->clvls_used <= clvl_id)
            return -1;
    }

    if (clvl_id >= db->clvls_used) return -1;

    clvl_p = db->clvls_list[clvl_id];
    if (clvl_p == NULL) return -1;
    for (n = 0, item_p = clvl_p->items;
         n < clvl_p->items_used;
         n++,   item_p++)
        if (cx_strmemcasecmp(CxsdDbGetStr(db, item_p->name_ofs),
                             name, namelen) == 0)
        {
            *item_return = *item_p;
            return 0;
        }

    return -1;
}


int CxsdDbFindDevice(CxsdDb  db, const char *devname, ssize_t devnamelen)
{
  int         devid;
  const char *instname;

    if (devnamelen < 0) devnamelen = strlen(devname);

    for (devid = 0;  devid < db->numdevs;  devid++)
        if ((instname = CxsdDbGetStr(db, db->devlist[devid].instname_ofs)) != NULL  &&
            cx_strmemcasecmp(instname, devname, devnamelen) == 0)
            return devid;

    return -1;
}

static int only_digits(const char *s)
{
    for (;  *s != '\0';  s++)
        if (!isdigit(*s)) return 0;

    return 1;
}
static int FindChanInDevice(CxsdDb  db, int devid, const char *channame)
{
  int                    nsp_id;
  int                    nsline;

    if      (((nsp_id = db->devlist[devid].chan_nsp_id) > 0  &&
              (nsline = CxsdDbNspSrch(db, db->nsps_list[nsp_id], 
                                      channame)) >= 0)  ||
             ((nsp_id = db->devlist[devid].type_nsp_id) > 0  &&
              (nsline = CxsdDbNspSrch(db, db->nsps_list[nsp_id], 
                                      channame)) >= 0))
    {
        return db->nsps_list[nsp_id]->items[nsline].devchan_n;
    }
    else if (isdigit(*channame)  &&  only_digits(channame))
    {
        return atoi(channame);
    }
    else return -1;
}
int     CxsdDbResolveName(CxsdDb      db,
                          const char *name,
                          int        *devid_p,
                          int        *chann_p)
{
  int               parent_clvl_id;
  char             *p;
  char             *dot_p;
  size_t            len;
  int               last;
  int               r;
  CxsdDbClvlItem_t  item_data;
  CxsdDbCpntInfo_t *cp;

  int               finding_special;

  const char            *first_dot;
  const char            *after_d;
  size_t                 devnamelen;

  int                    gcn;
  int                    chan;
  int                    devid;

    /* 0. Perform checks */

    /* Forbid NULL names... */
    if (db   == NULL)  return CXSD_DB_RESOLVE_ERROR;
    if (name == NULL)  return CXSD_DB_RESOLVE_ERROR;
    /* ...as well as empty names */
    while (*name != '\0'  &&  isspace(*name)) name++;
    if (*name == '\0') return CXSD_DB_RESOLVE_ERROR;

    /* Forbid ".NNN" names (they'd present way too many difficulties) */
    if (*name == '.') return CXSD_DB_RESOLVE_ERROR;

    finding_special = 0;
    dot_p = strrchr(name, '.');
    if (dot_p != NULL)
    {
        if      (strcasecmp(dot_p + 1, "_devstate") == 0)
            finding_special = 1;
        else if (strcasecmp(dot_p + 1, "_devstate_description") == 0)
            finding_special = 2;
    }


    /* 1. Virtual hierarchy */
    for (p = name,      parent_clvl_id = -1, last = 0;
         !last;
         p = dot_p + 1, parent_clvl_id = item_data.ref_n)
    {
        dot_p = strchr(p, '.');
        if (dot_p == NULL)
        {
            dot_p = p + strlen(p);
            last = 1;
        }
        len = dot_p - p;

        r = CxsdDbClvlFindItem(db, parent_clvl_id, p, len, &item_data);
////fprintf(stderr, "rslv<%.*s>, last=%d: lvlid=%d r=%d, type=%d\n", len, p, last, parent_clvl_id, r, r<0?-9:item_data.type);
        if (r < 0) goto NOT_FOUND_IN_VIRTUAL;

        if (item_data.type == CXSD_DB_CLVL_ITEM_TYPE_CLEVEL)
        {
            if (last)
            {
                *chann_p = item_data.ref_n;
                return CXSD_DB_RESOLVE_CLEVEL;
            }
        }
        else
        {
            /* Handle DEVICE before others, because it is valid for
               both last and !last cases, albeit in different ways */
            if (item_data.type == CXSD_DB_CLVL_ITEM_TYPE_DEVICE)
            {
                devid = item_data.ref_n;
                if (devid < 0  ||  devid >= db->numdevs)
                    goto NOT_FOUND_IN_VIRTUAL; /*!!! In fact, a bug */
                if (last)
                {
                    *devid_p = devid;
                    return CXSD_DB_RESOLVE_DEVICE;
                }
                else
                {
                    if      (finding_special == 1)
                        chan = db->devlist[devid].chancount + CXSD_DB_CHAN_DEVSTATE_OFS;
                    else if (finding_special == 2)
                        chan = db->devlist[devid].chancount + CXSD_DB_CHAN_DEVSTATE_DESCRIPTION_OFS;
                    else
                    {
                        chan = FindChanInDevice(db, devid, dot_p + 1);
                        if (chan < 0  ||  chan >= db->devlist[devid].chancount)
                            goto NOT_FOUND_IN_VIRTUAL;
                    }

                    *devid_p = devid;
                    *chann_p = chan;
                    return CXSD_DB_RESOLVE_DEVCHN;
                }
            }

            /* Fill in data */
            if      (item_data.type == CXSD_DB_CLVL_ITEM_TYPE_DEVCHN  ||
                     item_data.type == CXSD_DB_CLVL_ITEM_TYPE_CPOINT)
            {
                if (!last)
                {
                    if      (finding_special == 1)
                        *chann_p = CXSD_DB_CHAN_DEVSTATE_OFS;
                    else if (finding_special == 2)
                        *chann_p = CXSD_DB_CHAN_DEVSTATE_DESCRIPTION_OFS;
                    else goto NOT_FOUND_IN_VIRTUAL;
                    /* Follow up to the endpoint */
                    gcn = item_data.ref_n | CXSD_DB_CPOINT_DIFF_MASK;
                    while (1)
                    {
                        cp  = db->cpnts + (gcn & CXSD_DB_CHN_CPT_IDN_MASK);
                        gcn = cp->ref_n;
                        if ((gcn & CXSD_DB_CPOINT_DIFF_MASK) == 0)
                        {
                            *devid_p = cp->devid;
                            *chann_p+= db->devlist[cp->devid].chancount;
                            return CXSD_DB_RESOLVE_DEVCHN;
                        }
                    }
                }
                *chann_p = item_data.ref_n | CXSD_DB_CPOINT_DIFF_MASK;
////fprintf(stderr, "item_data.ref_n=%d\n", item_data.ref_n);
                return CXSD_DB_RESOLVE_CPOINT;
            }
            else goto NOT_FOUND_IN_VIRTUAL;
        }
    }
 NOT_FOUND_IN_VIRTUAL:;

    /* 2. Hardware hierarchy */

    first_dot = strchr(name, '.');
    after_d   = first_dot == NULL? NULL : first_dot + 1;

    /* a. A direct reference to channel N? */
    if (only_digits(name))
    {
        gcn = atoi(name);
        if (gcn <= 0  ||  gcn >= db->numchans) return CXSD_DB_RESOLVE_ERROR;
        *chann_p = gcn;
        return CXSD_DB_RESOLVE_GLOBAL;
    }

    /* b. DEVNAME.CHAN_N or DEVNAME.CHANNAME */
    if (first_dot != NULL)
    {
        devnamelen = first_dot - name;
        devid = CxsdDbFindDevice(db, name, devnamelen);
        if (devid < 0) return CXSD_DB_RESOLVE_ERROR;

        if      (finding_special == 1)
            chan = db->devlist[devid].chancount + CXSD_DB_CHAN_DEVSTATE_OFS;
        else if (finding_special == 2)
            chan = db->devlist[devid].chancount + CXSD_DB_CHAN_DEVSTATE_DESCRIPTION_OFS;
        else
        {
            chan = FindChanInDevice(db, devid, after_d);
            if (chan < 0  ||  chan >= db->devlist[devid].chancount)
                return CXSD_DB_RESOLVE_ERROR;
        }

        *devid_p = devid;
        *chann_p = chan;
        return CXSD_DB_RESOLVE_DEVCHN;
    }
    else
    /* c. DEVNAME */
    {
        devid = CxsdDbFindDevice(db, name, -1);
        if (devid >= 0)
        {
            *devid_p = devid;
            return CXSD_DB_RESOLVE_DEVICE;
        }
    }

    return CXSD_DB_RESOLVE_ERROR;
}
