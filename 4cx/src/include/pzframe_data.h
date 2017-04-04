#ifndef __PZFRAME_DATA_H
#define __PZFRAME_DATA_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "misc_types.h"
#include "misc_macros.h"
#include "paramstr_parser.h"

#include "cx.h"
#include "cda.h"


enum // pzframe_type_dscr_t.behaviour flags
{
    PZFRAME_B_ARTIFICIAL = 1 << 0,
    PZFRAME_B_NO_SVD     = 1 << 1,
    PZFRAME_B_NO_ALLOC   = 1 << 2,
};

enum // pzframe_chand_scr_t.chan_type
{
    PZFRAME_CHAN_IS_PARAM = 0,  // Is a parameter
    PZFRAME_CHAN_IS_FRAME = 1,  // Is a "main" frame channel

    PZFRAME_CHAN_TYPE_MASK = 0xFFFF,

    PZFRAME_CHAN_RW_ONLY_MASK    = 1 << 16, // Is valid for non-readonly mode only
    PZFRAME_CHAN_MARKER_MASK     = 1 << 17, // Perform processing upon receiving this
    PZFRAME_CHAN_NO_RD_CONV_MASK = 1 << 18, // Implies CDA_DATAREF_OPT_NO_RD_CONV
    PZFRAME_CHAN_IMMEDIATE_MASK  = 1 << 19, // Re-display knob immediately
    PZFRAME_CHAN_ON_CYCLE_MASK   = 1 << 20, // Request IMMEDIATE channels w/o ON_UPDATE
};

enum
{
    PZFRAME_REASON_DATA     = 0,
    PZFRAME_REASON_PARAM    = 1,
    PZFRAME_REASON_RDSCHG   = 3,
    PZFRAME_REASON_RSLVSTAT = 10,
};


/********************************************************************/
struct _pzframe_data_t_struct;
//--------------------------------------------------------------------

typedef void (*pzframe_data_evproc_t)(struct _pzframe_data_t_struct *pfr,
                                      int                            reason,
                                      int                            info_int,
                                      void                          *privptr);
typedef int  (*pzframe_data_save_t)  (struct _pzframe_data_t_struct *pfr,
                                      const char *filespec,
                                      const char *subsys, const char *comment);
typedef int  (*pzframe_data_load_t)  (struct _pzframe_data_t_struct *pfr,
                                      const char *filespec);
typedef int  (*pzframe_data_filter_t)(struct _pzframe_data_t_struct *pfr,
                                      const char *filespec,
                                      time_t *cr_time,
                                      char *commentbuf, size_t commentbuf_size);

typedef struct
{
    pzframe_data_evproc_t   evproc;
    pzframe_data_save_t     save;
    pzframe_data_load_t     load;
    pzframe_data_filter_t   filter;
} pzframe_data_vmt_t;

typedef struct
{
    const char *name;
    int         change_important;  // Does affect info_changed
    int         chan_type;
    cxdtype_t   dtype;
    int         max_nelems;
    int         rd_source_nc;
} pzframe_chan_dscr_t;

typedef struct
{
    const char            *type_name;
    int                    behaviour;     // PZFRAME_B_nnn

//!!!    psp_paramdescr_t      *specific_params;
    pzframe_chan_dscr_t   *chan_dscrs;
    int                    chan_count;

    pzframe_data_vmt_t     vmt;           // Inheritance
} pzframe_type_dscr_t;

/********************************************************************/

typedef struct
{
    int                   readonly;
    int                   maxfrq;
    int                   running;
    int                   oneshot;
} pzframe_cfg_t;

extern psp_paramdescr_t  pzframe_data_text2cfg[];

/********************************************************************/

typedef struct
{
    void *p;
    int   n;
} pzframe_data_dpl_t;

typedef struct
{
    void       *current_val; // Pointer to current-value-buffer (if !=NULL)...
    CxAnyVal_t  valbuf;      // Buffer for small-sized values; current_val=NULL in such cases
    rflags_t    rflags;
    cx_time_t   timestamp;
} pzframe_chan_data_t;

typedef struct
{
    pzframe_data_evproc_t  cb;
    void                  *privptr;
} pzframe_data_cbinfo_t;
typedef struct _pzframe_data_t_struct
{
    pzframe_type_dscr_t   *ftd;
    pzframe_cfg_t          cfg;

    void                  *buf;

    cda_context_t          cid;
    cda_dataref_t         *refs;
    pzframe_data_dpl_t    *dpls;
    pzframe_chan_data_t   *cur_data; // [ftd->chan_count]
    pzframe_chan_data_t   *prv_data; // [ftd->chan_count]

    struct timeval         timeupd;

    rflags_t               rflags;

    int                    rds_had_changed;
    int                    other_info_changed;

    // Callbacks
    pzframe_data_cbinfo_t *cb_list;
    int                    cb_list_allocd;
} pzframe_data_t;
//-- protected -------------------------------------------------------
void PzframeDataCallCBs(pzframe_data_t *pfr,
                        int             reason,
                        int             info_int);

/********************************************************************/

void
PzframeDataFillDscr(pzframe_type_dscr_t *ftd, const char *type_name,
                    int behaviour,
                    pzframe_chan_dscr_t *chan_dscrs, int chan_count);

void PzframeDataInit      (pzframe_data_t *pfr, pzframe_type_dscr_t *ftd);
int  PzframeDataRealize   (pzframe_data_t *pfr,
                           cda_context_t   present_cid,
                           const char     *base);

int  PzframeDataAddEvproc (pzframe_data_t *pfr,
                           pzframe_data_evproc_t cb, void *privptr);

void PzframeDataSetRunMode(pzframe_data_t *pfr,
                           int running, int oneshot);

int  PzframeDataGetChanInt(pzframe_data_t *pfr, int cn, int    *val_p);
int  PzframeDataGetChanDbl(pzframe_data_t *pfr, int cn, double *val_p);

int  PzframeDataFdlgFilter(const char *filespec,
                           time_t *cr_time,
                           char *commentbuf, size_t commentbuf_size,
                           void *privptr);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PZFRAME_DATA_H */
