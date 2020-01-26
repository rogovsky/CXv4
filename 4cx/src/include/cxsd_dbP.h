#ifndef __CXSD_DBP_H
#define __CXSD_DBP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx_module.h"
#include "cxsd_driver.h"
#include "cxsd_db.h"


enum
{
    CXSD_DB_CHAN_LOGMASK_OFS              = 0,
    CXSD_DB_CHAN_RESERVED_1_OFS           = 1,
    CXSD_DB_CHAN_DEVSTATE_OFS             = 2,
    CXSD_DB_CHAN_DEVSTATE_DESCRIPTION_OFS = 3,
    CXSD_DB_AUX_CHANCOUNT                 = 4,
};

enum
{
    CXSD_DB_RESOLVE_ERROR  = -1,
    CXSD_DB_RESOLVE_GLOBAL = 0,
    CXSD_DB_RESOLVE_DEVCHN = +1,
    CXSD_DB_RESOLVE_CPOINT = +2,
    CXSD_DB_RESOLVE_DEVICE = +3,
    CXSD_DB_RESOLVE_CLEVEL = +4,
};

enum
{
    CXSD_DB_CPOINT_DIFF_MASK =  1 << 30,
    CXSD_DB_CHN_CPT_IDN_MASK = (1 << 30) - 1,
};


enum
{
    CXSD_DB_MAX_CHANGRPCOUNT = 30,
    CXSD_DB_MAX_BUSINFOCOUNT = 20,
};


typedef int cxsd_gchnid_t;
typedef int cxsd_cpntid_t;

typedef struct
{
    int                 is_builtin;
    int                 is_simulated;

    int                 instname_ofs;
    int                 typename_ofs;
    int                 drvname_ofs;
    int                 lyrname_ofs;

    int                 changrpcount;
    int                 businfocount;

    int                 type_nsp_id;
    int                 chan_nsp_id;
    int                 chancount;
    int                 wauxcount; // With AUX channels

    CxsdChanInfoRec     changroups[CXSD_DB_MAX_CHANGRPCOUNT];
    int                 businfo   [CXSD_DB_MAX_BUSINFOCOUNT];

    int                 options_ofs;
    int                 auxinfo_ofs;
} CxsdDbDevLine_t;

typedef struct
{
    int                 lyrname_ofs;
    int                 bus_n;
    int                 lyrinfo_ofs;
} CxsdDbLayerinfo_t;

typedef struct // Is a slightly modified copy from CxsdDbCpntInfo_t
{
    cx_time_t           fresh_age;
    int                 phys_rd_specified;
    double              phys_rds[2];

    CxAnyVal_t          q;
    cxdtype_t           q_dtype;

    CxAnyVal_t          range[2];
    cxdtype_t           range_dtype;

    int                 return_type;

    /* Strings */
    int                 ident_ofs;
    int                 label_ofs;
    int                 tip_ofs;
    int                 comment_ofs;
    int                 geoinfo_ofs;
    int                 rsrvd6_ofs;
    int                 units_ofs;
    int                 dpyfmt_ofs;
} CxsdDbDcPrInfo_t; // "DcPr" -- Device Channel PRoperties

typedef struct
{
    /* Tagret reference */
    int                 devid;
    int                 ref_n;       // Either channel in dev or cpoint id (or'ed with CXSD_DB_CPOINT_DIFF_MASK)

    /* {r,d} */
    int                 phys_rd_specified;
    double              phys_rds[2];

    /* Strings */
    int                 ident_ofs;
    int                 label_ofs;
    int                 tip_ofs;
    int                 comment_ofs;
    int                 geoinfo_ofs;
    int                 rsrvd6_ofs;
    int                 units_ofs;
    int                 dpyfmt_ofs;
} CxsdDbCpntInfo_t;

enum // Note: values are unified with CXSD_DB_RESOLVE_nnn
{
    CXSD_DB_CLVL_ITEM_TYPE_DEVCHN = 1,
    CXSD_DB_CLVL_ITEM_TYPE_CPOINT = 2,
    CXSD_DB_CLVL_ITEM_TYPE_DEVICE = 3,
    CXSD_DB_CLVL_ITEM_TYPE_CLEVEL = 4,
};
typedef struct
{
    int                 name_ofs;
    int                 type;         // One of CXSD_DB_CLVL_ITEM_TYPE_nnn
    int                 ref_n;        // DEVCHN,CPOINT: index in db.cpnts[]; DEVICE: name_ofs of corresponding device; CLEVEL: index in clvls[]
    int                 rsrvd;
} CxsdDbClvlItem_t;
typedef struct
{
    int                 items_used;
    int                 items_allocd;
    CxsdDbClvlItem_t    items[0];
} CxsdDbClvlInfo_t;

typedef struct
{
    int                 name_ofs;    //
    int                 devchan_n;   //
    int                 dcpr_id;     // if >0 - ID inside db->dcprs[]
} CxsdDbDcLine_t;
typedef struct
{
    /* Devtype-related */
    int                 typename_ofs;
    int                 chancount;
    int                 changrpcount;
    CxsdChanInfoRec     changroups[CXSD_DB_MAX_CHANGRPCOUNT];

    /* Namespace-related */
    int                 items_used;
    int                 items_allocd;
    CxsdDbDcLine_t      items[0];
} CxsdDbDcNsp_t;

