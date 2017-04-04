#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>

#include "misclib.h"
#include "timeval_utils.h"

#include "cm5307_camac.h"
#include "cxsd_driver.h"


typedef struct
{
    int  N;
    int  A;
    int  F;
    int  d;
} naf_spec_t;


static int         camac_fd;
static naf_spec_t  lam_spec;

enum
{   /* this enumeration according to man(3) pipe */
    PIPE_RD_SIDE = 0,
    PIPE_WR_SIDE = 1,
};
static int lam_pipe[2] = {-1, -1};


static void ParseNafSpec(const char *argv0, const char *spec, naf_spec_t *np)
{
  const char *p = spec;
  char       *errp;

    np->d = 0;
    if (
        ( // N
         (np->N  = strtol(p, &errp, 10)),
         (errp == p  ||  *errp != ',')
        )
        ||
        ( // A
         (p = errp + 1),
         (np->A  = strtol(p, &errp, 10)),
         (errp == p  ||  *errp != ',')
        )
        ||
        ( // F
         (p = errp + 1),
         (np->F  = strtol(p, &errp, 10)),
         (errp == p  ||  (*errp != '='  &&  *errp != '\0'))
        )
        ||
        ( // [=DATA]
         ((p = errp), (*p == '='))  &&
         (
          (p++),
          (np->d = strtol(p, &errp, 0)),
          (errp == p  ||  (*errp != '\0'  &&  *errp != '\0'  &&  !isspace(*errp)))
         )
        )
       )
    {
        fprintf(stderr, "%s: bad NAF spec \"%s\"\n", argv0, spec);
        exit(1);
    }
    p = errp;

    if (np->N < 1  ||  np->N > 24)
    {
        fprintf(stderr, "%s: N=%d is out of range [1-24] in \"%s\"\n",
                argv0, np->N, spec);
        exit(1);
    }
}

static void sigusr_handler(int sig __attribute__((unused)))
{
  static char snd_byte = 0;

fprintf(stderr, "zzz!\n");
    signal(SIGUSR1, sigusr_handler);
    write(lam_pipe[PIPE_WR_SIDE], &snd_byte, sizeof(snd_byte));
}
static void DoWait(const char *argv0, const char *spec)
{
  const char *p = spec;
  const char *errp;
  int             all;
  int             was_irq;
  int             expired;

  int             number;

  struct timeval  duration;
  struct timeval  deadline;
  struct timeval  now;          /* The time "now" for diff calculation */
  struct timeval  timeout;      /* Timeout for select() */
  fd_set          rfds;
  int             maxfd;
  int             r;

  int             dr;
  int             status;
  char            rcv_byte;

  static struct timeval time1st;
  static int            time1st_got = 0;

    /* Find out duration */
    all = *p == '.';
    p++;
    if (*p == '\0')
        number = -1;
    else
    {
        number = strtol(p, &errp, 0);
        if (errp == p)
        {
            fprintf(stderr, "%s: bad time spec in \"%s\"\n", argv0, spec);
            exit(1);
        }
        p = errp;

        /* Note: NO overflow-checking is performed (e.g. $[1<<30]h is valid, but will overflow */
        if      (*p == '\0'  ||
                 strcasecmp(p, "ms") == 0)
        {
            duration.tv_sec  = number / 1000;
            duration.tv_usec = number % 1000;
        }
        else if (strcasecmp(p, "us") == 0)
        {
            duration.tv_sec  = number / 1000000;
            duration.tv_usec = number % 1000000;
        }
        else if (strcasecmp(p, "s") == 0)
        {
            duration.tv_sec  = number;
            duration.tv_usec = 0;
        }
        else if (strcasecmp(p, "m") == 0)
        {
            duration.tv_sec  = number * 60;
            duration.tv_usec = 0;
        }
        else if (strcasecmp(p, "h") == 0)
        {
            duration.tv_sec  = number * 3600;
            duration.tv_usec = 0;
        }
        else
        {
            fprintf(stderr, "%s: bad time-units spec in \"%s\"\n", argv0, spec);
            exit(1);
        }
        gettimeofday(&deadline, NULL);
        timeval_add (&deadline, &deadline, &duration);
    }

    was_irq = 0;
    do
    {
        expired = 0;
        if (number > 0)
        {
            gettimeofday(&now, NULL);
            expired = (timeval_subtract(&timeout, &deadline, &now) != 0);
        }
        if (number <= 0  ||  expired)
            timeout.tv_sec = timeout.tv_usec = 0;
        
        FD_ZERO(&rfds);
        maxfd = -1;
        if (lam_pipe[PIPE_RD_SIDE] >= 0)
        {
            maxfd = lam_pipe[PIPE_RD_SIDE];
            FD_SET(maxfd, &rfds);
        }
        r = select(maxfd + 1,
                   &rfds, NULL, NULL,
                   number >= 0? &timeout : NULL);
        
        if      (r < 0)
        {
            if (!SHOULD_RESTART_SYSCALL())
            {
                fprintf(stderr, "%s: select(): %s\n", argv0, strerror(errno));
                exit(2);
            }
        }
        else if (r > 0)
        {
            while (read(lam_pipe[PIPE_RD_SIDE], &rcv_byte, sizeof(rcv_byte)) > 0);
            dr = lam_spec.d;
            status = do_naf(camac_fd, lam_spec.N, lam_spec.A, lam_spec.F, &dr);
            printf("# %s LAM, NAF(%d,%d,%d,%d): status:%c%c, dr=%d/0x%x/%s%o\n",
                   strcurtime(),
                   lam_spec.N, lam_spec.A, lam_spec.F, lam_spec.d,
                   status & CAMAC_X? 'X':'-', status & CAMAC_Q? 'Q':'-',
                   dr, dr, dr!=0? "0":"", dr);
            /* LAM should be re-enabled only here, instead of sigusr_handler(),
               because otherwise SIGUSR will be sent infinitely, blocking regular
               operation. */
            camac_setlam(CAMAC_REF, lam_spec.N, 1);
            was_irq = 1;
        }
        else /* r == 0 */
            expired = 1;

        if (expired  ||  (was_irq  &&  !all)) break;
    }
    while (1);
}

