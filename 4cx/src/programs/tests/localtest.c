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
#include "Chl.h"

#include "Xm/Xm.h"
#include "Xm/Label.h"
#include "Xh_globals.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"
#include "cda_d_local_api.h"


static int  DoWrite(cda_d_local_t  var,
                    void          *privptr,
                    void          *data,
                    cxdtype_t      dtype,
                    int            n_items)
{
  void     *addr;
  double    v;
  DataKnob  k = privptr;

fprintf(stderr, "%s\n", __FUNCTION__);
    if (dtype != CXDTYPE_DOUBLE  ||  n_items != 1) return -1;

    v = *((double*)data);

    cda_d_local_chan_n_props(var, NULL, NULL, NULL, &addr);
    *((double*)addr) = v;
    SetSimpleKnobValue(k, v);

    cda_d_local_update_chan(var, n_items, 0);

    return 0;
}
static void LocalKCB(DataKnob k, double v, void *privptr)
{
  int   var = ptr2lint(privptr);
  void *addr;

    cda_d_local_chan_n_props(var, NULL, NULL, NULL, &addr);
    *((double*)addr) = v;
    cda_d_local_update_chan(var, 1, 0);
    cda_d_local_update_cycle();
}
static void CreateLocalWindow(void)
{
  XhWindow      lwin;
  CxWidget      lgrid;
  int           n;
  int           count;
  const char   *name;
  cxdtype_t     dtype;
  void         *addr;
  Widget        l;
  XmString      s;
  DataKnob      k;

    lwin = XhCreateWindow("List of local vars", "zzz", "qqq",
                          XhWindowCompactMask | XhWindowGridMask,
                          NULL, NULL);
    lgrid = XhGetWindowWorkspace(lwin);
    XhGridSetGrid   (lgrid, -1*0,     -1*0);
    XhGridSetSpacing(lgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(lgrid,  1*0,      1*0);
    for (n = 0, count = 0;  n < cda_d_local_chans_count();  n++)
        if (cda_d_local_chan_n_props(n, &name, &dtype, NULL, &addr) >= 0  &&
            dtype == CXDTYPE_DOUBLE)
        {
            l = XtVaCreateManagedWidget("rowlabel",
                                        xmLabelWidgetClass,
                                        lgrid,
                                        XmNlabelString, s = XmStringCreateLtoR(name, xh_charset),
                                        NULL);
            XmStringFree(s);
            XhGridSetChildPosition(l, 0, count);
            XhGridSetChildFilling (l, 0, 0);

            k = CreateSimpleKnob(" look=text rw dpyfmt=%8.3f", 0, lgrid,
                                 LocalKCB, n);
            XhGridSetChildPosition(GetKnobWidget(k), 1, count);
            XhGridSetChildFilling (GetKnobWidget(k), 0, 0);
            SetSimpleKnobValue(k, *((double*)addr));
////fprintf(stderr, "%s: %p\n", name, addr);

            cda_d_local_override_wr(n, DoWrite, k);

            count++;
        }
    XhGridSetSize(lgrid, 2, count);
    XhShowWindow(lwin);
}


char myfilename[] = __FILE__;

static char *fallbacks[] = {XH_STANDARD_FALLBACKS};

int main(int argc, char *argv[])
{
  const char   *app_name  = "localtest";
  const char   *app_class = "LocalTest";
  char         *p;
  int           arg_n;
  char         *sysname;
  DataSubsys    ds;
  char         *defserver;

    set_signal(SIGPIPE, SIG_IGN);

    MotifKnobs_cdaRegisterKnobset();

    setenv("CDA_D_LOCAL_AUTO_CREATE", "1", 1);

    XhInitApplication(&argc, argv, app_name, app_class, fallbacks);

    arg_n = 1;
    
    /* Check if we are run "as is" instead of via symlink */
    p = strrchr(myfilename, '.');
    if (p != NULL) *p = '\0';
    p = strrchr(argv[0], '/');
    if (p != NULL) p++; else p = argv[0];
    if (strcmp(p, myfilename) == 0)
    {
        if (arg_n >= argc)
        {
            fprintf(stderr,
                    "%s: this program should be run either via symlink or as\n"
                    "%s <SYSNAME>\n",
                    argv[0], argv[0]);
            exit(1);
        }
        sysname = argv[arg_n++];
    }
    else
        sysname = p;

    /* Load the subsystem... */
    ds = CdrLoadSubsystem("file", sysname, argv[0]);
    if (ds == NULL)
    {
        fprintf(stderr, "%s(%s): %s\n",
                argv[0], sysname, CdrLastErr());
        exit(1);
    }

    /* ...and realize it */
    defserver = "local::";
    if (arg_n < argc)
        defserver = argv[arg_n++];
    
    if (CdrRealizeSubsystem(ds, 0, defserver, argv[0]) < 0)
    {
        fprintf(stderr, "%s: Realize(%s): %s\n",
                argv[0], sysname, CdrLastErr());
        exit(1);
    }

    /* Create a "list of local vars used" window */
    CreateLocalWindow();

    /* Finally, run the application */
    if (ChlRunSubsystem(ds, NULL, NULL) < 0)
    {
        fprintf(stderr, "%s: ChlRunSubsystem(%s): %s\n",
                argv[0], sysname, ChlLastErr());
        exit(1);
    }
    
    return 0;
}
