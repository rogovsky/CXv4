#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "misclib.h"

#include "Xh.h"
#include "Xh_fallbacks.h"
#include "Knobs.h"
#include "Cdr.h"

#include "datatreeP.h"


static DataSubsys  ds;
static DataKnob    k1;

static void TstKCB(DataKnob k, double v, void *privptr)
{
    printf("k=%p :=%f privptr=%p\n", k, v, privptr);
}

static void PopulateWindow(XhWindow  win)
{
  int  r;
    
    if (ds != NULL)
    {
        r = CreateKnob(ds->main_grouping, XhGetWindowWorkspace(win));
        fprintf(stderr, "CreateKnob(): r=%d, KnobsLastErr=\"%s\"\n", r, KnobsLastErr());
    }
    else
        k1 = CreateSimpleKnob(" look=selector rw label='Хрю!!!'"
                              " options=''"
                              //" items='123\b<1>\t456\b<2>\t789\b<3>'",
                              " items='#f10f,0.3,0.2,Кукареку - %5.3f'",
                              0, XhGetWindowWorkspace(win),
                              TstKCB, NULL);
}

static int CmdProc(XhWindow window, const char *cmd, int info_int)
{
    printf("%s(0x%p, \"%s\":%d)\n", __FUNCTION__, window, cmd, info_int);
    return 0;
}

static char *fallbacks[] = {XH_STANDARD_FALLBACKS};

int main(int argc, char *argv[])
{
  XhWindow      win;
  const char   *app_name  = "tstknobs";
  const char   *app_class = "TstKnobs";

    set_signal(SIGPIPE, SIG_IGN);

    XhInitApplication(&argc, argv, app_name, app_class, fallbacks);

    errno = 0;
    ds = CdrLoadSubsystem("file", argc > 1? argv[1] : "/etc/zprofile", argv[0]);
    printf("ds=%p, CdrLastErr=\"%s\"\n", ds, CdrLastErr());
  
    win = XhCreateWindow("Test window!!!", app_name, app_class,
                         XhWindowToolbarMask | XhWindowStatuslineMask |
                         XhWindowFormMask,
                         NULL, NULL);
    XhSetWindowCmdProc(win, CmdProc);
    PopulateWindow    (win);
    XhShowWindow      (win);
    XhRunApplication();
  
    return 0;
}
