#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <sys/time.h>

#include "misclib.h"

#include "cxlib.h"
#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_fallbacks.h"
#include "MotifKnobsP.h" // For ABSTRZE()/CNCRTZE()
#include "Chl.h" /*!!! For CHL_STDCMD_* */

#include "pzframe_data.h"
#include "pzframe_gui.h"
#include "pzframe_main.h"

#include "pixmaps/btn_open.xpm"
#include "pixmaps/btn_save.xpm"
#include "pixmaps/btn_start.xpm"
#include "pixmaps/btn_once.xpm"
#include "pixmaps/btn_help.xpm"
#include "pixmaps/btn_quit.xpm"

#include "pixmaps/btn_mini_open.xpm"
#include "pixmaps/btn_mini_save.xpm"
#include "pixmaps/btn_mini_start.xpm"
#include "pixmaps/btn_mini_once.xpm"
#include "pixmaps/btn_mini_help.xpm"
#include "pixmaps/btn_mini_quit.xpm"


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

static int only_digits(const char *s)
{
    for (;  *s != '\0';  s++)
        if (!isdigit(*s)) return 0;

    return 1;
}

//////////////////////////////////////////////////////////////////////

static pzframe_gui_t       *global_gui = NULL;
static XhWindow             pz_win;
static MotifKnobs_leds_t    global_leds;


static void ShowRunMode(void)
{
    XhSetCommandOnOff  (pz_win, PZFRAME_MAIN_CMD_LOOP,  global_gui->pfr->cfg.running);
    //XhSetCommandOnOff  (pz_win, PZFRAME_MAIN_CMD_ONCE,  global_gui->pfr->cfg.oneshot);
    XhSetCommandEnabled(pz_win, PZFRAME_MAIN_CMD_ONCE, !global_gui->pfr->cfg.running);
}

static void SetRunMode (int is_loop)
{
    PzframeDataSetRunMode(global_gui->pfr, is_loop, 0);
    ShowRunMode();
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    char    app_name [32];
    char    app_class[32];
    char    title    [128];

    int     notoolbar;
    int     nostatusline;
    int     compact;
    int     noresize;
    int     minitoolbar;
} pzframe_main_opts_t;

static psp_paramdescr_t text2pzframe_main_opts[] =
{
    PSP_P_STRING("app_name",      pzframe_main_opts_t, app_name,     NULL),
    PSP_P_STRING("app_class",     pzframe_main_opts_t, app_class,    NULL),
    PSP_P_STRING("title",         pzframe_main_opts_t, title,        NULL),
  
    PSP_P_FLAG  ("notoolbar",    pzframe_main_opts_t, notoolbar,    1, 0),
    PSP_P_FLAG  ("nostatusline", pzframe_main_opts_t, nostatusline, 1, 0),
    PSP_P_FLAG  ("compact",      pzframe_main_opts_t, compact,      1, 0),
    PSP_P_FLAG  ("noresize",     pzframe_main_opts_t, noresize,     1, 0),

    PSP_P_FLAG ("minitoolbar",   pzframe_main_opts_t, minitoolbar,  1, 0),
    PSP_P_FLAG ("maxitoolbar",   pzframe_main_opts_t, minitoolbar,  0, 1),

    PSP_P_END()
};

static void EventProc(pzframe_gui_t *gui,
                      int            reason,
                      int            info_int,
                      void          *privptr)
{
    ShowRunMode();
}


static void PzframeMainLoadData(const char *filename)
{
    if (global_gui->pfr->ftd->vmt.load != NULL  &&
        global_gui->pfr->ftd->vmt.load(global_gui->pfr, filename) == 0)
        XhMakeMessage(pz_win, "Data read from \"%s\"", filename);
    else
        XhMakeMessage(pz_win, "Error reading from \"%s\": %s", filename, strerror(errno));

    /* Renew displayed data */
    PzframeGuiUpdate(global_gui, 1);

    SetRunMode(0);
}

