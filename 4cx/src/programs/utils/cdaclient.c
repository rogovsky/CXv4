#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

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

static int            option_noexpand  = 0;
static char          *option_baseref   = NULL;
static char          *option_defpfx    = NULL;
static int            option_help      = 0;
static int            option_monitor   = 0;
static const char    *option_outfile   = NULL;
static int            option_relative  = 0;
static double         option_timelim   = 0;
static int            option_w_unbuff  = 0;

static int            print_curval     = 0;
static int            print_cur_as_new = 0;
static int            print_time       = 0;
static int            print_refn       = 0;
static int            print_dtype      = 0;
static int            print_nelems     = 0;
static int            print_name       = 0;
static int            print_parens     = 1;
static int            print_quotes     = 1;
static int            print_quants     = 0;
static int            print_timestamp  = 0;
static int            print_rflags     = 0;
static int            print_fresh_ages = 0;
static int            print_rds        = 0;
static int            print_strings    = 0;
static int            print_srvlist    = 0;
static int            print_writes     = 0;
static int            print_unserved   = 0;

static cda_context_t  the_cid;
static int            nrefs     = 0;
static int            num2read  = 0;
static int            num2write = 0;


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

static void PerformWrite(refrec_t *rp)
{
  void          *buf;
  int            r;

    buf = rp->ur.databuf;
    if (buf == NULL) buf = &(rp->ur.val2wr);
    if ((r = cda_snd_ref_data(rp->ur.ref, rp->ur.dtype, rp->ur.num2wr, buf)) >= 0)
    {
        rp->ur.wr_req = 0;
        rp->ur.wr_snt = 1;
    }
    if (print_writes)
    {
        fprintf(outfile, "# WRITE:");
        if (r >= 0) fprintf(outfile, "OK");
        else        fprintf(outfile, "ERR:%d", errno);
        fprintf(outfile, " ");
        if (print_time) fprintf(outfile, "@%s  ", strcurtime());
        if (print_name) fprintf(outfile, "%s",    rp->ur.spec);
        fprintf(outfile, "\n");
    }
}

