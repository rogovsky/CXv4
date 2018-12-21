#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>

#include "misclib.h"

#include "Xh.h"
#include "Xh_fallbacks.h"
#include "Knobs.h"
#include "Cdr.h"
#include "Chl.h"
#include "MotifKnobs_cda.h"

#include "cxlib.h"
#include "cda.h"

#include "cdaP.h"

#include "cxsd_logger.h"
#include "cxsd_db.h"
#include "cxsd_dbP.h"
#include "cxsd_hw.h"
#include "cxsd_modmgr.h"
#include "cxsd_frontend.h"
#include "cxsd_extension.h"


//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
#include "misclib.h"
#include "cxsd_driver.h"
#include "sendqlib.h"


void *public_funcs_list [] =
{
    set_fd_flags,
    DoDriverLog,
    sq_init,
    n_write,
};
//--------------------------------------------------------------------

#define DEF_FILE_DEVLIST     "./cxsd-devlist.lst"

enum
{
    DEF_OPT_VERBOSITY     = LOGL_NOTICE,
    DEF_OPT_CACHERFW      = 1,
    DEF_OPT_BASECYCLESIZE = 1000000,
};

static int         option_simulate                 = 0;
static int         option_defdrvlog_mask           = 0;
static int         option_verbosity                = DEF_OPT_VERBOSITY;
static const char *option_dbref                    = DEF_FILE_DEVLIST;
static int         option_basecyclesize            = DEF_OPT_BASECYCLESIZE;
static int         option_cacherfw                 = DEF_OPT_CACHERFW;
static char        config_drivers_path  [PATH_MAX] = ".";
static char        config_frontends_path[PATH_MAX] = ".";
static char        config_exts_path     [PATH_MAX] = ".";
static char        config_libs_path     [PATH_MAX] = ".";

//--------------------------------------------------------------------
static void ParseCommandLine(int argc, char *argv[])
{
  __label__  ADVICE_HELP, PRINT_HELP;

  int            c;              /* Option character */
  int            err       = 0;  /* ++'ed on errors */
  char          *endptr;

  int            log_set_mask;
  int            log_clr_mask;
  char          *log_parse_r;

    while ((c = getopt(argc, argv, "b:E:f:F:hl:L:M:sv:w:")) != EOF)
    {
        switch (c)
        {
            case 'b':
                option_basecyclesize = strtoul(optarg, &endptr, 10);
                if (endptr == optarg  ||  *endptr != '\0')
                {
                    fprintf(stderr,
                            "%s: '-b' argument should be an integer (in us)\n",
                            argv[0]);
                    err++;
                }
                break;
                
            case 'f':
                option_dbref = optarg;
                break;
                
            case 'h':
                goto PRINT_HELP;
                
            case 'l':
                log_parse_r = ParseDrvlogCategories(optarg, NULL,
                                                    &log_set_mask, &log_clr_mask);
                if (log_parse_r != NULL)
                {
                    fprintf(stderr,
                            "%s: error parsing -l switch: %s\n",
                            argv[0], log_parse_r);
                    err++;
                }
                option_defdrvlog_mask =
                    (option_defdrvlog_mask &~ log_clr_mask) | log_set_mask;
                break;

            case 'M':
                strzcpy(config_drivers_path,   optarg, sizeof(config_drivers_path));
                break;
                
            case 'F':
                strzcpy(config_frontends_path, optarg, sizeof(config_frontends_path));
                break;
                
            case 'E':
                strzcpy(config_exts_path,      optarg, sizeof(config_exts_path));
                break;
                
            case 'L':
                strzcpy(config_libs_path,      optarg, sizeof(config_libs_path));
                break;
                
            case 's':
                option_simulate = 1;
                break;
                
            case 'v':
                if (*optarg >= '0'  &&  *optarg <= '9'  &&
                    optarg[1] == '\0')
                    option_verbosity = *optarg - '0';
                else
                {
                    fprintf(stderr,
                            "%s: argument of `-v' should be between 0 and 9\n",
                            argv[0]);
                    err++;
                }
                break;
                
            case 'w':
                option_cacherfw = 1;
                if (tolower(*optarg) == 'n'  ||  tolower(*optarg) == 'f'  ||
                    *optarg == '0')
                    option_cacherfw = 0;
                break;
                
            case '?':
            default:
                err++;
        }
    }
    
    if (err) goto ADVICE_HELP;

    return;

 ADVICE_HELP:
    fprintf(stderr, "Try `%s -h' for more information.\n", argv[0]);
    exit(1);

 PRINT_HELP:
    printf("Usage: %s [options]\n"
           "\n"
           "Options:\n"
           "  -b TIME   server base cycle size, in µs (def=%d)\n"
           "  -f DBREF  obtain HW config from DBREF instead of " DEF_FILE_DEVLIST "\n"
           "  -h        display this help and exit\n"
           "  -s        simulate hardware; for debugging only\n"
           "  -v N      verbosity level: 0 - lowest, 9 - highest (def=%d)\n"
           "  -w Y/N    cache reads from write channels (def=%s)\n"
           , argv[0],
           DEF_OPT_BASECYCLESIZE,
           DEF_OPT_VERBOSITY,
           DEF_OPT_CACHERFW? "yes" : "no");
    exit(0);
}