static void DoIO  (const char *argv0, const char *spec)
{
  const char *p = spec;
  naf_spec_t  n;
  int         dr;
  int         status;

    /* Skip leading spaces, if any */
    while (*p != '\0'  &&  isspace(*p)) p++;

    /* An empty spec? */
    if (*p == '\0'  ||  *p == '#') return;
    
    if (strcasecmp("z", p) == 0)
    {
        //!!! Do Z
    }
    else
    {
        ParseNafSpec(argv0, spec, &n);

        dr = n.d;
        status = do_naf(camac_fd, n.N, n.A, n.F, &dr);
        printf("NAF(%d,%d,%d,%d): status:%c%c, dr=%d/0x%x/%s%o\n",
               n.N, n.A, n.F, n.d,
               status & CAMAC_X? 'X':'-', status & CAMAC_Q? 'Q':'-',
               dr, dr, dr!=0? "0":"", dr);
    }
}

int main (int argc, char *argv[])
{
  int            c;              /* Option character */
  int            err       = 0;  /* ++'ed on errors */
  char           buf[100];

    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);

    camac_fd = init_cm5307_camac();
    if (camac_fd < 0)
    {
        fprintf(stderr, "%s: init_cm5307_camac()=%d, err=%s\n",
                argv[0], camac_fd, strerror(errno));
        return 1;
    }
    
    while ((c = getopt(argc, argv, "hi:")) > 0)
    {
        switch (c)
        {
            case 'h': goto PRINT_HELP;

            case 'i':
                ParseNafSpec(argv[0], optarg, &lam_spec);
                if (pipe(lam_pipe) < 0)
                {
                    fprintf(stderr, "%s: pipe(): %s\n", argv[0], strerror(errno));
                    exit(2);
                }
                if (set_fd_flags(lam_pipe[PIPE_RD_SIDE], O_NONBLOCK, 1) < 0  ||
                    set_fd_flags(lam_pipe[PIPE_WR_SIDE], O_NONBLOCK, 1) < 0)
                {
                    fprintf(stderr, "%s: set_fd_flags(): %s\n", argv[0], strerror(errno));
                    exit(2);
                }
                signal(SIGUSR1, sigusr_handler);
                camac_setlam(CAMAC_REF, lam_spec.N, 1);
                break;

            case '?':
            default:
                err++;
        }
    }

    if (err) goto ADVICE_HELP;

    for (;  optind < argc;  optind++)
    {
        if      (argv[optind][0] == '-')
        {
            while (fgets(buf, sizeof(buf) - 1, stdin) != NULL)
            {
                buf[sizeof(buf) - 1] = '\0';
                if (buf[0] == ':')
                    DoWait(argv[0], buf);
                else
                    DoIO  (argv[0], buf);
            }
        }
        else if (argv[optind][0] == ':'  ||  argv[optind][0] == '.')
            DoWait(argv[0], argv[optind]);
        else
            DoIO  (argv[0], argv[optind]);
    }
    
    return 0;

 ADVICE_HELP:
    fprintf(stderr, "Try `%s -h' for more information.\n", argv[0]);
    exit(1);

 PRINT_HELP:
    fprintf(stderr, "Usage: %s [options] COMMAND...\n", argv[0]);
    fprintf(stderr, "    -h               display this help and exit\n");
    fprintf(stderr, "    -i N,A,F[=DATA]  watch for LAM\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  COMMAND can be either\n");
    fprintf(stderr, "    z\n");
    fprintf(stderr, "  or\n");
    fprintf(stderr, "    N,A,F[=DATA]\n");
    fprintf(stderr, "  or\n");
    fprintf(stderr, "    :[milliseconds-to-wait]\n");
    exit(0);
}