static void PzframeMainSaveData(const char *filename, const char *comment)
{
    if (global_gui->pfr->ftd->vmt.save != NULL  &&
        global_gui->pfr->ftd->vmt.save(global_gui->pfr, filename, NULL, comment) == 0)
        XhMakeMessage(pz_win, "Data saved to \"%s\"", filename);
    else
        XhMakeMessage(pz_win, "Error saving to \"%s\": %s", filename, strerror(errno));
}

static void LEDS_KeepaliveProc(XtPointer     closure __attribute__((unused)),
                               XtIntervalId *id      __attribute__((unused)))
{
    XtAppAddTimeOut(xh_context, 1000, LEDS_KeepaliveProc, NULL);
    MotifKnobs_leds_update(&global_leds);
}

static xh_actdescr_t toolslist[] =
{
    XhXXX_TOOLCMD   (CHL_STDCMD_LOAD_MODE,  "Read measurement from file", btn_open_xpm,  btn_mini_open_xpm),
    XhXXX_TOOLCMD   (CHL_STDCMD_SAVE_MODE,  "Save measurement to file",   btn_save_xpm,  btn_mini_save_xpm),
    XhXXX_TOOLLABEL ("ZZZ"),
    //XhXXX_SEPARATOR (),
    XhXXX_TOOLCHK   (PZFRAME_MAIN_CMD_LOOP,  "Periodic measurement",      btn_start_xpm, btn_mini_start_xpm),
    XhXXX_TOOLCMD   (PZFRAME_MAIN_CMD_ONCE,  "One measurement",           btn_once_xpm,  btn_mini_once_xpm),
    XhXXX_SEPARATOR (),
    //XhXXX_TOOLCMD   (PZFRAME_MAIN_CMD_SET_GOOD,       "Save current data as good",  btn_setgood_xpm),
    //XhXXX_TOOLCMD   (PZFRAME_MAIN_CMD_RST_GOOD,       "Forget saved data",          btn_rstgood_xpm),
    XhXXX_TOOLLEDS  (),
    XhXXX_TOOLCMD   (CHL_STDCMD_SHOW_HELP,   "Short help",                 btn_help_xpm, btn_mini_help_xpm),
    XhXXX_TOOLFILLER(),
    XhXXX_TOOLCMD   (CHL_STDCMD_QUIT,        "Quit the program",           btn_quit_xpm, btn_mini_quit_xpm),
    XhXXX_END()
};


static CxWidget save_nfndlg = NULL;
static CxWidget load_nfndlg = NULL;

static int CommandProc(XhWindow win, const char *cmd, int info_int)
{
  char          filename[PATH_MAX];
  char          comment[400];
  
    if      (strcmp(cmd, CHL_STDCMD_LOAD_MODE)  == 0)
    {
        check_snprintf(filename, sizeof(filename),
            DATATREE_DATA_PATTERN_FMT, global_gui->pfr->ftd->type_name);
        if (load_nfndlg == NULL)
            load_nfndlg = XhCreateLoadDlg(win, "loadModeDialog",
                                          NULL, NULL,
                                          PzframeDataFdlgFilter, global_gui->pfr,
                                          filename,
                                          CHL_STDCMD_LOAD_MODE_ACTION, NULL);
        XhLoadDlgShow(load_nfndlg);
    }
    else if (strcmp(cmd, CHL_STDCMD_SAVE_MODE)  == 0)
    {
        if (save_nfndlg == NULL)
            save_nfndlg = XhCreateSaveDlg(win, "saveModeDialog",
                                          NULL, NULL,
                                          CHL_STDCMD_SAVE_MODE_ACTION);
        XhSaveDlgShow(save_nfndlg);
    }
    else if (strcmp(cmd, CHL_STDCMD_LOAD_MODE_ACTION) == 0)
    {
        XhLoadDlgHide(load_nfndlg);
        if (info_int == 0)
        {
            XhLoadDlgGetFilename(load_nfndlg, filename, sizeof(filename));
            if (filename[0] == '\0') goto RET;
            PzframeMainLoadData(filename);
        }
    }
    else if (strcmp(cmd, CHL_STDCMD_SAVE_MODE_ACTION) == 0)
    {
        XhSaveDlgHide(save_nfndlg);
        if (info_int == 0)
        {
            XhSaveDlgGetComment(save_nfndlg, comment, sizeof(comment));
            check_snprintf(filename, sizeof(filename),
                DATATREE_DATA_PATTERN_FMT, global_gui->pfr->ftd->type_name);
            XhFindFilename(filename, filename, sizeof(filename));
            PzframeMainSaveData(filename, comment);
        }
    }
    else if (strcmp(cmd, PZFRAME_MAIN_CMD_LOOP) == 0)
        SetRunMode(info_int);
    else if (strcmp(cmd, PZFRAME_MAIN_CMD_ONCE) == 0)
    {
        PzframeDataSetRunMode(global_gui->pfr, -1, 1);
        ShowRunMode();
    }
    else if (strcmp(cmd, CHL_STDCMD_SHOW_HELP)  == 0)
        ChlShowHelp(win, CHL_HELP_ALL &~ CHL_HELP_MOUSE);
    else if (strcmp(cmd, CHL_STDCMD_QUIT)       == 0)
        exit(0);
    else return 0;

 RET:
    return 1;
}
        

