#include <sys/param.h> /* For PATH_MAX */

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_histplot.h"

#include "MotifKnobs_histplot_noop.h"


enum
{
    DEF_PLOT_W = 600,
    DEF_PLOT_H = 400,
};

typedef struct
{
    MotifKnobs_histplot_t  mkhp;
    DataKnob my_k;

    int   width;
    int   height;
    char *plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];

    int   mode;
    int   no_mode;
    int   fixed;
    int   horz_ctls;
} histplot_privrec_t;

static psp_lkp_t mode_lkp[] =
{
    {"line", MOTIFKNOBS_HISTPLOT_MODE_LINE},
    {"wide", MOTIFKNOBS_HISTPLOT_MODE_WIDE},
    {"dots", MOTIFKNOBS_HISTPLOT_MODE_DOTS},
    {"blot", MOTIFKNOBS_HISTPLOT_MODE_BLOT},
    {NULL, 0}
};

static int AddPluginParser(const char *str, const char **endptr,
                           void *rec, size_t recsize,
                           const char *separators, const char *terminators,
                           void *privptr __attribute__((unused)), char **errstr)
{
  char       **plots = rec;
  int          count = recsize / sizeof(*plots);

  const char  *beg_p;
  const char  *srcp;
  size_t       len;

  int          nl;

    if (str == NULL) return PSP_R_OK; // Initialization: (NULL->value)

    /* Advance until terminator (NO quoting/backslashing support */
    for (srcp = beg_p = str;
         *srcp != '\0'                       &&
         strchr(terminators, *srcp) == NULL  &&
         strchr(separators,  *srcp) == NULL;
         srcp++);
    len = srcp - beg_p;

    if (endptr != NULL) *endptr = srcp;

    if (rec == NULL) return PSP_R_OK; // Skipping: (str->NULL)

    if (srcp == beg_p) return PSP_R_OK; // Empty value -- do nothing

    /* Find unused cell */
    for (nl = 0;  nl < count  &&  plots[nl] != NULL;  nl++) ;
    if (nl < count)
    {
        if ((plots[nl] = malloc(len + 1)) == NULL)
        {
            *errstr = "malloc() failure";
            return PSP_R_USRERR;
        }
        memcpy(plots[nl], beg_p, len);
        plots[nl][len] = '\0';
    }
    else
    {
        *errstr = "too many plots requested";
        return PSP_R_USRERR;
    }

    return PSP_R_OK;
}

static psp_paramdescr_t text2histplotopts[] =
{
    PSP_P_INT    ("width",  histplot_privrec_t, width,    DEF_PLOT_W, 50, 5000),
    PSP_P_INT    ("height", histplot_privrec_t, height,   DEF_PLOT_H, 50, 5000),

    PSP_P_MSTRING("plot1",  histplot_privrec_t, plots[0], NULL, 100),
    PSP_P_MSTRING("plot2",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  1?  1 : 0], NULL, 100),
    PSP_P_MSTRING("plot3",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  2?  2 : 0], NULL, 100),
    PSP_P_MSTRING("plot4",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  3?  3 : 0], NULL, 100),
    PSP_P_MSTRING("plot5",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  4?  4 : 0], NULL, 100),
    PSP_P_MSTRING("plot6",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  5?  5 : 0], NULL, 100),
    PSP_P_MSTRING("plot7",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  6?  6 : 0], NULL, 100),
    PSP_P_MSTRING("plot8",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  7?  7 : 0], NULL, 100),
    PSP_P_MSTRING("plot9",  histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  8?  8 : 0], NULL, 100),
    PSP_P_MSTRING("plot10", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX >  9?  9 : 0], NULL, 100),
    PSP_P_MSTRING("plot11", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX > 10? 10 : 0], NULL, 100),
    PSP_P_MSTRING("plot12", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX > 11? 11 : 0], NULL, 100),
    PSP_P_MSTRING("plot13", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX > 12? 12 : 0], NULL, 100),
    PSP_P_MSTRING("plot14", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX > 13? 13 : 0], NULL, 100),
    PSP_P_MSTRING("plot15", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX > 14? 14 : 0], NULL, 100),
    PSP_P_MSTRING("plot16", histplot_privrec_t, plots[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX > 15? 15 : 0], NULL, 100),
    PSP_P_PLUGIN ("add",    histplot_privrec_t, plots, AddPluginParser, NULL),

    PSP_P_LOOKUP ("mode",   histplot_privrec_t, mode,     MOTIFKNOBS_HISTPLOT_MODE_LINE, mode_lkp),
    PSP_P_FLAG   ("nomode", histplot_privrec_t, no_mode,  1, 0),
    
    PSP_P_FLAG   ("fixed",  histplot_privrec_t, fixed,    1, 0),
    PSP_P_FLAG   ("chgbl",  histplot_privrec_t, fixed,    0, 1),

    PSP_P_FLAG   ("horz_ctls", histplot_privrec_t, horz_ctls, 1, 0),
    PSP_P_FLAG   ("vert_ctls", histplot_privrec_t, horz_ctls, 0, 1),

    PSP_P_END()
};

