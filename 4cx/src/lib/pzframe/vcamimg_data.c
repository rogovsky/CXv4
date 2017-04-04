#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "misclib.h"
#include "timeval_utils.h"

#include "vcamimg_data.h"


static int  DoSave  (pzframe_data_t *pfr,
                     const char     *filespec,
                     const char     *subsys, const char *comment)
{
//    return VcamimgDataSave(pzframe2vcamimg_data(pfr), filespec,
//                           subsys, comment);
}

static int  DoLoad  (pzframe_data_t *pfr,
                     const char     *filespec)
{
//    return VcamimgDataLoad(pzframe2vcamimg_data(pfr), filespec);
}

static int  DoFilter(pzframe_data_t *pfr,
                     const char     *filespec,
                     time_t         *cr_time,
                     char           *commentbuf, size_t commentbuf_size)
{
//    return VcamimgDataFilter(pfr, filespec, cr_time,
//                             commentbuf, commentbuf_size);
}

pzframe_data_vmt_t vcamimg_data_std_pzframe_vmt =
{
    .evproc = NULL,
    .save   = DoSave,
    .load   = DoLoad,
    .filter = DoFilter,
};
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
                       int srcmaxval)
{
    bzero(vtd, sizeof(*vtd));
    PzframeDataFillDscr(&(vtd->ftd), type_name,
                        behaviour,
                        chan_dscrs, chan_count);
    vtd->ftd.vmt = vcamimg_data_std_pzframe_vmt;

    if      (reprof_cxdtype(chan_dscrs[data_cn].dtype) != CXDTYPE_REPR_INT)
    {
        fprintf(stderr,
                "%s %s(type_name=\"%s\"): unsupported data-representation %d\n",
                strcurtime(), __FUNCTION__, type_name,
                reprof_cxdtype(chan_dscrs[data_cn].dtype));
    }

    vtd->data_cn   = data_cn;

    vtd->data_w    = data_w;
    vtd->data_h    = data_h;
    vtd->show_w    = show_w;
    vtd->show_h    = show_h;
    vtd->sofs_x    = sofs_x;
    vtd->sofs_y    = sofs_y;

    vtd->bpp       = bpp;
    vtd->srcmaxval = srcmaxval;
}

void VcamimgDataInit       (vcamimg_data_t *vci, vcamimg_type_dscr_t *vtd)
{
    bzero(vci, sizeof(*vci));
    vci->vtd = vtd;
    PzframeDataInit(&(vci->pfr), &(vtd->ftd));
}

int  VcamimgDataRealize    (vcamimg_data_t *vci,
                            cda_context_t   present_cid,
                            const char     *base)
{
  int                  r;

    /* 0. Call parent */
    r = PzframeDataRealize(&(vci->pfr), present_cid, base);
    if (r != 0) return r;

    return 0;
}