static void ShowHelp(const char *argv0, void *zzz /*!!! M-m-m??? Pass tables[],count? */)
{
    printf("Usage: %s SERVER.BASE [OPTIONS]\n", argv0);
    
    exit(1);
}

static const char *fallbacks[] =
{
    XH_STANDARD_FALLBACKS,
};

int  PzframeMain(int argc, char *argv[],
                 const char *def_app_name, const char *def_app_class,
                 pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd,
                 psp_paramdescr_t *table2, void *rec2,
                 psp_paramdescr_t *table4, void *rec4)
{
  pzframe_main_opts_t  opts;

  const char          *file_to_load = NULL;
  const char          *base_to_use  = NULL;

  int                  n;

  Widget               w;

  enum {PK_PARAM, PK_BASE, PK_FILE};
  int          pkind;
  const char  *p_eq; // '='
  const char  *p_sl; // '/'
  const char  *p_cl; // ':'

  void             *recs  [] =
  {
//      &(gui->pfr->cfg.param_iv),
      rec2,
      &(gui->pfr->cfg),
      rec4,
      &(gui->look),
      &(opts),
  };
  psp_paramdescr_t *tables[] =
  {
//      gkd->ftd->specific_params,
      table2,
      pzframe_data_text2cfg,
      table4,
      pzframe_gui_text2look,
      text2pzframe_main_opts,
  };

  CxWidget             leds_button;
  CxWidget             leds_grid;

    global_gui = gui;

    /**** Initialize toolkits ***************************************/
    XhSetColorBinding("BG_DEFAULT",   "#f9eaa0");
    XhSetColorBinding("TS_DEFAULT",   "#fdf8e3");
    XhSetColorBinding("BS_DEFAULT",   "#bbb078");
    XhSetColorBinding("BG_ARMED",     "#fcf4d0");
    XhSetColorBinding("GRAPH_REPERS", "#00FF00");
  
    /* Initialize and parse command line */
    XhInitApplication(&argc, argv, def_app_name, def_app_class, fallbacks);

    /**** Prepare PSP-tables ****************************************/
    /*!!!*/
    /* main */
    {
      psp_paramdescr_t  *item;

        for (item = text2pzframe_main_opts;  item->name != NULL;  item++)
        {
            if (strcasecmp(item->name, "app_name")  == 0)
                item->var.p_string.defval = def_app_name;
            if (strcasecmp(item->name, "app_class") == 0)
                item->var.p_string.defval = def_app_class;
        }
    }

    /**** Parse command line parameters *****************************/
    /* Initialize everything to defaults */
    bzero(&opts, sizeof(opts));
    psp_parse_v(NULL, NULL,
                recs, tables, countof(tables),
                '=', "", "", 0);
    /* Walk through command line */
    for (n = 1;  n < argc;  n++)
    {
        if (strcmp(argv[n], "-h")     == 0  ||
            strcmp(argv[n], "-help")  == 0  ||
            strcmp(argv[n], "--help") == 0)
            ShowHelp(argv[0], NULL);

        if (strcmp(argv[n], "-readonly") == 0) argv[n]++;


        /* Paramkind determination heuristics */
        pkind = PK_PARAM;
        p_eq = strchr(argv[n], '=');
        p_sl = strchr(argv[n], '/');
        p_cl = strchr(argv[n], ':');

        // =~ /[a-zA-Z]=/
        if (p_eq != NULL  &&  only_letters(argv[n], p_eq - argv[n]))
            pkind = PK_PARAM;

        // 
        if (argv[n][0] == '/'  ||  memcmp(argv[n], "./", 2) == 0  ||
            (p_sl != NULL  &&  (p_eq == NULL  ||  p_sl < p_eq)))
            pkind = PK_FILE;

        if (p_eq == NULL  &&  p_sl == NULL  &&
            p_cl != NULL)
            pkind = PK_BASE;

        if      (pkind == PK_FILE)
            file_to_load = argv[n];
        else if (pkind == PK_BASE)
            base_to_use  = argv[n];
        else
        {
            if (psp_parse_v(argv[n], NULL,
                            recs, tables, countof(tables),
                            '=', "", "",
                            PSP_MF_NOINIT) != PSP_R_OK)
                fprintf(stderr, "%s: %s\n", argv[0], psp_error());
        }
    }

    /**** Use data from command line ********************************/
    if (base_to_use == NULL) base_to_use = "";

    /**** Create window *********************************************/
    for (n = 0;  n < countof(toolslist);  n++)
        if (toolslist[n].type == XhACT_LABEL)
        {
            toolslist[n].label = global_gui->pfr->ftd->type_name;
            break;
        }
    if (opts.title[0] == '\0')
    {
        check_snprintf(opts.title, sizeof(opts.title),
                       "%s: %s",
                       global_gui->pfr->ftd->type_name,
                       base_to_use[0] != '\0'? base_to_use : "???");
    }

    pz_win = XhCreateWindow(opts.title, opts.app_name, opts.app_class,
                            (opts.notoolbar     ?  0 : XhWindowToolbarMask)     |
                            (opts.nostatusline  ?  0 : XhWindowStatuslineMask)  |
                            (opts.noresize == 0  &&
                             (global_gui->d->bhv & PZFRAME_GUI_BHV_NORESIZE) == 0?
                                                   0 : XhWindowUnresizableMask) |
                            (opts.compact  == 0 ?  0 : XhWindowCompactMask)     |
                            (opts.minitoolbar==0?  0 : XhWindowToolbarMiniMask),
                            NULL,
                            toolslist);
    XhSetWindowCmdProc (pz_win, CommandProc);

    global_gui->look.noleds = !opts.notoolbar;
    if (global_gui->d->vmt.realize != NULL  &&
        global_gui->d->vmt.realize(global_gui,
                                   CNCRTZE(XhGetWindowWorkspace(pz_win)),
                                   CDA_CONTEXT_ERROR, base_to_use) < 0) return 1;

    w = global_gui->top;
    attachleft  (w, NULL, 0);
    attachright (w, NULL, 0);
    attachtop   (w, NULL, 0);
    attachbottom(w, NULL, 0);

    if ((leds_button = XhGetWindowAlarmleds(pz_win)) != NULL)
    {
        leds_grid = XhCreateGrid(leds_button, "alarmLeds");
        attachtop   (leds_grid, NULL, 0);
        attachbottom(leds_grid, NULL, 0);
        XhGridSetGrid(leds_grid, 0, 0);
        MotifKnobs_leds_create(&global_leds,
                               leds_grid, opts.minitoolbar? -15 : 20,
                               global_gui->pfr->cid, MOTIFKNOBS_LEDS_PARENT_GRID);
        LEDS_KeepaliveProc(NULL, NULL);
    }

    /**** Run *******************************************************/
    SetRunMode(global_gui->pfr->cfg.running);
    XhShowWindow(pz_win);

    if (file_to_load != NULL)
        PzframeMainLoadData(file_to_load);

    XhRunApplication();

    return 0;
}
