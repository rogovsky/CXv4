#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "misclib.h"
#include "timeval_utils.h"

#include "wirebpm_data.h"

static void DoEvproc(pzframe_data_t *pfr,
                     int             reason,
                     int             info_int,
                     void           *privptr)
{
  wirebpm_data_t *bpm = pzframe2wirebpm_data(pfr);

    bpm->mes.ext_i = bpm->ext_i_val;
}

static int  DoSave  (pzframe_data_t *pfr,
                     const char     *filespec,
                     const char     *subsys, const char *comment)
{
//    return WirebpmDataSave(pzframe2wirebpm_data(pfr), filespec,
//                           subsys, comment);
}

static int  DoLoad  (pzframe_data_t *pfr,
                     const char     *filespec)
{
//    return WirebpmDataLoad(pzframe2wirebpm_data(pfr), filespec);
}

static int  DoFilter(pzframe_data_t *pfr,
                     const char     *filespec,
                     time_t         *cr_time,
                     char           *commentbuf, size_t commentbuf_size)
{
//    return WirebpmDataFilter(pfr, filespec, cr_time,
//                             commentbuf, commentbuf_size);
}

pzframe_data_vmt_t wirebpm_data_std_pzframe_vmt =
{
    .evproc = DoEvproc,
    .save   = DoSave,
    .load   = DoLoad,
    .filter = DoFilter,
};
void
WirebpmDataFillStdDscr(wirebpm_type_dscr_t *wtd, const char *type_name,
                       int behaviour,
                       pzframe_chan_dscr_t *chan_dscrs, int chan_count,
                       /* Data characteristics */
                       int data_cn)
{
    bzero(wtd, sizeof(*wtd));
    PzframeDataFillDscr(&(wtd->ftd), type_name,
                        behaviour | PZFRAME_B_NO_SVD,
                        chan_dscrs, chan_count);
    wtd->ftd.vmt = wirebpm_data_std_pzframe_vmt;

    if      (reprof_cxdtype(chan_dscrs[data_cn].dtype) != CXDTYPE_REPR_INT)
    {
        fprintf(stderr,
                "%s %s(type_name=\"%s\"): unsupported data-representation %d\n",
                strcurtime(), __FUNCTION__, type_name,
                reprof_cxdtype(chan_dscrs[data_cn].dtype));
    }

    wtd->data_cn   = data_cn;

}

void WirebpmDataInit       (wirebpm_data_t *bpm, wirebpm_type_dscr_t *wtd)
{
    bzero(bpm, sizeof(*bpm));
    bpm->wtd = wtd;
    PzframeDataInit(&(bpm->pfr), &(wtd->ftd));
}

static void ExtIEvproc(int            uniq,
                       void          *privptr1,
                       cda_dataref_t  ref,
                       int            reason,
                       void          *info_ptr,
                       void          *privptr2)
{
  wirebpm_data_t *bpm = privptr2;

    cda_process_ref (bpm->ext_i_ref,
                     CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC,
                     NAN,
                     NULL, 0);
    cda_get_ref_dval(bpm->ext_i_ref,
                     &(bpm->ext_i_val),
                     NULL, NULL,
                     NULL, NULL);
}
int  WirebpmDataRealize    (wirebpm_data_t *bpm,
                            cda_context_t   present_cid,
                            const char     *base)
{
  int                  r;

    /* 0. Call parent */
    r = PzframeDataRealize(&(bpm->pfr), present_cid, base);
    if (r != 0) return r;

    if (bpm->dcnv.ext_i_fla[0] != '\0')
    {
        bpm->ext_i_ref = cda_add_formula(bpm->pfr.cid,
                                         base,
                                         bpm->dcnv.ext_i_fla,
                                         CDA_OPT_RD_FLA,
                                         NULL, 0,
                                         CDA_REF_EVMASK_UPDATE,
                                         ExtIEvproc, bpm);
    }

    return 0;
}

psp_paramdescr_t *WirebpmDataGetDcnvTable(void)
{
  static psp_paramdescr_t table[] =
  {
      PSP_P_STRING ("ext_i_fla", wirebpm_dcnv_t, ext_i_fla, NULL),
      PSP_P_END()
  };

    return table;
}
