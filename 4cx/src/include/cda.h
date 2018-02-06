#ifndef __CDA_H
#define __CDA_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <unistd.h>

#include "cx.h"
#include "cx_common_types.h"


enum {CDA_PATH_MAX = 1024};
    

typedef CxDataRef_t     cda_dataref_t;
enum {CDA_DATAREF_ERROR = -1,
      CDA_DATAREF_NONE  = NULL_CxDataRef};

typedef int             cda_context_t;
enum {CDA_CONTEXT_ERROR = -1};

typedef int             cda_varparm_t;
enum {CDA_VARPARM_ERROR = -1};


/* Context-level options - for cda_new_context() */
enum
{
    CDA_CONTEXT_OPT_NONE       = 0,
    CDA_CONTEXT_OPT_NO_OTHEROP = 1 << 0,  // Don't support CXCF_FLAG_OTHEROP rflag
    CDA_CONTEXT_OPT_IGN_UPDATE = 1 << 1,  // Ignore UPDATE events (intended for cx-starter)
};

/* Dataref-level options - for cda_add_chan() and cda_add_formula() */
enum
{
    CDA_DATAREF_OPT_NONE       = 0,
    CDA_DATAREF_OPT_PRIVATE    = 1 << 31,  // Force registration of distinct dataref, even if this name is already present; additionally, later registrations wouldn't alias to this one
    CDA_DATAREF_OPT_NO_RD_CONV = 1 << 30,  // Don't perform {R,D}-conversion
    CDA_DATAREF_OPT_SHY        = 1 << 29,  // Drop written-but-not-sent data upon disconnects
    CDA_DATAREF_OPT_FIND_ONLY  = 1 << 28,  // Check if channel is registered; return its ref upon success or DATAREF_ERROR upon absence
    CDA_DATAREF_OPT_ON_UPDATE  = 1 << 27,  // Notify about this channel's updates immediately, not upon CYCLE (this flag is passed to server by CX plugin)
    CDA_DATAREF_OPT_rsrvd26    = 1 << 26,  // 
    CDA_DATAREF_OPT_NO_WR_WAIT = 1 << 25,  // Don't buffer writes until {R,D} info is received
};

/* Processing options - for cda_process_ref() */
enum
{
    CDA_OPT_NONE       = 0,
    CDA_OPT_IS_W       = 1 << 0,  // Writes are allowed (useful in formulas)
    CDA_OPT_READONLY   = 1 << 1,  // Writes are forbidden, even in IS_W formulas and with "allow_w" prefixes
    CDA_OPT_HAS_PARAM  = 1 << 2,  // Input parameter (user-supplied value) present; useful in write-formulas
    CDA_OPT_RETVAL_RQD = 1 << 3,  // Return value required; useful in read-formulas

    CDA_OPT_DO_EXEC    = 1 << 16, // Allow formula execution

    CDA_OPT_RD_FLA     = CDA_OPT_RETVAL_RQD,
    CDA_OPT_WR_FLA     = CDA_OPT_IS_W | CDA_OPT_HAS_PARAM,
};

/* cda_process_ref() and cda_snd_ref_data() result codes */
enum
{
    CDA_PROCESS_SEVERE_ERR   = -2,      // Is used internally between cda_core and cda_d_*
    CDA_PROCESS_ERR          = -1,      // Error
    CDA_PROCESS_DONE         =  0,      // Success
    CDA_PROCESS_FLAG_BUSY    =  1 << 0, // Formula execution in progress (e.g. a "sleep" command)
    CDA_PROCESS_FLAG_REFRESH =  1 << 1, // Should refresh window (because some interlan values could have been changed by formula)
};

/* Context-level event masks and reasons */
enum
{
    CDA_CTX_R_CYCLE    = 0,  CDA_CTX_EVMASK_CYCLE    = 1 << CDA_CTX_R_CYCLE,
    CDA_CTX_R_SRVSTAT  = 1,  CDA_CTX_EVMASK_SRVSTAT  = 1 << CDA_CTX_R_SRVSTAT,
    CDA_CTX_R_NEWSRV   = 2,  CDA_CTX_EVMASK_NEWSRV   = 1 << CDA_CTX_R_NEWSRV,
};

