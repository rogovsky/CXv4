#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>

#include "misc_macros.h"
#include "misclib.h"
#include "timeval_utils.h"
#include "memcasecmp.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_fallbacks.h"

#include "cx_version.h"
#include "datatreeP.h" // For DataSubsys->cid only

#include "cx-starter_msg.h"
#include "cx-starter_cfg.h"
#include "cx-starter_Cdr.h"
#include "cx-starter_con.h"
#include "cx-starter_x11.h"
#include "cx-starter_gui.h"

#include "pixmaps/cx-starter.xpm"


static XhWindow    onewin;
static Widget      dash;

static Widget      prog_menu, prog_menu_start, prog_menu_stop, prog_menu_restart;
static Widget      serv_menu, serv_menu_start, serv_menu_stop, serv_menu_restart;

static int         prog_menu_client;
static int         serv_menu_client;

enum
{
    CMD_START = 1000,
    CMD_STOP,
    CMD_RESTART
};

static int option_passive   = 0;
static int option_noprocess = 0;

static cfgbase_t    *cur_cfgbase = NULL;
static subsyslist_t *cur_syslist = NULL;

#define DEFAULT_CONFIG_NAME "cx-starter.conf"


typedef struct
{
    const char *filespec;
} cfgfile_t;

static cfgfile_t *configs_list        = NULL;
static int        configs_list_allocd = 0;

// GetCfgFileSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, CfgFile, cfgfile_t,
                                 configs, filespec, NULL, (void*)1,
				 0, 10, 0,
				 , , void)

static void AddConfig(const char *filespec)
{
  int        id;
  cfgfile_t *cp;

    id = GetCfgFileSlot();
    if (id < 0)
    {
        fprintf(stderr, "FATAL: unable to add config\n");
	exit(1);
    }
    cp = AccessCfgFileSlot(id);
    cp->filespec = filespec;
}


static void ParseCommandLine(int argc, char *argv[])
{
  int  x;
    
    for (x = 1;  x < argc;  x++)
    {
        if      (strchr(argv[x], '/') != NULL)
        {
            AddConfig(argv[x]);
        }
        else if (strcmp(argv[x], "-passive")   == 0)
            option_passive   = 1;
        else if (strcmp(argv[x], "-noprocess") == 0)
            option_noprocess = 1;
        else if (strcmp(argv[x], "-h")      == 0  ||
                 strcmp(argv[x], "-help")   == 0  ||
                 strcmp(argv[x], "--help")  == 0)
        {
            printf("Usage: %s [-passive] [-readonly] [CONFIG_FILE]...\n", argv[0]);
            exit(1);
        }
/* "List of systems to display" -- not implemented in v4
        else
        {
            if (reqd_count >= MAXSYSTEMS)
            {
                fprintf(stderr, "%s: sorry, too many systems are requested; limit is %d\n",
                        argv[0], MAXSYSTEMS);
                exit(1);
            }
            
            reqd_systems[reqd_count] = argv[x];
            reqd_count++;
        }
*/
    }

    if (configs_list_allocd == 0)
        AddConfig(DEFAULT_CONFIG_NAME);
}



static void HelpCB(Widget     w          __attribute__((unused)),
                   XtPointer  closure,
                   XtPointer  call_data  __attribute__((unused)))
{
  static CxWidget  help_box = NULL;
  static char     *help_text =
      "CX-starter является центром управления пульта\xA0- он служит "
      "для запуска прикладных программ и серверов, "
      "а также для слежения за их состоянием.\n"
      "    В колонке \xABName\xBB расположены кнопки запуска приложений; "
      "в колонке \xAB?\xBB отображается текущее состояние приложения "
      "(выход за границы допустимых диапазонов, программные либо "
      "аппаратные ошибки, и т.д.), "
      "а в колонке \xABSrv\xBB отображается состояние серверов, "
      "используемых приложением (зеленый цвет\xA0- все в порядке, "
      "красный\xA0- сервер не запущен, синий\xA0- сервер завис).\n"
      "    <Правая кнопка мыши> на кнопке приложения или лампочке сервера "
      "вызывает меню для запуска либо останова.  Для запуска приложения "
      "достаточно нажать на его кнопку.  Если приложение уже запущено на экране "
      "(но, например, скрыто под другими окнами или свернуто в пиктограмму), "
      "то оно будет вытянуто наверх."
      ;

    if (help_box == NULL)
    {
        help_box = XhCreateStdDlg(onewin, "help", "CX-starter help",
                                  NULL, "CX-starter - справка",
                                  XhStdDlgFOk | XhStdDlgFResizable);

        XtVaCreateManagedWidget("helpMessage", xmTextWidgetClass, help_box,
                                XmNvalue,           help_text,
                                XmNcolumns,         80,
                                XmNeditMode,        XmMULTI_LINE_EDIT,
                                XmNwordWrap,        True,
                                XmNresizeHeight,    True,

                                XmNshadowThickness,       0,
                                XmNeditable,              False,
                                XmNcursorPositionVisible, False,
                                XmNtraversalOn,           False,
                                NULL);
    }

    XhStdDlgShow(help_box);
}

