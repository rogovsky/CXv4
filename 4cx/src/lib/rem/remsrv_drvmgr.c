#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/*!!!*/#include <netinet/tcp.h>

#include "misclib.h"

#include "remdrv_proto_v4.h"
#include "remsrv.h"
#include "remcxsdP.h"


static char  *argv0;
static short  srv_port      = REMDRV_DEFAULT_PORT;
static int    dontdaemonize = 0;
static int    lsock         = -1;
static int    csock         = -1;

static remsrv_conscmd_descr_t *specific_cmd_table;
static remsrv_clninfo_t        specific_clninfo;
static const char             *specific_prompt_name;
static const char             *specific_issue_name;


/*### devid management #############################################*/


int  AllocateDevID(void)
{
  int            devid;

    /* Allocation starts from 1 to escape having drivers with devid=0 */
    for (devid = 1;  devid < remcxsd_maxdevs;  devid++)
        if (!(remcxsd_devices[devid].in_use))
        {
            InitRemDevRec(remcxsd_devices + devid);
            return devid;
        }

    return -1;
}

void FreeDevID    (int devid)
{
  remcxsd_dev_t *dev = remcxsd_devices + devid;
  int            s_devid;

    remcxsd_debug("[%d] %s()", devid, __FUNCTION__);

    if (devid < 0  ||  devid >= remcxsd_maxdevs)
    {
        remcxsd_debug("%s(): devid=%d, out_of[0...%d)",
                      __FUNCTION__, devid, remcxsd_maxdevs);
        return;
    }
    if (!(dev->in_use)) return; // Guard mainly for SetDevState() ?

    if (dev->being_processed)
    {
        dev->being_destroyed = 1;
        return;
    }

    if (dev->inited)
    {
        dev->being_processed = 1; // A guard against nested calls to FreeDevID from LEAVE_DRIVER_S()
        ENTER_DRIVER_S(devid, s_devid);
        if (dev->metric           != NULL  &&
            dev->metric->term_dev != NULL)
            dev->metric->term_dev(devid, dev->devptr);
        if (dev->lyrid != 0                                  &&
            remcxsd_layers[-dev->lyrid]             != NULL  &&
            remcxsd_layers[-dev->lyrid]->disconnect != NULL)
            remcxsd_layers[-dev->lyrid]->disconnect(devid);
        LEAVE_DRIVER_S(s_devid);
    }

    sl_deq_tout    (dev->tid); dev->tid = -1;
    fdio_deregister(dev->fhandle);
    sl_del_fd      (dev->s_fdh);
    close          (dev->s);

    fdio_do_cleanup(devid);
    sl_do_cleanup  (devid);
    if (dev->metric              != NULL  &&
        dev->metric->privrecsize != 0)
    {
        if (dev->metric->paramtable != NULL)
            psp_free(dev->devptr, dev->metric->paramtable);
        safe_free  (dev->devptr);
    }

    dev->in_use = 0;
}


/*###  #######################################################*/

