#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#include "misc_macros.h"
#include "misclib.h"
#include "timeval_utils.h"

#include "cankoz_numbers.h"
#include "cankoz_strdb.h"

#ifndef CANHAL_FILE_H
  #error The "CANHAL_FILE_H" macro is undefined
#else
  #include CANHAL_FILE_H
#endif


//////////////////////////////////////////////////////////////////////

static int  CanutilOpenDev  (const char *argv0,
                             const char *linespec, const char *baudspec)
{
  int   fd;
  int   minor;
  int   speed_n;
  char *err;
    
    if (linespec[0] == 'c'  &&  linespec[1] == 'a'  &&  linespec[2] == 'n')
        linespec += 3;
    minor = strtol(linespec, &err, 10);
    if (err == linespec  ||  *err != '\0')
    {
        fprintf(stderr, "%s: bad CAN_LINE spec \"%s\"\n", argv0, linespec);
        exit(1);
    }

    if (baudspec == NULL) baudspec = "125";
    if      (strcasecmp(baudspec, "125")  == 0) speed_n = 0;
    else if (strcasecmp(baudspec, "250")  == 0) speed_n = 1;
    else if (strcasecmp(baudspec, "500")  == 0) speed_n = 2;
    else if (strcasecmp(baudspec, "1000") == 0) speed_n = 3;
    else
    {
        fprintf(stderr, "%s: bad speed spec \"%s\" (allowed are 125, 250, 500, 1000)\n",
                argv0, baudspec);
        exit(1);
    }

    fd = canhal_open_and_setup_line(minor, speed_n, &err);
    if (fd < 0)
    {
        fprintf(stderr, "%s: unable to open line %d: %s: %s\n",
                argv0, minor, err, strerror(errno));
        exit(2);
    }
    
    return fd;
}

static void CanutilCloseDev (int fd)
{
    canhal_close_line(fd);
}

static void CanutilReadFrame(int fd, int *id_p, int *dlc_p, uint8 *data)
{
  int     r;
  fd_set  fds;                                                    \
  char   *err;
    
    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        r = select(fd + 1, &fds, NULL, NULL, NULL);
        if (r == 1)
        {
            r = canhal_recv_frame(fd, id_p, dlc_p, data);
            if (r != CANHAL_OK)
            {
                if      (r == CANHAL_ZERO)   err = "ZERO";
                else if (r == CANHAL_BUSOFF) err = "BUSOFF";
                else                         err = strerror(errno);
                fprintf(stderr, "%s: recv_frame: %s\n",
                        "argv0", err);
                exit (2);
            }
            return;
        }
    }
}

static void CanutilSendFrame(int fd, int id,    int  dlc,   uint8 *data)
{
  int     r;
  char   *err;
    
    r = canhal_send_frame(fd, id, dlc, data);
    if (r != CANHAL_OK)
    {
        if      (r == CANHAL_ZERO)   err = "ZERO";
        else if (r == CANHAL_BUSOFF) err = "BUSOFF";
        else                         err = strerror(errno);
        fprintf(stderr, "%s: send_frame(id=%d=0x%x, dlc=%d, [0]=0x%02x): %s\n",
                "argv0", id, id, dlc, 
                dlc > 0? data[0] : 0xFFFFFFFF,
                err);
        exit (2);
    }
}

//////////////////////////////////////////////////////////////////////

static const char *linespec        = NULL;
static const char *option_baudrate = NULL;
static int         option_kozak    = 0;
static int         option_hex_data = 0;
static int         option_hex_ids  = 0;
static enum
{
    TIMES_OFF,
    TIMES_REL,
    TIMES_ISO
}                  option_times    = TIMES_OFF;

static int can_fd = -1;

static const char *koz_prionames[8] = {"0", "1", "2", "3", "4", "b", "u", "r"};

static int ParseKozPrio(const char *p, char **errp)
{
  int  r = -1;
    
    if      (*p >= '0'  &&  *p <= '7') r = *p - '0';
    else if (*p == 'b')                r = 5;
    else if (*p == 'u')                r = 6;
    else if (*p == 'r')                r = 7;

    *errp = (r >= 0)? p + 1 : p;
    return r;
}

static int ParseKozResv(const char *p, char **errp)
{
    if (*p >= '0'  &&  *p <= '3')
    {
        *errp = p + 1;
        return *p - '0';
    }

    *errp = p;
    return -1;
}