//--------------------------------------------------------------------

extern DECLARE_CXSD_DBLDR    (file);
extern CDA_DECLARE_DAT_PLUGIN(insrv);
extern DECLARE_CXSD_FRONTEND (cx);

cx_module_rec_t *builtins_list[] =
{
    &(CXSD_DBLDR_MODREC_NAME   (file) .mr),
    &(CDA_DAT_P_MODREC_NAME    (insrv).mr),
    &(CXSD_FRONTEND_MODREC_NAME(cx)   .mr),
};

static void InitBuiltins(void)
{
  int  n;

    for (n = 0;  n < countof(builtins_list);  n++)
    {
        if (builtins_list[n]->init_mod != NULL  &&
            builtins_list[n]->init_mod() < 0)
            logline(LOGF_MODULES, 0, "%s(): %08x.%s.init_mod() failed",
                    __FUNCTION__,
                    builtins_list[n]->magicnumber, builtins_list[n]->name);
    }
}

//--------------------------------------------------------------------

static  CxsdDb db;

static void ReadHWConfig(const char *argv0)
{
    CxsdHwSetSimulate(option_simulate);
    CxsdHwSetDefDrvLogMask(option_defdrvlog_mask);
    CxsdSetDrvLyrPath(config_drivers_path);
    db = CxsdDbLoadDb(argv0, "file", option_dbref);
    if (db == NULL)
    {
        fprintf(stderr, "%s %s: unable to read database \"%s\"\n",
                strcurtime(), argv0, option_dbref);
        exit(1);
    }
}

static void cleanuper(int uniq)
{
    cda_do_cleanup(uniq);
    cx_do_cleanup (uniq);
}

static void ActivateHW  (const char *argv0)
{
    /*!!! Should check return values! */
    CxsdHwSetCleanup(cleanuper);
    CxsdHwSetDb(db);
    CxsdHwActivate(argv0);
    CxsdHwSetCycleDur(option_basecyclesize);
}

//////////////////////////////////////////////////////////////////////

char myfilename[] = __FILE__;

static char *fallbacks[] = {XH_STANDARD_FALLBACKS};

int main(int argc, char *argv[])
{
  const char   *app_name  = "stand";
  const char   *app_class = "Stand";
  char         *p;
  int           arg_n;
  char         *sysname;
  DataSubsys    ds;
  char         *defserver;
  int           option_instance = -1;
  char         *endptr;

    set_signal(SIGPIPE, SIG_IGN);

    MotifKnobs_cdaRegisterKnobset();

    XhInitApplication(&argc, argv, app_name, app_class, fallbacks);
    ParseCommandLine(argc, argv);

    arg_n = optind;
    
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

    /* Parse other commandline data */
    defserver = "insrv::";
    for (;  arg_n < argc;  arg_n++)
    {
        if (argv[arg_n][0] == ':')
        {
            option_instance = strtoul(&(argv[arg_n][1]), &endptr, 10);
            if (endptr == &(argv[arg_n][1])  ||  *endptr != '\0')
            {
                fprintf(stderr, "%s: invalid <server> specification\n", argv[0]);
                exit(1);
            }
            if (option_instance > CX_MAX_SERVER)
            {
                fprintf(stderr, "%s: server specification \"%s\" out of range 0-%d\n",
                        argv[0], argv[arg_n], CX_MAX_SERVER);
                exit(1);
            }
        }
        else
            defserver = argv[arg_n];
    }

    /* Load the subsystem... */
    ds = CdrLoadSubsystem("file", sysname, argv[0]);
    if (ds == NULL)
    {
        fprintf(stderr, "%s(%s): %s\n",
                argv[0], sysname, CdrLastErr());
        exit(1);
    }

    // ----
    InitBuiltins();
    sl_set_uniq_checker  (cxsd_uniq_checker);
    fdio_set_uniq_checker(cxsd_uniq_checker);
    if (cxsd_log_init("stand", option_verbosity, 1) < 0)
        exit(1);
    ReadHWConfig (argv[0]);
    if (option_instance >= 0  &&
        CxsdActivateFrontends(option_instance) != 0)
        exit(1);
    ActivateHW   (argv[0]);
    // ----

    /* ...and realize it */
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