static void ReadDriverName(int devid, void *unsdptr,
                           sl_fdh_t fdh, int fd, int mask, void *privptr)
{
  remcxsd_dev_t     *dev   = remcxsd_devices + devid;
  int                r;

  CxsdDriverModRec **item;

    ////fprintf(stderr, "%s(%d)/%d\n", __FUNCTION__, fd, devid);
  
    if ((mask & SL_RD) == 0) return;
  
    errno = 0;
    r = read(dev->s, dev->drvname + dev->drvnamelen, 1);
    if (r <= 0)
    {
        remcxsd_debug("%s: read=%d: %s", __FUNCTION__, r, strerror(errno));
        FreeDevID(devid);
        return;
    }

    if (dev->drvname[dev->drvnamelen] != '\0'  &&
        dev->drvname[dev->drvnamelen] != '\n')
    {
        dev->drvnamelen++;
        if (dev->drvnamelen < sizeof(dev->drvname)) return;

        remcxsd_report_to_fd(dev->s, REMDRV_C_RRUNDP,
                             "Driver name too long - limit is %zu chars",
                             sizeof(dev->drvname) - 1);
        FreeDevID(devid);
        return;
    }
    dev->drvname[dev->drvnamelen] = '\0';

    /* Okay, the whole name has arrived */
    sl_deq_tout(dev->tid);   dev->tid   = -1;
    sl_del_fd  (dev->s_fdh); dev->s_fdh = -1;

    /*!!!strip-off optional "@layer" and try to find such layer? */
    if (remcxsd_numlyrs > 0) dev->lyrid = -1;

    for (item = remsrv_drivers;
         item != NULL  &&  *item != NULL  &&  (*item)->mr.name != NULL;
         item++)
        if (strcmp(dev->drvname, (*item)->mr.name) == 0)
            dev->metric = *item;
    remcxsd_debug("[%d] request for driver \"%s\" -- %s",
                  devid, dev->drvname, dev->metric != NULL? "found" : "not-found");
    if (dev->metric == NULL)
    {
        remcxsd_report_to_fd(dev->s, REMDRV_C_RRUNDP,
                             "no such driver - \"%s\"",
                             dev->drvname);
        FreeDevID(devid);
        return;

    }

    dev->fhandle = fdio_register_fd(devid, NULL, /*!!!uniq*/
                                    dev->s, FDIO_STREAM, ProcessPacket, NULL,
                                    REMDRV_MAXPKTSIZE,
                                    sizeof(remdrv_pkt_header_t),
                                    FDIO_OFFSET_OF(remdrv_pkt_header_t, pktsize),
                                    FDIO_SIZEOF   (remdrv_pkt_header_t, pktsize),
#if   BYTE_ORDER == LITTLE_ENDIAN
                                    FDIO_LEN_LITTLE_ENDIAN,
#elif BYTE_ORDER == BIG_ENDIAN
                                    FDIO_LEN_BIG_ENDIAN,
#else
#error Sorry, the "BYTE_ORDER" is neither "LITTLE_ENDIAN" nor "BIG_ENDIAN"
#endif
                                    1,
                                    0);
    ////fprintf(stderr, "register=%d\n", dr->fhandle);
}

static void CleanupUnfinishedReq(int uniq, void *unsdptr, sl_tid_t tid, void *privptr)
{
  int        devid = ptr2lint(privptr);
  
    FreeDevID(devid);
}

static void RemSrvAcceptHostConnection(int uniq, void *unsdptr,
                                       fdio_handle_t handle, int reason,
                                       void *inpkt __attribute((unused)), size_t inpktsize __attribute((unused)),
                                       void *privptr __attribute((unused)))
{
  int            s;
  socklen_t      len = 0;
  int            devid;
  remcxsd_dev_t *dev;
  int            on = 1;
  socklen_t      addrlen;

    remcxsd_debug("%s()", __FUNCTION__);
  
    if (reason != FDIO_R_ACCEPT) return; /*?!*/
  
    s = fdio_accept(handle, NULL, &len);
    if (s < 0)
    {
        remcxsd_debug("%s: fdio_accept=%d, %s", __FUNCTION__, s, strerror(errno));
        return;
    }

    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    
    devid = AllocateDevID();
    if (devid < 0)
    {
        remcxsd_report_to_fd(s, REMDRV_C_RRUNDP, "%s: unable to AllocateDevID()", __FUNCTION__);
        close(s);
        return;
    }
    remcxsd_debug("\tdevid=%d", devid);

    dev = remcxsd_devices + devid;
    dev->s = s;

    dev->s_fdh = sl_add_fd(devid, NULL, /*!!!uniq*/ s, SL_RD, ReadDriverName, NULL);

    dev->when = time(NULL);
    addrlen = sizeof(dev->cln_addr);
    getpeername(s, (struct sockaddr *)&(dev->cln_addr), &addrlen);
    
    dev->tid = sl_enq_tout_after(devid, NULL, /*!!!uniq*/ 10*1000000, CleanupUnfinishedReq, lint2ptr(devid));
}



/*###  #######################################################*/

static int Limited_vfdprintf(int fd, const char *format, va_list ap)
{
  char     buf[1000];

    vsnprintf(buf, sizeof(buf), format, ap);
    buf[sizeof(buf) - 1] = '\0';

    return write(fd, buf, strlen(buf));
}

