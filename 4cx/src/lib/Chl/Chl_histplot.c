#include "Chl_includes.h"

#include <Xm/Frame.h>


typedef histplot_rec_t dlgrec_t;

static void CreateHistPlotDialog(XhWindow  win, DataSubsys subsys, dlgrec_t *rec);

static dlgrec_t *ThisDialog(XhWindow win)
{
  chl_privrec_t *privrec = _ChlPrivOf(win);
  dlgrec_t      *rec     = &(privrec->hp);
  
    if (!rec->initialized) CreateHistPlotDialog(win, privrec->subsys, rec);
    return rec;
}

//////////////////////////////////////////////////////////////////////

enum
{
    DEF_PLOT_W = 600,
    DEF_PLOT_H = 400,
};

static void ProcessMkHpEvent(MotifKnobs_histplot_t *hp,
                             void                  *privptr,
                             int                    reason,
                             void                  *info_ptr)
{
  dlgrec_t                         *rec    = privptr;
  MotifKnobs_histplot_mb3_evinfo_t *evinfo = info_ptr;
  XhWindow                          win;
  DataSubsys                        subsys;
  char                              filename[PATH_MAX];
  int                               err;

    if      (reason == MOTIFKNOBS_HISTPLOT_REASON_CLOSE)
    {
        XhStdDlgHide(rec->box);
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_SAVE)
    {
        win    = XhWindowOf(rec->box);
        subsys = ((chl_privrec_t *)(_ChlPrivOf(win)))->subsys;

        check_snprintf(filename, sizeof(filename), DATATREE_PLOT_PATTERN_FMT, subsys->sysname);
        XhFindFilename(filename, filename, sizeof(filename));
        if (MotifKnobs_SaveHistplotData(hp, filename, NULL) >= 0)
        {
            XhMakeMessage (win, "History-plot saved to \"%s\"", filename);
            ChlRecordEvent(win, "History-plot saved to \"%s\"", filename);
        }
        else
        {
            err = errno;
            XhMakeMessage (win, "Error saving history-plot to \"%s\": %s", filename, strerror(err));
            ChlRecordEvent(win, "Error saving history-plot to \"%s\": %s", filename, strerror(err));
        }
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_MB3)
    {
        MotifKnobs_Mouse3Handler(evinfo->k->w, evinfo->k,
                                 evinfo->event, evinfo->continue_to_dispatch);
    }
}

static void PlotUpdateProc(void *privptr, int reason)
{
  dlgrec_t *rec = privptr;

    if      (reason == HISTPLOT_REASON_UPDATE_PLOT_GRAPH)
        MotifKnobs_UpdateHistplotGraph(&(rec->mkhp));
    else if (reason == HISTPLOT_REASON_UPDATE_PROT_PROPS)
        MotifKnobs_UpdateHistplotProps(&(rec->mkhp));
}

static void CreateHistPlotDialog(XhWindow  win, DataSubsys subsys, dlgrec_t *rec)
{
  Widget      frame;
  char        buf[100];
  const char *sysname = subsys->sysname;

    check_snprintf(buf, sizeof(buf),
                   "%s: History plot", sysname != NULL? sysname : "???SYSNAME???");
    rec->box = XhCreateStdDlg(win, "histPlot", buf, NULL, NULL,
                              XhStdDlgFNothing | XhStdDlgFNoMargins | XhStdDlgFResizable | XhStdDlgFZoomable);

    frame = XtVaCreateManagedWidget("frame", xmFrameWidgetClass,
                                    rec->box,
                                    XmNshadowType,      XmSHADOW_IN,
                                    XmNshadowThickness, 1,
                                    NULL);
    MotifKnobs_CreateHistplot(&(rec->mkhp), frame,
                              DEF_PLOT_W, DEF_PLOT_H,
                              MOTIFKNOBS_HISTPLOT_FLAG_WITH_CLOSE_B,
                              subsys->cyclesize_us,
                              ProcessMkHpEvent, rec);
    XtVaSetValues(rec->box, XmNdefaultButton, rec->mkhp.ok, NULL);
    express_hist_interest(CdrGetMainGrouping(subsys),
                          PlotUpdateProc, rec,
                          1);

    rec->initialized = 1;
}

//////////////////////////////////////////////////////////////////////

void _ChlToHistPlot_m(DataKnob k)
{
  XhWindow       win = XhWindowOf(k->w);
  chl_privrec_t *privrec = _ChlPrivOf(win);
  dlgrec_t      *rec = ThisDialog(win);

    if (CdrAddHistory(k, privrec->subsys->def_histring_len) != 0) return;
    MotifKnobs_AddToHistplot(&(rec->mkhp), k);
    XhStdDlgShow(rec->box);
}
