#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "misc_macros.h"
#include "misclib.h"
#include "timeval_utils.h"
#include "cxscheduler.h"

#include "cx.h"
#include "cda.h"

#include "console_cda_util.h"

#ifdef BUILTINS_DECLARATION_H_FILE
  #include BUILTINS_DECLARATION_H_FILE
#endif /* BUILTINS_DECLARATION_H_FILE */


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;
    util_refrec_t  ur;
    int            is_trgr;
} refrec_t;

static refrec_t *refrecs_list        = NULL;
static int       refrecs_list_allocd = 0;

// GetRefRecSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, RefRec, refrec_t,
                                 refrecs, in_use, 0, 1,
                                 0, 10, 1000,
                                 , , void)

static void RlsRefRecSlot(int rn)
{
  refrec_t *rp = AccessRefRecSlot(rn);

    safe_free(rp->ur.spec);
    rp->in_use = 0;
}

static int rp2rn(refrec_t *rp)
{
    return rp - refrecs_list;
}

//////////////////////////////////////////////////////////////////////

static FILE          *outfile         = NULL;

static int            option_noexpand = 0;
static char          *option_baseref  = NULL;
static char          *option_defpfx   = NULL;
static int            option_help     = 0;
static int            option_headers  = 0;
static const char    *option_outfile  = NULL;
static int            option_period   = 0;
static int            option_times    = 0;
static int            option_relative = 0;
static double         option_timelim  = 0;

static cda_context_t  the_cid;
static int            nrefs          = 0;

static struct timeval time1st;
static int            time1st_got    = 0;

static int            times_done;


static int data_printer(refrec_t *rp, void *privptr)
{
    fprintf(outfile, " ");
    PrintDatarefData(outfile, &(rp->ur), UTIL_PRINT_PARENS|UTIL_PRINT_QUOTES);

    return 0;
}
static void PrintAllData(void)
{
  struct timeval timenow;
  struct timeval timeage; // "Time Age", or akin "mileage"

    gettimeofday(&timenow, NULL);
    if (!time1st_got)
    {
        time1st     = timenow;
        time1st_got = 1;
    }
    timeval_subtract(&timeage, &timenow, &time1st);

    fprintf(outfile, "%lu.%03d %s %7lu.%03d ",
            (long) timenow.tv_sec, (int)(timenow.tv_usec / 1000),
            stroftime_msc(&timenow, "-"),
            (long) timeage.tv_sec, (int)(timeage.tv_usec / 1000));
    ForeachRefRecSlot(data_printer, NULL);
    fprintf(outfile, "\n");
    
    if (option_times > 0)
    {
        times_done++;
        if (times_done >= option_times) exit(EC_OK);
    }
}

static void ProcessContextEvent(int            uniq,
                                void          *privptr1,
                                cda_context_t  cid,
                                int            reason,
                                int            info_int,
                                void          *privptr2)
{
}

static void ProcessDatarefEvent(int            uniq,
                                void          *privptr1,
                                cda_dataref_t  ref,
                                int            reason,
                                void          *info_ptr,
                                void          *privptr2)
{
  int            rn = ptr2lint(privptr2);
  refrec_t      *rp = AccessRefRecSlot(rn);
  const char    *src_p;

    if      (reason == CDA_REF_R_RSLVSTAT)
    {
        if (ptr2lint(info_ptr) == CDA_RSLVSTAT_NOTFOUND)
        {
            if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";
            if (option_relative)                        src_p = rp->ur.spec;
            fprintf(stderr, "%s channel \"%s\" not found\n",
                    strcurtime(), src_p);
        }
    }
    else if (reason == CDA_REF_R_UPDATE)
    {
        if (rp->is_trgr) PrintAllData();
    }
}