static int Limited_fdprintf(int fd, const char *format, ...)
           __attribute__ ((format (printf, 2, 3)));
static int Limited_fdprintf(int fd, const char *format, ...)
{
  va_list  ap;
  int      r;

    va_start(ap, format);
    r = Limited_vfdprintf(fd, format, ap);
    va_end(ap);

    return r;
}

typedef struct
{
    int                 in_use;       // Is this cell currently used
    
    int                 s;            // Host communication socket
    int                 s_fdh;
    time_t              when;
    struct sockaddr_in  cln_addr;
    struct sockaddr_in  srv_addr;

    char                cmdbuf[100];  // Command
    int                 cmdbufused;   // # of bytes already read
    char                prev_c;       // Last (previous) char read
} consrec_t;

static consrec_t consinfo[10];      // An arbitrary limit

static int AllocateConsID(void)
{
  int        consid;
  consrec_t *cr;

    /* Allocation starts from 1 */
    for (consid = 1;  consid < countof(consinfo);  consid++)
        if (!(consinfo[consid].in_use))
        {
            cr = consinfo + consid;
            bzero(cr, sizeof(*cr));
            cr->in_use   = 1;
            cr->s        = -1;
            return consid;
        }

    return -1;
}

static void FreeConsID(int consid)
{
  consrec_t *cr = consinfo + consid;

    sl_del_fd      (cr->s_fdh);
    close          (cr->s);
    cr->s = -1;

    consinfo[consid].in_use = 0;
}

static remsrv_conscmd_descr_t * FindCommand(remsrv_conscmd_descr_t *table, const char *name)
{
  remsrv_conscmd_descr_t *cmd_p;

    for (cmd_p = table;
         cmd_p != NULL  &&  cmd_p->name != NULL;
         cmd_p++)
        if (strcmp(cmd_p->name, name) == 0) return cmd_p;

    return NULL;
}

