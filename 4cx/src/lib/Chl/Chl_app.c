#include "Chl_includes.h"
#include "MotifKnobs_cda_leds.h"

#include "cxscheduler.h"

#include "KnobsP.h"


#include "pixmaps/btn_open.xpm"
#include "pixmaps/btn_save.xpm"
#include "pixmaps/btn_quit.xpm"
#include "pixmaps/btn_pause.xpm"
#include "pixmaps/btn_help.xpm"

#include "pixmaps/btn_mini_open.xpm"
#include "pixmaps/btn_mini_save.xpm"
#include "pixmaps/btn_mini_quit.xpm"
#include "pixmaps/btn_mini_pause.xpm"
#include "pixmaps/btn_mini_help.xpm"


//////////////////////////////////////////////////////////////////////

// GetCblistSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Cblist, chl_histinterest_cbinfo_t,
                                 histinterest_cb, proc, NULL, (void*)1,
                                 0, 2, 0,
                                 cr->, cr,
                                 chl_privrec_t *cr, chl_privrec_t *cr)

//////////////////////////////////////////////////////////////////////

static int  _ChlHistInterest_m(DataKnob k,
                               _m_histplot_update proc, void *privptr,
                               int set1_fgt0)
{
  DataSubsys      subsys = get_knob_subsys(k);
  chl_privrec_t  *cr;

  int                        id;
  chl_histinterest_cbinfo_t *p;

    ////fprintf(stderr, "%s '%s' sys=%p\n", __FUNCTION__, k->ident, subsys);
    if (subsys == NULL) return -1;
    cr = subsys->gui_privptr;

    if (set1_fgt0 == 1)
    {
        id = GetCblistSlot(cr);
        if (id < 0) return -1;
        p = AccessCblistSlot(id, cr);
        p->proc    = proc;
        p->privptr = privptr;
    }
    else
    {
        /*!!! Find the slot and set p->proc=NULL */
    }

    return 0;
}

static void _ChlSndPhys_m(DataKnob k, double v)
{
    CdrSetKnobValue(get_knob_subsys(k), k, v, 0);
}

static void _ChlSndText_m(DataKnob k, const char *s)
{
    CdrSetKnobText (get_knob_subsys(k), k, s, 0);
}

static int  _ChlHistorize_m(DataKnob k)
{
  DataSubsys subsys = get_knob_subsys(k);

    return CdrAddHistory(k, subsys != NULL? subsys->def_histring_len : 86400);
}

static dataknob_cont_vmt_t ChlSysVMT =
{
    {.type = DATAKNOB_CONT},
    _ChlSndPhys_m,
    _ChlSndText_m,
    _ChlShowProps_m,
    _ChlShowBigVal_m,
    _ChlToHistPlot_m,
    NULL, // _ChlShowAlarm_m,
    NULL, // AckAlarm
    NULL, // NewData
    NULL, // SetVis
    NULL, // ChildPropsChg
    _ChlHistInterest_m,
    _ChlHistorize_m
};

//////////////////////////////////////////////////////////////////////

enum
{
    MIN_CYCLESIZE_US = 1000, MAX_CYCLESIZE_US = 1800*1000000 /*1800*1000000 fits into 31 bits (signed int32) */,
    MIN_HISTRING_LEN = 10,   MAX_HISTRING_LEN = 86400*10
};

typedef struct
{
    int     notoolbar;
    int     nostatusline;
    int     compact;
    int     resizable;

    int     minitoolbar;
    int     with_freeze_btn;
    int     with_save_btn;
    int     with_load_btn;
    int     num_bnr_cols;
    int     with_tbar_loggia;
    int     forbid_argv_params;

    double  cyclesize_secs;
    int     def_histring_len;
} appopts_t;

