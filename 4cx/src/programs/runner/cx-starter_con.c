#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/utsname.h>

#include "misclib.h"
#include "memcasecmp.h"

#include "cx-starter_msg.h"
#include "cx-starter_con.h"


/*********************************************************************
*  Host identity section                                             *
*********************************************************************/

static char thishostname[100];

void ObtainHostIdentity(void)
{
  struct utsname  ubuf;
    
    if (uname(&ubuf) != 0)
    {
        fprintf(stderr, "%s: uname(): %s\n", __FUNCTION__, strerror(errno));
        return;
    }

    strzcpy(thishostname, ubuf.nodename, sizeof(thishostname));
}

int IsThisHost         (const char *hostname)
{
  int         is_fqdn;
  const char *dp;

    ////fprintf(stderr, "thishost='%s', host='%s'\n", thishostname, hostname);
  
    if (thishostname[0] == '\0'  ||
        hostname == NULL  ||  hostname[0] == '\0') return 0;

    is_fqdn = (hostname[strlen(hostname) - 1] == '.');

    if (strcasecmp(hostname, thishostname) == 0) return 1;

    dp = strchr(thishostname, '.');
    if (dp != NULL  &&
        cx_strmemcasecmp(hostname, thishostname, dp - thishostname) == 0) return 1;
    if (strcasecmp(hostname, "localhost") == 0) return 1;
        
    return 0;
}


/*********************************************************************
*  Command running section                                           *
*********************************************************************/

static int is_passive = 0;

void SetPassiveness    (int option_passive)
{
    is_passive = option_passive;
}

void RunCommand        (const char *cmd)
{
  const char *prog_args[10];
  int         prog_argc;

  const char *prog_p;
  const char *colon;
  size_t      len;
  
  char        host_buf[100];

  pid_t       pid;

    reportinfo("%s(\"%s\")", __FUNCTION__, cmd);

    if (is_passive) return;
    
    prog_argc = 0;

    prog_p = cmd;
    
    if (*prog_p == '@')
    {
        prog_p++;
        colon = strchr(prog_p, ':');
        if (colon == NULL)
        {
            fprintf(stderr, "%s(): missing ':' (required for \"@host:command\") in \"%s\"\n",
                    __FUNCTION__, cmd);
            return;
        }

        len = colon - prog_p;
        if (len > sizeof(host_buf) - 1) len = sizeof(host_buf) - 1;
        memcpy(host_buf, prog_p, len);
        host_buf[len] = '\0';

        prog_p = colon + 1;

        prog_args[prog_argc++] = "/usr/bin/ssh";
        //prog_args[prog_argc++] = "-t"; // No-no-no! We do NOT want the "-t" by default!
        //prog_args[prog_argc++] = "-qtt"; // Maybe a "-qtt", but as for now, we'll better live without it
        prog_args[prog_argc++] = host_buf;
    }
    else
    {
        prog_args[prog_argc++] = "/bin/sh";
        prog_args[prog_argc++] = "-c";
    }

    ////fprintf(stderr, "\t'%s'\n", prog_p);
    prog_args[prog_argc++] = prog_p;
    prog_args[prog_argc]   = NULL;

    switch (pid = fork())
    {
        case 0:
            set_signal(SIGHUP, SIG_IGN);
            execv(prog_args[0], prog_args);
            exit(1);

        case -1:
            fprintf(stderr, "%s(): unable to fork(): %s\n",
                    __FUNCTION__, strerror(errno));
            break;

        default:
            reportinfo("PID=%ld", (long)pid);
    }
}
