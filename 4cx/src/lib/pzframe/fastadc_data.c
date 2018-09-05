#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "misclib.h"
#include "timeval_utils.h"

#include "fastadc_data.h"


int               fastadc_data_magn_factors[7]  = {1, 2, 4, 10, 20, 50, 100};
int               fastadc_data_cmpr_factors[13] = {-100, -50, -20, -10, -4, -2,
                                                   1, 2, 4, 10, 20, 50, 100};


//////////////////////////////////////////////////////////////////////

static void ExtXsPerPointEvproc(int            uniq,
                                void          *privptr1,
                                cda_dataref_t  ref,
                                int            reason,
                                void          *info_ptr,
                                void          *privptr2)
{
  fastadc_data_t *adc = privptr2;

    if (reason == CDA_REF_R_RSLVSTAT  &&  ptr2lint(info_ptr) == CDA_RSLVSTAT_NOTFOUND)
    {
        adc->ext_xs_per_point_val = 0;
        fprintf(stderr, "%s %s(): ext_xs_per_point source \"%s\": channel not found\n",
                strcurtime(), __FUNCTION__,
                adc->dcnv.ext_xs_per_point_src);
    }
    else if (reason == CDA_REF_R_UPDATE)
    {
        cda_get_ref_data(ref, 0, sizeof(adc->ext_xs_per_point_val), &(adc->ext_xs_per_point_val));
    }
}

#define DEFINE_MINMAX_FINDER(dtype, rtype)                \
static void FindMinMax_##dtype(dtype *buf, int numpts,    \
                              rtype *min_p, rtype *max_p) \
{                                                         \
  register dtype *p      = buf;                           \
  register int     count = numpts;                        \
  register rtype   min_v;                                 \
  register rtype   max_v;                                 \
                                                          \
    if (count == 0)                                       \
    {                                                     \
        *min_p = -1;                                      \
        *max_p = +1;                                      \
        return;                                           \
    }                                                     \
    for (min_v = max_v = *p; count > 0; p++, count--)     \
    {                                                     \
        if (*p < min_v) min_v = *p;                       \
        if (*p > max_v) max_v = *p;                       \
    }                                                     \
    *min_p = min_v;                                       \
    *max_p = max_v;                                       \
}
DEFINE_MINMAX_FINDER(uint32,  int)
DEFINE_MINMAX_FINDER(uint16,  int)
DEFINE_MINMAX_FINDER(uint8,   int)
DEFINE_MINMAX_FINDER( int32,  int)
DEFINE_MINMAX_FINDER( int16,  int)
DEFINE_MINMAX_FINDER( int8,   int)
DEFINE_MINMAX_FINDER(float64, double)
DEFINE_MINMAX_FINDER(float32, double)

