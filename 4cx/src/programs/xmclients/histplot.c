#include <sys/param.h> /* For PATH_MAX */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <math.h>

#include "misclib.h"
#include "memcasecmp.h"
#include "timeval_utils.h"
#include "cxscheduler.h"

#include "ppf4td.h"

#include "Xh.h"
#include "Xh_fallbacks.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "Cdr.h"
#include "CdrP.h"
#include "MotifKnobs_histplot.h"


//// ctype extensions ////////////////////////////////////////////////

static inline int isletnum(int c)
{
    return isalnum(c)  ||  c == '_'  ||  c == '-';
}

static int only_letters(const char *s, int count)
{
    for (;  count > 0;  s++, count--)
        if (!isletnum(*s)) return 0;

    return 1;
}

//// paramstr_parser extensions //////////////////////////////////////

static int is_present_in_table(const char *name_b, size_t namelen,
                               psp_paramdescr_t *table)
{
  psp_paramdescr_t *item;

    if (namelen == 0) return 0;

    for (item = table;  item->name != NULL;  item++)
        if (item->t != PSP_T_NOP  &&
            cx_strmemcasecmp(item->name, name_b, namelen) == 0)
            return 1;
    return 0;
}

//////////////////////////////////////////////////////////////////////

enum
{
    DEF_PLOT_W = 600+1*0,
    DEF_PLOT_H = 400,
};

enum
{
    DEF_CYCLESIZE_US = 1000000,
    MIN_CYCLESIZE_US = 1000, MAX_CYCLESIZE_US = 1800*1000000 /*1800*1000000 fits into 31 bits (signed int32) */,

    DEF_HISTRING_LEN = 86400,
    MIN_HISTRING_LEN = 10,   MAX_HISTRING_LEN = 86400*10
};
        
const char   *app_name  = "histplot";
const char   *app_class = "HistPlot";
const char   *win_title = "History plot";
const char   *sysname   = "histplot";


static XhWindow               win;
static DataKnob               grp;
static MotifKnobs_histplot_t  mkhp;
static DataSubsys             ds;

typedef struct
{
    int     plot_width;
    int     plot_height;

    char   *defserver;
    char   *baseref;

    double  cyclesize_secs;
    int     histring_len;

    int     mode;
    int     horz_ctls;
} globopts_t;

// A copy from MotifKnobs_histplot_noop.c
static psp_lkp_t mode_lkp[] =
{
    {"line", MOTIFKNOBS_HISTPLOT_MODE_LINE},
    {"wide", MOTIFKNOBS_HISTPLOT_MODE_WIDE},
    {"dots", MOTIFKNOBS_HISTPLOT_MODE_DOTS},
    {"blot", MOTIFKNOBS_HISTPLOT_MODE_BLOT},
    {NULL, 0}
};

typedef struct
{
    char   *label;
    char   *units;
    double  disprange[2];
} chanopts_t;

static psp_paramdescr_t text2globopts[] =
{
    PSP_P_INT    ("width",       globopts_t, plot_width,  DEF_PLOT_W, 50, 5000),
    PSP_P_INT    ("height",      globopts_t, plot_height, DEF_PLOT_H, 50, 5000),

    PSP_P_LOOKUP ("mode",        globopts_t, mode,        -1, mode_lkp),

    PSP_P_MSTRING("defserver",   globopts_t, defserver,   NULL,       100),
    PSP_P_MSTRING("baseref",     globopts_t, baseref,     NULL,       100),

    PSP_P_REAL   ("hist_period", globopts_t, cyclesize_secs, DEF_CYCLESIZE_US/1000000.0, MIN_CYCLESIZE_US/1000000.0, MAX_CYCLESIZE_US/1000000.0),
    PSP_P_INT    ("hist_len",    globopts_t, histring_len,   DEF_HISTRING_LEN,           MIN_HISTRING_LEN,           MAX_HISTRING_LEN),

    PSP_P_FLAG   ("horz_ctls",   globopts_t, horz_ctls,      1, 0),
    PSP_P_FLAG   ("vert_ctls",   globopts_t, horz_ctls,      0, 1),

    PSP_P_END()
};

