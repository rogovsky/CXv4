#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "misc_macros.h"

#include "remdrv_proto_v4.h"


static char  *argv0;
static short  port = REMDRV_DEFAULT_PORT;
static int    dontdaemonize = 0;
static int    verbose       = 0;
static int    lsock = -1;
static const char *argv_params[2] = {[0 ... 1] = NULL};

static void ShowHelp(void)
{
    fprintf(stderr, "Usage: %s [-d] [-v] [-p port] [PREFIX [SUFFIX]]\n", argv0);
    fprintf(stderr, "    '-d' switch prevents daemonization\n");
    fprintf(stderr, "    '-v' switch raises verbosity level\n");
    exit(1);
}

static void ErrnoFail(const char *message)
{
    fprintf(stderr, "%s: %s: %s\n", argv0, message, strerror(errno));
    exit(1);
}

static void ParseCommandLine(int argc, char *argv[])
{
  int             argn;
  int             pidx;
  struct servent *sp;
    
    for (argn = 1, pidx = 0;  argn < argc;  argn++)
    {
        if (argv[argn][0] == '-')
        {
            switch (argv[argn][1])
            {
                case '\0':
                    fprintf(stderr, "%s: '-' requires a switch letter\n", argv0);

                case 'h':
                    ShowHelp();

                case 'd':
                    dontdaemonize = 1;
                    break;

                case 'v':
                    verbose       = 1;
                    break;

                case 'p':
                    argn++;
                    if (argn >= argc)
                    {
                        fprintf(stderr, "%s: '-p' requires a port number", argv[0]);
                        ShowHelp();
                    }
                    port = atoi(argv[argn]);
                    if (port == 0)
                    {
#if 0
                        sp = getservbyname(argv[argn], "tcp");
                        if (sp != NULL)
                            port = sp->s_port;
                        else
#endif
                        {
                            fprintf(stderr, "%s: bad port spec '%s'\n", argv0, argv[argn]);
                            exit(1);
                        }
                    }
                    break;

                default:
                    fprintf(stderr, "%s: unknown option '%s'\n", argv0, argv[argn]);
                    ShowHelp();
            }
        }
        else
        {
            if (pidx < countof(argv_params)) argv_params[pidx++] = argv[argn];
        }
    }
}

#ifndef __OPTIMIZE__  /* That fscked uClibc... */
  #ifdef htonl       // 31.01.2005 And because of that damned cross m68k-elf-gcc (egcs-2.91.66 19990314), which loves to segfault upon redefines...
    #undef htonl
  #endif
  #define htonl(x) (x)
  #ifdef htons
    #undef htons
  #endif
  #define htons(x) (x)
#endif

static void CreateSocket(void)
{
  struct sockaddr_in  iaddr;		/* Address to bind `inet_entry' to */
  int                 on     = 1;	/* "1" for REUSE_ADDR */
  int                 r;

    /* Create a socket */
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0)
        ErrnoFail("unable to create listening socket");

    /* Bind it to a name */
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    iaddr.sin_family      = AF_INET;
    iaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iaddr.sin_port        = htons(port);
    r = bind(lsock, (struct sockaddr *) &iaddr,
             sizeof(iaddr));
    if (r < 0)
        ErrnoFail("unable to bind() listening socket");

    /* Mark it as listening */
    r = listen(lsock, 20);
    if (r < 0)
        ErrnoFail("unable to listen() listening socket");
}

