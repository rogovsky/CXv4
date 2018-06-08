#ifndef __VCAMIMG_DATA_H
#define __VCAMIMG_DATA_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_data.h"


/********************************************************************/
struct _vcamimg_data_t_struct;
/********************************************************************/

typedef struct
{
    pzframe_type_dscr_t    ftd;

    int                    data_cn;

    int                    data_w;
    int                    data_h;
    int                    show_w;
    int                    show_h;
    int                    sofs_x;
    int                    sofs_y;

    int                    bpp;
    int                    srcmaxval;
} vcamimg_type_dscr_t;

static inline vcamimg_type_dscr_t *pzframe2vcamimg_type_dscr(pzframe_type_dscr_t *ftd)
{
    return (ftd == NULL)? NULL
        :(vcamimg_type_dscr_t *)(ftd); /*!!! Should use OFFSETOF() -- (uint8*)ftd-OFFSETOF(vcamimg_type_dscr_t, ftd) */
}

typedef struct _vcamimg_data_t_struct
{
    pzframe_data_t         pfr;
    vcamimg_type_dscr_t   *vtd;  // Should be =.pfr.ftd
} vcamimg_data_t;

static inline vcamimg_data_t *pzframe2vcamimg_data(pzframe_data_t *pzframe_data)
{
    return (pzframe_data == NULL)? NULL
        : (vcamimg_data_t *)(pzframe_data); /*!!! Should use OFFSETOF() -- (uint8*)pzframe_data-OFFSETOF(vcamimg_data_t, pfr) */
}

/********************************************************************/

void
VcamimgDataFillStdDscr(vcamimg_type_dscr_t *vtd, const char *type_name,
                       int behaviour,
                       pzframe_chan_dscr_t *chan_dscrs, int chan_count,
                       /* Image characteristics */
                       int data_cn,
                       int data_w, int data_h,
                       int show_w, int show_h,
                       int sofs_x, int sofs_y,
                       int bpp,
                       int srcmaxval);

void VcamimgDataInit       (vcamimg_data_t *vci, vcamimg_type_dscr_t *vtd);
int  VcamimgDataRealize    (vcamimg_data_t *vci,
                            cda_context_t   present_cid,
                            const char     *base);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __VCAMIMG_DATA_H */