// A combination of Cdr_via_ppf4td.c::RNG_fparser() and MotifKnobs_histplot_noop.c::AddPluginParser()
static int RangePluginParser(const char *str, const char **endptr,
                            void *rec, size_t recsize,
                            const char *separators, const char *terminators,
                            void *privptr __attribute__((unused)), char **errstr)
{
  double *dp = rec;
  double  v1;
  double  v2;
  char   *p;
  char   *err;

    if (str == NULL) return PSP_R_OK; // Initialization: (NULL->value)

    /* "Min" part */
    p = str;
    v1 = strtod(p, &err);
    if (err == p  ||  *err != '-')
    {
        *errstr = "error in min specification";
        return PSP_R_USRERR;
    }

    /* "Max" part */
    p = err + 1; // Skip '-'
    v2 = strtod(p, &err);
    if (err == p  ||
        (*err != '\0'      &&
         strchr(terminators, *err) == NULL  &&
         strchr(separators,  *err) == NULL)
       )
    {
        *errstr = "error in max specification";
        return PSP_R_USRERR;
    }

    if (dp != NULL) // // Skipping: (str->NULL)
    {
        dp[0] = v1;
        dp[1] = v2;
    }

    *endptr = err;

    return PSP_R_OK;
}
static psp_paramdescr_t text2chanopts[] =
{
    PSP_P_MSTRING("label",     chanopts_t, label,     NULL, 100),
    PSP_P_MSTRING("units",     chanopts_t, units,     NULL, 100),
    PSP_P_PLUGIN ("disprange", chanopts_t, disprange, RangePluginParser, NULL),
    PSP_P_END()
};

static globopts_t      globopts;
static int             cyclesize_us = 1000000;

static struct timeval  cycle_start;  /* The time when the current cycle had started */
static struct timeval  cycle_end;
static sl_tid_t        cycle_tid            = -1;

static void CycleCallback(int       uniq     __attribute__((unused)),
                          void     *privptr1,
                          sl_tid_t  tid      __attribute__((unused)),
                          void     *privptr2)
{
    ////fprintf(stderr, "%s %s\n", strcurtime(), __FUNCTION__);
    cycle_tid = -1;
    cycle_start = cycle_end;
    timeval_add_usecs(&cycle_end, &cycle_start, cyclesize_us);

    cycle_tid = sl_enq_tout_at(0, NULL,
                               &cycle_end, CycleCallback, NULL);

    /* 1. Shift history */
    CdrShiftHistory(ds);
    /* 2. Update display */
    MotifKnobs_UpdateHistplotGraph(&mkhp);
}

static void ProcessMkHpEvent(MotifKnobs_histplot_t *hp,
                             void                  *privptr,
                             int                    reason,
                             void                  *info_ptr)
{
  MotifKnobs_histplot_mb3_evinfo_t *evinfo = info_ptr;
  char                              filename[PATH_MAX];

    if      (reason == MOTIFKNOBS_HISTPLOT_REASON_CLOSE)
    {
        /* No [Close] button (MOTIFKNOBS_HISTPLOT_FLAG_WITH_CLOSE_B is
           NOT specified), so no way to get here and no reason to close anyway. */
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_SAVE)
    {
        check_snprintf(filename, sizeof(filename), DATATREE_PLOT_PATTERN_FMT, sysname);
        XhFindFilename(filename, filename, sizeof(filename));
        if (MotifKnobs_SaveHistplotData(hp, filename, NULL) >= 0)
            fprintf(stdout, "%s %s: history-plot saved to \"%s\"\n", strcurtime(), "histplot", filename);
        else
            fprintf(stdout, "%s %s: error saving history-plot to \"%s\": %s\n", strcurtime(), "histplot", filename, strerror(errno));
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_MB3)
    {
        /* As we are Chl-less, there's no "Knob properties" support */
    }
}