static void ProcessAdcInfo(fastadc_data_t *adc)
{
  fastadc_type_dscr_t *atd = adc->atd;
  pzframe_data_t      *pfr = &(adc->pfr);

  int                  nl;

  int                  numpts;
  int                  is_on;
  cxdtype_t            dtype;

  int                  cn;

    for (nl = 0, adc->mes.cur_numpts = 0;  nl < atd->num_lines; nl++)
    {
        if (atd->line_dscrs[nl].data_cn >= 0)
        {
            /* 1. numpts */
            /* a. Get */
            if      (atd->line_dscrs[nl].cur_numpts_cn >= 0)
                PzframeDataGetChanInt(pfr,
                                      atd->line_dscrs[nl].cur_numpts_cn,
                                      &numpts);
            else if (atd->line_dscrs[nl].cur_numpts_cn < -1)
                numpts = -atd->line_dscrs[nl].cur_numpts_cn;
            else
                PzframeDataGetChanInt(pfr,
                                      atd->common_cur_numpts_cn,
                                      &numpts);
            /* b. Limit */
            if (numpts < 0)
                numpts = 0;
            if (numpts > atd->max_numpts)
                numpts = atd->max_numpts;
            /* c. Is biggest? */
            if (adc->mes.cur_numpts < numpts)
                adc->mes.cur_numpts = numpts;
            /* d. Store */
            adc->mes.plots[nl].numpts = numpts;

            /* 2. on */
            if     (atd->line_dscrs[nl].is_on_cn >= 0)
                PzframeDataGetChanInt(pfr,
                                      atd->line_dscrs[nl].is_on_cn,
                                      &is_on);
            else
                is_on = 1;
            adc->mes.plots[nl].on = (is_on != 0);

            /* 3. x_buf, x_buf_dtype; 4. range */
            adc->mes.plots[nl].cur_range       = atd->line_dscrs[nl].range;
            if (adc->mes.plots[nl].on)
            {
                adc->mes.plots[nl].x_buf       = 
                    pfr->cur_data      [atd->line_dscrs[nl].data_cn].current_val;
                adc->mes.plots[nl].x_buf_dtype = dtype =
                    atd->ftd.chan_dscrs[atd->line_dscrs[nl].data_cn].dtype;

                if (atd->line_dscrs[nl].range_min_cn >= 0)
                {
                    if (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT)
                        PzframeDataGetChanInt(pfr,
                                              atd->line_dscrs[nl].range_min_cn,
                                              adc->mes.plots[nl].cur_range.int_r + 0);
                    else
                        PzframeDataGetChanDbl(pfr,
                                              atd->line_dscrs[nl].range_min_cn,
                                              adc->mes.plots[nl].cur_range.dbl_r + 0);
                }
                if (atd->line_dscrs[nl].range_max_cn >= 0)
                {
                    if (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT)
                        PzframeDataGetChanInt(pfr,
                                              atd->line_dscrs[nl].range_max_cn,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                    else
                        PzframeDataGetChanDbl(pfr,
                                              atd->line_dscrs[nl].range_max_cn,
                                              adc->mes.plots[nl].cur_range.dbl_r + 1);
                }
                if (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT)
                {
                    /* a. If range is absent, find it from data */
                    if (adc->mes.plots[nl].cur_range.int_r[0] ==
                        adc->mes.plots[nl].cur_range.int_r[1])
                    {
                        if      (dtype == CXDTYPE_INT32)
                            FindMinMax_int32 (adc->mes.plots[nl].x_buf, numpts,
                                              adc->mes.plots[nl].cur_range.int_r + 0,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                        else if (dtype == CXDTYPE_INT16)
                            FindMinMax_int16 (adc->mes.plots[nl].x_buf, numpts,
                                              adc->mes.plots[nl].cur_range.int_r + 0,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                        else if (dtype == CXDTYPE_INT8)
                            FindMinMax_int8  (adc->mes.plots[nl].x_buf, numpts,
                                              adc->mes.plots[nl].cur_range.int_r + 0,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                        else if (dtype == CXDTYPE_UINT32)
                            FindMinMax_uint32(adc->mes.plots[nl].x_buf, numpts,
                                              adc->mes.plots[nl].cur_range.int_r + 0,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                        else if (dtype == CXDTYPE_UINT16)
                            FindMinMax_uint16(adc->mes.plots[nl].x_buf, numpts,
                                              adc->mes.plots[nl].cur_range.int_r + 0,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                        else /* (dtype == CXDTYPE_UINT8) */
                            FindMinMax_uint8 (adc->mes.plots[nl].x_buf, numpts,
                                              adc->mes.plots[nl].cur_range.int_r + 0,
                                              adc->mes.plots[nl].cur_range.int_r + 1);
                        pfr->other_info_changed = 1;
                    }
                    /* b. Symmetrize */
                    FastadcSymmMinMaxInt(adc->mes.plots[nl].cur_range.int_r + 0,
                                         adc->mes.plots[nl].cur_range.int_r + 1);
                }
                else
                {
                    /* a. If range is absent, find it from data */
                    if (adc->mes.plots[nl].cur_range.dbl_r[0] ==
                        adc->mes.plots[nl].cur_range.dbl_r[1])
                    {
                        if      (dtype == CXDTYPE_DOUBLE)
                            FindMinMax_float64(adc->mes.plots[nl].x_buf, numpts,
                                               adc->mes.plots[nl].cur_range.dbl_r + 0,
                                               adc->mes.plots[nl].cur_range.dbl_r + 1);
                        else /* (dtype == CXDTYPE_SINGLE) */
                            FindMinMax_float32(adc->mes.plots[nl].x_buf, numpts,
                                               adc->mes.plots[nl].cur_range.dbl_r + 0,
                                               adc->mes.plots[nl].cur_range.dbl_r + 1);
                        pfr->other_info_changed = 1;
                    }
                    /* b. Symmetrize */
////fprintf(stderr, "[%d].dbl_r==; :=%e,%e\n", nl, adc->mes.plots[nl].cur_range.dbl_r[0], adc->mes.plots[nl].cur_range.dbl_r[1]);
                    FastadcSymmMinMaxDbl(adc->mes.plots[nl].cur_range.dbl_r + 0,
                                         adc->mes.plots[nl].cur_range.dbl_r + 1);
////fprintf(stderr, "    :=%e,%e\n", nl, adc->mes.plots[nl].cur_range.dbl_r[0], adc->mes.plots[nl].cur_range.dbl_r[1]);
                }
            }
            else
            {
                adc->mes.plots[nl].x_buf       = NULL;
                adc->mes.plots[nl].x_buf_dtype = CXDTYPE_INT32;
                adc->mes.plots[nl].numpts      = 0;
            }
        }
    }
////fprintf(stderr, "%s %s numpts=%d\n", strcurtime(), __FUNCTION__, adc->mes.cur_numpts);

if(0+1)
{
    cn = atd->xs_per_point_cn;
    if      (cn >= 0) PzframeDataGetChanInt(&(adc->pfr), cn, &(adc->mes.xs_per_point));
    else if (cn < -1) adc->mes.xs_per_point = -cn;
    else              adc->mes.xs_per_point = 0;

    cn = atd->xs_divisor_cn;
    if      (cn >= 0) PzframeDataGetChanInt(&(adc->pfr), cn, &(adc->mes.xs_divisor));
    else if (cn < -1) adc->mes.xs_divisor = -cn;
    else              adc->mes.xs_divisor = 0;

    cn = atd->xs_factor_cn;
    if      (cn >= 0) PzframeDataGetChanInt(&(adc->pfr), cn, &(adc->mes.xs_factor));
    else if (cn < -1) adc->mes.xs_factor = -cn;
    else              adc->mes.xs_factor = 0;

    cn = atd->common_cur_ptsofs_cn;
    if      (cn >= 0) PzframeDataGetChanInt(&(adc->pfr), cn, &(adc->mes.cur_ptsofs));
    else              adc->mes.cur_ptsofs = 0;

    /* Note: no need to compare with previous value (to signal info_changed)
       because source channel should be marked with change_important=1,
       or this is just a constant (if xs_per_point_cn<0) */
    adc->mes.exttim = (adc->mes.xs_per_point == 0);

    if (adc->mes.ext_xs_per_point != adc->ext_xs_per_point_val)
    {
        adc->mes.ext_xs_per_point  = adc->ext_xs_per_point_val;
        if (adc->mes.exttim) pfr->other_info_changed = 1;
    }
}

    /* Call ADC-type-specific processing */
    if (atd->info2mes != NULL)
        atd->info2mes(adc, adc->pfr.cur_data, &(adc->mes));
}

//////////////////////////////////////////////////////////////////////
static
int  PzframeDataPutChanInt(pzframe_data_t *pfr, int cn, int    val)
{
    if (cn < 0  ||  cn >= pfr->ftd->chan_count)
    {
        errno = EINVAL;
        return -1;
    }
    if (pfr->ftd->chan_dscrs[cn].max_nelems != 1  &&
        pfr->ftd->chan_dscrs[cn].max_nelems != 0 /*==0 means 1*/)
    {
        errno = EDOM;
        return -1;
    }

    switch (pfr->ftd->chan_dscrs[cn].dtype)
    {
        case CXDTYPE_INT8:   pfr->cur_data[cn].valbuf.i8  = val; break;
        case CXDTYPE_INT16:  pfr->cur_data[cn].valbuf.i16 = val; break;
        case 0: /* 0 means INT32 */
        case CXDTYPE_INT32:  pfr->cur_data[cn].valbuf.i32 = val; break;
        case CXDTYPE_UINT8:  pfr->cur_data[cn].valbuf.u8  = val; break;
        case CXDTYPE_UINT16: pfr->cur_data[cn].valbuf.u16 = val; break;
        case CXDTYPE_UINT32: pfr->cur_data[cn].valbuf.u32 = val; break;
#if MAY_USE_INT64  ||  1
        case CXDTYPE_INT64:  pfr->cur_data[cn].valbuf.i64 = val; break;
        case CXDTYPE_UINT64: pfr->cur_data[cn].valbuf.u64 = val; break;
#endif /* MAY_USE_INT64 */
        case CXDTYPE_SINGLE: pfr->cur_data[cn].valbuf.f32 = val; break;
        case CXDTYPE_DOUBLE: pfr->cur_data[cn].valbuf.f64 = val; break;
        default:
            errno = ERANGE;
            return -1;
    }

    return 0;
}

static
int  PzframeDataPutChanDbl(pzframe_data_t *pfr, int cn, double val)
{
    if (cn < 0  ||  cn >= pfr->ftd->chan_count)
    {
        errno = EINVAL;
        return -1;
    }
    if (pfr->ftd->chan_dscrs[cn].max_nelems != 1  &&
        pfr->ftd->chan_dscrs[cn].max_nelems != 0 /*==0 means 1*/)
    {
        errno = EDOM;
        return -1;
    }

    switch (pfr->ftd->chan_dscrs[cn].dtype)
    {
        case CXDTYPE_INT8:   pfr->cur_data[cn].valbuf.i8  = val; break;
        case CXDTYPE_INT16:  pfr->cur_data[cn].valbuf.i16 = val; break;
        case 0: /* 0 means INT32 */
        case CXDTYPE_INT32:  pfr->cur_data[cn].valbuf.i32 = val; break;
        case CXDTYPE_UINT8:  pfr->cur_data[cn].valbuf.u8  = val; break;
        case CXDTYPE_UINT16: pfr->cur_data[cn].valbuf.u16 = val; break;
        case CXDTYPE_UINT32: pfr->cur_data[cn].valbuf.u32 = val; break;
#if MAY_USE_INT64  ||  1
        case CXDTYPE_INT64:  pfr->cur_data[cn].valbuf.i64 = val; break;
        case CXDTYPE_UINT64: pfr->cur_data[cn].valbuf.u64 = val; break;
#endif /* MAY_USE_INT64 */
        case CXDTYPE_SINGLE: pfr->cur_data[cn].valbuf.f32 = val; break;
        case CXDTYPE_DOUBLE: pfr->cur_data[cn].valbuf.f64 = val; break;
        default:
            errno = ERANGE;
            return -1;
    }

    return 0;
}
//////////////////////////////////////////////////////////////////////

static void fprintf_s_printable(FILE *fp, const char *s)
{
    if (s == NULL) return;
    while (*s != '\0')
    {
        fprintf(fp, "%c", isprint(*s)? *s : '?');
        s++;
    }
}

static void rtrim_line(char *line)
{
  char *p = line + strlen(line) - 1;

    while (p > line  &&
           (*p == '\n'  ||  isspace(*p)))
        *p-- = '\0';
}

static const char *sign_lp_s    = "#=Data:";
static const char *subsys_lp_s  = "#=SUBSYSTEM:";
static const char *crtime_lp_s  = "#=CREATE-TIME:";
static const char *comment_lp_s = "#=COMMENT:";
static const char *param_lp_s   = "#=PARAM";


static
int  FastadcDataSave       (fastadc_data_t *adc, const char *filespec,
                            const char *subsys, const char *comment)
{
  fastadc_type_dscr_t *atd = adc->atd;
  pzframe_type_dscr_t *ftd = adc->pfr.ftd;

  FILE        *fp;
  time_t       timenow = time(NULL);

  int          np;
  int          nl;
  int          x;

  cxdtype_t    dtype;
  int          max_nelems;
  int          ival;
  double       fval;

    if ((fp = fopen(filespec, "w")) == NULL) return -1;

    /* Standard header */
    fprintf(fp, "%s%s\n", sign_lp_s, atd->ftd.type_name);
    if (subsys != NULL)
    {
        fprintf(fp, "%s ", subsys_lp_s);
        fprintf_s_printable(fp, subsys);
        fprintf(fp, "\n");
    }
    fprintf(fp, "%s %lu %s", crtime_lp_s, (unsigned long)(timenow), ctime(&timenow)); /*!: No '\n' because ctime includes a trailing one. */
    if (comment != NULL)
    {
        fprintf(fp, "%s ", comment_lp_s);
        fprintf_s_printable(fp, comment);
        fprintf(fp, "\n");
    }

    /* Parameters */
    for (np = 0;  np < ftd->chan_count;  np++)
        if ( ftd->chan_dscrs[np].name != NULL  &&
            (ftd->chan_dscrs[np].chan_type & PZFRAME_CHAN_TYPE_MASK) == PZFRAME_CHAN_IS_PARAM)
        {
            dtype      = ftd->chan_dscrs[np].dtype;
            max_nelems = ftd->chan_dscrs[np].max_nelems;
            if (dtype      == 0) dtype      = CXDTYPE_INT32;
            if (max_nelems == 0) max_nelems = 1;

            if (max_nelems == 1)
            {
                if      (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT)
                {
                    PzframeDataGetChanInt(&(adc->pfr), np, &ival);
                    fprintf(fp, "%s %s %d\n", param_lp_s, ftd->chan_dscrs[np].name, ival);
                }
                else if (reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT)
                {
                    PzframeDataGetChanDbl(&(adc->pfr), np, &fval);
                    fprintf(fp, "%s %s %e\n", param_lp_s, ftd->chan_dscrs[np].name, fval);
                }
            }
        }

    /* Data */
    /* Header... */
    fprintf(fp, "\n# %7s %8s", "N", adc->mes.exttim? "n" : atd->xs);
    for (nl = 0;  nl < atd->num_lines;  nl++)
        fprintf(fp, " c%d", nl);
    for (nl = 0;  nl < atd->num_lines;  nl++)
        fprintf(fp, " v%d", nl);
    fprintf(fp, "\n");
    /* ...data itself */
    for (x = 0;  x < adc->mes.cur_numpts;  x++)
    {
        fprintf(fp, " %6d %d", x, FastadcDataX2XS(&(adc->mes), x));
        for (nl = 0;  nl < atd->num_lines;  nl++)
        {
            fprintf(fp, " ");
            if (adc->mes.plots[nl].on  &&  adc->mes.plots[nl].numpts > x)
            {
                if (reprof_cxdtype(adc->mes.plots[nl].x_buf_dtype) == CXDTYPE_REPR_INT)
                    fprintf(fp, "%9d",
                            plotdata_get_int(adc->mes.plots + nl, x));
                else
                    fprintf(fp, adc->dcnv.lines[nl].dpyfmt,
                            plotdata_get_dbl(adc->mes.plots + nl, x));
            }
            else
                fprintf(fp, "0");
        }
        for (nl = 0;  nl < atd->num_lines;  nl++)
        {
            fprintf(fp, " ");
            if (adc->mes.plots[nl].on  &&  adc->mes.plots[nl].numpts > x)
                fprintf(fp, adc->dcnv.lines[nl].dpyfmt,
                        FastadcDataGetDsp(adc, &(adc->mes), nl, x));
            else
                fprintf(fp, "0");
        }
        fprintf(fp, "\n");
    }

    fclose(fp);

    return 0;
}

static void ZeroAllData(fastadc_data_t *adc)
{
  pzframe_data_t      *pfr = &(adc->pfr);
  pzframe_type_dscr_t *ftd = pfr->ftd;
  int                  np;

  cxdtype_t            dtype;
  int                  max_nelems;
  size_t               valsize;
  void                *dst;

    for (np = 0;  np < ftd->chan_count;  np++)
        if (ftd->chan_dscrs[np].name != NULL)
        {
            dtype      = ftd->chan_dscrs[np].dtype;
            max_nelems = ftd->chan_dscrs[np].max_nelems;
            if (dtype      == 0) dtype      = CXDTYPE_INT32;
            if (max_nelems == 0) max_nelems = 1;
            valsize = sizeof_cxdtype(dtype) * max_nelems;

            if (valsize > sizeof(pfr->cur_data[np].valbuf))
                dst = pfr->cur_data[np].current_val;
            else
                dst = &(pfr->cur_data[np].valbuf);

            if (valsize != 0)
                bzero(dst, valsize);
            pfr->cur_data[np].rflags = 0;
            bzero(     &(pfr->cur_data[np].timestamp),
                  sizeof(pfr->cur_data[np].timestamp));
        }

    pfr->rflags = 0;
}
static
int  FastadcDataLoad       (fastadc_data_t *adc, const char *filespec)
{
#if 1
  fastadc_type_dscr_t *atd = adc->atd;
  pzframe_type_dscr_t *ftd = adc->pfr.ftd;

  FILE        *fp;
  char         line[1000];
  int          lineno;
  int          data_started;

  char        *p;
  char        *endp;
  char         name[100];
  size_t       namelen;

  int          np;
  cxdtype_t    dtype;
  int          max_nelems;
  int          ival = 0;  // Only to prevent "may be used uninitialized"
  double       fval = 0;  // Only to prevent "may be used uninitialized"
  int          x;
  int          nl;

    /* Try to open file and read the 1st line */
    if ((fp = fopen(filespec, "r")) == NULL  ||
        fgets(line, sizeof(line) - 1, fp)  == NULL)
    {
        if (fp != NULL) fclose(fp);
        return -1;
    }

    /* Check signature */
    rtrim_line(line);
    if (memcmp(line, sign_lp_s,   strlen(sign_lp_s))   != 0    ||
        strcasecmp(line + strlen(sign_lp_s), atd->ftd.type_name) != 0)
    {
        fprintf(stderr, "%s %s(\"%s\"): sig mismatch\n",
                strcurtime(), __FUNCTION__, filespec);
        errno = 0;
        return -1;
    }

    /* Initialize everything with zeros... */
    ZeroAllData(adc);

    lineno = 1;
    data_started = 0;
    x            = 0;
    while (fgets(line, sizeof(line) - 1, fp) != NULL)
    {
        lineno++;
        rtrim_line(line);
        p = line; while (*p != '\0'  &&  isspace(*p)) p++;

        if (*p == '\0') goto NEXT_LINE;

        if (memcmp(p, param_lp_s, strlen(param_lp_s)) == 0)
        {
            if (data_started)
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: parameter AFTER data\n",
                        strcurtime(), __FUNCTION__, filespec, lineno);
                goto PARSE_ERROR;
            }

            /* Get np */
            /*  a. Get name */
            p += strlen(param_lp_s);
            while (*p != '\0'  &&  isspace(*p)) p++;             // leading spaces
            for (endp = p;  *endp != '\0'  &&  !isspace(*endp);  endp++); // name itself
            namelen = endp - p;
            if (namelen > sizeof(name) - 1)
                namelen = sizeof(name) - 1;
            if (namelen == 0) /* EOL right after "#=PARAM"? */
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: error parsing parameter-name\n",
                        strcurtime(), __FUNCTION__, filespec, lineno);
                goto PARSE_ERROR;
            }
            memcpy(name, p, namelen); name[namelen] = '\0';
            p = endp;

            /*  b. Find np */
            for (np = 0;  np < ftd->chan_count;  np++)
                if ( ftd->chan_dscrs[np].name != NULL  &&
                    (ftd->chan_dscrs[np].chan_type & PZFRAME_CHAN_TYPE_MASK)
                     == PZFRAME_CHAN_IS_PARAM)
                {
                    dtype      = ftd->chan_dscrs[np].dtype;
                    max_nelems = ftd->chan_dscrs[np].max_nelems;
                    if (dtype      == 0) dtype      = CXDTYPE_INT32;
                    if (max_nelems == 0) max_nelems = 1;

                    if (max_nelems == 1  &&
                        (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT  ||
                         reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT)  &&
                        strcasecmp(name, ftd->chan_dscrs[np].name) == 0)
                        goto END_PARAM_SEARCH;
                }
 END_PARAM_SEARCH:;

            if (np >= ftd->chan_count)
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: unknown parameter \"%s\"\n",
                        strcurtime(), __FUNCTION__, filespec, lineno, name);
                goto PARSE_ERROR;
            }

            /* Get value */
            while (*p != '\0'  &&  isspace(*p)) p++;
            if      (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT)
            {
                ival = strtol(p, &endp, 0);
                if (endp == p  ||  (*endp != '\0'  &&  !isspace(*endp)))
                {
                    fprintf(stderr, "%s %s(\"%s\") line %d: error parsing int-parameter-value of \"%s\"\n",
                            strcurtime(), __FUNCTION__, filespec, lineno, name);
                    goto PARSE_ERROR;
                }
                PzframeDataPutChanInt(&(adc->pfr), np, ival);
            }
            else if (reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT)
            {
                fval = strtod(p, &endp);
                if (endp == p  ||  (*endp != '\0'  &&  !isspace(*endp)))
                {
                    fprintf(stderr, "%s %s(\"%s\") line %d: error parsing flt-parameter-value of \"%s\"\n",
                            strcurtime(), __FUNCTION__, filespec, lineno, name);
                    goto PARSE_ERROR;
                }
                PzframeDataPutChanDbl(&(adc->pfr), np, fval);
            }
        }
        else if (*p == '#') goto NEXT_LINE;
        else if (!isdigit(*p))
        {
            fprintf(stderr, "%s %s(\"%s\") line %d: garbage\n",
                    strcurtime(), __FUNCTION__, filespec, lineno);
            goto PARSE_ERROR;
        }
        else
        {
            /* Okay -- that's data */

            if (!data_started)
            {
                ProcessAdcInfo(adc);
                data_started = 1;
            }

            if (x > adc->mes.cur_numpts)
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: too many measurements (x=%d), exceeding cur_numpts=%d\n",
                        strcurtime(), __FUNCTION__, filespec, lineno, x, adc->mes.cur_numpts);
                break;
            }

            /* Skip N... */
            while (*p != '\0'  &&  !isspace(*p)) p++;
            /* ...and xs */
            while (*p != '\0'  &&  isspace(*p))  p++;
            while (*p != '\0'  &&  !isspace(*p)) p++;

            for (nl = 0;  nl < atd->num_lines;  nl++)
            {
                while (*p != '\0'  &&  isspace(*p))  p++;
                if (reprof_cxdtype(adc->mes.plots[nl].x_buf_dtype) == CXDTYPE_REPR_INT)
                    ival = strtol(p, &endp, 0);
                else
                    fval = strtod(p, &endp);
                if (endp == p  ||  (*endp != '\0'  &&  !isspace(*endp)))
                {
                    fprintf(stderr, "%s %s(\"%s\") line %d: error parsing line#%d@x=%d\n",
                            strcurtime(), __FUNCTION__, filespec, lineno, nl, x);
                    goto PARSE_ERROR;
                }
                p = endp;
                if (adc->mes.plots[nl].on  &&  adc->mes.plots[nl].numpts > x)
                {
                    if (reprof_cxdtype(adc->mes.plots[nl].x_buf_dtype) == CXDTYPE_REPR_INT)
                        plotdata_put_int(adc->mes.plots + nl, x, ival);
                    else
                        plotdata_put_dbl(adc->mes.plots + nl, x, fval);
                }
            }
            x++;
        }

 NEXT_LINE:;
    }

    fclose(fp);

    /* A 2nd pass for min/max auto-scaling */
    ProcessAdcInfo(adc);

    return 0;

 PARSE_ERROR:;
    ZeroAllData(adc);
    ProcessAdcInfo(adc);
    errno = 0;