///////////////////////////////////////////////////////////////////////

static int ChangeClientStatus(int sys, int on)
{
  subsys_t     *sr      = AccessSubsysSlot(sys, cur_syslist);
  Window        win;
  
  Widget        w       = XhGetWindowShell(onewin);
  Display      *dpy     = XtDisplay(w);

  struct timeval  deadline;
  struct timeval  now;
  
  const char   *cmd;
  char          cmd_buf[PATH_MAX];
  const char   *home;
  const char   *cx_root;

  const char   *pfx1;
  const char   *pfx2;

    /* Guard against ... */
    if (on)
    {
        gettimeofday(&now, NULL);
        timeval_add_usecs(&deadline, &(sr->last_start_time), 5*1000000);
        if (!TV_IS_AFTER(now, deadline)) return 0;
    }

    win = SearchForWindow(XtDisplay(w), XtScreen(w), sr->cfg.app_name, sr->cfg.title);

    if (win != 0)
    {
        if (on)
        {
            XhENTER_FAILABLE();
            /*!!! Shouldn't we use "XMapRaised(dpy, win);" here? */
            XMapWindow         (dpy, win);
            XRaiseWindow       (dpy, win);
            XSetInputFocus     (dpy, win, RevertToPointerRoot, CurrentTime);
            PointToTargetWindow(XhGetWindowWorkspace(onewin), XhGetWindowShell(onewin), win);
            XhLEAVE_FAILABLE();

            XFlush(dpy);
            
            return 0;
        }
        else
        {
            if (sr->cfg.stop_cmd != NULL)
            {
                RunCommand(sr->cfg.stop_cmd);
            }
            else
            {
                XhENTER_FAILABLE();
                if (!option_passive)
                {
                    XKillClient(dpy, win);
                    reportinfo("XKillClient(\"%s\":0x%08lx)",
                               (
                                sr->cfg.app_name != NULL  &&
                                sr->cfg.app_name[0] != '\0'
                               ) ? sr->cfg.app_name
                                 : sr->cfg.title,
                               (unsigned long)win /*!!! from Xdefs.h & X.h; but gcc barks anyway... */);
                }
                XhLEAVE_FAILABLE();
                XFlush(dpy);
            }
            
            return 1;
        }
    }
    else
    {
        if (!on) return 0;
        
        cmd = sr->cfg.start_cmd;
        
        if (cmd == NULL)
        {
            if (sr->cfg.kind == SUBSYS_CX)
            {
                home    = getenv("HOME");
                cx_root = getenv("CX_ROOT");
                if (home == NULL) home = "/-NON_EXISTENT-";

                /*!!!DIRSTRUCT*/
                pfx1 = home;
                pfx2 = "/4pult";

                if (cx_root != NULL)
                {
                    pfx1 = cx_root;
                    pfx2 = "";
                }

                snprintf(cmd_buf, sizeof(cmd_buf),
                         "cd %s%s/settings/%s; exec %s%s/bin/%s%s%s",
                         pfx1, pfx2, sr->cfg.name,
                         pfx1, pfx2, sr->cfg.name,
                         sr->cfg.params==NULL? "": " ",
                         sr->cfg.params==NULL? "": sr->cfg.params);

                cmd = cmd_buf;
            }
        }
        
        if (cmd != NULL) RunCommand(cmd);

        gettimeofday(&(sr->last_start_time), NULL);
        
        return 1;
    }
}

static int SplitSrvspec(const char *spec,
                        char *hostbuf, size_t hostbufsize,
                        int  *number)
{
  const char *colonp;
  char       *endptr;
  size_t      len;

    if (spec == NULL  ||
        (*spec != ':'  &&  !isalnum(*spec))) return -1;
  
    colonp = strchr(spec, ':');
    if (colonp != NULL)
    {
        *number = (int)(strtol(colonp + 1, &endptr, 10));
        if (endptr == colonp + 1  ||  *endptr != '\0')
            return -1;
        len = colonp - spec;
    }
    else
    {
        *number = 0;
        len = strlen(spec);
    }

    if (len > hostbufsize - 1) len = hostbufsize - 1;
    if (len > 0) memcpy(hostbuf, spec, len);
    hostbuf[len] = '\0';

    return 0;
}