static int  AddGroupingChan(const char *argv0, const char *spec)
{
  DataKnob        k;

  chanopts_t      opts;

  const char     *cp;
  const char     *cl_p;
  const char     *p_eq;  // '='
  char            buf[100];
  size_t          slen;
  int             r;
  int             flags;
  int             width;
  int             precision;
  char            conv_c;

  int             num_params;
  const char     *label_s;

  char            artbuf[100]; // ARTificial spec BUFfer

    bzero(&opts, sizeof(opts));
    psp_parse(NULL, NULL, &opts, '=', ",", ":", text2chanopts);

    if (spec == NULL)
    {
        sprintf(artbuf, "line%d", grp->u.c.count);
        spec = artbuf;
    }

    k = grp->u.c.content + grp->u.c.count;
    k->type   = DATAKNOB_KNOB;
    k->uplink = grp;
    strzcpy(k->u.k.dpyfmt, "%8.3f", sizeof(k->u.k.dpyfmt));

    cp = spec;
    if (*cp == '%')
    {
        cl_p = strchr(cp, ':');
        if      (cl_p == NULL)
        {
            fprintf(stderr, "%s %s: ':' expected after %%DPYFMT spec in \"%s\"\n",
                    strcurtime(), argv0, spec);
            /* Here we leave 'cp' untouched and pass it as-is for channel registration */
        }
        else if ((slen = cl_p - cp) > sizeof(buf) - 1)
        {
            fprintf(stderr, "%s %s: ':' %%DPYFMT spec too long in \"%s\"\n",
                    strcurtime(), argv0, spec);
            /* Here we also leave 'cp' untouched */
        }
        else
        {
            memcpy(buf, cp, slen);
            buf[slen] = '\0';

            r =      ParseDoubleFormat(GetTextFormat(buf),
                                       &flags, &width, &precision, &conv_c);
            if      (r >= 0)
            {
                CreateDoubleFormat(k->u.k.dpyfmt, sizeof(k->u.k.dpyfmt), 20, 10,
                                   flags, width, precision, conv_c);
            }
            else if (ParseIntFormat   (GetIntFormat(buf),
                                       &flags, &width, &precision, &conv_c) >= 0)
                fprintf(stderr, "%s %s: %%DPYFMT spec in \"%s\" is an integer format, while float required; forcing \"%s\"\n",
                        strcurtime(), argv0, spec,
                        k->u.k.dpyfmt);
            else
                fprintf(stderr, "%s %s: invalid %%DPYFMT spec in \"%s\": %s; forcing \"%s\"\n",
                        strcurtime(), argv0, spec,
                        printffmt_lasterr(),
                        k->u.k.dpyfmt);

            /* Put "cp" after "%DPYFMT:" */
            cp = cl_p + 1;
        }
    }

    while (1)
    {
        while (*cp == ',') cp++;  // Skip ALL commas

        p_eq = strchr(cp, '=');
        if (p_eq != NULL  &&  only_letters(cp, p_eq - cp)  &&
            is_present_in_table(cp, p_eq - cp,  text2chanopts))
        {
            if (psp_parse_as(cp, &cp,
                             &opts,
                             '=', ",", ":",
                             text2chanopts,
                             PSP_MF_NOINIT) != PSP_R_OK)
            {
                fprintf(stderr,
                        "%s %s: error in \"%s\" specification: %s\n",
                        strcurtime(), argv0, spec, psp_error());
                        exit(1);
            }
            if (*cp == ':') cp++; // Skip ONE colon (next ones can be part of channel reference)
        }
        else break;
    }

    k->ident     = opts.label; // Note: it is already malloc()'d
    k->u.k.units = opts.units; // ...and this one too
    num_params = DATAKNOB_NUM_STD_PARAMS;
    if ((k->u.k.params = malloc(sizeof(CxKnobParam_t) * num_params)) == NULL)
    {
        fprintf(stderr, "%s %s: error allocating parameters for chan#%d\n",
                strcurtime(), argv0, grp->u.c.count);
    }
    else
    {
        bzero(k->u.k.params, sizeof(CxKnobParam_t) * num_params);
        k->u.k.num_params = num_params;
        k->u.k.params[DATAKNOB_PARAM_DISP_MIN].value = opts.disprange[0];
        k->u.k.params[DATAKNOB_PARAM_DISP_MAX].value = opts.disprange[1];
    }

    if ((k->u.k.rd_src =
         strdup(cp)) == NULL)
        fprintf(stderr,
                "%s %s: failed to strdup(\"%s\")\n",
                strcurtime(), argv0, cp);
    else if (k->ident == NULL)
    {
        
        label_s = strrchr(k->u.k.rd_src, '.');
        if (label_s == NULL) label_s = k->u.k.rd_src;
        else                 label_s++; // Skip '.' itself
        k->ident = strdup(label_s);
    }

    grp->u.c.count++;

    return 0;
}