#endif
    return -1;
}

static
int  FastadcDataFilter     (pzframe_data_t *pfr,
                            const char *filespec,
                            time_t *cr_time,
                            char *commentbuf, size_t commentbuf_size)
{
  fastadc_data_t      *adc = pzframe2fastadc_data(pfr);
  fastadc_type_dscr_t *atd = adc->atd;

  FILE        *fp;
  char         line[1000];
  int          lineno;
  int          was_sign = 0;

  char        *cp;
  char        *err;
  time_t       sng_time;

    if ((fp = fopen(filespec, "r")) == NULL) return -1;

    if (commentbuf_size > 0) *commentbuf = '\0';

    lineno = 0;
    while (lineno < 10  &&  fgets(line, sizeof(line) - 1, fp) != NULL)
    {
        lineno++;
        rtrim_line(line);

        if      (memcmp(line, sign_lp_s,   strlen(sign_lp_s))   == 0    &&
                 strcasecmp(line + strlen(sign_lp_s), atd->ftd.type_name) == 0)
            was_sign = 1;
        else if (memcmp(line, crtime_lp_s, strlen(crtime_lp_s)) == 0)
        {
            cp = line + strlen(crtime_lp_s);
            while (isspace(*cp)) cp++;

            sng_time = (time_t)strtol(cp, &err, 0);
            if (err != cp  &&  (*err == '\0'  ||  isspace(*err)))
                *cr_time = sng_time;
        }
        else if (memcmp(line, comment_lp_s, strlen(comment_lp_s)) == 0)
        {
            cp = line + strlen(comment_lp_s);
            while (isspace(*cp)) cp++;

            strzcpy(commentbuf, cp, commentbuf_size);
        }
    }
    
    fclose(fp);

    return was_sign? 0 : 1;
}
//////////////////////////////////////////////////////////////////////