static void DoSend(const char *argv0, const char *spec)
{
  const char *p = spec;
  char       *errp;

  int         id;
  int         koz_kid;
  int         koz_prio;
  int         koz_resv;
  int         dlc;
  int         datum;
  uint8       data[8];
    
    /* Skip "@TIME ", if present */
    if (*p == '@')
        while (*p != '\0'  &&  !isspace(*p)) p++;

    /* Skip leading spaces, if any */
    while (*p != '\0'  &&  isspace(*p)) p++;

    /* An empty spec? */
    if (*p == '\0'  ||  *p == '#') return;
    
    /* Is it a kKID/PRIO[.RESV], or just ID? */
    if (tolower(*p) == 'k')
    {
        p++; // Skip 'k'

        koz_resv = 0;
        if (
            ( // KID
             (koz_kid  = strtol(p, &errp, 0)),
             (errp == p  ||  *errp != '/')
            )
            ||
            ( // /PRIO
             (p = errp + 1),
             ((koz_prio = ParseKozPrio(p, &errp)) < 0)
            )
            ||
            ( // [.RESV]
             ((p = errp), (*p == '.')) &&
             (
              (p++),
              ((koz_resv = ParseKozResv(p, &errp)) < 0)
             )
            )
           )
        {
            fprintf(stderr, "%s: bad kozak-ID spec in \"%s\"\n", argv0, spec);
            exit(1);
        }
        p = errp;

        id = (koz_resv & 3) | ((koz_kid & 63) << 2) | ((koz_prio & 7) << 8);
    }
    else
    {
        id = strtol(p, &errp, 0);
        if (errp == p)
        {
            fprintf(stderr, "%s: bad ID spec in \"%s\"\n", argv0, spec);
            exit(1);
        }
        p = errp;
    }

    if (*p != ':')
    {
        fprintf(stderr, "%s: ':' expected after ID in \"%s\"\n", argv0, spec);
        exit(1);
    }
    p++;

    for (dlc = 0;  ;)
    {
        if (*p == '\0'  ||  *p == '#'  ||  isspace(*p)) break;
        
        if (dlc >= countof(data))
        {
            fprintf(stderr, "%s: too many data bytes in \"%s\"\n", argv0, spec);
            exit(1);
        }
        
        datum = strtol(p, &errp, 0);
        if (errp == p)
        {
            fprintf(stderr, "%s: bad byte#%d spec in \"%s\"\n", argv0, dlc, spec);
            exit(1);
        }
        data[dlc++] = datum;
        p = errp;

        if (*p == ',')
            p++;
        else
            break;
    }

    CanutilSendFrame(can_fd, id, dlc, data);
}


static void DoRecv(const char *argv0, const char *spec)
{
  const char *p = spec;
  char       *errp;

  int         msecs;

  struct timeval  deadline;
  struct timeval  now;          /* The time "now" for diff calculation */
  struct timeval  timeout;      /* Timeout for select() */
  fd_set          rfds;
  int             r;

  static struct timeval time1st;
  static int            time1st_got = 0;

  int             can_id;
  int             can_dlc;
  uint8           can_data[8];
  struct timeval  timenow;
  int             koz_kid;
  int             koz_prio;
  int             koz_resv;
  int             koz_cnfm;
  int             x;
  int             id_devcode;
  int             id_hwver;
  int             id_swver;
  int             id_reason;
    
    /* Find out duration */
    p++;
    if (*p == '\0')
        msecs = -1;
    else
    {
        msecs = strtol(p, &errp, 0);
        if (errp == p)
        {
            fprintf(stderr, "%s: bad milliseconds spec in \"%s\"\n", argv0, spec);
            exit(1);
        }
        p = errp;
        
        gettimeofday(&deadline, NULL);
        timeval_add_usecs(&deadline, &deadline, msecs * 1000);
    }
    
    while (1)
    {
        if (msecs >= 0)
        {
            gettimeofday(&now, NULL);
            if (timeval_subtract(&timeout, &deadline, &now) != 0) return;
        }

        FD_ZERO(&rfds);
        FD_SET(can_fd, &rfds);
        r = select(can_fd + 1, &rfds, NULL, NULL, msecs < 0? NULL : &timeout);
        if      (r == 0)
            return;
        else if (r <  0)
        {
            fprintf(stderr, "%s: select()=-1 processing \"%s\": %s\n", 
                    argv0, spec, strerror(errno));
            exit (2);
        }
        else
        {
            CanutilReadFrame(can_fd, &can_id, &can_dlc, can_data);
            gettimeofday(&timenow, NULL);
            if (!time1st_got) 
            {
                time1st     = timenow;
                time1st_got = 1;
            }
        
            if      (option_times == TIMES_REL)
            {
                timeval_subtract(&timenow, &timenow, &time1st);
                printf("@%ld.%06ld ", (long)timenow.tv_sec, (long)timenow.tv_usec);
            }
            else if (option_times == TIMES_ISO)
            {
                printf("@%s.%06ld ", stroftime(timenow.tv_sec, "-"), (long)timenow.tv_usec);
            }

            koz_kid  = (can_id >> 2) & 63;
            koz_prio = (can_id >> 8) & 7;
            koz_resv = can_id & 3;
            koz_cnfm = (can_id == (can_id & 0x7FF));
            if (option_kozak  &&  koz_cnfm)
                printf("%sk%d/%s.%d", koz_kid < 10? " ":"", koz_kid, koz_prionames[koz_prio], koz_resv);
            else
                printf(option_hex_ids? "0x%X" : "%u", can_id);
            printf(":");

            for (x = 0;  x < can_dlc;  x++)
                printf(option_hex_data? "%s0x%02X" : "%s%d",
                       x == 0? "" : ",",
                       can_data[x]);

            if (!koz_cnfm)
            {
                printf("# Non-kozak-conformant");
            }
            else if (option_kozak  &&  koz_prio    == CANKOZ_PRIO_REPLY  &&
                     can_dlc > 0   &&  can_data[0] == CANKOZ_DESC_GETDEVATTRS)
            {
                id_devcode = can_dlc > 1? can_data[1] : -1;
                id_hwver   = can_dlc > 2? can_data[2] : -1;
                id_swver   = can_dlc > 3? can_data[3] : -1;
                id_reason  = can_dlc > 4? can_data[4] : -1;

                printf(" # %2d: DevCode=%d:%s HWver=%d SWver=%d Reason=%d:%s",
                       koz_kid,
                       id_devcode,
                       id_devcode >= 0  &&  id_devcode < countof(cankoz_devtypes)  &&  cankoz_devtypes[id_devcode] != NULL?
                           cankoz_devtypes[id_devcode] : "???",
                       id_hwver,
                       id_swver,
                       id_reason,
                       id_reason  >= 0  &&  id_reason  < countof(cankoz_GETDEVATTRS_reasons)?
                           cankoz_GETDEVATTRS_reasons[id_reason] : "???"
                       );
            }

            printf("\n");
        }
    }
}