typedef struct
{
    const char  *srvname;
    srvparams_t  sprms;
} srvpsrchinfo_t;
static int SrvParamsGatherer(srvparams_t *p, void *privptr)
{
  srvparams_t *sprms_p = privptr;
  const char  *ast_p;

    if (strcasecmp(sprms_p->name, p->name) == 0
        ||
        ((ast_p = strchr(p->name, '*')) != NULL  &&
         cx_memcasecmp(sprms_p->name, p->name, ast_p - p->name) == 0
        )
       )
    {
        if (p->start_cmd != NULL) sprms_p->start_cmd = p->start_cmd;
        if (p->stop_cmd  != NULL) sprms_p->stop_cmd  = p->stop_cmd;
        if (p->user      != NULL) sprms_p->user      = p->user;
        if (p->params    != NULL) sprms_p->params    = p->params;
    }

    return 0;
}
static int ChangeServerStatus(int sys, int ns, int on)
{
  subsys_t     *sr  = AccessSubsysSlot(sys, cur_syslist);
  cda_serverstatus_t  curstatus;

  const char   *srvname;
  char          host[256];
  int           instance;
  char         *p;

  const char   *at_host;
  const char   *cf_host;

  const char   *cmd;
  char          cmd_buf[1000];
  char         *c_at;
  char         *c_colon;
  char         *c_user;
  char         *c_usr_at;
  const char   *cx_root;
  const char   *workdir;
  const char   *cfgfile;
  const char   *devlist;
  const char   *pidbase;

  int           srv;
  srvparams_t   sprms;
  const char   *params;
  int           is_v2cx;
    
    /* Should we do anything at all? */
    curstatus = cda_status_of_srv(sr->ds->cid, ns) != CDA_SERVERSTATUS_DISCONNECTED;
    if (curstatus == on) return 0;

    is_v2cx = 0;
    p = cda_status_srv_scheme(sr->ds->cid, ns);
    if      (p == NULL) /*!!!???*/;
    else if (strcasecmp(p, "cx")   == 0) ;
    else if (strcasecmp(p, "v2cx") == 0) is_v2cx = 1;
    else return 0;
    /* Obtain server's host and instance... */
    srvname = cda_status_srv_name(sr->ds->cid, ns);
    if (SplitSrvspec(srvname, host, sizeof(host), &instance) != 0) return 0;
    for (p = host;  *p != '\0';  p++) *p = tolower(*p);
    at_host = cf_host = host;
    
    /* Dig out all parameters concerning this server */
    bzero(&sprms, sizeof(sprms));
    sprms.name = srvname;
    ForeachSrvpSlot(SrvParamsGatherer, &sprms, cur_cfgbase);
    
    /* Commands on this host are run as-is, remote ones via ssh */
    if (IsThisHost(at_host)) at_host = "";
    c_at = c_colon = c_user = c_usr_at = "";
    if (at_host[0] != '\0')
    {
        c_at    = "@";
        c_colon = ":";
        
        if (sprms.user != NULL)
        {
            c_user   = sprms.user;
            c_usr_at = "@";
        }
    }

    if (is_v2cx)
    {
        /*!!!DIRSTRUCT*/
        workdir = "~/pult";
        cfgfile = "cxd.conf";
        devlist = "blklist";
        pidbase = "cxd";
    }
    else
    {
        /*!!!DIRSTRUCT*/
        workdir = "~/4pult";
        cfgfile = "cxsd.conf";
        devlist = "devlist";
        pidbase = "cxsd";
    }

    /* Okay, let's change the status */    
    if (on)
    {
        if (sprms.start_cmd != NULL)
            cmd = sprms.start_cmd;
        else
        {
            if (at_host[0] == '\0')
            {
                cx_root = getenv("CX_ROOT");
                if (cx_root != NULL) workdir = cx_root;
            }

            params = sprms.params;
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "%s%s%s%s%scd %s && ./sbin/cxsd -c configs/%s -f configs/%s%s%s-%d.lst%s%s :%d",
                     c_at, c_user, c_usr_at, at_host, c_colon, workdir,
                     cfgfile, devlist,
                     cf_host[0]!= '\0' ? "-"    : "", cf_host, instance,
                     params    != NULL ? " "    : "", 
                     params    != NULL ? params : "",
                     instance);

            cmd = cmd_buf;
        }
    }
    else
    {
        if (sprms.stop_cmd != NULL)
            cmd = sprms.stop_cmd;
        else
        {
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "%s%s%s%s%skill `cat /var/tmp/%s-%d.pid`",
                     c_at, c_user, c_usr_at, at_host, c_colon, pidbase, instance);

            cmd = cmd_buf;
        }
    }

#if 0
    fprintf(stderr, "RUN %s\n", cmd);
#else
    RunCommand(cmd);
#endif

    return 1;
}

static void RunClientCB(Widget     w          __attribute__((unused)),
                        XtPointer  closure,
                        XtPointer  call_data  __attribute__((unused)))
{
  int         sys = ptr2lint(closure);
  subsys_t   *sr  = AccessSubsysSlot(sys, cur_syslist);
  int         n;

    if (sr->cfg.kind == SUBSYS_CX)
        for (n = 0;  n < sr->leds.count;  n++)
            if (ChangeServerStatus(sys, n, 1)) SleepBySelect(1000000);
  
    ChangeClientStatus(sys, 1);
}