static void DoEvproc(pzframe_data_t *pfr,
                     int             reason,
                     int             info_int,
                     void           *privptr)
{
  fastadc_data_t *adc = pzframe2fastadc_data(pfr);

  int             nl;

    if      (reason == PZFRAME_REASON_DATA) ProcessAdcInfo(adc);
    else if (reason == PZFRAME_REASON_RDSCHG)
    {
        for (nl = 0;  nl < adc->atd->num_lines;  nl++)
            if (info_int == adc->atd->line_dscrs[nl].data_cn)
                adc->lines_rds_rcvd[nl] = 1;
    }
}

static int  DoSave  (pzframe_data_t *pfr,
                     const char     *filespec,
                     const char     *subsys, const char *comment)
{
    return FastadcDataSave(pzframe2fastadc_data(pfr), filespec,
                           subsys, comment);
}

static int  DoLoad  (pzframe_data_t *pfr,
                     const char     *filespec)
{
    return FastadcDataLoad(pzframe2fastadc_data(pfr), filespec);
}

static int  DoFilter(pzframe_data_t *pfr,
                     const char     *filespec,
                     time_t         *cr_time,
                     char           *commentbuf, size_t commentbuf_size)
{
    return FastadcDataFilter(pfr, filespec, cr_time,
                             commentbuf, commentbuf_size);
}

