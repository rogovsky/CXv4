#ifndef __FASTADC_DATA_H
#define __FASTADC_DATA_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_data.h"
#include "plotdata.h"


enum
{
    FASTADC_MAX_LINES = 16,
};

enum
{
    FASTADC_DATA_CN_MISSING   = -1,

    FASTADC_DATA_IS_ON_ALWAYS = -2,
};

/********************************************************************/
struct _fastadc_data_t_struct;
struct _fastadc_mes_t_struct;
//--------------------------------------------------------------------
typedef void   (*fastadc_info2mes_t)   (struct _fastadc_data_t_struct *adc,
                                        pzframe_chan_data_t           *info,
                                        struct _fastadc_mes_t_struct  *mes_p);
typedef int    (*fastadc_x2xs_t)       (pzframe_chan_data_t *info, int x);
/********************************************************************/

typedef struct
{
    const char       *name;
    const char       *units;
    const char       *dpyfmt;
    int               data_cn;
    int               cur_numpts_cn;
    int               is_on_cn;
    double            default_r;
    plotdata_range_t  range;
    int               range_min_cn;
    int               range_max_cn;
} fastadc_line_dscr_t;

typedef struct
{
    pzframe_type_dscr_t    ftd;

    int                    max_numpts;
    int                    num_lines;
    const char            *line_name_prefix;
    int                    common_cur_numpts_cn;
    int                    common_cur_ptsofs_cn;
    int                    xs_per_point_cn;
    int                    xs_divisor_cn;
    int                    xs_factor_cn;
    const char            *common_dpyfmt;

    fastadc_line_dscr_t   *line_dscrs;

    fastadc_info2mes_t     info2mes;
    fastadc_x2xs_t         x2xs;
    const char            *xs;
} fastadc_type_dscr_t;

static inline fastadc_type_dscr_t *pzframe2fastadc_type_dscr(pzframe_type_dscr_t *ftd)
{
    return (ftd == NULL)? NULL
        : (fastadc_type_dscr_t *)(ftd); /*!!! Should use OFFSETOF() -- (uint8*)ftd-OFFSETOF(fastadc_type_dscr_t, ftd) */
}

/********************************************************************/

typedef struct
{
    int     show;
    int     magn;
    int     invp;
    int     _pad_;
    char    units [10];
    char    descr [40];
    char    dpyfmt[20];
} fastadc_dcnv_one_t;

typedef struct
{
    fastadc_dcnv_one_t  lines[FASTADC_MAX_LINES];
    int                 cmpr;
    char                ext_xs_per_point_src[100];
} fastadc_dcnv_t;

/*!!! Bad... Should better live in _gui, but must be accessible to _data somehow... */
extern int               fastadc_data_magn_factors[7];
extern int               fastadc_data_cmpr_factors[13];

/********************************************************************/

typedef struct _fastadc_mes_t_struct
{
    int             xs_per_point;
    int             xs_divisor;
    int             xs_factor;
    int             exttim;
    int             ext_xs_per_point; // is copied from ext_xs_per_point_val upon update

    int             cur_numpts;
    int             cur_ptsofs;

    plotdata_t      plots[FASTADC_MAX_LINES];
} fastadc_mes_t;

typedef struct _fastadc_data_t_struct
{
    pzframe_data_t         pfr;
    fastadc_type_dscr_t   *atd;  // Should be =.pfr.ftd
    fastadc_dcnv_t         dcnv;

    cda_dataref_t          ext_xs_per_point_ref;
    int32                  ext_xs_per_point_val;

    uint8                  lines_rds_rcvd[FASTADC_MAX_LINES];

    fastadc_mes_t          mes;
} fastadc_data_t;

static inline fastadc_data_t *pzframe2fastadc_data(pzframe_data_t *pzframe_data)
{
    return (pzframe_data == NULL)? NULL
        : (fastadc_data_t *)(pzframe_data); /*!!! Should use OFFSETOF() -- (uint8*)pzframe_data-OFFSETOF(fastadc_data_t, pfr) */
}

/********************************************************************/

static inline void FastadcSymmMinMaxInt(int    *min_val, int    *max_val)
{
  int     abs_max;

    if (*min_val < 0  &&  *max_val > 0)
    {
        abs_max = -*min_val;
        if (abs_max < *max_val)
            abs_max = *max_val;

        *min_val = -abs_max;
        *max_val =  abs_max;
    }
}

static inline void FastadcSymmMinMaxDbl(double *min_val, double *max_val)
{
  double  abs_max;

    if (*min_val < 0  &&  *max_val > 0)
    {
        abs_max = -*min_val;
        if (abs_max < *max_val)
            abs_max = *max_val;

        *min_val = -abs_max;
        *max_val =  abs_max;
    }
}

/********************************************************************/

void
FastadcDataFillStdDscr(fastadc_type_dscr_t *atd, const char *type_name,
                       int behaviour,
                       pzframe_chan_dscr_t *chan_dscrs, int chan_count,
                       /* ADC characteristics */
                       int max_numpts, int num_lines,
                       int common_cur_numpts_cn,
                       int common_cur_ptsofs_cn,
                       int xs_per_point_cn,
                       int xs_divisor_cn,
                       int xs_factor_cn,
                       fastadc_info2mes_t info2mes,
                       fastadc_line_dscr_t *line_dscrs,
                       /* Data specifics */
                       const char  *line_name_prefix,
                       const char *common_dpyfmt,
                       fastadc_x2xs_t x2xs, const char *xs);

void FastadcDataInit       (fastadc_data_t *adc, fastadc_type_dscr_t *atd);
int  FastadcDataRealize    (fastadc_data_t *adc,
                            cda_context_t   present_cid,
                            const char     *base);
int  FastadcDataCalcDpyFmts(fastadc_data_t *adc);

psp_paramdescr_t *FastadcDataCreateText2DcnvTable(fastadc_type_dscr_t *atd,
                                                  char **mallocd_buf_p);

//--------------------------------------------------------------------

int    FastadcDataX2XS   (fastadc_mes_t *mes_p, int x);

double FastadcDataPvl2Dsp(fastadc_data_t *adc,
                          int nl, double pvl);
double FastadcDataGetDsp (fastadc_data_t *adc,
                          fastadc_mes_t  *mes_p,
                          int nl, int x);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FASTADC_DATA_H */