static psp_paramdescr_t text2appopts[] =
{
    PSP_P_FLAG("notoolbar",          appopts_t, notoolbar,          1, 0),
    PSP_P_FLAG("nostatusline",       appopts_t, nostatusline,       1, 0),
    PSP_P_FLAG("compact",            appopts_t, compact,            1, 0),
    PSP_P_FLAG("resizable",          appopts_t, resizable,          1, 0),

    PSP_P_FLAG("minitoolbar",        appopts_t, minitoolbar,        1, 1),
    PSP_P_FLAG("maxitoolbar",        appopts_t, minitoolbar,        0, 0),

    PSP_P_FLAG("with_freeze_btn",    appopts_t, with_freeze_btn,    1, 0),
    PSP_P_FLAG("with_save_btn",      appopts_t, with_save_btn,      1, 0),
    PSP_P_FLAG("with_load_btn",      appopts_t, with_load_btn,      1, 0),
    PSP_P_INT ("toolbar_bnr_cols",   appopts_t, num_bnr_cols,       0,     0, 100),
    PSP_P_FLAG("with_tbar_loggia",   appopts_t, with_tbar_loggia,   1, 0),
    PSP_P_FLAG("forbid_argv_params", appopts_t, forbid_argv_params, 1, 0),

    PSP_P_REAL("hist_period",        appopts_t, cyclesize_secs,     1.0,   MIN_CYCLESIZE_US/1000000.0, MAX_CYCLESIZE_US/1000000.0),
    PSP_P_INT ("hist_len",           appopts_t, def_histring_len,   86400, MIN_HISTRING_LEN,           MAX_HISTRING_LEN),

    PSP_P_END()
};


static struct timeval  cycle_start;  /* The time when the current cycle had started */
static struct timeval  cycle_end;
static sl_tid_t        cycle_tid            = -1;

static int CallUpdate(chl_histinterest_cbinfo_t *p, void *privptr)
{
    p->proc(p->privptr, ptr2lint(privptr));
    return 0;
}
static void CycleCallback(int       uniq     __attribute__((unused)),
                          void     *privptr1,
                          sl_tid_t  tid      __attribute__((unused)),
                          void     *privptr2)
{
  DataSubsys      subsys = privptr1;
  chl_privrec_t  *cr     = privptr2;

    cycle_tid = -1;
    cycle_start = cycle_end;
    timeval_add_usecs(&cycle_end, &cycle_start, subsys->cyclesize_us);

    cycle_tid = sl_enq_tout_at(0, subsys,
                               &cycle_end, CycleCallback, cr);

    if (subsys->is_freezed) return;

    /* 1. Shift history */
    CdrShiftHistory(subsys);
    /* 2. Update all displays */
    ForeachCblistSlot(CallUpdate, lint2ptr(HISTPLOT_REASON_UPDATE_PLOT_GRAPH), cr);
}
void _ChlAppUpdatePlots(chl_privrec_t  *cr)
{
    ForeachCblistSlot(CallUpdate, lint2ptr(HISTPLOT_REASON_UPDATE_PROT_PROPS), cr);
}

static int CmdProc(XhWindow window, const char *cmd, int info_int)
{
  chl_privrec_t *cr     = _ChlPrivOf(window);
  DataKnob       root_k = CdrGetMainGrouping(cr->subsys);
  int            r;

    ////fprintf(stderr, "%s %s:%d\n", __FUNCTION__, cmd, info_int);  
    if ((r = CdrBroadcastCmd  (root_k, cmd, info_int)) != 0) return r;
    if (cr->upper_commandproc != NULL  &&
        (r = cr->upper_commandproc  (window, cmd, info_int)) != 0) return r;
    return ChlHandleStdCommand(window, cmd, info_int);
}

static void LEDS_KeepaliveProc(XtPointer     closure,
                               XtIntervalId *id      __attribute__((unused)))
{
  DataSubsys  subsys = closure;

    XtAppAddTimeOut(xh_context, 1000, LEDS_KeepaliveProc, closure);
    MotifKnobs_leds_update(subsys->leds_ptr);
}