typedef struct _CxsdDbInfo_t_struct
{
    int                 is_readonly;

    int                 numdevs;
    CxsdDbDevLine_t    *devlist;
    int                 numchans;

    int                 numlios;
    CxsdDbLayerinfo_t  *liolist;

    CxsdDbDcPrInfo_t   *dcprs;
    int                 dcprs_used;
    int                 dcprs_allocd;

    CxsdDbCpntInfo_t   *cpnts;
    int                 cpnts_used;
    int                 cpnts_allocd;
    CxsdDbClvlInfo_t  **clvls_list; // Note: it is an array of POINTERS, not structs
    int                 clvls_used;
    int                 clvls_allocd;

    CxsdDbDcNsp_t     **nsps_list;  // Note: it is an array of POINTERS, not structs
    int                 nsps_used;
    int                 nsps_allocd;

    char               *strbuf;     // dictionary
    size_t              strbuf_used;
    size_t              strbuf_allocd;
} CxsdDbInfo_t;


CxsdDb  CxsdDbCreate (void);

int     CxsdDbAddDev (CxsdDb db, CxsdDbDevLine_t   *dev_p,
                      const char *instname, const char *typename,
                      const char *drvname,  const char *lyrname,
                      const char *options,  const char *auxinfo);
int     CxsdDbAddLyrI(CxsdDb db,
                      const char *lyrname, int bus_n,
                      const char *lyrinfo);
int     CxsdDbAddMem (CxsdDb db, const char        *mem, size_t len);
int     CxsdDbAddStr (CxsdDb db, const char        *str);
const char
       *CxsdDbGetStr (CxsdDb db, int ofs);

int     CxsdDbAddNsp (CxsdDb db, CxsdDbDcNsp_t     *nsp);
const CxsdDbDcNsp_t
       *CxsdDbGetNsp (CxsdDb db, int nsp_id);
int     CxsdDbFindNsp(CxsdDb db, const char *typename);

int     CxsdDbNspAddL(CxsdDb db, CxsdDbDcNsp_t **nsp_p, const char *name);
int     CxsdDbNspSrch(CxsdDb db, CxsdDbDcNsp_t  *nsp,   const char *name);

int     CxsdDbAddDcPr(CxsdDb db, CxsdDbDcPrInfo_t dcpr_data);

int     CxsdDbAddCpnt(CxsdDb db, CxsdDbCpntInfo_t cpnt_data);

int     CxsdDbAddClvl     (CxsdDb db);
int     CxsdDbClvlAddItem (CxsdDb db, int clvl_id, CxsdDbClvlItem_t item);
int     CxsdDbClvlFindItem(CxsdDb db, int clvl_id,
                           const char *name, size_t namelen,
                           CxsdDbClvlItem_t *item_return);

//////////////////////////////////////////////////////////////////////


#define CXSD_DBLDR_MODREC_SUFFIX _dbldr_rec
#define CXSD_DBLDR_MODREC_SUFFIX_STR __CX_STRINGIZE(CXSD_DBLDR_MODREC_SUFFIX)

enum {CXSD_DBLDR_MODREC_MAGIC = 0x644c6244}; /* Little-endian 'DbLd' */
enum {
    CXSD_DBLDR_MODREC_VERSION_MAJOR = 1,
    CXSD_DBLDR_MODREC_VERSION_MINOR = 0,
    CXSD_DBLDR_MODREC_VERSION = CX_ENCODE_VERSION(CXSD_DBLDR_MODREC_VERSION_MAJOR,
                                                  CXSD_DBLDR_MODREC_VERSION_MINOR)
};

typedef CxsdDb (*CxsdDbLdr_t)(const char *argv0,
                              const char *reference);

typedef struct _CxsdDbLdrModRec_struct
{
    cx_module_rec_t  mr;

    CxsdDbLdr_t      ldr;

    struct _CxsdDbLdrModRec_struct *next;
} CxsdDbLdrModRec;

#define CXSD_DBLDR_MODREC_NAME(name) \
    __CX_CONCATENATE(name, CXSD_DBLDR_MODREC_SUFFIX)

#define DECLARE_CXSD_DBLDR(name) \
    CxsdDbLdrModRec CXSD_DBLDR_MODREC_NAME(name)

#define DEFINE_CXSD_DBLDR(name, comment,                             \
                          init_m, term_m,                            \
                          ldr_f)                                     \
    CxsdDbLdrModRec CXSD_DBLDR_MODREC_NAME(name) =                   \
    {                                                                \
        {                                                            \
            CXSD_DBLDR_MODREC_MAGIC, CXSD_DBLDR_MODREC_VERSION,      \
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m                                           \
        },                                                           \
        ldr_f,                                                       \
        NULL                                                         \
    }


int  CxsdDbRegisterDbLdr  (CxsdDbLdrModRec *metric);
int  CxsdDbDeregisterDbLdr(CxsdDbLdrModRec *metric);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_DBP_H */
