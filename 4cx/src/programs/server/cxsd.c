#include "cxsd_includes.h"

/* For cleanup only */
#include "cxlib.h"
#include "cda.h"


/* ==== Cleanup stuff ============================================= */

static void InterceptSignals(void *SIGFATAL_handler,
                             void *SIGCHLD_handler,
                             void *SIGHUP_handler,
                             void *exitproc0 __attribute__((unused)),
                             void *exitproc2 __attribute__((unused)))
{
  static int interesting_signals[] =
  {
      SIGINT,  SIGQUIT,   SIGILL,  SIGABRT, SIGIOT,  SIGBUS,
      SIGFPE,  SIGUSR1,   SIGSEGV, SIGUSR2, SIGALRM, SIGTERM,
#ifdef SIGSTKFLT
      SIGSTKFLT,
#endif
      SIGXCPU, SIGVTALRM, SIGPROF,
      -1
  };

  int        x;

    /* Intercept signals */
    for (x = 0;  interesting_signals[x] >= 0;  x++)
        set_signal(interesting_signals[x], SIGFATAL_handler);
    
    set_signal(SIGHUP,  SIGHUP_handler?  SIGHUP_handler  : SIGFATAL_handler);
    set_signal(SIGCHLD, SIGCHLD_handler? SIGCHLD_handler : SIG_IGN);
    
    set_signal(SIGPIPE, SIG_IGN);
    set_signal(SIGCONT, SIG_IGN);

    /* And register a call-at-exit handler */
#if OPTION_USE_ON_EXIT
    on_exit(exitproc2, NULL);
#else
    atexit(exitproc0);
#endif /* OPTION_USE_ON_EXIT */
}

static void DoClean(void)
{
    /* For now we do nothing -- since we have ho children. */
}

static void onsig(int sig)
{
    logline(LOGF_SYSTEM, 0, "signal %d (\"%s\") arrived", sig, strsignal(sig));
    DoClean();
    errno = 0;
    exit(sig);
}

static void onhup(int sig)
{
    ll_reopen(0);
    logline(LOGF_SYSTEM, 0, "signal %d (\"%s\") arrived, re-opening logs", sig, strsignal(sig));
}

static void exitproc2(int code, void *arg __attribute__((unused)))
{
    if (pid_file_created) unlink(pid_file);
    if (normal_exit) return;
    logline(LOGF_SYSTEM, 0, "exit(%d), errno=%d (\"%s\")", 
            code, errno, strerror(errno));
}

static void exitproc0(void)
{
    if (pid_file_created) unlink(pid_file);
    if (normal_exit) return;
    logline(LOGF_SYSTEM, 0, "exit, errno=%d (\"%s\")",
                          errno, strerror(errno));
}

static void PrepareClean(void)
{
    if (!option_donttrapsignals)
        InterceptSignals(onsig, NULL, onhup, exitproc0, exitproc2);
}

/* ---- End of cleanup stuff -------------------------------------- */

//////////////////////////////////////////////////////////////////////

/*
 *  Daemonize
 *      Makes the program into daemon.
 *      Forks into another process, switches into a new session,
 *      closes stdin/stdout/stderr.
 */

static void Daemonize(void)
{
  int fd;

    {
      const char *p = getenv("LOGNAME");
        if (p != NULL  &&  strstr(p, "emanov") != NULL)
            fprintf(stderr, "%s emanofeding to background\n", strcurtime());
    }

    /* Substitute with a child to release the caller */
    switch (fork())
    {
        case  0:  break;                                 /* Child */
        case -1:  perror("cxsd can't fork()"); exit(1);  /* Failed to fork */
        default:  normal_exit = 1; exit(0);              /* Parent */
    }

    /* Switch to a new session */
    if (setsid() < 0)
    {
        perror("cxsd can't setsid()");
        exit(1);
    }

    /* Get rid of stdin, stdout and stderr files and terminal */
    /* We don't need to disassociate from tty explicitly, thanks to setsid() */
    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1) {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2) close(fd);
    }

    errno = 0;
}