static int  ExecConsCommand    (char             *cmd,
                                void             *context,
                                remsrv_cprintf_t  do_printf)
{
  char                   *cp = cmd;
  int                     nargs;
  const char             *argp[100]; // An arbitrary limit

  int                     n;
  int                     x;
  const char             *devinfo_p;
  char                   *endptr;

  int                     add_dtls; // "Add details"
  char                    time_s[100];

  int                     is_kill;

  remsrv_conscmd_descr_t *cmd_p;

    for (nargs = 0;  nargs < countof(argp) - 1;  )
    {
        /* Skip leading/separating whitespace */
        while (*cp != '\0'  &&  isspace(*cp)) cp++;
        /* Treat '#' as comment */
        if (*cp == '\0'  ||  *cp == '#') break;
        /* Consume non-space */
        argp[nargs++] = cp;
        while (*cp != '\0'  &&  !isspace(*cp)) cp++;
        /* Put separating NUL if not already at a terminating NUL */
        if (*cp == '\0') break;
        else             *cp++ = '\0';
    }
    argp[nargs] = NULL;
    ////fprintf(stderr, "cmd=\"%s\": nargs=%d, argp[0]=\"%s\"\n", cmd, nargs, argp[0]);

    if      (nargs == 0)
    {
        // Empty line or comment -- do nothing
    }
    else if (strcmp(argp[0], "?") == 0  ||  strcmp(argp[0], "help") == 0)
    {
        do_printf(context, "Neither \"help\" nor \"?\" commands are implemented yet. Sorry.\n");
    }
    else if (strcmp(argp[0], "who") == 0)
    {
        add_dtls = (nargs > 1);
        for (n = 0;  n < countof(consinfo);  n++)
            if (consinfo[n].in_use)
            {
                if (add_dtls)
                    sprintf(time_s, ":%d@%s",
                            ntohs(consinfo[n].cln_addr.sin_port),
                            stroftime(consinfo[n].when, "_"));
                else
                    time_s[0] = '\0';
                do_printf(context, "%3d:%s%s \n",
                          n, inet_ntoa(consinfo[n].cln_addr.sin_addr), time_s);
            }
    }
    else if (strcmp(argp[0], "date") == 0)
    {
        do_printf(context, "%s\n", strcurtime());
    }
    else if (strcmp(argp[0], "list") == 0)
    {
        add_dtls = (nargs > 1);
        for (n = 0;  n < remcxsd_maxdevs;  n++)
            if (remcxsd_devices[n].in_use)
            {
                if (specific_clninfo != NULL)
                    devinfo_p = specific_clninfo(n);
                else
                    devinfo_p = "-";
                
                do_printf(context, "%3d:%-15s ",
                          n, inet_ntoa(remcxsd_devices[n].cln_addr.sin_addr));
                if (add_dtls)
                    do_printf(context, ":%d@%s ",
                              ntohs(remcxsd_devices[n].cln_addr.sin_port),
                              stroftime(remcxsd_devices[n].when, "_"));
                if (remcxsd_devices[n].businfocount == 0)
                    do_printf(context, "-");
                else
                    for (x = 0;  x < remcxsd_devices[n].businfocount;  x++)
                        do_printf(context, "%s%d",
                                  x > 0? "," : "", remcxsd_devices[n].businfo[x]);
                do_printf(context, " %s %s\n",
                          devinfo_p,
                          remcxsd_devices[n].drvname);
            }
    }
    else if ((is_kill = (strcmp(argp[0], "kill")  == 0))  ||
                         strcmp(argp[0], "close") == 0)
    {
        for (x = 1;  x < nargs;  x++)
        {
            n = strtol(argp[x], &endptr, 10);
            if (endptr == argp[x]  ||  *endptr != '\0')
                do_printf(context, "%s: invalid devid spec \"%s\"\n",
                          argp[0], argp[x]);
            else if (n < 0  ||  n >= remcxsd_maxdevs  ||
                     remcxsd_devices[n].in_use == 0)
                do_printf(context, "%s: no such device %d\n",
                          argp[0], n);
            else
            {
                if (is_kill)
                {
                    DoDriverLog(n, DRIVERLOG_EMERG,
                                "kill command from remsrv console");
                    SetDevState(n, DEVSTATE_OFFLINE, 0,
                                "remsrv: Terminated from console");
                }
                else
                {
                    DoDriverLog(n, DRIVERLOG_EMERG,
                                "close command from remsrv console");
                    FreeDevID  (n);
                }
            }
        }
    }
    else if (strcmp(argp[0], "bye")    == 0  ||
             strcmp(argp[0], "quit")   == 0  ||
             strcmp(argp[0], "logout") == 0  ||
                    argp[0][0] == '\x04')
    {
        do_printf(context, "Bye.\n");
        return +1;
    }
#ifdef REMSRV_DRVMGR_ALLOW_TERMINATE
    else if (strcmp(argp[0], "terminate") == 0)
    {
        exit(0);
    }
#endif
    else
    {
        cmd_p = FindCommand(specific_cmd_table, argp[0]);
        if (cmd_p != NULL)
                cmd_p->processer(nargs, argp, context, do_printf);
        else
            do_printf(context, "Unknown command \"%s\"\n", argp[0]);
    }

    return 0;
}


static void PrintConsPrompt(consrec_t *cr)
{
    Limited_fdprintf(cr->s, "%s@%s> ",
                     specific_prompt_name != NULL? specific_prompt_name : "???",
                     inet_ntoa(cr->srv_addr.sin_addr));
}

static void drvmgr_do_printf(void *context, const char *format, ...)
            __attribute__ ((format (printf, 2, 3)));
static void drvmgr_do_printf(void *context, const char *format, ...)
{
  consrec_t *cr = context;
  va_list    ap;

    va_start(ap, format);
    Limited_vfdprintf(cr->s, format, ap);
    va_end(ap);
}