static xh_actdescr_t stdtoolslist[] =
{
    XhXXX_TOOLCMD(CHL_STDCMD_LOAD_MODE, "Load mode",            btn_open_xpm,  btn_mini_open_xpm),
    XhXXX_TOOLCMD(CHL_STDCMD_SAVE_MODE, "Save mode",            btn_save_xpm,  btn_mini_save_xpm),
    XhXXX_TOOLBANNER(0),
    XhXXX_TOOLLOGGIA(),
    XhXXX_SEPARATOR(),
    XhXXX_TOOLCHK(CHL_STDCMD_FREEZE,    "Freeze",               btn_pause_xpm, btn_mini_pause_xpm),
    XhXXX_TOOLLEDS(),
    XhXXX_TOOLCMD(CHL_STDCMD_SHOW_HELP, "Short help",           btn_help_xpm,  btn_mini_help_xpm),
    XhXXX_TOOLFILLER(),
    XhXXX_TOOLCMD(CHL_STDCMD_QUIT,      "Quit the program",     btn_quit_xpm,  btn_mini_quit_xpm),
    XhXXX_END()
};
int  ChlRunSubsystem(DataSubsys subsys,
                     xh_actdescr_t *toolslist, XhCommandProc commandproc)
{
  XhWindow        win;
  chl_privrec_t  *cr;
  const char     *win_title    = CdrFindSection(subsys, DSTN_WINTITLE,   "main");
  const char     *win_name     = CdrFindSection(subsys, DSTN_WINNAME,    "main");
  const char     *win_class    = CdrFindSection(subsys, DSTN_WINCLASS,   "main");
  const char     *win_options  = CdrFindSection(subsys, DSTN_WINOPTIONS, "main");

  int             n;

  data_section_t *p;

  appopts_t       opts;
  int             toolbar_mask     = XhWindowToolbarMask;
  int             minitoolbar_mask = 0;
  int             statusline_mask  = XhWindowStatuslineMask;
  int             compact_mask     = 0;
  int             resizable_mask   = XhWindowUnresizableMask;

  CxWidget        root_w;

  CxWidget        leds_button;
  CxWidget        leds_grid;

    ChlClearErr();

    if (toolslist == NULL) toolslist = stdtoolslist;

    if (win_title == NULL) win_title = "(UNTITLED)";
    if (win_name  == NULL) win_name  = "pult";//"unnamed";
    if (win_class == NULL) win_class = "Pult";//"UnClassed";

    bzero(&opts, sizeof(opts)); // Is required, since none of the flags have defaults specified
    if (psp_parse(win_options, NULL,
                  &opts,
                  Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                  text2appopts) != PSP_R_OK)
    {
        ChlSetErr("%s(): win_options: %s",
                  __FUNCTION__, psp_error());
        return -1;
    }
    if (opts.forbid_argv_params == 0)
        for (n = 0, p = subsys->sections;
             n < subsys->num_sections;
             n++,   p++)
            if (strcasecmp(p->type, DSTN_ARGVWINOPT) == 0)
            {
                if (psp_parse_as(p->data, NULL,
                                 &opts, 
                                 Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                                 text2appopts,
                                 PSP_MF_NOINIT) != PSP_R_OK)
                {
                    ChlSetErr("%s: argv[] options: %s (\"%s\")",
                              __FUNCTION__, psp_error(), (char*)(p->data));
                    return -1;
                }
            }
    subsys->cyclesize_us     = opts.cyclesize_secs * 1000000.0;
    if (subsys->cyclesize_us < MIN_CYCLESIZE_US) subsys->cyclesize_us = MIN_CYCLESIZE_US;
    if (subsys->cyclesize_us > MAX_CYCLESIZE_US) subsys->cyclesize_us = MAX_CYCLESIZE_US;
    subsys->def_histring_len = opts.def_histring_len;

    if (opts.notoolbar||0)    toolbar_mask     = 0;
    if (opts.nostatusline) statusline_mask  = 0;
    if (opts.compact)      compact_mask     = XhWindowCompactMask;
    if (opts.resizable)    resizable_mask   = 0;

    if (opts.minitoolbar)  minitoolbar_mask = XhWindowToolbarMiniMask;

    /* Wipe out unused buttons in case of standard toolbar and modify what needed */
    if (toolslist == stdtoolslist)
    {
        for (n = 0;  toolslist[n].type != XhACT_END;  n++)
            if (
                (toolslist[n].type == XhACT_CHECKBOX                      &&
                 strcasecmp(toolslist[n].cmd, CHL_STDCMD_FREEZE)    == 0  &&
                 opts.with_freeze_btn  == 0)
                ||
                (toolslist[n].type == XhACT_COMMAND                       &&
                 strcasecmp(toolslist[n].cmd, CHL_STDCMD_LOAD_MODE) == 0  &&
                 opts.with_load_btn    == 0)
                ||
                (toolslist[n].type == XhACT_COMMAND                       &&
                 strcasecmp(toolslist[n].cmd, CHL_STDCMD_SAVE_MODE) == 0  &&
                 opts.with_save_btn    == 0)
                ||
                (toolslist[n].type == XhACT_LOGGIA                        &&
                 opts.with_tbar_loggia == 0)
               )
                toolslist[n].type = XhACT_NOP;
            else if (toolslist[n].type == XhACT_BANNER)
                toolslist[n].cmd = lint2ptr(opts.num_bnr_cols);
    }

    win = XhCreateWindow(win_title, win_name, win_class,
                         toolbar_mask | statusline_mask | compact_mask | resizable_mask |
                         minitoolbar_mask,
                         NULL, toolslist);
    if (win == NULL)
    {
        ChlSetErr("XhCreateWindow=NULL!");
        return -1;
    }
    
    cr = _ChlPrivOf(win);
    if (cr == NULL)
    {
        ChlSetErr("%s: malloc(chl_privrec_t) fail", __FUNCTION__);
        return -1;
    }
    cr->subsys = subsys;
    subsys->gui_privptr = cr;

    cr->upper_commandproc = commandproc;
    XhSetWindowCmdProc(win, CmdProc);

    subsys->sys_vmtlink = &ChlSysVMT;
    if (CreateKnob(CdrGetMainGrouping(subsys), XhGetWindowWorkspace(win)) < 0)
    {
        ChlSetErr("%s", KnobsLastErr());
        return -1;
    }
    if (opts.resizable)
    {
        root_w = GetKnobWidget(CdrGetMainGrouping(subsys));
        attachleft  (root_w, NULL, 0);
        attachright (root_w, NULL, 0);
        attachtop   (root_w, NULL, 0);
        attachbottom(root_w, NULL, 0);
    }

    if ((leds_button = XhGetWindowAlarmleds(win)) != NULL  &&
        (subsys->leds_ptr = XtCalloc(1, sizeof(MotifKnobs_leds_t))) != NULL)
    {
        leds_grid = XhCreateGrid(leds_button, "alarmLeds");
        attachtop   (leds_grid, NULL, 0);
        attachbottom(leds_grid, NULL, 0);
        XhGridSetGrid(leds_grid, 0, 0);
        MotifKnobs_leds_create(subsys->leds_ptr,
                               leds_grid, opts.minitoolbar? -15 : 20,
                               subsys->cid, MOTIFKNOBS_LEDS_PARENT_GRID);
        LEDS_KeepaliveProc((XtPointer)subsys, NULL);
    }

    gettimeofday(&cycle_start, NULL);
    timeval_add_usecs(&cycle_end, &cycle_start, subsys->cyclesize_us);
    cycle_tid = sl_enq_tout_at(0, subsys,
                               &cycle_end, CycleCallback, cr);

    XhShowWindow      (win);
    XhRunApplication();
  
    return 0;
}

