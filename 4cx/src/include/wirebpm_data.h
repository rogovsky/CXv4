#ifndef __WIREBPM_DATA_H
#define __WIREBPM_DATA_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_data.h"


/********************************************************************/
struct _wirebpm_data_t_struct;
/********************************************************************/

typedef struct
{
    pzframe_type_dscr_t    ftd;

    int                    data_cn;

} wirebpm_type_dscr_t;

static inline wirebpm_type_dscr_t *pzframe2wirebpm_type_dscr(pzframe_type_dscr_t *ftd)
{
    return (ftd == NULL)? NULL
        :(wirebpm_type_dscr_t *)(ftd); /*!!! Should use OFFSETOF() -- (uint8*)ftd-OFFSETOF(wirebpm_type_dscr_t, ftd) */
}

typedef struct
{
    char  ext_i_fla[100];
} wirebpm_dcnv_t;

typedef struct
{
    double  ext_i;  // is copied from ext_i_val upon update
} wirebpm_mes_t;

typedef struct _wirebpm_data_t_struct
{
    pzframe_data_t         pfr;
    wirebpm_type_dscr_t   *wtd;  // Should be =.pfr.ftd
    wirebpm_dcnv_t         dcnv;

    cda_dataref_t          ext_i_ref;
    double                 ext_i_val;

    wirebpm_mes_t          mes;
} wirebpm_data_t;

static inline wirebpm_data_t *pzframe2wirebpm_data(pzframe_data_t *pzframe_data)
{
    return (pzframe_data == NULL)? NULL
        : (wirebpm_data_t *)(pzframe_data); /*!!! Should use OFFSETOF() -- (uint8*)pzframe_data-OFFSETOF(wirebpm_data_t, pfr) */
}

/********************************************************************/

void
WirebpmDataFillStdDscr(wirebpm_type_dscr_t *wtd, const char *type_name,
                       int behaviour,
                       pzframe_chan_dscr_t *chan_dscrs, int chan_count,
                       /* Data characteristics */
                       int data_cn);

void WirebpmDataInit       (wirebpm_data_t *bpm, wirebpm_type_dscr_t *wtd);
int  WirebpmDataRealize    (wirebpm_data_t *bpm,
                            cda_context_t   present_cid,
                            const char     *base);

psp_paramdescr_t *WirebpmDataGetDcnvTable(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __WIREBPM_DATA_H */