static void FprintfDbl(FILE *fp, double v)
{
  char        buf[400]; // So that 1e308 fits
  const char *format;

    format = "%.6f";
    if      (v == 0.0)      format = "%1.1f";
    else if (v == round(v)) format = "%.1f";
    fprintf(fp, "%s", snprintf_dbl_trim(buf, sizeof(buf), format, v, 1));
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

  int            phys_count;
  double        *phys_rds;
  char           rds_str[1000];
  int            nrd;

  int            sn;
  int            was_smth;
  char          *strings [8];
  static char   *strnames[8] = {"ident",   "label",  "tip",   "comment",
                                "geoinfo", "rsrvd6", "units", "dpyfmt"};

  cx_time_t      fresh_age;
  int            fresh_age_specified;

  CxAnyVal_t     q;
  cxdtype_t      q_dtype;

    if      (reason == CDA_REF_R_RSLVSTAT)
    {
        if (ptr2lint(info_ptr) == 0)
        {
            if      (option_relative)                        src_p = rp->ur.spec;
            else if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";
            fprintf(stderr, "# %s channel \"%s\" not found\n",
                    print_time?strcurtime():"", src_p);

            if (rp->ur.rd_req)
            {
                rp->ur.rd_req = 0;
                num2read--;
            }
            if (rp->ur.wr_req  ||  rp->ur.wr_snt)
            {
                rp->ur.wr_req = 0;
                rp->ur.wr_snt = 0;
                num2write--;
            }
        }
    }
    else if (reason == CDA_REF_R_UPDATE  ||
             reason == CDA_REF_R_CURVAL  &&  print_cur_as_new)
    {
        ////fprintf(stderr, "<%s> %d nr=%d nw=%d\n", rp->ur.spec, reason, num2read, num2write);
        if      (rp->ur.rd_req)
        {
            PrintDatarefData(outfile,
                             &(rp->ur),
                             (print_time      ? UTIL_PRINT_TIME      : 0) |
                             (print_refn      ? UTIL_PRINT_REFN      : 0) |
                             (print_dtype     ? UTIL_PRINT_DTYPE     : 0) |
                             (print_nelems    ? UTIL_PRINT_NELEMS    : 0) |
                             (print_name      ? UTIL_PRINT_NAME      : 0) |
                             (print_parens    ? UTIL_PRINT_PARENS    : 0) |
                             (print_quotes    ? UTIL_PRINT_QUOTES    : 0) |
                             (option_relative ? UTIL_PRINT_RELATIVE  : 0) |
                             (print_timestamp ? UTIL_PRINT_TIMESTAMP : 0) |
                             (print_rflags    ? UTIL_PRINT_RFLAGS    : 0) |
                             UTIL_PRINT_NEWLINE);
            if (!option_monitor)
            {
                rp->ur.rd_req = 0;
                num2read--;
            }
        }
        else if (rp->ur.wr_req)
#if 1
            PerformWrite(rp);
#else
        {
            buf = rp->ur.databuf;
            if (buf == NULL) buf = &(rp->ur.val2wr);
            if (cda_snd_ref_data(rp->ur.ref, rp->ur.dtype, rp->ur.num2wr, buf) >= 0)
            {
                rp->ur.wr_req = 0;
                rp->ur.wr_snt = 1;
            }
        }
#endif
        else if (rp->ur.wr_snt)
        {
            rp->ur.wr_snt = 0;
            num2write--;
        }
    }
    else if (reason == CDA_REF_R_CURVAL)
    {
        fprintf(outfile, "# CURVAL: ");
        /* Copy from UPDATE branch above */
        PrintDatarefData(outfile,
                         &(rp->ur),
                         (print_time      ? UTIL_PRINT_TIME      : 0) |
                         (print_refn      ? UTIL_PRINT_REFN      : 0) |
                         (print_dtype     ? UTIL_PRINT_DTYPE     : 0) |
                         (print_nelems    ? UTIL_PRINT_NELEMS    : 0) |
                         (print_name      ? UTIL_PRINT_NAME      : 0) |
                         (print_parens    ? UTIL_PRINT_PARENS    : 0) |
                         (print_quotes    ? UTIL_PRINT_QUOTES    : 0) |
                         (option_relative ? UTIL_PRINT_RELATIVE  : 0) |
                         (print_timestamp ? UTIL_PRINT_TIMESTAMP : 0) |
                         (print_rflags    ? UTIL_PRINT_RFLAGS    : 0) |
                         UTIL_PRINT_NEWLINE);
    }
    else if (reason == CDA_REF_R_RDSCHG)
    {
        if      (option_relative)                        src_p = rp->ur.spec;
        else if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";

        if (cda_phys_rds_of_ref(rp->ur.ref, &phys_count, &phys_rds) < 0)
        {
            fprintf(stderr, "# %s ERROR reading RDs\n",
                    print_time?strcurtime():"");
            return;
        }
        fprintf(outfile, "# %s RDs(%s)[%d]={", 
                print_time?strcurtime():"", src_p, phys_count);
        for (nrd = 0;  nrd < phys_count; nrd++)
        {
            if (nrd > 0) fprintf(outfile, ", ");
            fprintf(outfile, "{");
            FprintfDbl(outfile, phys_rds[nrd*2 + 0]);
            fprintf(outfile, ",");
            FprintfDbl(outfile, phys_rds[nrd*2 + 1]);
            fprintf(outfile, "}");
        }
        fprintf(outfile, "}\n");
    }
    else if (reason == CDA_REF_R_STRSCHG)
    {
        if      (option_relative)                        src_p = rp->ur.spec;
        else if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";

        if (cda_strings_of_ref(rp->ur.ref,
                               strings + 0, strings + 1,
                               strings + 2, strings + 3,
                               strings + 4, strings + 5,
                               strings + 6, strings + 7) < 0)
        {
            fprintf(stderr, "# %s ERROR reading strings\n",
                    print_time?strcurtime():"");
            return;
        }
        fprintf(outfile, "# %s strings(%s)={",
                print_time?strcurtime():"", src_p);
        for (sn = 0, was_smth = 0;  sn < countof(strings);  sn++)
            if (strings[sn] != NULL)
            {
                fprintf(outfile, "%s%s:\"%s\"",
                        was_smth? ", " : "",
                        strnames[sn], strings[sn]);
                was_smth = 1;
            }
        fprintf(outfile, "}\n");
    }
    else if (reason == CDA_REF_R_FRESHCHG)
    {
        if      (option_relative)                        src_p = rp->ur.spec;
        else if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";

        fresh_age_specified = cda_fresh_age_of_ref(rp->ur.ref, &fresh_age);
        if      (fresh_age_specified < 0)
        {
            fprintf(stderr, "# %s ERROR reading fresh age\n",
                    print_time?strcurtime():"");
            return;
        }
        else if (fresh_age_specified)
            fprintf(outfile, "# %s fresh_age(%s)=%ld.%09ld\n",
                    print_time?strcurtime():"", src_p, fresh_age.sec, fresh_age.nsec);
        else
            fprintf(outfile, "# %s fresh_age(%s)=UNSPECIFIED\n",
                    print_time?strcurtime():"", src_p);
    }
    else if (reason == CDA_REF_R_QUANTCHG)
    {
        if      (option_relative)                        src_p = rp->ur.spec;
        else if (cda_src_of_ref(rp->ur.ref, &src_p) < 0) src_p = "UNKNOWN";

        if (cda_quant_of_ref(rp->ur.ref, &q, &q_dtype) < 0)
        {
            fprintf(stderr, "# %s ERROR reading quant\n",
                    print_time?strcurtime():"");
            return;
        }
        fprintf(outfile, "# %s quant(%s)=",
                print_time?strcurtime():"", src_p);

        if      (q_dtype == CXDTYPE_UNKNOWN) fprintf(outfile,  "UNKNOWN");
        else if (q_dtype == CXDTYPE_TEXT)    fprintf(outfile,  "@t:???");
        else if (q_dtype == CXDTYPE_UCTEXT)  fprintf(outfile,  "@u:???");
        else if (q_dtype == CXDTYPE_SINGLE) {fprintf(outfile,  "@s:");
                                             FprintfDbl(outfile, q.f32);}
        else if (q_dtype == CXDTYPE_DOUBLE) {fprintf(outfile,  "@d:");
                                             FprintfDbl(outfile, q.f64);}
        else if (q_dtype == CXDTYPE_INT8)    fprintf(outfile,  "@b:%d", q.i8);
        else if (q_dtype == CXDTYPE_INT16)   fprintf(outfile,  "@h:%d", q.i16);
        else if (q_dtype == CXDTYPE_INT32)   fprintf(outfile,  "@i:%d", q.i32);
        else if (q_dtype == CXDTYPE_INT64)   fprintf(outfile,  "@q:%lld", q.i64);
        else if (q_dtype == CXDTYPE_UINT8)   fprintf(outfile, "@+b:%u", q.u8);
        else if (q_dtype == CXDTYPE_UINT16)  fprintf(outfile, "@+h:%u", q.u16);
        else if (q_dtype == CXDTYPE_UINT32)  fprintf(outfile, "@+i:%u", q.u32);
        else if (q_dtype == CXDTYPE_UINT64)  fprintf(outfile, "@+q:%llu", q.u64);
        else                                 fprintf(outfile, "?");

        fprintf(outfile, "\n");
    }

    if (num2read == 0  &&  num2write == 0) sl_break();
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
    if (rp->ur.wr_req) num2write++;

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
    if (strcmp(chanref, "-") == 0) return;

    /* Spaces or '=' sign */
    while (*endptr != '\0'  &&
           (isspace(*endptr)  /*||  *endptr == '='*/))
           endptr++;

    if (*endptr != '\0'  &&  *endptr != '#')
    {
        ParseDatarefVal(argv0, endptr, &endptr, &ur);
        ur.wr_req = 1;
    }
    else
    {
        ur.rd_req = 1;
    }

    if (option_noexpand)
        RememberOneChan(argv0, chanref, &ur, NULL);
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
        if (rp->ur.wr_req) num2write--;

        RlsRefRecSlot(rn);
        nrefs--;
        return 0;  // Despite an error, we MUST return 0, because it is an iterator (with non-0 further channels would be skipped)
    }
    cda_add_dataref_evproc(rp->ur.ref,
                           CDA_REF_EVMASK_UPDATE | CDA_REF_EVMASK_RSLVSTAT |
                           (CDA_REF_EVMASK_CURVAL   * print_curval)     |
                           (CDA_REF_EVMASK_RDSCHG   * print_rds)        |
                           (CDA_REF_EVMASK_STRSCHG  * print_strings)    |
                           (CDA_REF_EVMASK_FRESHCHG * print_fresh_ages) |
                           (CDA_REF_EVMASK_QUANTCHG * print_quants),
                           ProcessDatarefEvent, lint2ptr(rn));

    if (option_w_unbuff  &&  rp->ur.wr_req) PerformWrite(rp);

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