static void Daemonize(void)
{
  int   fd;
  char *args[1];

    if (dontdaemonize) return;

    /* Substitute with a child to release the caller */
    switch (vfork())
    {
        case  0:  /* Child */
            break;                                

        case -1:  /* Failed to fork */
            ErrnoFail("can't fork()");

        default:  /* Parent */
            _exit(0);
    }

//    args[0] = NULL;
//    fprintf(stderr, "execv=%d\n", execv("/", args));
    
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

static void ReportProblem(int fd, int code, const char *format, ...)
            __attribute__ ((format (printf, 3, 4)));

static void ReportProblem(int fd, int code, const char *format, ...)
{
  struct {remdrv_pkt_header_t hdr;  int32 data[1000];} pkt;
  int      len;
  va_list  ap;

    va_start(ap, format);
    len = vsprintf((char *)(pkt.data), format, ap);
    va_end(ap);

    bzero(&pkt.hdr, sizeof(pkt.hdr));
    pkt.hdr.pktsize = sizeof(pkt.hdr) + len + 1;
    pkt.hdr.command = code;

    write(1, &pkt, pkt.hdr.pktsize);
}

static void Run(void)
{
  int                 r;
  int                 fd;
  struct sockaddr_in  isrc;
  socklen_t           len;
  const char *prefix = argv_params[0];
  const char *suffix = argv_params[1];
  char                name[1024];
  char                path[1024];
  int                 pctr;
  char               *args[2];

    while (1)
    {
        len = sizeof(isrc);
        fd = accept(lsock, (struct sockaddr *)&isrc, &len);

        if (fd < 0)
        {
            fprintf(stderr, "accept=%d: %s\n", fd, strerror(errno));
            goto CONTINUE_WHILE;
        }

        /* Get the program name */
        for (pctr = 0;  pctr < sizeof(name);  pctr++)
        {
            r = read(fd, name + pctr, 1);
            if (r != 1)
            {
                fprintf(stderr, "read(name): r = %d: %s\n", r,
                        (r < 0)? strerror(errno) : "(connection closed?)");
                close(fd);
                goto CONTINUE_WHILE;
            }

            if (name[pctr] == '\n'  ||  name[pctr] == '\0') break;
            //if (name[pctr] == '\r') pctr--; /*!!! Bug!!!*/
        }
        name[pctr] = '\0';

        if (verbose)
            fprintf(stderr, "%s: request to run \"%s\"\n", argv0, name);

        /* Prepare name of file to execute */
        if (prefix == NULL) prefix = "";
        if (suffix == NULL) suffix = "";
        if (name[0] != '/')
            check_snprintf(path, sizeof(path), "%s%s%s", prefix, name, suffix);
        else
            strzcpy(path, name, sizeof(path));
        
        /* Fork into separate process */
        r = vfork();
        switch (r)
        {
            case  0:  /* Child */
                break;
            case -1:  /* Failed to fork */
                fprintf(stderr, "fork=%d: %s\n", r, strerror(errno));
                ReportProblem(fd, REMDRV_C_RRUNDP, "%s::%s(): fork=%d: %s",
                              __FILE__, __FUNCTION__, r, strerror(errno));
                /*FALLTHROUGH*/
            default:  /* Parent */
                close(fd);
                goto CONTINUE_WHILE;
        }

        /* Clone connection fd into stdin/stdout. !!!Don't touch stderr!!! */
        dup2(fd, 0);  fcntl(0, F_SETFD, 0);
        dup2(fd, 1);  fcntl(1, F_SETFD, 0);
        close(fd);

        /* And close listening socket -- drivelets shouldn't see it */
        close(lsock);

        /* Exec the required program */
        args[0] = path;
        args[1] = NULL;
//        fprintf(stderr, "About to exec \n\"%s\"\n", path);
        r = execv(path, args);
        fprintf(stderr, "%s: can't execv(\"%s\"): %s\n", argv0, path, strerror(errno));
        ReportProblem(1, REMDRV_C_RRUNDP, "%s::%s(): can't execv(\"%s\"): %s",
                      __FILE__, __FUNCTION__, path, strerror(errno));
        _exit(0);
        
 CONTINUE_WHILE:;
    }
}

static void SIGCHLD_handler(int signum);

static void InstallSIGCHLD_handler(void)
{
    signal(SIGCHLD, SIGCHLD_handler);
}

static void SIGCHLD_handler(int signum)
{
  int  status;
    
//    fprintf(stderr, "SIGCHLD\n");
    while (waitpid(-1, &status, WNOHANG) > 0);
    InstallSIGCHLD_handler();
}

#if 0
static void InstallSIGCHLD_handler(void)
{
  struct sigaction  sa;

    bzero(&sa, sizeof(sa));
    sa.handler = SIGCHLD_handler;
    sa.flags = SA_NOCLDSTOP | SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}
#endif

int main(int argc, char *argv[])
{
    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Intercept signals */
    signal(SIGPIPE, SIG_IGN);
    InstallSIGCHLD_handler();

    argv0 = argv[0];
    ParseCommandLine(argc, argv);
    CreateSocket();
    Daemonize();
    Run();
    
    return 0;
}