static void ActivatePlotRow(int row)
{
  DataKnob        victim;

    victim = grp->u.c.content + row;
    CdrAddHistory(victim, globopts.histring_len);
    MotifKnobs_AddToHistplot(&mkhp, victim);
}

static void LoadPlot(const char *argv0, const char *filename)
{
  ppf4td_ctx_t    ctx;
  int             r;
  int             ch;

  size_t          tring_size;

  int             fixed_list;

  int             t_index;
  int             row; // This is "col" in a file, but row in a plot

  struct timeval  at;
  int             ival;
  double          dval;
  char            buf[1000];
  char           *err;
  DataKnob        k;

  int             idx;

  double          minval;
  double          maxval;

  enum
  {
      ENTITY_PARSE_FLAGS =
          PPF4TD_FLAG_NOQUOTES |
          PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM
  };

    if (strcmp(filename, "-") == 0) filename = "/dev/stdin";
    r = ppf4td_open(&ctx, "plaintext", filename);
    if (r != 0)
    {
        fprintf(stderr, "%s %s: failed to open \"%s\": %s\n",
                strcurtime(), argv0, filename, strerror(errno));
        exit(1);
    }

    tring_size = sizeof(*(mkhp.timestamps_ring)) * globopts.histring_len;
    if ((mkhp.timestamps_ring = malloc(tring_size)) == NULL)
    {
        fprintf(stderr, "%s %s: WARNING: can't alloc timestamps\n",
                strcurtime(), argv0);
    }
    else
    {
        bzero(mkhp.timestamps_ring, tring_size);
    }

    fixed_list = mkhp.rows_used != 0;

    for (t_index = 0;  t_index < globopts.histring_len; /* DO-NOTHING */)
    {
        ppf4td_skip_white(&ctx);
        if (ppf4td_is_at_eol(&ctx)) goto SKIP_TO_NEXT_LINE;
        r = ppf4td_peekc(&ctx, &ch);
        if (r <= 0  ||  ch == EOF) goto END_PARSE_FILE;

        /* A comment or a header line? */
        if (ch == '#')
        {
            if (ppf4td_cur_line(&ctx) > 1  ||  fixed_list) goto SKIP_TO_NEXT_LINE;
            /* A header line, let's try to parse it */

            // 1. "#Time(s-01.01.1970)"
            while (1)
            {
                if (ppf4td_peekc(&ctx, &ch) < 0) goto END_PARSE_FILE;
                if (ch == '\r'  ||  ch == '\n')  goto SKIP_TO_NEXT_LINE;
                if (isspace(ch))
                {
                    ppf4td_skip_white(&ctx);
                    break;
                }
                else
                    ppf4td_nextc(&ctx, &ch);
            }

            // 2. "YYYY-MM-DD-HH:MM:SS.mss"
            while (1)
            {
                if (ppf4td_peekc(&ctx, &ch) < 0) goto END_PARSE_FILE;
                if (ch == '\r'  ||  ch == '\n')  goto SKIP_TO_NEXT_LINE;
                if (isspace(ch))
                {
                    ppf4td_skip_white(&ctx);
                    break;
                }
                else
                    ppf4td_nextc(&ctx, &ch);
            }

            // 3. "secs-since0"
            while (1)
            {
                if (ppf4td_peekc(&ctx, &ch) < 0) goto END_PARSE_FILE;
                if (ch == '\r'  ||  ch == '\n')  goto SKIP_TO_NEXT_LINE;
                if (isspace(ch))
                {
                    ppf4td_skip_white(&ctx);
                    break;
                }
                else
                    ppf4td_nextc(&ctx, &ch);
            }

            // 4. Channel specs
            for (row = 0;
                 row < MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX;
                 row++)
            {
                if (ppf4td_is_at_eol(&ctx)) goto SKIP_TO_NEXT_LINE;
                r = ppf4td_get_string(&ctx,
                                      PPF4TD_FLAG_EOLTERM | PPF4TD_FLAG_SPCTERM,
                                      buf, sizeof(buf));
                if (r < 0)                  goto SKIP_TO_NEXT_LINE;
                r = AddGroupingChan(argv0, buf);
                if (r < 0)                  goto SKIP_TO_NEXT_LINE;
                ppf4td_skip_white(&ctx);
            }

            goto SKIP_TO_NEXT_LINE;
        }

        /* I. A SECONDS.MILLISECONDS timestamp */
        /* a. Seconds */
        /*!!! Must read into int64, otherwise 2038 problem will arise */
        if (ppf4td_get_int(&ctx, 0, &ival, 10, NULL) < 0)  goto SKIP_TO_NEXT_LINE;
        if (ival < 0)                                      goto SKIP_TO_NEXT_LINE;
        at.tv_sec = ival;
        /* b. Dot */
        if (ppf4td_peekc  (&ctx, &ch) <= 0  ||  ch == EOF) goto END_PARSE_FILE;
        if (ch != '.')                                     goto SKIP_TO_NEXT_LINE;
        ppf4td_nextc(&ctx, &ch);
        /* c. Milliseconds */
        if (ppf4td_get_int(&ctx, 0, &ival, 10, NULL) < 0)  goto SKIP_TO_NEXT_LINE;
        if (ival < 0  ||  ival > 1000)                     goto SKIP_TO_NEXT_LINE;
        at.tv_usec = ival * 1000;
        ppf4td_skip_white(&ctx);

        /* II. YYYY-MM-DD-HH:MM:SS.mss */
        r = ppf4td_get_string(&ctx,
                              PPF4TD_FLAG_JUST_SKIP   | 
                                  PPF4TD_FLAG_EOLTERM | PPF4TD_FLAG_SPCTERM,
                              NULL, 0);
        if (r < 0) goto SKIP_TO_NEXT_LINE;
        ppf4td_skip_white(&ctx);

        /* III. secs-since0 */
        r = ppf4td_get_string(&ctx,
                              PPF4TD_FLAG_JUST_SKIP   | 
                                  PPF4TD_FLAG_EOLTERM | PPF4TD_FLAG_SPCTERM,
                              NULL, 0);
        if (r < 0) goto SKIP_TO_NEXT_LINE;
        ppf4td_skip_white(&ctx);

        /* IV. Data */
        for (row = 0;
             row < (fixed_list? mkhp.rows_used : MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX);
             row++)
        {
            r = ppf4td_get_string(&ctx, ENTITY_PARSE_FLAGS, buf, sizeof(buf));
            if (r < 0)
            {
                /*!!!???*/
                goto SWITCH_TO_NEXT_LINE;
            }
            else
            {
                dval = strtod(buf, &err);
                if (err == buf  ||  *err != '\0')
                {
                    if (ppf4td_is_at_eol(&ctx)) goto SWITCH_TO_NEXT_LINE;
                    r = ppf4td_peekc(&ctx, &ch);
                    if (r < 0)                  goto SWITCH_TO_NEXT_LINE;
                    if (ch == '#')              goto SWITCH_TO_NEXT_LINE;
                    if (!isspace(ch)) /*!!! Is this really needed? In what situation? */
                        ppf4td_get_string(&ctx,
                                          PPF4TD_FLAG_JUST_SKIP   | 
                                              PPF4TD_FLAG_EOLTERM | PPF4TD_FLAG_SPCTERM,
                                          NULL, 0);
                    goto NEXT_ROW;
                }

                /* Activate this row */
                while (grp->u.c.count <= row)
                    AddGroupingChan(argv0, NULL);
                while (mkhp.rows_used <= row)
                    ActivatePlotRow(mkhp.rows_used);

                /* Store value */
                k = grp->u.c.content + row;
                k->u.k.histring[t_index] = dval;
                k->u.k.histring_used = t_index + 1;
            }
 NEXT_ROW:
            ppf4td_skip_white(&ctx);
        }

 SWITCH_TO_NEXT_LINE:
        /* Increment t_index only if some value(s) taken at this line */
        if (row > 0)
        {
            if (mkhp.timestamps_ring != NULL)
            {
                mkhp.timestamps_ring[t_index] = at;
                mkhp.timestamps_ring_used = t_index + 1;
            }
            t_index++;
        }

 SKIP_TO_NEXT_LINE:
        /* First, skip everything till EOL */
        while (ppf4td_peekc(&ctx, &ch) > 0  &&
               ch != '\r'  &&  ch != '\n')
            ppf4td_nextc(&ctx, &ch);
        /* Than skip as many CR/LFs as possible */
        while (ppf4td_peekc(&ctx, &ch) > 0  &&
               (ch == '\r'  ||  ch == '\n'))
            ppf4td_nextc(&ctx, &ch)/*, fprintf(stderr, "\t{%c}\n", ch)*/;

        /* Check for EOF */
        if (ppf4td_peekc(&ctx, &ch) <= 0) goto END_PARSE_FILE;
    }
 END_PARSE_FILE:
//fprintf(stderr, "t_index=%d\n", t_index);

    ppf4td_close(&ctx);

    /* Auto-scale rows without disprange */
    for (row = 0, k = grp->u.c.content;
         row < mkhp.rows_used;
         row++,   k++)
        if (k->u.k.num_params   >    DATAKNOB_PARAM_DISP_MAX  &&
            k->u.k.params[DATAKNOB_PARAM_DISP_MIN].value 
            >=
            k->u.k.params[DATAKNOB_PARAM_DISP_MAX].value)
        {
            for (idx = 0;
                 idx < k->u.k.histring_used  &&  isnan(k->u.k.histring[idx]); // May also use "k->u.k.histring[idx]!=k->u.k.histring[idx]", since NaN!=NaN
                 idx++) ;
            if (idx < k->u.k.histring_used)
            {
                for (minval = maxval = k->u.k.histring[idx];
                     idx < k->u.k.histring_used;
                     idx++)
                    if (!isnan(k->u.k.histring[idx]))
                    {
                        if (minval > k->u.k.histring[idx]) minval = k->u.k.histring[idx];
                        if (maxval < k->u.k.histring[idx]) maxval = k->u.k.histring[idx];
                    }
                k->u.k.params[DATAKNOB_PARAM_DISP_MIN].value = minval;
                k->u.k.params[DATAKNOB_PARAM_DISP_MAX].value = maxval;
            }
        }
}