static void ProgMenuCB(Widget     w          __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data  __attribute__((unused)))
{
  int         cmd = ptr2lint(closure);
  int         sys = prog_menu_client & 0xFFFF;

    switch (cmd)
    {
        case CMD_START:
            ChangeClientStatus(sys, 1);
            break;

        case CMD_STOP:
            ChangeClientStatus(sys, 0);
            break;

        case CMD_RESTART:
            if (ChangeClientStatus(sys, 0)) SleepBySelect(500000);
            ChangeClientStatus    (sys, 1);
            break;
    }
}

static void ProgMenuHandler(Widget     w  __attribute__((unused)),
                            XtPointer  closure,
                            XEvent    *event,
                            Boolean   *continue_to_dispatch)
{
  int           sys  = ptr2lint(closure);
  XButtonEvent *bev  = (XButtonEvent *) event;

    if (bev->button != Button3) return;
    *continue_to_dispatch = False;

    if (XtIsManaged(prog_menu)) XtUnmanageChild(prog_menu);

    prog_menu_client = sys;
    
    XmMenuPosition(prog_menu, bev);
    XtManageChild(prog_menu);
}


static void ServMenuCB(Widget     w          __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data  __attribute__((unused)))
{
  int         cmd = ptr2lint(closure);
  int         sys = serv_menu_client & 0xFFFF;
  int         ns  = (serv_menu_client >> 16) & 0xFFFF;

    switch (cmd)
    {
        case CMD_START:
            ChangeServerStatus    (sys, ns, 1);
            break;

        case CMD_STOP:
            ChangeServerStatus    (sys, ns, 0);
            break;

        case CMD_RESTART:
            if (ChangeServerStatus(sys, ns, 0)) SleepBySelect(500000);
            ChangeServerStatus    (sys, ns, 1);
            break;
    }
}

static void ServMenuHandler(Widget     w  __attribute__((unused)),
                            XtPointer  closure,
                            XEvent    *event,
                            Boolean   *continue_to_dispatch)
{
  int           srv  = ptr2lint(closure);
  XButtonEvent *bev  = (XButtonEvent *) event;

    if (bev->button != Button3) return;
    *continue_to_dispatch = False;

    if (XtIsManaged(serv_menu)) XtUnmanageChild(serv_menu);

    serv_menu_client = srv;
    
    XmMenuPosition(serv_menu, bev);
    XtManageChild(serv_menu);
}

static void CatchRightClick(Widget w, XtEventHandler proc, XtPointer client_data)
{
//    XtInsertEventHandler(w, ButtonPressMask, False, proc, client_data, XtListHead);
    XtAddEventHandler   (w, ButtonPressMask, False, proc, client_data);
}

static void AddProgMenu(Widget w, int sys)
{
    CatchRightClick(w, ProgMenuHandler, lint2ptr(sys));
    XmAddToPostFromList(prog_menu, w);
}

static void AddServMenu(Widget w, int sys, int ns)
{
    CatchRightClick(w, ServMenuHandler, lint2ptr((ns << 16) + sys));
    XmAddToPostFromList(serv_menu, w);
}

///////////////////////////////////////////////////////////////////////

