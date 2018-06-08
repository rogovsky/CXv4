#ifndef __CDAP_H
#define __CDAP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdarg.h>

#include "cx_version.h"
#include "misc_macros.h"

#include "cx_module.h"
#include "cda.h"


typedef int             cda_srvconn_t;
enum {CDA_SRVCONN_ERROR = -1};
                            
typedef int             cda_hwcnref_t;
enum {CDA_HWCNREF_ERROR = -1};

//--------------------------------------------------------------------

enum
{
    CDA_DAT_P_ERROR     = -1,
    CDA_DAT_P_NOTREADY  =  0,
    CDA_DAT_P_OPERATING = +1,
};

enum
{
    CDA_DAT_P_DEL_SRV_SUCCESS  = 0,
    CDA_DAT_P_DEL_SRV_DEFERRED = 1,
};

enum
{
    CDA_DAT_P_GET_SERVER_OPT_NONE         = 0,
    CDA_DAT_P_GET_SERVER_OPT_NOLIST       = 16 << 16,

    CDA_DAT_P_GET_SERVER_OPT_SRVTYPE_mask = 0x000000FF,
};

//////////////////////////////////////////////////////////////////////

#define CDA_DAT_P_MODREC_SUFFIX     _dat_p_rec
#define CDA_DAT_P_MODREC_SUFFIX_STR __CX_STRINGIZE(CDA_DAT_P_MODREC_SUFFIX_STR)

enum
{
    CDA_DAT_P_MODREC_MAGIC = 0xCDA0DA1A,

    CDA_DAT_P_MODREC_VERSION_MAJOR = 1,
    CDA_DAT_P_MODREC_VERSION_MINOR = 0,
    CDA_DAT_P_MODREC_VERSION = CX_ENCODE_VERSION(CDA_DAT_P_MODREC_VERSION_MAJOR,
                                                 CDA_DAT_P_MODREC_VERSION_MINOR)
};


typedef int  (*cda_dat_p_new_chan_f)(cda_dataref_t ref, const char *name,
                                     int options, // CDA_DATAREF_OPT_ON_UPDATE only
                                     cxdtype_t dtype, int nelems);
typedef void (*cda_dat_p_del_chan_f)(void *pdt_privptr, cda_hwcnref_t hwr);
typedef int  (*cda_dat_p_snd_data_f)(void *pdt_privptr, cda_hwcnref_t hwr,
                                     cxdtype_t dtype, int nelems, void *value);
typedef int  (*cda_dat_p_lock_op_f) (void *pdt_privptr,
                                     int   count, cda_hwcnref_t *hwrs,
                                     int   operation, int lock_id);

typedef int  (*cda_dat_p_new_srv_f) (cda_srvconn_t  sid, void *pdt_privptr,
                                     int            uniq,
                                     const char    *srvrspec,
                                     const char    *argv0,
                                     int            srvtype);
typedef int  (*cda_dat_p_del_srv_f) (cda_srvconn_t  sid, void *pdt_privptr);


typedef struct cda_dat_p_rec_t_struct
{
    cx_module_rec_t       mr;

    size_t                privrecsize;

    char                  sep_ch;            // '.'
    char                  upp_ch;            // ':'
    char                  opt_ch;            // '@'

    cda_dat_p_new_chan_f  new_chan;
    cda_dat_p_del_chan_f  del_chan;
    cda_dat_p_snd_data_f  snd_data;
    cda_dat_p_lock_op_f   do_lock;

    cda_dat_p_new_srv_f   new_srv;
    cda_dat_p_del_srv_f   del_srv;

    struct cda_dat_p_rec_t_struct *next;
    int                            ref_count;
} cda_dat_p_rec_t;

#define CDA_DAT_P_MODREC_NAME(name) \
    __CX_CONCATENATE(name,CDA_DAT_P_MODREC_SUFFIX)

#define CDA_DECLARE_DAT_PLUGIN(name) \
    cda_dat_p_rec_t CDA_DAT_P_MODREC_NAME(name)
    