/*
 *  CreatePidFile
 *      Creates a file containing the pid, so that "kill `cat cxsd-NN.pid`"
 *      can be used to terminate the daemon. Upon exit the file is removed.
 */

static void CreatePidFile(const char *argv0)
{
  FILE *fp;
  
    sprintf(pid_file, FILE_CXSDPID_FMT, config_pid_file_dir, option_instance);

    fp = fopen(pid_file, "w");
    if (fp == NULL)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, 0,
                "%s: %s: unable to create pidfile \"%s\"",
                argv0, __FUNCTION__, pid_file);
        DoClean();
        normal_exit = 1;
        exit(1);
    }
    if (fprintf(fp, "%d\n", getpid()) < 0  ||  fclose(fp) < 0)
    {
        logline(LOGF_SYSTEM | LOGF_ERRNO, 0,
                "%s: %s: unable to write pidfile \"%s\"",
                argv0, __FUNCTION__, pid_file);
        DoClean();
        normal_exit = 1;
        exit(1);
    }

    pid_file_created = 1;
}

enum {ACCESS_HBT_SECS = 600};

static void PerformAccess(int uniq, void *unsdptr,
                          sl_tid_t tid, void *privptr)
{
  int             pid_fd;
  char            pid_buf[1];

    /* Access pid-file... */
    if (pid_file_created  &&  (pid_fd = open(pid_file, O_RDONLY)) >= 0)
    {
        read(pid_fd, pid_buf, sizeof(pid_buf));
        close(pid_fd);
    }
    
    /* ...and logs */
    ll_access();

    sl_enq_tout_after(0, NULL, /*!!!uniq*/
                      ACCESS_HBT_SECS * 1000000, PerformAccess, NULL);
}

//////////////////////////////////////////////////////////////////////

static  CxsdDb db;

static void ReadHWConfig(const char *argv0)
{
    CxsdHwSetSimulate     (option_simulate);
    CxsdHwSetCacheRFW     (option_cacherfw);
    CxsdHwSetDefDrvLogMask(option_defdrvlog_mask);
    CxsdSetDrvLyrPath     (config_drivers_path);
    db = CxsdDbLoadDb(argv0, "file", option_dbref);
    if (db == NULL)
    {
        fprintf(stderr, "%s %s: unable to read database \"%s\"\n",
                strcurtime(), argv0, option_dbref);
        normal_exit = 1;
        exit(1);
    }
}

static void on_timeback(void)
{
    CxsdHwTimeChgBack();
    ///fprintf(stderr, "Time-back-change!!!\n");
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
    sl_set_on_timeback_proc(on_timeback);
}

//////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  char cxsd_N[100];

    ParseCommandLine(argc, argv);
    sprintf(cxsd_N, "cxsd#%d", option_instance);
    PrepareClean();

    InitBuiltins(NULL);
    sl_set_uniq_checker  (cxsd_uniq_checker);
    fdio_set_uniq_checker(cxsd_uniq_checker);

    ReadConfig   (argv[0], "plaintext", option_configfile);
    UseCmdConfigs(argv[0]);
    if (cxsd_log_init(cxsd_N, option_verbosity, option_dontdaemonize) < 0)
        {normal_exit = 1; exit(1);}
    ReadHWConfig (argv[0]);

    if (option_norun) {normal_exit = 1; exit(0);}
    if (CxsdActivateFrontends(option_instance) != 0) {normal_exit = 1; exit(1);}
    if (!option_dontdaemonize) Daemonize();
    CreatePidFile(argv[0]);
    PerformAccess(0, NULL, -1, NULL);
    ActivateHW   (argv[0]);

    logline(LOGF_ACCESS  , 0, "ACCESS  ");
    logline(LOGF_DEBUG   , 0, "DEBUG   ");
    logline(LOGF_MODULES , 0, "MODULES ");
    logline(LOGF_HARDWARE, 0, "HARDWARE");

    logline(LOGF_SYSTEM, 0, "*** Start ***");
    sl_main_loop();

    //normal_exit = 1;
    return 1;
}
