#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "misclib.h"
#include "memcasecmp.h"
#include "timeval_utils.h"
#include "cxscheduler.h"

#include "Xh.h"
#include "Xh_fallbacks.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "Cdr.h"
#include "CdrP.h"
#include "MotifKnobs_histplot.h"


enum
{
    DEF_PLOT_W = 600,
    DEF_PLOT_H = 400,
};


static XhWindow               win;
static MotifKnobs_histplot_t  mkhp;
static DataSubsys             ds;

int                width        = DEF_PLOT_W;
int                height       = DEF_PLOT_H;
static const char *defserver    = NULL;
static const char *baseref      = NULL;
static int         cyclesize_us = 1000000;
static int         histring_len = 0;
static int         horz_ctls    = 0;

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

    if      (reason == MOTIFKNOBS_HISTPLOT_REASON_CLOSE)
    {
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_SAVE)
    {
    }
    else if (reason == MOTIFKNOBS_HISTPLOT_REASON_MB3)
    {
    }
}


static char *fallbacks[] = {XH_STANDARD_FALLBACKS};

void *_CreateKnobReference = CreateKnob;
int main(int argc, char *argv[])
{
  int           arg_n;

  const char   *p_eq;  // '='
  const char   *p_val; // =p_eq+1
  size_t        len;
  const char   *errp;
  double        cyclesize_sec;

  const char   *app_name  = "histplot";
  const char   *app_class = "HistPlot";
  const char   *win_title = "History plot";
  const char   *sysname   = "histplot";

  data_section_t *nsp;
  DataKnob        grp;
  DataKnob        k;

  const char     *label_s;
  int             vict_n;
  DataKnob        victim;

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

    arg_n = 1;
    while (arg_n < argc)
    {
        if      ((p_eq = strchr(argv[arg_n], '=')) != NULL)
        {
            p_val = p_eq + 1;
            len = p_eq - argv[arg_n];
//fprintf(stderr, "len=%zd\n", len);
            if      (len == 0)
                fprintf(stderr,
                        "%s %s: WARNING: wrong parameter spec \"%s\"\n",
                        strcurtime(), argv[0], argv[arg_n]);
            else if (cx_strmemcasecmp("defserver", argv[arg_n], len) == 0)
            {
                defserver = p_val;
            }
            else if (cx_strmemcasecmp("baseref", argv[arg_n], len) == 0)
            {
                ds->main_grouping->u.c.refbase = p_val;
            }
            else if (cx_strmemcasecmp("hist_period", argv[arg_n], len) == 0)
            {
                cyclesize_sec = strtod(p_val, &errp);
                if (errp == p_val  ||  (*errp != '\0'))
                {
                    fprintf(stderr,
                            "%s %s: error in hist_period spec \"%s\"\n",
                            strcurtime(), argv[0], argv[arg_n]);
                    exit(1);
                }
                else
                {
                    /* Note: hard upper limit is about 2000: 2^30/1000000 */
                    if (cyclesize_sec < 0.001  ||  cyclesize_sec > 1000)
                    {
                        fprintf(stderr,
                                "%s %s: insane in hist_period spec \"%s\" is out_of[0.001-1000]\n",
                                strcurtime(), argv[0], argv[arg_n]);
                        exit(1);
                    }
                    cyclesize_us = cyclesize_sec * 1000000;
                }
            }
            else if (cx_strmemcasecmp("hist_len",  argv[arg_n], len) == 0)
            {
                histring_len = strtol(p_val, &errp, 10);
                if (errp == p_val  ||  (*errp != '\0'))
                {
                    fprintf(stderr,
                            "%s %s: error in hist_len spec \"%s\"\n",
                            strcurtime(), argv[0], argv[arg_n]);
                    exit(1);
                }
            }
            else if (cx_strmemcasecmp("horz_ctls",  argv[arg_n], len) == 0)
            {
                horz_ctls = strtol(p_val, &errp, 10);
                if (errp == p_val  ||  (*errp != '\0'))
                {
                    fprintf(stderr,
                            "%s %s: error in horz_ctls spec \"%s\"\n",
                            strcurtime(), argv[0], argv[arg_n]);
                    exit(1);
                }
            }
            else
                fprintf(stderr,
                        "%s %s: WARNING: unknown parameter spec \"%s\"\n",
                        strcurtime(), argv[0], argv[arg_n]);
        }
        else if (grp->u.c.count < MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX)
        {
            k = grp->u.c.content + grp->u.c.count;
            k->type   = DATAKNOB_KNOB;
            k->uplink = grp;
            if ((k->u.k.rd_src =
                 strdup(argv[arg_n])) == NULL)
                fprintf(stderr,
                        "%s %s: failed to strdup(\"%s\")\n",
                        strcurtime(), argv[0], argv[arg_n]);
            else
            {
                
                label_s = strrchr(argv[arg_n], '.');
                if (label_s == NULL) label_s = argv[arg_n];
                else                 label_s++; // Skip '.' itself
                k->label = strdup(label_s);
                strzcpy(k->u.k.dpyfmt, "%8.3f", sizeof(k->u.k.dpyfmt));
                grp->u.c.count++;
            }
        }
        else
            fprintf(stderr,
                    "%s %s: WARNING: too many channels, ignoring \"%s\"\n",
                    strcurtime(), argv[0], argv[arg_n]);

        arg_n++;
    }

    win = XhCreateWindow(win_title, app_name, app_class,
                         0,
                         NULL, NULL);
    if (win == NULL)
    {
        fprintf(stderr, "%s %s: XhCreateWindow=NULL!\n", strcurtime(), argv[0]);
        exit(1);
    }
    
    if (MotifKnobs_CreateHistplot(&mkhp, XhGetWindowWorkspace(win),
                                  width, height,
                                  MOTIFKNOBS_HISTPLOT_FLAG_FIXED |
                                      (horz_ctls? MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS : 0),
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
    for (vict_n = 0;  vict_n < grp->u.c.count;  vict_n++)
    {
        victim = grp->u.c.content + vict_n;
        CdrAddHistory(victim, histring_len);
        MotifKnobs_AddToHistplot(&mkhp, victim);
    }
    
    if (CdrRealizeSubsystem(ds, 0, defserver, argv[0]) < 0)
    {
        fprintf(stderr, "%s: Realize(%s): %s\n",
                argv[0], sysname, CdrLastErr());
        exit(1);
    }
    gettimeofday(&cycle_start, NULL);
    timeval_add_usecs(&cycle_end, &cycle_start, cyclesize_us);
    cycle_tid = sl_enq_tout_at(0, NULL,
                               &cycle_end, CycleCallback, NULL);

    XhShowWindow      (win);
    XhRunApplication();

    return 0;
}