static void CreateWindow(void)
{
  CxWidget  wksp;
  Widget    w;
  XmString  s;
  Arg       al[20];
  int       ac;

    wksp = XhGetWindowWorkspace(onewin);
    dash = XtVaCreateManagedWidget("container",
                                   xmFormWidgetClass,
                                   wksp,
                                   XmNshadowThickness, 0,
                                   NULL);
    /* I. Create menus */

    /* 1. Program (application) menu */
    ac = 0;
    XtSetArg(al[ac], XmNpopupEnabled, XmPOPUP_DISABLED);  ac++;
    prog_menu = XmCreatePopupMenu(wksp, "prog_menu", al, ac);
    XtVaCreateManagedWidget("", xmLabelWidgetClass, prog_menu,
                            XmNlabelString, s = XmStringCreateLtoR("Application", xh_charset),
                            NULL);
    XmStringFree(s);

    XtVaCreateManagedWidget("", xmSeparatorWidgetClass, prog_menu,
                            XmNorientation, XmHORIZONTAL,
                            NULL);

    prog_menu_start   = XtVaCreateManagedWidget("", xmPushButtonWidgetClass, prog_menu,
                                                XmNlabelString, s = XmStringCreateLtoR("Start", xh_charset),
                                                NULL);
    XmStringFree(s);
    XtAddCallback(prog_menu_start, XmNactivateCallback,
                  ProgMenuCB, (XtPointer)CMD_START);

    prog_menu_stop    = XtVaCreateManagedWidget("", xmPushButtonWidgetClass, prog_menu,
                                                XmNlabelString, s = XmStringCreateLtoR("Stop", xh_charset),
                                                NULL);
    XmStringFree(s);
    XtAddCallback(prog_menu_stop, XmNactivateCallback,
                  ProgMenuCB, (XtPointer)CMD_STOP);

    prog_menu_restart = XtVaCreateManagedWidget("", xmPushButtonWidgetClass, prog_menu,
                                                XmNlabelString, s = XmStringCreateLtoR("Restart", xh_charset),
                                                NULL);
    XmStringFree(s);
    XtAddCallback(prog_menu_restart, XmNactivateCallback,
                  ProgMenuCB, (XtPointer)CMD_RESTART);

    /* 2. Server menu */
    ac = 0;
    XtSetArg(al[ac], XmNpopupEnabled, XmPOPUP_DISABLED);  ac++;
    serv_menu = XmCreatePopupMenu(wksp, "serv_menu", al, ac);
    XtVaCreateManagedWidget("", xmLabelWidgetClass, serv_menu,
                            XmNlabelString, s = XmStringCreateLtoR("Server", xh_charset),
                            NULL);
    XmStringFree(s);

    XtVaCreateManagedWidget("", xmSeparatorWidgetClass, serv_menu,
                            XmNorientation, XmHORIZONTAL,
                            NULL);

    serv_menu_start   = XtVaCreateManagedWidget("", xmPushButtonWidgetClass, serv_menu,
                                                XmNlabelString, s = XmStringCreateLtoR("Start", xh_charset),
                                                NULL);
    XmStringFree(s);
    XtAddCallback(serv_menu_start, XmNactivateCallback,
                  ServMenuCB, (XtPointer)CMD_START);

    serv_menu_stop    = XtVaCreateManagedWidget("", xmPushButtonWidgetClass, serv_menu,
                                                XmNlabelString, s = XmStringCreateLtoR("Stop", xh_charset),
                                                NULL);
    XmStringFree(s);
    XtAddCallback(serv_menu_stop, XmNactivateCallback,
                  ServMenuCB, (XtPointer)CMD_STOP);

#if 0
    serv_menu_restart = XtVaCreateManagedWidget("", xmPushButtonWidgetClass, serv_menu,
                                                XmNlabelString, s = XmStringCreateLtoR("Restart", xh_charset),
                                                NULL);
    XmStringFree(s);
    XtAddCallback(serv_menu_restart, XmNactivateCallback,
                  ServMenuCB, (XtPointer)CMD_RESTART);
#endif

    /* V. Help button */
    w = XtVaCreateManagedWidget("helpButton",
                                xmPushButtonWidgetClass,
                                wksp,
                                XmNlabelString,     s = XmStringCreateLtoR("?", xh_charset),
                                XmNtraversalOn,     False,
                                XmNtopAttachment,   XmATTACH_WIDGET,
                                XmNtopWidget,       dash,
                                XmNleftAttachment,  XmATTACH_FORM,
                                XmNrightAttachment, XmATTACH_FORM,
                                XmNleftOffset,      10,
                                XmNrightOffset,     10,
                                NULL);
    XmStringFree(s);
    XtAddCallback(w, XmNactivateCallback, HelpCB, NULL);
}

///////////////////////////////////////////////////////////////////////

typedef struct
{
  CxWidget  grid;
  int       nlines;
  Widget    c1;
  Widget    c2;
  Widget    c3;
  int       e2;
  int       e3;
} scol_t;

static void InitCol(scol_t *cp, CxWidget parent)
{
  XmString  s;

    bzero(cp, sizeof(*cp));

    /* Create the container grid */
    cp->grid = XhCreateGrid(parent, "grid");
    XhGridSetPadding(cp->grid, 1, 1);
//    XhGridSetSize   (cp->grid, 3, MAXSYSTEMS);
    XhGridSetGrid   (cp->grid, 1*0, 0);
    
    /* Captions */
    
    cp->c1 = XtVaCreateManagedWidget("collabel",
                                     xmLabelWidgetClass,
                                     cp->grid,
                                     XmNlabelString, s = XmStringCreateLtoR("Name", xh_charset),
                                     NULL);
    XmStringFree(s);
    XhGridSetChildPosition (cp->c1,  0, 0);
    XhGridSetChildFilling  (cp->c1,  0, 0);
    XhGridSetChildAlignment(cp->c1, -1, +1);
  
    cp->c2 = XtVaCreateManagedWidget("collabel",
                                     xmLabelWidgetClass,
                                     cp->grid,
                                     XmNlabelString, s = XmStringCreateLtoR("?", xh_charset),
                                     NULL);
    XmStringFree(s);
    XhGridSetChildPosition (cp->c2,  1, 0);
    XhGridSetChildFilling  (cp->c2,  0, 0);
    XhGridSetChildAlignment(cp->c2, -1, +1);
  
    cp->c3 = XtVaCreateManagedWidget("collabel",
                                     xmLabelWidgetClass,
                                     cp->grid,
                                     XmNlabelString, s = XmStringCreateLtoR("Srv", xh_charset),
                                     NULL);
    XmStringFree(s);
    XhGridSetChildPosition (cp->c3,  2, 0);
    XhGridSetChildFilling  (cp->c3,  0, 0);
    XhGridSetChildAlignment(cp->c3, -1, +1);
}