/* Dataref-level event masks and reasons */
enum
{
    CDA_REF_R_UPDATE   = 0,  CDA_REF_EVMASK_UPDATE   = 1 << CDA_REF_R_UPDATE,
    CDA_REF_R_STATCHG  = 1,  CDA_REF_EVMASK_STATCHG  = 1 << CDA_REF_R_STATCHG,
    CDA_REF_R_STRSCHG  = 2,  CDA_REF_EVMASK_STRSCHG  = 1 << CDA_REF_R_STRSCHG,
    CDA_REF_R_RDSCHG   = 3,  CDA_REF_EVMASK_RDSCHG   = 1 << CDA_REF_R_RDSCHG,
    CDA_REF_R_FRESHCHG = 4,  CDA_REF_EVMASK_FRESHCHG = 1 << CDA_REF_R_FRESHCHG,
    CDA_REF_R_QUANTCHG = 5,  CDA_REF_EVMASK_QUANTCHG = 1 << CDA_REF_R_QUANTCHG,
    CDA_REF_R_RSLVSTAT = 10, CDA_REF_EVMASK_RSLVSTAT = 1 << CDA_REF_R_RSLVSTAT,
    CDA_REF_R_CURVAL   = 11, CDA_REF_EVMASK_CURVAL   = 1 << CDA_REF_R_CURVAL,
    CDA_REF_R_LOCKSTAT = 12, CDA_REF_EVMASK_LOCKSTAT = 1 << CDA_REF_R_LOCKSTAT,
};

enum
{
    CDA_LOCK_RLS = 0,
    CDA_LOCK_SET = 1,
};

typedef enum /* Note: the statuses are ordered by decreasing of severity, so that a "most severe of several" status can be easily chosen */
{
    CDA_SERVERSTATUS_ERROR        = -1,  // Error in parameter (invalid ref, cid or nth)
    CDA_SERVERSTATUS_DISCONNECTED =  0,  // Is disconnected
    CDA_SERVERSTATUS_FROZEN       = +1,  // Connected but frozen (unused as of 06.02.2018)
    CDA_SERVERSTATUS_ALMOSTREADY  = +2,  // Connected "almost"   (unused as of 06.02.2018)
    CDA_SERVERSTATUS_NORMAL       = +3,  // Is connected
} cda_serverstatus_t;


static inline int cda_ref_is_sensible(cda_dataref_t ref)
{
    return ref > 0;
}

//////////////////////////////////////////////////////////////////////

typedef void (*cda_context_evproc_t)(int            uniq,
                                     void          *privptr1,
                                     cda_context_t  cid,
                                     int            reason,
                                     int            info_int,
                                     void          *privptr2);

typedef void (*cda_dataref_evproc_t)(int            uniq,
                                     void          *privptr1,
                                     cda_dataref_t  ref,
                                     int            reason,
                                     void          *info_ptr,
                                     void          *privptr2);
                                     

/* Context management */

cda_context_t  cda_new_context(int                   uniq,   void *privptr1,
                               const char           *defpfx, int   options,
                               const char           *argv0,
                               int                   evmask,
                               cda_context_evproc_t  evproc,
                               void                 *privptr2);
int            cda_del_context(cda_context_t         cid);

int            cda_add_context_evproc(cda_context_t         cid,
                                      int                   evmask,
                                      cda_context_evproc_t  evproc,
                                      void                 *privptr2);
int            cda_del_context_evproc(cda_context_t         cid,
                                      int                   evmask,
                                      cda_context_evproc_t  evproc,
                                      void                 *privptr2);


/* Channels management */

cda_dataref_t  cda_add_chan   (cda_context_t         cid,
                               const char           *base,
                               const char           *spec,
                               int                   options,
                               cxdtype_t             dtype,
                               int                   max_nelems,
                               int                   evmask,
                               cda_dataref_evproc_t  evproc,
                               void                 *privptr2);
int            cda_del_chan   (cda_dataref_t ref);

int            cda_add_dataref_evproc(cda_dataref_t         ref,
                                      int                   evmask,
                                      cda_dataref_evproc_t  evproc,
                                      void                 *privptr2);