pzframe_data_vmt_t fastadc_data_std_pzframe_vmt =
{
    .evproc = DoEvproc,
    .save   = DoSave,
    .load   = DoLoad,
    .filter = DoFilter,
};
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
                       fastadc_x2xs_t x2xs, const char *xs)
{
    bzero(atd, sizeof(*atd));
    PzframeDataFillDscr(&(atd->ftd), type_name,
                        behaviour,
                        chan_dscrs, chan_count);
    atd->ftd.vmt = fastadc_data_std_pzframe_vmt;

    atd->max_numpts           = max_numpts;
    atd->num_lines            = num_lines;

    atd->common_cur_numpts_cn = common_cur_numpts_cn;
    atd->common_cur_ptsofs_cn = common_cur_ptsofs_cn;
    atd->xs_per_point_cn      = xs_per_point_cn;
    atd->xs_divisor_cn        = xs_divisor_cn;
    atd->xs_factor_cn         = xs_factor_cn;
    atd->info2mes             = info2mes;
    atd->line_dscrs           = line_dscrs;

    atd->line_name_prefix     = line_name_prefix;
    atd->common_dpyfmt        = common_dpyfmt;
    atd->x2xs                 = x2xs;
    atd->xs                   = xs;
}

void FastadcDataInit       (fastadc_data_t *adc, fastadc_type_dscr_t *atd)
{
  int  nl;

    bzero(adc, sizeof(*adc));
    adc->atd = atd;
    PzframeDataInit(&(adc->pfr), &(atd->ftd));

    for (nl = 0;  nl < atd->num_lines;  nl++)
    {
        adc->mes.plots[nl].x_buf_dtype = (atd->line_dscrs[nl].data_cn >= 0)?
            atd->ftd.chan_dscrs[atd->line_dscrs[nl].data_cn].dtype : CXDTYPE_INT32;

        /* Set default ranges */
        adc->mes.plots[nl].all_range   =
        adc->mes.plots[nl].cur_range   = atd->line_dscrs[nl].range;

        if (reprof_cxdtype(adc->mes.plots[nl].x_buf_dtype) == CXDTYPE_REPR_INT)
            FastadcSymmMinMaxInt(adc->mes.plots[nl].cur_range.int_r + 0,
                                 adc->mes.plots[nl].cur_range.int_r + 1);
        else
            FastadcSymmMinMaxDbl(adc->mes.plots[nl].cur_range.dbl_r + 0,
                                 adc->mes.plots[nl].cur_range.dbl_r + 1);
    }
}

