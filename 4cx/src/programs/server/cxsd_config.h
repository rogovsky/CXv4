#ifndef __CXSD_CONFIG_H
#define __CXSD_CONFIG_H


#include "cxsd_core_commons.h"


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __CXSD_CONFIG_C
  #define D
  #define V(value...) = value
#else
  #define D extern
  #define V(value...)
#endif /* __CXSD_CONFIG_C */


#define DEF_FILE_CXSD_CONF   "./cxsd.conf"
#define DEF_FILE_CXHOSTS     "./.cxhosts"
#define DEF_FILE_DEVLIST     "./cxsd-devlist.lst"

#define FILE_CXSDPID_FMT     "%s/cxsd-%d.pid"

enum
{
    DEF_OPT_VERBOSITY     = LOGL_NOTICE,
    DEF_OPT_CACHERFW      = 1,
    DEF_OPT_BASECYCLESIZE = 1000000,
};


/* option_NNN -- command-line switches */

D const char *option_configfile       V(DEF_FILE_CXSD_CONF);    /* Config file name */
D int         option_dontdaemonize    V(0);                     /* Don't daemonize */
D int         option_donttrapsignals  V(0);                     /* Don't trap signals */
D int         option_norun            V(0);                     /* Parse config only */
D int         option_simulate         V(CXSD_SIMULATE_OFF);     /* Simulate hardware */
D int         option_verbosity        V(DEF_OPT_VERBOSITY);     /* Verbosity level */
D int         option_instance         V(0);                     /* Server # */
D const char *option_dbref            V(DEF_FILE_DEVLIST);      /* Hardware list file */
D int         option_cacherfw         V(DEF_OPT_CACHERFW);      /* Cache reads from write channels */
D int         option_defdrvlog_mask   V(0);                     /**/
D int         option_basecyclesize    V(DEF_OPT_BASECYCLESIZE); /* Server period (in us) */

D char        config_pid_file_dir  [PATH_MAX]  V(".");
D char        config_drivers_path  [PATH_MAX]  V(".");
D char        config_frontends_path[PATH_MAX]  V(".");
D char        config_exts_path     [PATH_MAX]  V(".");
D char        config_libs_path     [PATH_MAX]  V(".");
D char        config_cxhosts_file  [PATH_MAX]  V(DEF_FILE_CXHOSTS);
    

void ParseCommandLine(int argc, char *argv[]);
void ReadConfig      (const char *argv0,
                      const char *def_scheme, const char *reference);
void UseCmdConfigs   (const char *argv0);


#undef D
#undef V


#endif /* __CXSD_CONFIG_H */
