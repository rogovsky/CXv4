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


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;
    util_refrec_t  ur;
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
    /*!!! Note: we do NOT safe_free(rp->ur.databuf), because it can be
      shared between multiple channels, and we do NOT duplicate it in 
      RememberOneChan() */
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
static const char    *option_comment  = NULL;
static int            option_help     = 0;
static const char    *option_outfile  = NULL;
static int            option_relative = 0;
static double         option_timelim  = 0;

static cda_context_t  the_cid;
static int            nrefs     = 0;
static int            num2read  = 0;


static void ProcessContextEvent(int            uniq,
                                void          *privptr1,
                                cda_context_t  cid,
                                int            reason,
                                int            info_int,
                                void          *privptr2)
{
////fprintf(stderr, "%s  %s(cid=%d, reason=%d, info_int=%d)\n", strcurtime(), __FUNCTION__, cid, reason, info_int);
    switch (reason)
    {
    }
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
  void          *buf;

    if      (reason == CDA_REF_R_RSLVSTAT)
    {
        if (ptr2lint(info_ptr) == 0)
        {
            if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";
            if (option_relative)                        src_p = rp->ur.spec;
            fprintf(stderr, "%s channel \"%s\" not found\n",
                    strcurtime(), src_p);

            if (rp->ur.rd_req)
            {
                rp->ur.rd_req = 0;
                num2read--;
            }
        }
    }
    else if (reason == CDA_REF_R_UPDATE)
    {
        if      (rp->ur.rd_req)
        {
            rp->ur.rd_req = 0;
            num2read--;
        }
    }

    if (num2read == 0) sl_break();
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

    if (rp->ur.rd_req) num2read++;

    nrefs++;
}
static void RememberChannel(const char *argv0, const char *command)
{
  char           chanref[CDA_PATH_MAX];
  char          *endptr;

  util_refrec_t  ur;

  const char    *errmsg;

    bzero(&ur, sizeof(ur));

    /* Channel name */
    ParseDatarefSpec(argv0, command,
                     &endptr, chanref, sizeof(chanref), &ur);

    ur.rd_req = 1;

    if (option_noexpand)
        RememberOneChan(argv0, chanref, &ur, NULL);
    else
    {
        errmsg = ExpandName(argv0, chanref, &ur, RememberOneChan, NULL);
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
        fprintf(stderr, "%s %s: cda_add_dchan(\"%s\"): %s\n",
                strcurtime(), argv0, rp->ur.spec, cda_last_err());
        /* Note: we do NOT exit(EC_ERR) and allow other references to proceed */

        if (rp->ur.rd_req) num2read--;

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
        // Skip "@TIME" prefix
        if (*p == '@'  &&  isdigit(p[1]))
        {
            // Timestamp itself...
            while (*p != '\0'  &&  !isspace(*p)) p++;
            // ...spaces
            while (*p != '\0'  &&   isspace(*p)) p++;
        }

        if (*p != '\0'  &&  *p != '#') RememberChannel(argv0, p);
    }

    fclose(fp);
}

static int  SaveChannel(refrec_t *rp, void *privptr)
{
  int         rn    = rp2rn(rp);

    PrintDatarefData(outfile, &(rp->ur),
                     UTIL_PRINT_DTYPE                             |
                     UTIL_PRINT_NAME                              |
                     (option_relative ? UTIL_PRINT_RELATIVE  : 0) |
                     UTIL_PRINT_PARENS                            |
                     UTIL_PRINT_QUOTES                            |
                     UTIL_PRINT_TIMESTAMP                         |
                     UTIL_PRINT_NEWLINE);

    return 0;
}
static void SaveData(void)
{
  time_t       timenow = time(NULL);
  const char  *cp;

static const char *subsys_lp_s  = "#!SUBSYSTEM:";
static const char *crtime_lp_s  = "#!CREATE-TIME:";
static const char *comment_lp_s = "#!COMMENT:";

    fprintf(outfile, "%s %lu %s", crtime_lp_s, (unsigned long)(timenow), ctime(&timenow)); /*!: No '\n' because ctime includes a trailing one. */
    fprintf(outfile, "%s ", comment_lp_s);
    if (option_comment != NULL)
        for (cp = option_comment;  *cp != '\0';  cp++)
            fprintf(outfile, "%c", !iscntrl(*cp)? *cp : ' ');
    fprintf(outfile, "\n\n");
    ForeachRefRecSlot(SaveChannel, NULL);
}

static void finish_proc(int uniq, void *privptr1,
                        sl_tid_t tid,                   void *privptr2)
{
    sl_break();
}

int main(int argc, char *argv[])
{
  int            c;
  int            err = 0;

  struct timeval  now;
  struct timeval  timelim;
  double          dummy;

    set_signal(SIGPIPE, SIG_IGN);

    while ((c = getopt(argc, argv, "1b:C:d:f:ho:rT:")) != EOF)
        switch (c)
        {
            case '1':
                option_noexpand = 1;
                break;

            case 'b':
                option_baseref = optarg;
                break;

            case 'C':
                option_comment = optarg;
                break;

            case 'd':
                option_defpfx  = optarg;
                break;

            case 'f':
                ReadFromFile(argv[0], optarg);
                break;

            case 'h':
                option_help     = 1;
                break;

            case 'o':
                option_outfile  = optarg;
                break;

            case 'r':
                option_relative = 1;
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
               "Options:\n"
               "  -1          -- do NOT expand {} and <> in channel names\n"
               "  -b BASEREF\n"
               "  -f FILENAME -- read list of channels from FILENAME (one per line)\n"
               "  -o OUTFILE  -- send output to OUTFILE\n"
               "  -r          -- print relative channel names\n"
               "  -T DURATION -- how many seconds to run (time limit)\n"
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

    if (option_timelim > 0)
    {
        timelim.tv_sec  = option_timelim;
        timelim.tv_usec = modf(option_timelim, &dummy) * 1000000;
        gettimeofday(&now, NULL);
        timeval_add(&timelim, &now, &timelim);
        sl_enq_tout_at(0, NULL, &timelim, finish_proc, NULL);
    }

    sl_main_loop();

    SaveData();

    return EC_OK;
}