/*********************************************************************
  Standard command management
*********************************************************************/

static int LoadModeFilter(const char *fname, 
                          time_t *cr_time,
                          char *labelbuf, size_t labelbuf_size,
                          void *privptr  __attribute__((unused)))
{
    return CdrStatSubsystemMode(fname, cr_time, labelbuf, labelbuf_size);
}
int ChlHandleStdCommand(XhWindow window, const char *cmd, int info_int)
{
  chl_privrec_t *cr = _ChlPrivOf(window);
  stddlgs_rec_t *dr = &(cr->dr);
  char           filename[PATH_MAX];
  char           comment[400];

    if      (strcmp(cmd, CHL_STDCMD_LOAD_MODE) == 0)
    {
        check_snprintf(filename, sizeof(filename),
            DATATREE_MODE_PATTERN_FMT, cr->subsys->sysname);
        if (dr->load_nfndlg == NULL)
            dr->load_nfndlg = XhCreateLoadDlg(window, "loadModeDialog",
                                              NULL, NULL,
                                              LoadModeFilter, NULL,
                                              filename,
                                              CHL_STDCMD_LOAD_MODE_ACTION, NULL);
        XhLoadDlgShow(dr->load_nfndlg);
    }
    else if (strcmp(cmd, CHL_STDCMD_SAVE_MODE) == 0)
    {
        if (dr->save_nfndlg == NULL)
            dr->save_nfndlg = XhCreateSaveDlg(window, "saveModeDialog",
                                              NULL, NULL,
                                              CHL_STDCMD_SAVE_MODE_ACTION);
        XhSaveDlgShow(dr->save_nfndlg);
    }
    else if (strcmp(cmd, CHL_STDCMD_LOAD_MODE_ACTION) == 0)
    {
        XhLoadDlgHide(dr->load_nfndlg);
        if (info_int == 0)
        {
            XhLoadDlgGetFilename(dr->load_nfndlg, filename, sizeof(filename));
            if (filename[0] == '\0') goto RET;
            ChlLoadWindowMode(window, filename,
                              CDR_MODE_OPT_PART_EVERYTHING,
                              comment, sizeof(comment));
            XhSetToolbarBanner(window, comment);
        }
    }
    else if (strcmp(cmd, CHL_STDCMD_SAVE_MODE_ACTION) == 0)
    {
        XhSaveDlgHide(dr->save_nfndlg);
        if (info_int == 0)
        {
            XhSaveDlgGetComment(dr->save_nfndlg, comment, sizeof(comment));
            check_snprintf(filename, sizeof(filename),
                DATATREE_MODE_PATTERN_FMT, cr->subsys->sysname);
            XhFindFilename(filename, filename, sizeof(filename));
            ChlSaveWindowMode(window, filename,
                              CDR_MODE_OPT_PART_EVERYTHING,
                              comment);
        }
    }
    else if (strcmp(cmd, CHL_STDCMD_FREEZE) == 0)
        cr->subsys->is_freezed = info_int;
    else if (strcmp(cmd, CHL_STDCMD_SHOW_HELP) == 0)
        ChlShowHelp(window, CHL_HELP_ALL);
    else if (strcmp(cmd, CHL_STDCMD_QUIT)      == 0)
        exit(0);
    else return 0;

 RET:
    return 1;
}