#define CDA_DEFINE_DAT_PLUGIN(name, comment,                         \
                              init_m, term_m,                        \
                              privrecsize,                           \
                              sep_ch, upp_ch, opt_ch,                \
                              new_chan, del_chan,                    \
                              snd_data, do_lock,                     \
                              new_srv,  del_srv)                     \
    cda_dat_p_rec_t CDA_DAT_P_MODREC_NAME(name) =                    \
    {                                                                \
        {                                                            \
            CDA_DAT_P_MODREC_MAGIC, CDA_DAT_P_MODREC_VERSION,        \
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m,                                          \
        },                                                           \
        privrecsize,                                                 \
        sep_ch, upp_ch, opt_ch,                                      \
                                                                     \
        new_chan, del_chan,                                          \
        snd_data, do_lock,                                           \
        new_srv,  del_srv,                                           \
                                                                     \
        NULL, 0                                                      \
    }

int cda_register_dat_plugin  (cda_dat_p_rec_t *metric);
int cda_deregister_dat_plugin(cda_dat_p_rec_t *metric);

//////////////////////////////////////////////////////////////////////

#define CDA_FLA_P_MODREC_SUFFIX     _fla_p_rec
#define CDA_FLA_P_MODREC_SUFFIX_STR __CX_STRINGIZE(CDA_FLA_P_MODREC_SUFFIX_STR)

enum
{
    CDA_FLA_P_MODREC_MAGIC = 0xCDA0F01A,

    CDA_FLA_P_MODREC_VERSION_MAJOR = 1,
    CDA_FLA_P_MODREC_VERSION_MINOR = 0,
    CDA_FLA_P_MODREC_VERSION = CX_ENCODE_VERSION(CDA_FLA_P_MODREC_VERSION_MAJOR,
                                                 CDA_FLA_P_MODREC_VERSION_MINOR)
};


typedef int  (*cda_fla_p_create_t) (cda_dataref_t  ref,
                                    void          *fla_privptr,
                                    int            uniq,
                                    const char    *base,
                                    const char    *spec,
                                    int            options,
                                    CxKnobParam_t *params,
                                    int            num_params);
typedef void (*cda_fla_p_destroy_t)(void          *fla_privptr);
typedef int  (*cda_fla_p_add_evp_t)(void          *fla_privptr,
                                    int                   evmask,
                                    cda_dataref_evproc_t  evproc,
                                    void                 *privptr2);
typedef int  (*cda_fla_p_del_evp_t)(void          *fla_privptr,
                                    int                   evmask,
                                    cda_dataref_evproc_t  evproc,
                                    void                 *privptr2);
typedef int  (*cda_fla_p_execute_t)(void          *fla_privptr,
                                    int            options,
                                    CxKnobParam_t *params,
                                    int            num_params,
                                    double         userval,
                                    double        *result_p,
                                    CxAnyVal_t    *rv_p, cxdtype_t *rv_dtype_p,
                                    rflags_t      *rflags_p,
                                    cx_time_t     *timestamp_p);
typedef void (*cda_fla_p_stop_t)   (void          *fla_privptr);
typedef int  (*cda_fla_p_source_t) (void          *fla_privptr,
                                    char          *buf,
                                    size_t         bufsize);
typedef int  (*cda_fla_p_srvs_of_t)(void          *fla_privptr,
                                    uint8 *conns_u, int conns_u_size);


typedef struct cda_fla_p_rec_t_struct
{
    cx_module_rec_t         mr;


    size_t                  privrecsize;

    cda_fla_p_create_t      create;
    cda_fla_p_destroy_t     destroy;
    cda_fla_p_add_evp_t     add_evproc;
    cda_fla_p_del_evp_t     del_evproc;
    cda_fla_p_execute_t     execute;
    cda_fla_p_stop_t        stop;
    cda_fla_p_source_t      source;
    cda_fla_p_srvs_of_t     srvs_of;

    struct cda_fla_p_rec_t_struct *next;
    int                            ref_count;
} cda_fla_p_rec_t;

#define CDA_FLA_P_MODREC_NAME(name) \
    __CX_CONCATENATE(name,CDA_FLA_P_MODREC_SUFFIX)

#define CDA_DECLARE_FLA_PLUGIN(name) \
    cda_fla_p_rec_t CDA_FLA_P_MODREC_NAME(name)
    