static void FiniCol(scol_t *cp)
{
  int  ncols;

    ncols = 3;
    if (cp->e3 == 0  &&  0)
    {
        XtUnmanageChild(cp->c3);
        ncols = 2;
        if (cp->e2 == 0)
        {
            XtUnmanageChild(cp->c2);
            ncols = 1;
        }
    }

    XhGridSetSize(cp->grid, ncols, cp->nlines + 1);
}

static void BindServMenus(int sys, int from, int to)
{
  subsys_t *sr = AccessSubsysSlot(sys, cur_syslist);
  int       ns;

    for (ns = from;  ns <= to;  ns++)
        AddServMenu(sr->leds.leds[ns].w, sys, ns);
}

static CxWidget CreateMyLed(CxWidget parent_grid, int x)
{
  CxWidget  w;
  XmString  s;
    
    w = XtVaCreateManagedWidget("LED",
                                xmToggleButtonWidgetClass,
                                parent_grid,
                                XmNtraversalOn,    False,
                                XmNsensitive,      False,
                                XmNlabelString,    s = XmStringCreateSimple(""),
                                XmNindicatorSize,  15,
                                XmNspacing,        0,
                                XmNmarginWidth,    0,
                                XmNmarginHeight,   0,
                                XmNindicatorOn,    XmINDICATOR_BOX,
                                XmNindicatorType,  XmN_OF_MANY,
                                XmNset,            False,
                                XmNvisibleWhenOff, False,
                                NULL);
    XmStringFree(s);

    if (x >= 0) XhGridSetChildPosition(w, x, 0);
    
    return w;
}

static inline int is_empty(const char *p)
{
    return p == NULL  ||  *p == '\0';
}
static int BtnCreator(subsys_t *sr, void *privptr)
{
  scol_t        *onecol_p = privptr;
  int            sys = sr - cur_syslist->sys_list; /*!!! HACK!!! */
  Widget         leftw;
  unsigned char  sep_type;
  const char    *btn_label;
  XmString       s;
  int            y = onecol_p->nlines + 1;
  CxWidget       cw;
  CxWidget       l2;
  
    if      (sr->cfg.kind == SUBSYS_NEWCOL)
    {
        FiniCol(onecol_p);
        leftw = onecol_p->grid;
        InitCol(onecol_p, dash);
        attachleft(onecol_p->grid, leftw, 10);
	return 0; // To escape "onecol_p->nlines++"
    }
    else if (sr->cfg.kind == SUBSYS_SEPR  ||
             sr->cfg.kind == SUBSYS_SEPR2)
    {
        sep_type                                   = XmSINGLE_LINE;
        if (sr->cfg.kind == SUBSYS_SEPR2) sep_type = XmDOUBLE_LINE;
        sr->btn = XtVaCreateManagedWidget("-", xmSeparatorWidgetClass, 
	                                  onecol_p->grid,
                                          XmNorientation,        XmHORIZONTAL,
                                          XmNseparatorType,      sep_type,
                                          XmNhighlightThickness, 0,
                                          NULL);
        XhGridSetChildPosition (sr->btn,  0, y);
        XhGridSetChildFilling  (sr->btn,  1, 0);
        XhGridSetChildAlignment(sr->btn, +1, 0);
    }
    else
    {
        btn_label = sr->cfg.label;
	if (is_empty(btn_label)) btn_label = sr->cfg.name;
	sr->btn = XtVaCreateManagedWidget("push_i",
                                          xmPushButtonWidgetClass,
                                          onecol_p->grid,
                                          XmNlabelString, s = XmStringCreateSimple(btn_label),
                                          XmNtraversalOn, False,
                                          NULL);
        XmStringFree(s);
        XtVaGetValues(sr->btn, XmNbackground, &(sr->btn_defbg), NULL);
        XhGridSetChildPosition (sr->btn,  0, y);
        XhGridSetChildFilling  (sr->btn,  1, 0);
        XhGridSetChildAlignment(sr->btn, +1, 0);

        if (sr->cfg.comment != NULL) XhSetBalloon(sr->btn, sr->cfg.comment);
        if (sr->cfg.kind == SUBSYS_CX  &&  sr->ds == NULL)
	    XtVaSetValues(sr->btn, XmNsensitive, False, NULL);
        else
	{
            XtAddCallback(sr->btn, XmNactivateCallback,
                          RunClientCB, lint2ptr(sys));
	    AddProgMenu(sr->btn, sys);

            if (sr->cfg.kind == SUBSYS_CX)
            {
////fprintf(stderr, ":%s %d\n", btn_label, sr->ds->cid);
                cw = XhCreateGrid(onecol_p->grid, "alarmLeds1");
                XhGridSetSize   (cw, 2, 1);
                XhGridSetGrid   (cw, 0, 0);
                XhGridSetPadding(cw, 0, 0);
                XhGridSetChildPosition (cw,  1, y);
                XhGridSetChildFilling  (cw,  0, 0);
                XhGridSetChildAlignment(cw, -1, 0);
                
                sr->a_led = CreateMyLed(cw, 0);
                sr->d_led = CreateMyLed(cw, 1);

                l2 = XhCreateGrid(onecol_p->grid, "alarmLeds2");
                XhGridSetGrid   (l2,  0, 0);
                XhGridSetPadding(l2,  0, 0);
                XhGridSetChildPosition (l2,  2, y);
                XhGridSetChildFilling  (l2,  0, 0);
                XhGridSetChildAlignment(l2, -1, 0);
                MotifKnobs_leds_create(&(sr->leds),
                                       l2, 0-15,
                                       sr->ds->cid, MOTIFKNOBS_LEDS_PARENT_GRID);
                BindServMenus(sys, 0, sr->leds.count-1);
                sr->oldleds_count = sr->leds.count;
            }
	}
    }
    onecol_p->nlines++;

    return 0;
}
static void PopulateWindow(void)
{
  scol_t  onecol;

    InitCol(&onecol, dash);
    ForeachSubsysSlot(BtnCreator, &onecol, cur_syslist);
    FiniCol(&onecol);
}

