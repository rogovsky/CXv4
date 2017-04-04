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
#include "CdrP.h" // For CdrCreateSection() only
#include "Chl.h"
#include "MotifKnobs_cda.h"

#include "datatreeP.h"

#ifdef SPECIFIC_KNOBSET_H_FILE
  #include SPECIFIC_KNOBSET_H_FILE
#endif /* SPECIFIC_KNOBSET_H_FILE */


char myfilename[] = __FILE__;

static char *fallbacks[] = {XH_STANDARD_FALLBACKS};

int main(int argc, char *argv[])
{
  const char   *app_name  = "pult";
  const char   *app_class = "Pult";
  int           as_is;
  char         *p;
  char         *basename;
  int           arg_n;
  char         *sysname      = NULL;
  DataSubsys    ds;
  char         *defserver    = NULL;
  char         *file_to_load = NULL;
  int           option_readonly = 0;

  typedef enum {PK_SWITCH, PK_SUBSYS, PK_PARAM, PK_BASE, PK_FILE} pkind_t;
  pkind_t       pkind;
  const char   *p_eq; // '='
  const char   *p_sl; // '/'
  const char   *p_cl; // ':'

    set_signal(SIGPIPE, SIG_IGN);

    MotifKnobs_cdaRegisterKnobset();
#ifdef SPECIFIC_REGISTERKNOBSET_CODE
    SPECIFIC_REGISTERKNOBSET_CODE;
#endif /* SPECIFIC_REGISTERKNOBSET_CODE */

    /* Check if we are run "as is" instead of via symlink */
    p = strrchr(myfilename, '.');
    if (p != NULL) *p = '\0';
    basename = strrchr(argv[0], '/');
    if (basename != NULL) basename++; else basename = argv[0];
    as_is = (strcmp(basename, myfilename) == 0);

    arg_n = 1;
    if      (!as_is)                                  sysname = basename;
    else if (argc > arg_n  &&  argv[arg_n][0] != '-') sysname = argv[arg_n++];
    if (sysname != NULL)
    {
        ds = CdrLoadSubsystem("file", sysname, argv[0]);
        if (ds == NULL)
        {
            fprintf(stderr, "%s(%s): %s\n",
                    argv[0], sysname, CdrLastErr());
            exit(1);
        }
    }
    else
        ds = NULL;

////fprintf(stderr, "sysname=%s ds=%p\n", sysname, ds);
    if (ds != NULL)
    {
        if ((p = CdrFindSection(ds, DSTN_WINNAME,    "main")) != NULL)
	    app_name  = p;
	if ((p = CdrFindSection(ds, DSTN_WINCLASS,   "main")) != NULL)
	    app_class = p;
    }

    XhInitApplication(&argc, argv, app_name, app_class, fallbacks);

#if 1
    for (/* arg_n assigned earlier */;  arg_n < argc;  arg_n++)
    {
        /* Paramkind determination heuristics */
        pkind = PK_PARAM;
        p_eq = strchr(argv[arg_n], '=');
        p_sl = strchr(argv[arg_n], '/');
        p_cl = strchr(argv[arg_n], ':');

        if      (argv[arg_n][0] == '-')
            pkind = PK_SWITCH;
        else if (sysname == NULL)
            pkind = PK_SUBSYS;
        else
        {
            if (argv[arg_n][0] == '/'  ||  memcmp(argv[arg_n], "./", 2) == 0  ||
                (p_sl != NULL  &&  (p_eq == NULL  ||  p_sl < p_eq)))
                pkind = PK_FILE;

            if (p_eq == NULL  &&  p_sl == NULL  &&
                p_cl != NULL)
                pkind = PK_BASE;
        }

        if      (pkind == PK_SWITCH)
        {
            if      (strcmp(argv[arg_n], "-h")     == 0  ||
                     strcmp(argv[arg_n], "-help")  == 0  ||
                     strcmp(argv[arg_n], "--help") == 0)
                ;
            else if (strcmp(argv[arg_n], "-readonly") == 0)
                option_readonly = 1;
            else
            {
                fprintf(stderr, "%s: unknown switch \"%s\"\n",
                        argv[0], argv[arg_n]);
                exit(1);
            }
        }
        else if (pkind == PK_SUBSYS)
        {
            sysname      = argv[arg_n];
////fprintf(stderr, "sysname=<%s>\n", sysname);
            /* Load the subsystem... */
            if (ds == NULL) ds = CdrLoadSubsystem("file", sysname, argv[0]);
            if (ds == NULL)
            {
                fprintf(stderr, "%s(%s): %s\n",
                        argv[0], sysname, CdrLastErr());
                exit(1);
            }
        }
        else if (pkind == PK_PARAM)
        {
            if (CdrCreateSection(ds, DSTN_ARGVWINOPT,
                                 NULL, 0,
                                 argv[arg_n], strlen(argv[arg_n]) + 1) == NULL)
                fprintf(stderr, "%s: CdrCreateSection(, DSTN_ARGVWINOPT) error\n",
                        argv[0]);
        }
        else if (pkind == PK_BASE)
            defserver    = argv[arg_n];
        else if (pkind == PK_FILE)
            /* Is in fact unused, while cda is able to delay
               {R,D}-conversion till {R,D}s become known */
            file_to_load = argv[arg_n];
    }
    CdrSetSubsystemRO(ds, option_readonly);
#else
    arg_n = 1;
    
    /* Check if we are run "as is" instead of via symlink */
    if (as_is)
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
        sysname = basename;

    /* Load the subsystem... */
    if (ds == NULL) ds = CdrLoadSubsystem("file", sysname, argv[0]);
    if (ds == NULL)
    {
        fprintf(stderr, "%s(%s): %s\n",
                argv[0], sysname, CdrLastErr());
        exit(1);
    }

    /* ...and realize it */
    defserver = "";
    if (arg_n < argc)
        defserver = argv[arg_n++];
#endif
    
    if (CdrRealizeSubsystem(ds, 0, defserver, argv[0]) < 0)
    {
        fprintf(stderr, "%s: Realize(%s): %s\n",
                argv[0], sysname, CdrLastErr());
        exit(1);
    }

    /* Finally, run the application */
    if (ChlRunSubsystem(ds, NULL, NULL) < 0)
    {
        fprintf(stderr, "%s: ChlRunSubsystem(%s): %s\n",
                argv[0], sysname, ChlLastErr());
        exit(1);
    }
    
    return 0;
}