int            cda_del_dataref_evproc(cda_dataref_t         ref,
                                      int                   evmask,
                                      cda_dataref_evproc_t  evproc,
                                      void                 *privptr2);

int            cda_lock_chans (int count, cda_dataref_t *refs,
                               int operation);

char *cda_combine_base_and_spec(cda_context_t         cid,
                                const char           *base,
                                const char           *spec,
                                char                 *namebuf,
                                size_t                namebuf_size);

/* Simplified channels API */
cda_dataref_t cda_add_dchan(cda_context_t  cid,
                            const char    *name);
int           cda_get_dcval(cda_dataref_t  ref, double *v_p);
int           cda_set_dcval(cda_dataref_t  ref, double  val);


/* Formulae management */

cda_dataref_t  cda_add_formula(cda_context_t         cid,
                               const char           *base,
                               const char           *spec,
                               int                   options,
                               CxKnobParam_t        *params,
                               int                   num_params,
                               int                   evmask,
                               cda_dataref_evproc_t  evproc,
                               void                 *privptr2);

cda_dataref_t  cda_add_varchan(cda_context_t         cid,
                               const char           *varname);

cda_varparm_t  cda_add_varparm(cda_context_t         cid,
                               const char           *varname);


/**/
int            cda_src_of_ref           (cda_dataref_t ref, const char **src_p);
int            cda_dtype_of_ref         (cda_dataref_t ref);
int            cda_max_nelems_of_ref    (cda_dataref_t ref);
int            cda_current_nelems_of_ref(cda_dataref_t ref);
int            cda_fresh_age_of_ref     (cda_dataref_t ref, cx_time_t *fresh_age_p);
int            cda_quant_of_ref         (cda_dataref_t ref,
                                         CxAnyVal_t   *q_p, cxdtype_t *q_dtype_p);
int            cda_current_dtype_of_ref (cda_dataref_t ref);

int            cda_strings_of_ref       (cda_dataref_t  ref,
                                         char    **ident_p,
                                         char    **label_p,
                                         char    **tip_p,
                                         char    **comment_p,
                                         char    **geoinfo_p,
                                         char    **rsrvd6_p,
                                         char    **units_p,
                                         char    **dpyfmt_p);
int            cda_phys_rds_of_ref      (cda_dataref_t  ref,
                                         int      *phys_count_p,
                                         double  **rds_p);

int                 cda_status_of_ref_sid(cda_dataref_t ref);

int                 cda_status_srvs_count(cda_context_t  cid);
cda_serverstatus_t  cda_status_of_srv    (cda_context_t  cid, int nth);
const char         *cda_status_srv_scheme(cda_context_t  cid, int nth);
const char         *cda_status_srv_name  (cda_context_t  cid, int nth);

int                 cda_srvs_of_ref      (cda_dataref_t ref,
                                          uint8 *conns_u, int conns_u_size);

int                 cda_add_server_conn  (cda_context_t  cid,
                                          const char    *srvref);

/**********************/

int cda_process_ref(cda_dataref_t ref, int options,
                    /* Inputs */
                    double userval,
                    CxKnobParam_t *params, int num_params);
int cda_get_ref_dval(cda_dataref_t ref,
                     double     *curv_p,
                     CxAnyVal_t *curraw_p, cxdtype_t *curraw_dtype_p,
                     rflags_t *rflags_p, cx_time_t *timestamp_p);
int cda_rd_convert  (cda_dataref_t ref, double raw, double *result_p);

int cda_snd_ref_data(cda_dataref_t ref,
                     cxdtype_t dtype, int nelems,
                     void *data);
int cda_get_ref_data(cda_dataref_t ref,
                     size_t ofs, size_t size, void *buf);
int cda_get_ref_stat(cda_dataref_t ref,
                     rflags_t *rflags_p, cx_time_t *timestamp_p);
int cda_acc_ref_data(cda_dataref_t ref,
                     void **buf_p, size_t *size_p);

int cda_stop_formula(cda_dataref_t ref);


//////////////////////////////////////////////////////////////////////

const char *cda_strserverstatus_short(cda_serverstatus_t status);

/* Error descriptions */
char *cda_last_err(void);


void  cda_do_cleanup(int uniq);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CDA_H */