#define CDA_DEFINE_FLA_PLUGIN(name, comment,                         \
                              init_m, term_m,                        \
                              privrecsize,                           \
                              cre_f, des_f,                          \
                              aep_f, dep_f,                          \
                              exe_f, stp_f,                          \
                              src_f, sof_f)                          \
    cda_fla_p_rec_t CDA_FLA_P_MODREC_NAME(name) =                    \
    {                                                                \
        {                                                            \
            CDA_FLA_P_MODREC_MAGIC, CDA_FLA_P_MODREC_VERSION,        \
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m,                                          \
        },                                                           \
        privrecsize,                                                 \
                                                                     \
        cre_f, des_f,                                                \
        aep_f, dep_f,                                                \
        exe_f, stp_f,                                                \
        src_f, sof_f,                                                \
                                                                     \
        NULL, 0                                                      \
    }

int  cda_register_fla_plugin  (cda_fla_p_rec_t *metric);
int  cda_deregister_fla_plugin(cda_fla_p_rec_t *metric);

//////////////////////////////////////////////////////////////////////

cda_context_t cda_dat_p_get_cid    (cda_dataref_t    source_ref);

void *cda_dat_p_get_server         (cda_dataref_t    source_ref,
                                    cda_dat_p_rec_t *metric,
                                    const char      *srvrspec,
                                    int              options);
void  cda_dat_p_set_server_state   (cda_srvconn_t  sid, int state);
void  cda_dat_p_del_server_finish  (cda_srvconn_t  sid);

void  cda_dat_p_update_server_cycle(cda_srvconn_t  sid);
void  cda_dat_p_update_dataset     (cda_srvconn_t  sid,
                                    int            count,
                                    cda_dataref_t *refs,
                                    void         **values,
                                    cxdtype_t     *dtypes,
                                    int           *nelems,
                                    rflags_t      *rflags,
                                    cx_time_t     *timestamps,
                                    int            is_update);
void  cda_dat_p_defunct_dataset    (cda_srvconn_t  sid,
                                    int            count,
                                    cda_dataref_t *refs);
void  cda_fla_p_update_fla_result  (cda_dataref_t  ref,
                                    double         value,
                                    CxAnyVal_t     raw,
                                    int            raw_dtype,
                                    rflags_t       rflags,
                                    cx_time_t      timestamp);

void  cda_dat_p_set_hwr            (cda_dataref_t  ref, cda_hwcnref_t hwr);
void  cda_dat_p_report_rslvstat    (cda_dataref_t  ref, int rslvstat);
void  cda_dat_p_set_ready          (cda_dataref_t  ref, int is_ready);
void  cda_dat_p_set_phys_rds       (cda_dataref_t  ref,
                                    int            phys_count,
                                    double        *rds);
void  cda_dat_p_set_fresh_age      (cda_dataref_t  ref, cx_time_t fresh_age);
void  cda_dat_p_set_quant          (cda_dataref_t  ref,
                                    CxAnyVal_t     q, cxdtype_t q_dtype);
void  cda_dat_p_set_strings        (cda_dataref_t  ref,
                                    const char    *ident,
                                    const char    *label,
                                    const char    *tip,
                                    const char    *comment,
                                    const char    *geoinfo,
                                    const char    *rsrvd6,
                                    const char    *units,
                                    const char    *dpyfmt);

void *cda_dat_p_privp_of_sid(cda_srvconn_t  sid);
int   cda_dat_p_suniq_of_sid(cda_srvconn_t  sid);
char *cda_dat_p_argv0_of_sid(cda_srvconn_t  sid);
char *cda_dat_p_srvrn_of_sid(cda_srvconn_t  sid);

char *cda_dat_p_argv0_of_ref(cda_dataref_t  ref);

void cda_dat_p_report(cda_srvconn_t  sid, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void cda_ref_p_report(cda_dataref_t  ref, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

//////////////////////////////////////////////////////////////////////


/* Error descriptions setting API */

void cda_clear_err(void);
void cda_set_err  (const char *format, ...)
    __attribute__((format (printf, 1, 2)));
void cda_vset_err (const char *format, va_list ap);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CDAP_H */