static void ProcessMkHpEvent(MotifKnobs_histplot_t *hp,
                             void                  *privptr,
                             int                    reason,
                             void                  *info_ptr)
{
  histplot_privrec_t               *me     = privptr;
  MotifKnobs_histplot_mb3_evinfo_t *evinfo = info_ptr;
  XhWindow                          win;
  DataSubsys                        subsys;
  char                              filename[PATH_MAX];

    if      (reason == MOTIFKNOBS_HISTPLOT_REASON_CLOSE)
    {
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_SAVE)
    {
        win    = XhWindowOf     (me->my_k->w);
        subsys = get_knob_subsys(me->my_k);

        check_snprintf(filename, sizeof(filename), DATATREE_PLOT_PATTERN_FMT, subsys->sysname);
        XhFindFilename(filename, filename, sizeof(filename));

        if (MotifKnobs_SaveHistplotData(hp, filename, NULL) >= 0)
            XhMakeMessage (win, "History-plot saved to \"%s\"", filename);
        else
            XhMakeMessage (win, "Error saving history-plot to \"%s\": %s", filename, strerror(errno));
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_MB3)
    {
        MotifKnobs_Mouse3Handler(evinfo->k->w, evinfo->k,
                                 evinfo->event, evinfo->continue_to_dispatch);
    }
}

static void PlotUpdateProc(void *privptr, int reason)
{
  histplot_privrec_t *me                   = privptr;

    if      (reason == HISTPLOT_REASON_UPDATE_PLOT_GRAPH)
        MotifKnobs_UpdateHistplotGraph(&(me->mkhp));
    else if (reason == HISTPLOT_REASON_UPDATE_PROT_PROPS)
        MotifKnobs_UpdateHistplotProps(&(me->mkhp));
}

static int CreateHistplotNoop(DataKnob k, CxWidget parent)
{
  histplot_privrec_t *me         = (histplot_privrec_t *)k->privptr;
  DataSubsys          subsys;

  int                 n;
  DataKnob            victim;

    subsys = get_knob_subsys(k);
    if (subsys == NULL) return -1;

    me->my_k = k;

    if (MotifKnobs_CreateHistplot(&(me->mkhp), parent,
                                  me->width, me->height,
                                  0 |
                                      (me->no_mode?   MOTIFKNOBS_HISTPLOT_FLAG_NO_MODE_SWCH : 0) |
                                      (MOTIFKNOBS_HISTPLOT_FLAG_FIXED) |
                                      (me->horz_ctls? MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS    : 0),
                                  subsys->cyclesize_us,
                                  ProcessMkHpEvent, me) != 0)
        return -1;
    k->w = me->mkhp.form;
    express_hist_interest(k,
                          PlotUpdateProc, me,
                          1);

    for (n = 0;  n < countof(me->plots);  n++)
        if (me->plots[n] != NULL)
        {
            victim = datatree_find_node(k, me->plots[n], DATAKNOB_KNOB);
            if (victim == NULL)
                fprintf(stderr, "%s(%s/\"%s\"): plot%d: knob \"%s\" not found: %s\n",
                        __FUNCTION__, k->ident, k->label, n, me->plots[n], strerror(errno));
            else
            {
                historize_knob(victim);
                MotifKnobs_AddToHistplot(&(me->mkhp), victim);
            }
        }

    return 0;
}

static void DestroyHistplotNoop(DataKnob k)
{
  histplot_privrec_t *me         = (histplot_privrec_t *)k->privptr;

    express_hist_interest(k,
                          PlotUpdateProc, me,
                          0);

    if (k->w != NULL) XtDestroyWidget(CNCRTZE(k->w));
    k->w = NULL;
}

dataknob_noop_vmt_t motifknobs_histplot_vmt =
{
    {DATAKNOB_NOOP, "histplot",
        sizeof(histplot_privrec_t), text2histplotopts,
        0,
        CreateHistplotNoop, DestroyHistplotNoop, NULL, NULL}
};