static char *fallbacks[] = {XH_STANDARD_FALLBACKS};

void *_CreateKnobReference = CreateKnob;
int main(int argc, char *argv[])
{
  int           arg_n;

  const char   *file_to_load = NULL;

  data_section_t *nsp;
  int             row;

  enum {PK_SWITCH, PK_PARAM, PK_CHAN, PK_FILE};
  int             pkind;
  const char     *p_eq;  // '='
  const char     *p_sl;  // '/'
  const char     *p_cl;  // ':'

    set_signal(SIGPIPE, SIG_IGN);

    XhInitApplication(&argc, argv, app_name, app_class, fallbacks);

    if ((ds = malloc(sizeof(*ds))) == NULL)
    {
        fprintf(stderr, "%s %s: malloc(DataSubsys)=NULL!\n", strcurtime(), argv[0]);
        exit(1);
    }
    bzero(ds, sizeof(*ds));
    CdrCreateSection(ds, DSTN_SYSNAME,
                     NULL, 0,
                     sysname, strlen(sysname) + 1);
    nsp = CdrCreateSection(ds,
                           DSTN_GROUPING,
                           "main", strlen("main"),
                           NULL, sizeof(data_knob_t));
    ds->sysname       = CdrFindSection(ds, DSTN_SYSNAME,   NULL);
    ds->main_grouping = CdrFindSection(ds, DSTN_GROUPING,  "main");
    grp               = nsp->data;
    grp->type         = DATAKNOB_GRPG;
    grp->u.c.content  = malloc(sizeof(data_knob_t) * MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX);
    grp->u.c.subsyslink = ds;
    if (grp->u.c.content == NULL)
    {
        fprintf(stderr, "%s %s: malloc(content)=NULL!\n", strcurtime(), argv[0]);
        exit(1);
    }
    bzero(grp->u.c.content, sizeof(data_knob_t) * MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX);

    bzero(&globopts, sizeof(globopts));
    psp_parse(NULL, NULL, &globopts, '=', "", "", text2globopts);

    arg_n = 1;
    while (arg_n < argc)
    {
        pkind = PK_CHAN;
        p_eq = strchr(argv[arg_n], '=');
        p_sl = strchr(argv[arg_n], '/');
        p_cl = strchr(argv[arg_n], ':');

        if      (argv[arg_n][0] == '-')
            pkind = PK_SWITCH;
        else if (argv[arg_n][0] == '/'  ||  memcmp(argv[arg_n], "./", 2) == 0  ||
                 (p_sl != NULL  &&  (p_eq == NULL  ||  p_sl < p_eq)))
            pkind = PK_FILE;
        else if (is_present_in_table(argv[arg_n], strlen(argv[arg_n]), text2globopts))
            pkind = PK_PARAM; // That's for value-less global options (flags)
        else if (p_eq != NULL  &&  only_letters(argv[arg_n], p_eq - argv[arg_n])  &&
                 is_present_in_table(argv[arg_n], p_eq - argv[arg_n],  text2globopts))
            pkind = PK_PARAM;

        if      (pkind == PK_PARAM)
        {
            if (p_eq == argv[arg_n])
                fprintf(stderr,
                        "%s %s: WARNING: wrong parameter spec \"%s\"\n",
                        strcurtime(), argv[0], argv[arg_n]);
            else
            {
                if (psp_parse_as(argv[arg_n], NULL,
                                 &globopts,
                                 '=', "", "",
                                 text2globopts,
                                 PSP_MF_NOINIT) != PSP_R_OK)
                {
                    fprintf(stderr,
                            "%s %s: error in \"%s\" specification: %s\n",
                            strcurtime(), argv[0], argv[arg_n], psp_error());
                            exit(1);
                }
            }
        }
        else if (pkind == PK_SWITCH)
        {
            if      (strcmp(argv[arg_n], "-") == 0)
                file_to_load = argv[arg_n];
            else if (argv[arg_n][1] == 'f')
            {
                if (argv[arg_n][2] != '\0')  // "-fFILENAME"  - no space, in single parameter
                    file_to_load = argv[arg_n] + 2;
                else                         // "-f FILENAME" - with space, in 2 parameters
                {
                    arg_n++;
                    if (arg_n >= argc)
                    {
                        fprintf(stderr, "%s %s: \"-f\" requires a parameter\n",
                                strcurtime(), argv[0]);
                        exit(1);
                    }
                    file_to_load = argv[arg_n];
                }
            }
            else if (strcmp(argv[arg_n], "-h") == 0)
            {
                printf("Usage: %s [OPTIONS] [PARAMETERS] {CHANNELS_TO_PLOT}\n"
                       "\n"
                       "Options:\n"
                       "  -f FILENAME -- load data from specified file\n"
                       "  -h show this help\n"
                       "\n"
                       "Parameters:\n"
                       "  width=NNN       -- plot area width in pixels (default=%d)\n"
                       "  height=NNN      -- plot area height in pixels (default=%d)\n"
                       "  hist_period=NNN -- plotting period in seconds (default=%.1f)\n"
                       "  hist_len=NNNNN  -- history length in points (default=%d)\n"
                       "  mode=TYPE       -- plot mode (line, wide, dots, blot; default=line)\n"
                       "\n"
                       "CHANNELS_TO_PLOT format: [%DPYFMT:]{CHAN_PARAM:}REFERENCE\n"
                       "\n"
                       "  DPYFMT     is a printf format (one of %%f, %%e, %%g)\n"
                       "  CHAN_PARAM is one of:\n"
                       "    label=NAME        -- e.g. label=Power\n"
                       "    units=UNITS       -- e.g. units=W\n"
                       "    disprange=MIN-MAX -- e.g. disprange=-9.5-+19.5\n"
                       "  REFERENCE  is a CX channel name/reference\n",
                       argv[0], 
                       DEF_PLOT_W, DEF_PLOT_H,
                       DEF_CYCLESIZE_US/1000000.0, DEF_HISTRING_LEN);
                exit(0);
            }
            else
            {
                fprintf(stderr, "%s %s: unknown switch \"%s\"\n",
                        strcurtime(), argv[0], argv[arg_n]);
                exit(1);
            }
        }
        else if (pkind == PK_CHAN)
        {
            if (grp->u.c.count >= MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX)
            {
                fprintf(stderr,
                        "%s %s: WARNING: too many channels, ignoring \"%s\"\n",
                        strcurtime(), argv[0], argv[arg_n]);
                goto NEXT_ARG;
            }

            AddGroupingChan(argv[0], argv[arg_n]);
        }
        else /* PK_FILE */
        {
            file_to_load = argv[arg_n];
        }

 NEXT_ARG:
        arg_n++;
    }

    /* Activate parsed info */
    ds->main_grouping->u.c.refbase = globopts.baseref;
    cyclesize_us = globopts.cyclesize_secs * 1000000;

    win = XhCreateWindow(win_title, app_name, app_class,
                         0,
                         NULL, NULL);
    if (win == NULL)
    {
        fprintf(stderr, "%s %s: XhCreateWindow=NULL!\n", strcurtime(), argv[0]);
        exit(1);
    }
    
    if (MotifKnobs_CreateHistplot(&mkhp, XhGetWindowWorkspace(win),
                                  globopts.plot_width, globopts.plot_height,
                                  MOTIFKNOBS_HISTPLOT_FLAG_FIXED |
                                      (globopts.horz_ctls   ? MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS : 0) |
                                      (file_to_load != NULL ? MOTIFKNOBS_HISTPLOT_FLAG_VIEW_ONLY : 0),
                                  cyclesize_us,
                                  ProcessMkHpEvent, NULL) != 0)
    {
        fprintf(stderr, "%s %s: MotifKnobs_CreateHistplot=NULL!\n", strcurtime(), argv[0]);
        exit(1);
    }
    attachleft  (mkhp.form, NULL, 0);
    attachright (mkhp.form, NULL, 0);
    attachtop   (mkhp.form, NULL, 0);
    attachbottom(mkhp.form, NULL, 0);
    if (globopts.mode >= 0) MotifKnobs_SetHistplotMode(&mkhp, globopts.mode);
    for (row = 0;  row < grp->u.c.count;  row++)
        ActivatePlotRow(row);

    /* Here may either load a data file... */
    if (file_to_load != NULL)
    {
        LoadPlot(argv[0], file_to_load);
        MotifKnobs_UpdateHistplotGraph(&mkhp);
    }
    /* ...or activate the subsystem */
    if (file_to_load == NULL)
    {
        if (CdrRealizeSubsystem(ds, 0, globopts.defserver, argv[0]) < 0)
        {
            fprintf(stderr, "%s: Realize(%s): %s\n",
                    argv[0], sysname, CdrLastErr());
            exit(1);
        }
        gettimeofday(&cycle_start, NULL);
        timeval_add_usecs(&cycle_end, &cycle_start, cyclesize_us);
        cycle_tid = sl_enq_tout_at(0, NULL,
                                   &cycle_end, CycleCallback, NULL);
    }

    XhShowWindow      (win);
    XhRunApplication();

    return 0;
}
