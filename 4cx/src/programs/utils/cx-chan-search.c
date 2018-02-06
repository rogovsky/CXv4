#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cx.h"
#include "cxlib.h"


//////////////////////////////////////////////////////////////////////

enum
{
    EC_OK      = 0,
    EC_HELP    = 1,
    EC_USR_ERR = 2,
    EC_ERR     = 3,
};

static int   option_help    = 0;
static int   option_numeric = 0;

static char *chan_name;
static int   seeker;

static void evproc(int uniq, void *privptr1,
                   int cd, int reason, const void *info,
                   void *privptr2)
{
  cx_srch_info_t *sip = info;
  const char     *srv;
  in_addr_t       a;
  struct hostent *hp;

    if (option_numeric)
        srv = sip->srv_addr;
    else
    {
        /* For some reason, the gethostbyname("aaa.bbb.ccc.ddd") returns
           numeric address (the same "aaa.bbb.ccc.ddd"), while gethostbyaddr()
           gives a symbolic name. */
#if 1
        a = inet_addr(sip->srv_addr);
        hp = gethostbyaddr(&a, 4, AF_INET);
////        fprintf(stderr, "HP=%p, h_name=%s\n", hp, hp!=NULL? hp->h_name : "NULL");
#else
        hp = gethostbyname(sip->srv_addr);
////        fprintf(stderr, "hp=%p, h_name=%s\n", hp, hp!=NULL? hp->h_name : "NULL");
#endif
        if (hp == NULL)
            srv = sip->srv_addr;
        else
            srv = hp->h_name;
    }

    fprintf(stdout, "INFO: \"%s\" param1=%d param2=%d %s:%d\n",
            sip->name, sip->param1, sip->param2, srv, sip->srv_n);
}

int main(int argc, char *argv[])
{
  int   c;
  int   err = 0;
  int   saved_errno;
  int   r;

    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);

    set_signal(SIGPIPE, SIG_IGN);

    while ((c = getopt(argc, argv, "hn")) != EOF)
        switch (c)
        {
            case 'h':
                option_help = 1;
                break;

            case 'n':
                option_numeric = 1;
                break;

            case ':':
            case '?':
                err++;
        }

    if (option_help)
    {
        printf("Usage: %s [OPTIONS] CHANNEL_NAME\n"
               "    -n  don't resolve names\n"
               "    -h  show this help\n",
               argv[0]);
        exit(EC_HELP);
    }

    if (err)
    {
        fprintf(stderr, "Try '%s -h' for help.\n", argv[0]);
        exit(EC_USR_ERR);
    }

    if (optind >= argc)
    {
        fprintf(stderr, "%s: missing CHANNEL_NAME.\n", argv[0]);
        exit(EC_USR_ERR);
    }

    chan_name = argv[optind++];

    //////////////////////////////////////////////////////////////////

    seeker = cx_seeker(0, NULL, argv[0], NULL, evproc, NULL);
    if (seeker < 0)
    {
        saved_errno = errno;
        fprintf(stderr, "%s %s: cx_seeker(): %s\n",
                strcurtime(), argv[0], cx_strerror(saved_errno));
        exit(EC_ERR);
    }

    r = cx_begin(seeker); // fprintf(stderr, "r=%d <%s>\n", r, cx_strerror(errno));
    r = cx_srch (seeker, chan_name, 1234, 5678);  //fprintf(stderr, "r=%d <%s>\n", r, cx_strerror(errno));
    r = cx_run  (seeker); //fprintf(stderr, "r=%d <%s>\n", r, cx_strerror(errno));

    cx_close(seeker);
    
    return EC_OK;
}