static inline int notempty(const char *s)
{
    return s != NULL  &&  *s != '\0';
}
int  FastadcDataRealize    (fastadc_data_t *adc,
                            cda_context_t   present_cid,
                            const char     *base)
{
  fastadc_type_dscr_t *atd = adc->atd;

  int                  r;
  int                  nl;
  char                *p;

    /* 0. Call parent */
    r = PzframeDataRealize(&(adc->pfr), present_cid, base);
    if (r != 0) return r;

    /* 1. Prepare descriptions */
    for (nl = 0;  nl < adc->atd->num_lines;  nl++)
    {
        /* Create descriptions, if not specified by user... */
        if (        adc->dcnv.lines[nl].descr[0] == '\0')
        {
            sprintf(adc->dcnv.lines[nl].descr, "%s%s%s",
                    notempty(atd->line_name_prefix)? atd->line_name_prefix : "",
                    notempty(atd->line_name_prefix)? " "                   : "",
                    atd->line_dscrs[nl].name);
        }

        /* ...and sanitize 'em */
        for (p = adc->dcnv.lines[nl].descr;  *p != '\0';  p++)
            if (*p == '\''  ||  *p == '\"'  ||  *p == '\\'  ||  !isprint(*p))
                *p = '?';
    }            

    /* 2. DpyFmts */
    if ((r = FastadcDataCalcDpyFmts(adc)) < 0) return r;

    /* 3. External timing source? */
    if (adc->dcnv.ext_xs_per_point_src[0] != '\0')
    {
        adc->ext_xs_per_point_ref = cda_add_chan(adc->pfr.cid,
                                                 base,
                                                 adc->dcnv.ext_xs_per_point_src,
                                                 CDA_DATAREF_OPT_ON_UPDATE,
                                                 CXDTYPE_INT32, 1,
                                                 CDA_REF_EVMASK_UPDATE | CDA_REF_EVMASK_RSLVSTAT,
                                                 ExtXsPerPointEvproc, adc);
        if (adc->ext_xs_per_point_ref == CDA_DATAREF_ERROR)
            fprintf(stderr, "%s %s(): cda_add_chan(\"%s\"): %s\n",
                    strcurtime(), __FUNCTION__,
                    adc->dcnv.ext_xs_per_point_src, cda_last_err());
    }

    return 0;
}