static void RememberOneChan(const char    *argv0,
                            const char    *name,
                            util_refrec_t *urp,                   
                            void          *privptr)
{                                                                 
  int            rn;
  refrec_t      *rp;                                              
                                                                  
    rn = GetRefRecSlot();
    if (rn < 0)
    {
        fprintf(stderr, "%s %s: GetRefRecSlot(): %s\n",
                strcurtime(), argv0, strerror(errno));
        exit(EC_ERR);
    }

    rp = AccessRefRecSlot(rn);
    rp->ur = *urp;
    rp->ur.spec = strdup(name);
    if (rp->ur.spec == NULL)
    {
        fprintf(stderr, "MEMORY\n");
        exit(EC_ERR);
    }
    rp->is_trgr = ptr2lint(privptr);

    nrefs++;
}
static void RememberChannel(const char *argv0, const char *command)
{
  char           chanref[CDA_PATH_MAX];
  char          *endptr;

  util_refrec_t  ur;
  int            is_trgr;

  const char    *errmsg;

    is_trgr = 0;
    if (*command == '+')
    {
        is_trgr = 1;
        command++;
    }

    ParseDatarefSpec(argv0, command,
                     &endptr, chanref, sizeof(chanref), &ur);

    if (option_noexpand)
        RememberOneChan(argv0, chanref, &ur, lint2ptr(is_trgr));
    else
    {
        errmsg = ExpandName(argv0, chanref, &ur, RememberOneChan, lint2ptr(is_trgr));
        if (errmsg != NULL)
        {
            fprintf(stderr, "%s %s: \"%s\": %s\n",
                    strcurtime(), argv0, chanref, errmsg);
            exit(EC_USR_ERR);
        }
    }
}

static int  ActivateChannel(refrec_t *rp, void *privptr)
{
  const char *argv0 = privptr;
  int         rn    = rp2rn(rp);

    rp->ur.ref = cda_add_chan(the_cid,
                              option_baseref,
                              rp->ur.spec, rp->ur.options, rp->ur.dtype, rp->ur.n_items,
                              0, NULL, NULL);
    if (rp->ur.ref == CDA_DATAREF_ERROR)
    {
        fprintf(stderr, "%s %s: cda_add_chan(\"%s\"): %s\n",
                strcurtime(), argv0, rp->ur.spec, cda_last_err());
        /* Note: we do NOT exit(EC_ERR) and allow other references to proceed */

        RlsRefRecSlot(rn);
        nrefs--;
    }
    cda_add_dataref_evproc(rp->ur.ref,
                           CDA_REF_EVMASK_UPDATE | CDA_REF_EVMASK_RSLVSTAT,
                           ProcessDatarefEvent, lint2ptr(rn));

    return 0;
}

static void ReadFromFile(const char *argv0, const char *filename)
{
  FILE *fp;
  char  line[1000];
  char *p;

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "%s %s: fopen(\"%s\"): %s\n",
                strcurtime(), argv0, filename, strerror(errno));
        exit(EC_ERR);
    }

    while (fgets(line, sizeof(line) - 1, fp) != NULL)
    {
        // Rtrim
        for (p = line + strlen(line) - 1;
             p > line  &&  (*p == '\n'  ||  *p == '\r'  ||  isspace(*p));
             *p-- = '\0');
        // Ltrim
        for (p = line; *p != '\0'  &&  isspace(*p);  p++);

        if (*p != '\0'  &&  *p != '#') RememberChannel(argv0, line);
    }

    fclose(fp);
}

static void trigger_proc(int uniq, void *privptr1,
                         sl_tid_t tid,                   void *privptr2)
{
    PrintAllData();
    sl_enq_tout_after(0, NULL,
                      option_period*1000,
                      trigger_proc, NULL);
}

static void finish_proc(int uniq, void *privptr1,
                        sl_tid_t tid,                   void *privptr2)
{
    sl_break();
}