static void ReportLoadOrSave(XhWindow window,
                             int is_save, const char *filename, int result)
{
  const char *shortname;

    if (result != 0)
    {
        XhMakeMessage(window, "Unable to open \"%s\"", filename);
    }
    else
    {
        shortname = strrchr(filename, '/');
        if (shortname == NULL) shortname = filename;
        else                   shortname++;
        
        XhMakeMessage (window,
                       "Mode was %s \"%s\"",
                       is_save? "written to" : "read from",
                       shortname);
        ChlRecordEvent(window,
                       "Mode was %s \"%s\"",
                       is_save? "written to" : "read from",
                       filename);
    }
}

int    ChlSaveWindowMode(XhWindow window, const char *path, int options,
                         const char *comment)
{
  int  r = CdrSaveSubsystemMode(_ChlPrivOf(window)->subsys, path, options, comment);

    ReportLoadOrSave(window, 1, path, r);

    return r;
}

int    ChlLoadWindowMode(XhWindow window, const char *path, int options,
                         char *labelbuf, size_t labelbuf_size)
{
  int  r = CdrLoadSubsystemMode(_ChlPrivOf(window)->subsys, path, options,
                                labelbuf, labelbuf_size);

    ReportLoadOrSave(window, 0, path, r);

    return r;
}

void   ChlRecordEvent   (XhWindow window, const char *format, ...)
{
  chl_privrec_t *cr = _ChlPrivOf(window);
  char         buf[PATH_MAX];
  FILE        *fp;
  va_list      ap;
  
    snprintf(buf, sizeof(buf), DATATREE_OPS_PATTERN_FMT, cr->subsys->sysname);
    fp = fopen(buf, "a");
    va_start(ap, format);
    if (fp != NULL)
    {
        fprintf (fp, "%s ", strcurtime());
        vfprintf(fp, format, ap);
        fprintf (fp, "\n");
        fclose(fp);
    }
    else
    {
        fprintf (stderr, "%s Problem opening record-log \"%s\" for append: %s\n",
                 strcurtime(), buf, strerror(errno));
        fprintf (stderr, "%s The record is: ", strcurtime());
        vfprintf(stderr, format, ap);
        fprintf (stderr, "\n");
    }
    va_end(ap);
}