int  FastadcDataCalcDpyFmts(fastadc_data_t *adc)
{
  int          nl;

  const char  *src_dpyfmt;
    
  int          dpy_flags;
  int          dpy_width;
  int          dpy_precision;
  char         dpy_conv_c;
  int          dpy_shift;

    for (nl = 0;  nl < adc->atd->num_lines;  nl++)
    {
        src_dpyfmt = adc->atd->line_dscrs[nl].dpyfmt;
        if (src_dpyfmt == NULL) 
            src_dpyfmt = adc->atd->common_dpyfmt;
        if (ParseDoubleFormat(src_dpyfmt,
                              &dpy_flags, &dpy_width, &dpy_precision, &dpy_conv_c) < 0)
        {
            fprintf(stderr, "%s %s(): ParseDoubleFormat(\"%s\"): %s\n",
                    strcurtime(), __FUNCTION__,
                    src_dpyfmt, printffmt_lasterr());
            ParseDoubleFormat("%8.3f",
                              &dpy_flags, &dpy_width, &dpy_precision, &dpy_conv_c);
        }

        dpy_shift = round(log10(/*!!!fabs(adc->dcnv.lines[nl].coeff)*/1) + 0.4);
        ////printf("shift=%d\n", dpy_shift);
        CreateDoubleFormat(adc->dcnv.lines[nl].dpyfmt, sizeof(adc->dcnv.lines[nl].dpyfmt), 20, 10,
                           dpy_flags,
                           dpy_width,
                           dpy_precision >= dpy_shift? dpy_precision - dpy_shift : 0,
                           dpy_conv_c);
    }

    return 0;
}