static void subsys_evproc(int            uniq, 
                          void          *privptr,
                          cda_context_t  cid,
                          int            reason,
                          int            info_int,
                          void          *privptr2)
{
  int            sys = ptr2lint(privptr2);
  subsys_t      *sr  = AccessSubsysSlot(sys, cur_syslist);
  rflags_t       rflags;
  int            idx;

//fprintf(stderr, "sys=%d info_int=%d\n", sys, info_int);
    if      (reason == CDA_CTX_R_CYCLE)
    {
        /* Note: CdrProcessSubsystem() was already called by Cdr's ProcessContextEvent() */
        rflags = sr->ds->currflags &~ CXCF_FLAG_4WRONLY_MASK;
        if (sr->cfg.ignorevals) 
            rflags &= (CXRF_SERVER_MASK | CXCF_FLAG_DEFUNCT | CXCF_FLAG_NOTFOUND);

        if ((rflags ^ sr->oldrflags) & CXCF_FLAG_ALARM_MASK)
        {
            idx                                      = -1;
            if (rflags & CXCF_FLAG_ALARM_RELAX)  idx = XH_COLOR_JUST_GREEN;
            if (rflags & CXCF_FLAG_ALARM_ALARM)  idx = XH_COLOR_JUST_RED;
            XtVaSetValues(sr->btn,
                          XmNbackground, idx < 0? sr->btn_defbg : XhGetColor(idx),
                          NULL);
        }

        if ((rflags ^ sr->oldrflags) & CXCF_FLAG_COLOR_MASK)
        {
            idx                                      = XH_COLOR_JUST_GREEN;
            if (rflags & CXCF_FLAG_COLOR_YELLOW) idx = XH_COLOR_BG_IMP_YELLOW;
            if (rflags & CXCF_FLAG_COLOR_RED)    idx = XH_COLOR_BG_IMP_RED;
            if (rflags & CXCF_FLAG_COLOR_WEIRD)  idx = XH_COLOR_BG_WEIRD;
            XtVaSetValues(sr->a_led,
                          XmNset,         (rflags & CXCF_FLAG_COLOR_MASK) != 0,
                          XmNselectColor, XhGetColor(idx),
                          NULL);
        }

        if ((rflags ^ sr->oldrflags) & CXCF_FLAG_SYSERR_MASK)
        {
            idx                                      = XH_COLOR_JUST_GREEN;
            if (rflags & CXCF_FLAG_DEFUNCT)      idx = XH_COLOR_BG_DEFUNCT;
            if (rflags & CXCF_FLAG_SFERR_MASK)   idx = XH_COLOR_BG_SFERR;
            if (rflags & CXCF_FLAG_HWERR_MASK)   idx = XH_COLOR_BG_HWERR;
            if (rflags & CXCF_FLAG_NOTFOUND)     idx = XH_COLOR_BG_NOTFOUND;
            XtVaSetValues(sr->d_led,
                          XmNset,         (rflags & CXCF_FLAG_SYSERR_MASK) != 0,
                          XmNselectColor, XhGetColor(idx),
                          NULL);
        }

        sr->oldrflags = rflags;
    }
    else if (reason == CDA_CTX_R_NEWSRV)
    {
        /* Force leds to grow NOW */
        MotifKnobs_leds_grow(&(sr->leds));
        /* Any new-born leds, requiring a meny binding? */
        if (sr->oldleds_count < sr->leds.count)
        {
            BindServMenus(sys, sr->oldleds_count, sr->leds.count-1);
            sr->oldleds_count = sr->leds.count;
        }
    }
}

///////////////////////////////////////////////////////////////////////