static void ReadConsoleCommand(int uniq, void *unsdptr,
                               sl_fdh_t fdh, int fd, int mask, void *privptr)
{
  int             consid = ptr2lint(privptr);
  consrec_t      *cr = consinfo + consid;
  int             r;
  char            this_c;
  char            prev_c;

    if ((mask & SL_RD) == 0) return;
  
    errno = 0;
    r = read(cr->s, cr->cmdbuf + cr->cmdbufused, 1);
    ////fprintf(stderr, "::read=%d, cmdbufused=%d, c=%d\n", r, cr->cmdbufused, cr->cmdbuf[cr->cmdbufused]);
    if (r <= 0)
    {
        remcxsd_debug("%s: read=%d: %s", __FUNCTION__, r, strerror(errno));
        FreeConsID(consid);
        return;
    }

    this_c = cr->cmdbuf[cr->cmdbufused];
    prev_c = cr->prev_c;
    cr->prev_c = this_c;

    if (this_c == '\n'  &&  prev_c == '\r') return; // Ignore LF after CR (to understand CR,LF as a SINGLE <Enter>

    if (this_c != '\n'  &&  this_c != '\r')
    {
        cr->cmdbufused++;
        if (cr->cmdbufused < sizeof(cr->cmdbuf)) return;

        Limited_fdprintf(cr->s,
                         "Command too long - limit is %zu chars\n",
                         sizeof(cr->cmdbuf) - 1);
        FreeConsID(consid);
        return;
    }

    /* Okay, the whole command has arrived */
    cr->cmdbuf[cr->cmdbufused] = '\0';
    cr->cmdbufused = 0;

    r = ExecConsCommand(cr->cmdbuf, cr, drvmgr_do_printf);
    if (r > 0) FreeConsID(consid);
    PrintConsPrompt(cr);
}


static void RemSrvAcceptConsConnection(int uniq, void *unsdptr,
                                       fdio_handle_t handle, int reason,
                                       void *inpkt __attribute((unused)), size_t inpktsize __attribute((unused)),
                                       void *privptr __attribute((unused)))
{
  int        s;
  socklen_t  len = 0;
  int        consid;
  consrec_t *cr;
  int        on = 1;
  socklen_t  addrlen;

    remcxsd_debug("%s()", __FUNCTION__);
  
    if (reason != FDIO_R_ACCEPT) return; /*?!*/
  
    s = fdio_accept(handle, NULL, &len);
    if (s < 0)
    {
        remcxsd_debug("%s: fdio_accept=%d, %s", __FUNCTION__, s, strerror(errno));
        return;
    }

    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    
    consid = AllocateConsID();
    if (consid < 0)
    {
        Limited_fdprintf(s, "%s: unable to AllocateConsID()", __FUNCTION__);
        close(s);
        return;
    }

    cr = consinfo + consid;
    cr->s = s;

    cr->s_fdh = sl_add_fd(0, NULL, /*!!!uniq*/ s, SL_RD, ReadConsoleCommand, lint2ptr(consid));

    cr->when = time(NULL);
    addrlen = sizeof(cr->cln_addr);
    getpeername(s, (struct sockaddr *)&(cr->cln_addr), &addrlen);
    addrlen = sizeof(cr->srv_addr);
    getsockname(s, (struct sockaddr *)&(cr->srv_addr), &addrlen);

    Limited_fdprintf(s, "%s, built " __DATE__ " " __TIME__ "\n\n",
                     specific_issue_name != NULL? specific_issue_name : "??? server");
    PrintConsPrompt(cr);
}

/*### main() #######################################################*/

static void ShowHelp(void)
{
    fprintf(stderr, "Usage: %s [-d] [-e CONSOLE_COMMAND] [port]\n", argv0);
    fprintf(stderr, "    '-d' switch prevents daemonization\n");
    exit(1);
}

static void ErrnoFail(const char *format, ...)
            __attribute__ ((format (printf, 1, 2)));
static void ErrnoFail(const char *format, ...)
{
  va_list  ap;
  int      errcode = errno;
    
    fprintf(stderr, "%s: ", argv0);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(errcode));
    exit(1);
}

static void main_do_printf(void *context, const char *format, ...)
            __attribute__ ((format (printf, 2, 3)));