psp_paramdescr_t *FastadcDataCreateText2DcnvTable(fastadc_type_dscr_t *atd,
                                                  char **mallocd_buf_p)
{
  psp_paramdescr_t *ret           = NULL;
  int               ret_count;
  size_t            ret_size;

  int               x;
  int               nl;
  psp_paramdescr_t *srcp;
  psp_paramdescr_t *dstp;
  size_t            pcp_names_size;
  char             *pcp_names_buf = NULL;
  char             *pcp_names_p;

  static char      magn_lkp_labels[countof(fastadc_data_magn_factors)][10];
  static psp_lkp_t magn_lkp       [countof(fastadc_data_magn_factors) + 1];
  static char      cmpr_lkp_labels[countof(fastadc_data_cmpr_factors)][10];
  static psp_lkp_t cmpr_lkp       [countof(fastadc_data_cmpr_factors) + 1];
  static int       nnnn_lkps_inited = 0;

  static psp_paramdescr_t perchan_params[] =
  {
      PSP_P_FLAG   ("chan",     fastadc_dcnv_t, lines[0].show,  1, 1),
      PSP_P_FLAG   ("nochan",   fastadc_dcnv_t, lines[0].show,  0, 0),
      PSP_P_LOOKUP ("magn",     fastadc_dcnv_t, lines[0].magn,  1, magn_lkp),
      PSP_P_FLAG   ("straight", fastadc_dcnv_t, lines[0].invp,  0, 1),
      PSP_P_FLAG   ("inverse",  fastadc_dcnv_t, lines[0].invp,  1, 0),
      PSP_P_STRING ("units",    fastadc_dcnv_t, lines[0].units, "V"),
      PSP_P_STRING ("descr",    fastadc_dcnv_t, lines[0].descr, ""),
  };

  static psp_paramdescr_t end_params[] =
  {
      PSP_P_LOOKUP ("cmpr",             fastadc_dcnv_t, cmpr, 1, cmpr_lkp),
      PSP_P_STRING ("ext_xs_per_point", fastadc_dcnv_t, ext_xs_per_point_src, NULL),
      PSP_P_END()
  };

    if (!nnnn_lkps_inited)
    {
        for (x = 0;  x < countof(fastadc_data_magn_factors);  x++)
        {
            sprintf(magn_lkp_labels[x], "%d", fastadc_data_magn_factors[x]);
            magn_lkp[x].label = magn_lkp_labels          [x];
            magn_lkp[x].val   = fastadc_data_magn_factors[x];
        }
        for (x = 0;  x < countof(fastadc_data_cmpr_factors);  x++)
        {
            sprintf(cmpr_lkp_labels[x], "%d", fastadc_data_cmpr_factors[x]);
            cmpr_lkp[x].label = cmpr_lkp_labels          [x];
            cmpr_lkp[x].val   = fastadc_data_cmpr_factors[x];
        }
        nnnn_lkps_inited = 1;
    }

    /* Allocate a table */
    ret_count = countof(perchan_params) * atd->num_lines + countof(end_params);
    ret_size  = ret_count * sizeof(*ret);
    ret       = malloc(ret_size);
    if (ret == NULL)
    {
        fprintf(stderr, "%s %s(type=%s): unable to allocate %zu bytes for PSP-table of parameters\n",
                strcurtime(), __FUNCTION__, atd->ftd.type_name, ret_size);
        goto ERREXIT;
    }
    dstp = ret;

    /* Allocate a names-buffer */
    /* Calc size... */
    for (x = 0,  pcp_names_size = 0;
         x < countof(perchan_params);
         x++)
        for (nl = 0; nl < atd->num_lines;  nl++)
            pcp_names_size +=
                strlen(perchan_params[x].name)   +
                strlen(atd->line_dscrs[nl].name) +
                1;
    /* ...do allocate */
    pcp_names_buf   = malloc(pcp_names_size);
    if (pcp_names_buf == NULL)
    {
        fprintf(stderr, "%s %s(type=%s): unable to allocate %zu bytes for per-channel parameter names\n",
                strcurtime(), __FUNCTION__, atd->ftd.type_name, pcp_names_size);
        goto ERREXIT;
    }

    /* Fill in data */
    for (x = 0,  srcp = perchan_params,  pcp_names_p = pcp_names_buf;
         x < countof(perchan_params);
         x++,    srcp++ /*NO "dstp++" -- that is done in inner loop*/)
        for (nl = 0;
             nl < atd->num_lines;
             nl++, dstp++)
        {
            *dstp = *srcp;                                   // Copy main properties
            dstp->offset += sizeof(fastadc_dcnv_one_t) * nl; // Tweak offset to point to line'th element of respective array
            sprintf(pcp_names_p, "%s%s",                     // Store name...
                    srcp->name, atd->line_dscrs[nl].name);   // ...appending line-name
            dstp->name = pcp_names_p;                        // And remember it
            pcp_names_p += strlen(dstp->name) + 1;           // Advance buffer pointer

            /* Special case -- units */
            if (srcp->offset == PSP_OFFSET_OF(fastadc_dcnv_t, lines[0].units))
            {
                if (atd->line_dscrs[nl].dpyfmt != NULL)
                {
                    dstp->var.p_string.defval = atd->line_dscrs[nl].units;
                }
            }
        }

    /* END */
    for (x = 0,  srcp = end_params;
         x < countof(end_params);
         x++,    srcp++,  dstp++)
        *dstp = *srcp;

    if (mallocd_buf_p != NULL) *mallocd_buf_p = pcp_names_buf;
    
    return ret;

 ERREXIT:
    safe_free(pcp_names_buf);
    safe_free(ret);
    return NULL;
}

//////////////////////////////////////////////////////////////////////

int    FastadcDataX2XS   (fastadc_mes_t *mes_p, int x)
{
  int  multiplier;

    x += mes_p->cur_ptsofs;

    multiplier = mes_p->exttim? mes_p->ext_xs_per_point : mes_p->xs_per_point;
    if (multiplier <= 0) return x;

    if (mes_p->xs_divisor <= 0) return (x * multiplier);
    else                        return (x * multiplier) / mes_p->xs_divisor;
}

double FastadcDataPvl2Dsp(fastadc_data_t *adc,
                          int nl, double pvl)
{
  double         v;

    if (adc->lines_rds_rcvd[nl]  &&
        cda_rd_convert(adc->pfr.refs[adc->atd->line_dscrs[nl].data_cn],
                       pvl, &v) >= 0)
        return v;
    else
        return pvl / adc->atd->line_dscrs[nl].default_r;

    return pvl;
}

static inline
double FastadcDataGetPvl (fastadc_mes_t  *mes_p,
                          int nl, int x)
{
    if (reprof_cxdtype(mes_p->plots[nl].x_buf_dtype) == CXDTYPE_REPR_INT)
        return plotdata_get_int(mes_p->plots + nl, x);
    else
        return plotdata_get_dbl(mes_p->plots + nl, x);
}

double FastadcDataGetDsp (fastadc_data_t *adc,
                          fastadc_mes_t  *mes_p,
                          int nl, int x)
{
    return FastadcDataPvl2Dsp(adc, nl, FastadcDataGetPvl(mes_p, nl, x));
}