static int header_printer(refrec_t *rp, void *privptr)
{
    fprintf(outfile, " ");
    if (reprof_cxdtype(rp->ur.dtype) == CXDTYPE_REPR_FLOAT) 
        fprintf(outfile, "%s:", rp->ur.dpyfmt);
    fprintf    (outfile, "%s",  rp->ur.spec);

    return 0;
}
int main(int argc, char *argv[])
{
  int            c;
  int            err = 0;

  int            nsids;
  int            nth;

  struct timeval  now;
  struct timeval  timelim;
  double          dummy;

    set_signal(SIGPIPE, SIG_IGN);

#ifdef BUILTINS_REGISTRATION_CODE
    BUILTINS_REGISTRATION_CODE
#endif /* BUILTINS_REGISTRATION_CODE */

    while ((c = getopt(argc, argv, "1b:d:f:hHp:o:rt:T:")) != EOF)
        switch (c)
        {
            case '1':
                option_noexpand = 1;
                break;

            case 'b':
                option_baseref  = optarg;
                break;

            case 'd':
                option_defpfx   = optarg;
                break;

            case 'f':
                ReadFromFile(argv[0], optarg);
                break;

            case 'h':
                option_help     = 1;
                break;

            case 'H':
                option_headers  = 1;
                break;

            case 'o':
                option_outfile  = optarg;
                break;

            case 'p':
                option_period   = atoi(optarg);
                break;

            case 'r':
                option_relative = 1;
                break;

            case 't':
                option_times    = atoi(optarg);
                break;

            case 'T':
                option_timelim  = atof(optarg);
                break;

            case ':':
            case '?':
                err++;
        }

    if (option_help)
    {
        printf("Usage: %s [OPTIONS] CHANNEL {CHANNEL...}\n"
               "\n"
               "CHANNEL may have '+' prefix, meaning it works as trigger\n"
               "        (for full CHANNEL syntax see 'cdaclient -hh')\n"
               "\n"
               "Options:\n"
               "  -1          -- do NOT expand {} and <> in channel names\n"
               "  -b BASEREF\n"
               "  -f FILENAME -- read list of channels from FILENAME (one per line)\n"
               "  -H          -- print line of headers at start\n"
               "  -p PERIOD   -- print all data every PERIOD milliseconds\n"
               "  -o OUTFILE  -- send output to OUTFILE\n"
               "  -r          -- ignored (for compatibility)\n"
               "  -t TIMES    -- how many times to print data\n"
               "  -T DURATION -- how many seconds to run\n"
               "  -h  show this help\n",
               argv[0]);
        exit(EC_HELP);
    }

    if (err)
    {
        fprintf(stderr, "Try '%s -h' for help.\n", argv[0]);
        exit(EC_USR_ERR);
    }

    if (option_outfile != NULL  &&  *option_outfile != '\0'  &&
        strcmp(option_outfile, "-") != 0)
    {
        outfile = fopen(option_outfile, "w");
        if (outfile == NULL)
        {
            fprintf(stderr, "%s %s: can't open \"%s\" for writing: %s\n",
                    strcurtime(), argv[0], option_outfile, strerror(errno));
            exit(EC_ERR);
        }
    }
    if (outfile == NULL) outfile = stdout;
    /* Make outfile ALWAYS line-buffered */
    setvbuf(outfile, NULL, _IOLBF, 0);

    for (;  optind < argc;  optind++)
        RememberChannel(argv[0], argv[optind]);

    //////////////////////////////////////////////////////////////////

    the_cid = cda_new_context(0, NULL, option_defpfx, 0, argv[0],
                              CDA_CTX_EVMASK_CYCLE, ProcessContextEvent, NULL);
    if (the_cid == CDA_CONTEXT_ERROR)
    {
        fprintf(stderr, "%s %s: cda_new_context(\"%s\"): %s\n",
                strcurtime(), argv[0],
                option_defpfx != NULL? option_defpfx : "",
                cda_last_err());
        exit(EC_ERR);
    }
    
    ForeachRefRecSlot(ActivateChannel, argv[0]);
    if (nrefs == 0)
    {
        fprintf(stderr, "%s %s: no channels to work with (try '-h' for help)\n",
                strcurtime(), argv[0]);
        exit(EC_USR_ERR);
    }

    if (0/*print_srvlist*/)
    {
        nsids = cda_status_srvs_count(the_cid);
        fprintf(outfile, "servers[%d]={", nsids);
        for (nth = 0;  nth < nsids;  nth++)
            fprintf(outfile, "%s[%d] \"%s\"",
                    nth == 0? "" : ", ",
                    nth, cda_status_srv_name(the_cid, nth));
        fprintf(outfile, "}\n");
    }

    if (option_headers)
    {
        fprintf(outfile, "#Time(s-01.01.1970) YYYY-MM-DD-HH:MM:SS.mss secs-since0");
        ForeachRefRecSlot(header_printer, NULL);
        fprintf(outfile, "\n");
    }

    if (option_timelim > 0)
    {
        timelim.tv_sec  = option_timelim;
        timelim.tv_usec = modf(option_timelim, &dummy) * 1000000;
        gettimeofday(&now, NULL);
        timeval_add(&timelim, &now, &timelim);
        sl_enq_tout_at(0, NULL, &timelim, finish_proc, NULL);
    }
    if (option_period > 0)
        sl_enq_tout_after(0, NULL,
                          option_period*1000,
                          trigger_proc, NULL);

    sl_main_loop();
    
    return EC_OK;
}