int main (int argc, char *argv[])
{
  int            c;              /* Option character */
  int            err       = 0;  /* ++'ed on errors */
  char           buf[100];

    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);
    
    while ((c = getopt(argc, argv, "b:hkxXtT")) > 0)
    {
        switch (c)
        {
            case 'b': option_baudrate = optarg;       break;
            case 'h': goto PRINT_HELP;
            case 'k': option_kozak    = 1;            break;
            case 'x': option_hex_data = 1;            break;
            case 'X': option_hex_ids  = 1;            break;
            case 't': option_times    = TIMES_REL;    break;
            case 'T': option_times    = TIMES_ISO;    break;
            
            case '?':
            default:
                err++;
        }
    }

    if (err) goto ADVICE_HELP;

    if (optind >= argc)
    {
        fprintf(stderr, "%s: CAN_LINE is required\n", argv[0]);
        goto ADVICE_HELP;
    }
    linespec = argv[optind++];

    //////////////////////////////
    can_fd = CanutilOpenDev(argv[0], linespec, option_baudrate);

    if (optind >= argc)
        DoRecv(argv[0], ":");
    for (;  optind < argc;  optind++)
    {
        if      (argv[optind][0] == '-')
        {
            while (fgets(buf, sizeof(buf) - 1, stdin) != NULL)
            {
                buf[sizeof(buf) - 1] = '\0';
                if (buf[0] == ':')
                    DoRecv(argv[0], buf);
                else
                    DoSend(argv[0], buf);
            }
        }
        else if (argv[optind][0] == ':')
            DoRecv(argv[0], argv[optind]);
        else
            DoSend(argv[0], argv[optind]);
    }

    CanutilCloseDev(can_fd);
    
    return 0;

 ADVICE_HELP:
    fprintf(stderr, "Try `%s -h' for more information.\n", argv[0]);
    exit(1);

 PRINT_HELP:
    fprintf(stderr, "Usage: %s [options] CAN_LINE COMMAND...\n", argv[0]);
    fprintf(stderr, "    -b BAUDS  set baudrate (125/250/500/1000)\n");
    fprintf(stderr, "    -h        display this help and exit\n");
    fprintf(stderr, "    -k        print IDs in kozak notation\n");
    fprintf(stderr, "    -x        print data in hex\n");
    fprintf(stderr, "    -X        print packet IDs in hex\n");
    fprintf(stderr, "    -t        print relative timestamps\n");
    fprintf(stderr, "    -T        print ISO8601 timestamps\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  COMMAND can be either\n");
    fprintf(stderr, "    ID:byte0,byte1,...\n");
    fprintf(stderr, "  or\n");
    fprintf(stderr, "    kKID/PRIO[.RESV]:byte0,byte1,...\n");
    fprintf(stderr, "  or\n");
    fprintf(stderr, "    :[milliseconds-to-wait]\n");
    exit(0);
}