static int AddOneConfig(cfgfile_t *p, void *privptr)
{
  cfgbase_t *cfg = privptr;

//fprintf(stderr, "%s: %p+=%s\n", __FILE__, cfg, p->filespec);
    return CfgBaseMerge(cfg, NULL, p->filespec);
}
static cfgbase_t *ReadConfigs(void)
{
  cfgbase_t *cfg;
  int        r;

//exit(9);
    cfg = CfgBaseCreate(option_noprocess);
    if (cfg == NULL) return NULL;
//fprintf(stderr, "cfg=%p\n", cfg);

    r = ForeachCfgFileSlot(AddOneConfig, cfg);
    if (r >= 0)
    {
        CfgBaseDestroy(cfg);
	return NULL;
    }

    return cfg;
}

///////////////////////////////////////////////////////////////////////

static void ChildrenCatcher(int sig)
{
  pid_t  pid;
  int    status;
  int    saved_errno;

    saved_errno = errno;
  
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFSIGNALED(status))
            reportinfo("PID=%ld terminated by signal %d",
                       (long)pid, WTERMSIG(status));
    }
    
    set_signal(sig, ChildrenCatcher);

    errno = saved_errno;
}

/*********************************************************************
*  Main() section                                                    *
*********************************************************************/

static char *fallbacks[] =
{
    "*.push_i.shadowThickness:10", /*!!! <- for some reason, doesn't work...*/
    "*helpButton.shadowThickness:1",
    XH_STANDARD_FALLBACKS
};

int main(int argc, char *argv[])
{
  char        app_name [100];
  char        app_class[100];
  const char *bnp;

    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);
    
    printf("%s CX-starter, CX version %d.%d\n",
           strcurtime(), CX_VERSION_MAJOR, CX_VERSION_MINOR);
        
    ChildrenCatcher(SIGCHLD);

#if 1
    XhSetColorBinding("CTL3D_BG",       "#d0dbff");
    XhSetColorBinding("CTL3D_TS",       "#f1f4ff");
    XhSetColorBinding("CTL3D_BS",       "#9ca4c0");
    XhSetColorBinding("CTL3D_BG_ARMED", "#e7edff");
#else
    XhSetColorBinding("CTL3D_BG",       "#dbdbdb");
    XhSetColorBinding("CTL3D_TS",       "#f7f7f7");
    XhSetColorBinding("CTL3D_BS",       "#a7a7a7");
#endif
    
#if   0
    //
    XhSetColorBinding("BG_DEFAULT", "#dbe6db");
    XhSetColorBinding("TS_DEFAULT", "#f4f7f4");
    XhSetColorBinding("BS_DEFAULT", "#a4ada4");
    XhSetColorBinding("BG_ARMED",   "#edf2ed");
#elif 1
    // Bolotno-zheltyj
    XhSetColorBinding("BG_DEFAULT", "#dbdb90");
    XhSetColorBinding("TS_DEFAULT", "#f4f4de");
    XhSetColorBinding("BS_DEFAULT", "#a4a46c");
#elif 0
    // VGA color 1 (dark blue)
    XhSetColorBinding("FG_DEFAULT", "#FFFFFF");
    XhSetColorBinding("BG_DEFAULT", "#1818b2");
    XhSetColorBinding("TS_DEFAULT", "#9090da");
    XhSetColorBinding("BS_DEFAULT", "#121286");
#else
    // Cold bluish-gray
    XhSetColorBinding("BG_DEFAULT", "#d0dbff");
    XhSetColorBinding("TS_DEFAULT", "#f1f4ff");
    XhSetColorBinding("BS_DEFAULT", "#9ca4c0");
#endif

    bnp = strrchr(argv[0], '/');
    if (bnp != NULL) bnp++; else bnp = argv[0];
    strzcpy(app_name,  bnp, sizeof(app_name));
    strzcpy(app_class, bnp, sizeof(app_class)); app_class[0] = toupper(app_class[0]);

    XhInitApplication(&argc, argv, app_name, app_class, fallbacks);

    onewin = XhCreateWindow("CX-starter", app_name, app_class,
                            XhWindowUnresizableMask | XhWindowCompactMask,
                            NULL,
                            NULL);
    XhSetWindowIcon(onewin, cx_starter_xpm);

    ParseCommandLine(argc, argv);
    ObtainHostIdentity();
    SetPassiveness  (option_passive);

    cur_cfgbase = ReadConfigs();
    if (cur_cfgbase == NULL)
    {
        reportinfo("ReadConfigs() error");
        exit(1);
    }
    cur_syslist = CreateSubsysListFromCfgBase(cur_cfgbase);
    if (cur_syslist == NULL)
    {
        reportinfo("CreateSubsysListFromCfgBase() error");
        exit(1);
    }
    RealizeSubsysList(cur_syslist, subsys_evproc);

    CreateWindow  ();
    PopulateWindow();

    XhShowWindow(onewin);
    XhRunApplication();
    
    return 0;
}