static void main_do_printf(void *context, const char *format, ...)
{
  va_list    ap;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

static void ParseCommandLine(int argc, char *argv[])
{
  int             argn;
  char            c;
    
    for (argn = 1;  argn < argc;  argn++)
    {
        if (argv[argn][0] == '-')
        {
            c = argv[argn][1];
            switch (c)
            {
                case '\0':
                    fprintf(stderr, "%s: '-' requires a switch letter\n", argv0);

                case 'h':
                    ShowHelp();

                case 'd':
                    dontdaemonize = 1;
                    break;

                case 'e':
                    argn++;
                    if (argn >= argc)
                    {
                        fprintf(stderr, "%s: '-%c' requires an argument\n", argv0, c);
                        exit(2);
                    }
                    ExecConsCommand(argv[argn], NULL, main_do_printf);
                    break;

                default:
                    fprintf(stderr, "%s: unknown option '%s'\n", argv0, argv[argn]);
                    ShowHelp();
            }
        }
        else
        {
            srv_port = atoi(argv[argn]);
            if (srv_port <= 0)
            {
                fprintf(stderr, "%s: bad port spec '%s'\n", argv0, argv[argn]);
                exit(1);
            }
        }
    }
}

static int CreateSocket(const char *which, int port)
{
  int                 s;
  struct sockaddr_in  iaddr;		/* Address to bind `inet_entry' to */
  int                 on     = 1;	/* "1" for REUSE_ADDR */
  int                 r;

    /* Create a socket */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        ErrnoFail("unable to create %s listening socket", which);

    /* Bind it to a name */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    iaddr.sin_family      = AF_INET;
    iaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iaddr.sin_port        = htons(port);
    r = bind(s, (struct sockaddr *) &iaddr,
             sizeof(iaddr));
    if (r < 0)
        ErrnoFail("unable to bind() %s listening socket to port %d", which, port);

    /* Mark it as listening */
    r = listen(s, 20);
    if (r < 0)
        ErrnoFail("unable to listen() %s listening socket", which);

    return s;
}

static void Daemonize(void)
{
  int   fd;

    if (dontdaemonize) return;

    /* Substitute with a child to release the caller */
    switch (fork())
    {
        case  0:  /* Child */
            fprintf(stderr, "Child -- operating\n");
            break;                                

        case -1:  /* Failed to fork */
            ErrnoFail("can't fork()");

        default:  /* Parent */
            fprintf(stderr, "Parent -- exiting\n");
            _exit(0);
    }

    /* Switch to a new session */
    if (setsid() < 0)
    {
        ErrnoFail("can't setsid()");
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


int remsrv_main(int argc, char *argv[],
                const char             *prog_prompt_name,
                const char             *prog_issue_name,
                remsrv_conscmd_descr_t *prog_cmd_table,
                remsrv_clninfo_t        prog_clninfo)
{
  int  lyr_n;
  int  s_devid;

    specific_cmd_table   = prog_cmd_table;
    specific_clninfo     = prog_clninfo;
    specific_prompt_name = prog_prompt_name;
    specific_issue_name  = prog_issue_name;

    set_signal(SIGPIPE, SIG_IGN);
    sl_set_uniq_checker  (cxsd_uniq_checker);
    fdio_set_uniq_checker(cxsd_uniq_checker);
    argv0 = argv[0];
    ParseCommandLine(argc, argv);
    lsock = CreateSocket("main",    srv_port);
    csock = CreateSocket("console", srv_port + 1);
    fdio_register_fd(0, NULL, /*!!!uniq*/
                     lsock, FDIO_LISTEN, RemSrvAcceptHostConnection, NULL,
                     0, 0, 0, 0, FDIO_LEN_LITTLE_ENDIAN, 0, 0);
    fdio_register_fd(0, NULL, /*!!!uniq*/
                     csock, FDIO_LISTEN, RemSrvAcceptConsConnection, NULL,
                     0, 0, 0, 0, FDIO_LEN_LITTLE_ENDIAN, 0, 0);
    Daemonize();

    for (lyr_n = 1;  lyr_n < remcxsd_numlyrs;  lyr_n++)
        if (remcxsd_layers[lyr_n] != NULL)
        {
            if (remcxsd_layers[lyr_n]->mr.init_mod != NULL)
                remcxsd_layers[lyr_n]->mr.init_mod();
            ENTER_DRIVER_S(-lyr_n, s_devid);
            if (remcxsd_layers[lyr_n]->init_lyr != NULL)
                remcxsd_layers[lyr_n]->init_lyr(-lyr_n);
            LEAVE_DRIVER_S(s_devid);
        }

    return sl_main_loop() == 0? 0 : 1;
}