static int  PrintOneUnserved(refrec_t *rp, void *privptr)
{
  char  c;

    if      (rp->ur.rd_req) c = 'r';
    else if (rp->ur.wr_req) c = 'w';
    else if (rp->ur.wr_snt) c = 'c';
    else return 0;

    fprintf(outfile, "#  %c %s\n", c, rp->ur.spec);
    return 0;
}
static void finish_proc(int uniq, void *privptr1,
                        sl_tid_t tid,                   void *privptr2)
{
    if (print_unserved)
    {
        fprintf(outfile, "# List of unserved channels upon timeout\n");
        fprintf(outfile, "# (r: read, w: write, c: unconfirmed write):\n");
        ForeachRefRecSlot(PrintOneUnserved, NULL);
        fprintf(outfile, "# ----\n");
    }
    sl_break();
}

int main(int argc, char *argv[])
{
  int            c;
  int            err = 0;

  const char    *cp;
  int            val2set;

  int            nsids;
  int            nth;

  struct timeval  now;
  struct timeval  timelim;
  double          dummy;

    set_signal(SIGPIPE, SIG_IGN);

    while ((c = getopt(argc, argv, "1b:d:D:f:hmo:rT:w")) != EOF)
        switch (c)
        {
            case '1':
                option_noexpand = 1;
                break;

            case 'b':
                option_baseref = optarg;
                break;

            case 'd':
                option_defpfx  = optarg;
                break;

            case 'D':
                for (cp = optarg, val2set = 1;
                     *cp != '\0';
                     cp++)
                    switch (*cp)
                    {
                        case '+': val2set = 1;                break;
                        case '-': val2set = 0;                break;
                        case 'c': print_curval     = val2set; break;
                        case 'C': print_cur_as_new = val2set; break;
                        case 'T': print_time       = val2set; break;
                        case 'r': print_refn       = val2set; break;
                        case 'R': print_rds        = val2set; break;
                        case 'd': print_dtype      = val2set; break;
                        case 'N': print_nelems     = val2set; break;
                        case 'n': print_name       = val2set; break;
                        case 'o': print_parens     = val2set; break;
                        case 'q': print_quotes     = val2set; break;
                        case 't': print_timestamp  = val2set; break;
                        case 'f': print_rflags     = val2set; break;
                        case 'F': print_fresh_ages = val2set; break;
                        case 's': print_srvlist    = val2set; break;
                        case 'S': print_strings    = val2set; break;
                        case 'Q': print_quants     = val2set; break;
                        case 'w': print_writes     = val2set; break;
                        case '/': print_unserved   = val2set; break;
                        default:
                            fprintf(stderr, "%s %s: unknown print-option '%c'\n",
                                    strcurtime(), argv[0], *cp);
                            exit(EC_USR_ERR);
                    }
                break;

            case 'f':
                ReadFromFile(argv[0], optarg);
                break;

            case 'h':
                option_help++;
                break;

            case 'm':
                option_monitor  = 1;
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

            case 'w':
                option_w_unbuff = 1;
                break;

            case ':':
            case '?':
                err++;
        }

    // '-C' implies '-c'
    if (print_cur_as_new) print_curval = 1;

    if (option_help)
    {
        printf("Usage: %s [OPTIONS] {COMMANDS}\n"
               "\n"
               "COMMAND is either CHANNEL name (read) or CHANNEL=VALUE (write)\n"
               "\n"
               "Options:\n"
               "  -1          -- do NOT expand {} and <> in channel names\n"
               "  -b BASEREF\n"
               "  -D DISPMODE -- display mode control flags ('-hh' for details)\n"
               "  -f FILENAME -- read list of channels from FILENAME (one per line)\n"
               "  -m          -- monitor mode (run infinitely)\n"
               "  -o OUTFILE  -- send output to OUTFILE\n"
               "  -r          -- print relative channel names\n"
               "  -T DURATION -- how many seconds to run (time limit)\n"
               "  -w          -- send writes to cda immediately (w/o builtin buffering)\n"
               "  -h  show this help; -hh shows more details\n",
               argv[0]);
        if (option_help > 1)
        printf("\n"
               "DISPMODE flags consist of one or more characters:\n"
               " c  print Current values (prefixed with '# CURVAL:')\n"
               " C  print Current values as new (useful for a single chan w/o '-m')\n"
               " T  prefix all events with current Times\n"
               " r  print cda Ref ids\n"
               " R  print {R,d}s\n"
               " d  print dtypes\n"
               " N  print Nelems (useful for vector and text channels)\n"
               " n  print channel Name\n"
               " o  print parens around vector values (default=yes; \"-D-o\" for no)\n"
               " q  print Quotes around text values (default=yes; \"-D-q\" for no)\n"
               " t  print Timestamps\n"
               " f  print rFlags\n"
               " s  print list of Servers\n"
               " F  print Fresh ages\n"
               " Q  print Quants\n"
               " S  print Strings (label, comment, units, ...)\n"
               " w  print when performing writes (sending data)\n"
               " /  print unserved channels upon timeout (-T)\n"
               "\n"
               "CHANNEL may have optional prefixes:\n"
               " %%DPYFMT:         printf-type display format, must comply with dtype (int/float)\n"
               " @-:              turn on CDA_DATAREF_OPT_PRIVATE flag\n"
               " @.:              turn on CDA_DATAREF_OPT_NO_RD_CONV flag\n"
               " @/:              turn on CDA_DATAREF_OPT_SHY flag\n"
               " @+:              following integer dtype is unsigned (add CXDTYPE_USGN_MASK)\n"
               " @T[MAX_NELEMS]:  dtype and optional maximum nelems; T is one of:\n"
               "                   b  INT8  (Byte)\n"
               "                   h  INT16 (Short)\n"
               "                   i  INT32 (Int)\n"
               "                   q  INT64 (Quad)\n"
               "                   s  SINGLE\n"
               "                   d  DOUBLE\n"
               "                   t  TEXT\n"
               "                   u  UCTEXT\n"
               "                  default dtype is 'd', default MAX_NELEMS=1 (scalar)\n");
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

    if (print_srvlist)
    {
        nsids = cda_status_srvs_count(the_cid);
        fprintf(outfile, "servers[%d]={", nsids);
        for (nth = 0;  nth < nsids;  nth++)
            fprintf(outfile, "%s[%d] \"%s\"",
                    nth == 0? "" : ", ",
                    nth, cda_status_srv_name(the_cid, nth));
        fprintf(outfile, "}\n");
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
    
    return EC_OK;
}
