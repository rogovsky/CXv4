#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>

#include "cx_sysdeps.h"

#include "misc_macros.h"
#include "misclib.h"
#include "findfilein.h"
#include "memcasecmp.h"
#include "cxscheduler.h"
#include "timeval_utils.h"

#include "cx.h"
#include "cda.h"
#include "cdaP.h"

#include "cda_f_fla.h"


typedef struct
{
    int     numpts;
    double  data[0];
} lapprox_rec_t;

typedef union
{
    double         number;
    char          *str;          /* Note: str should NOT be freed, since it is NOT malloc()'d/strdup()'s, but is placed in fla->buf (after commands) and therefore all 'str's are freed automatically */
    cda_dataref_t  chanref;
    cda_varparm_t  varparm;
    int            displacement;
    lapprox_rec_t *lapprox_rp;
} fla_val_t;

typedef struct
{
    int32      cmd;      // OP_nnn
    int32      flags;    // FLAG_xxx
    fla_val_t  arg;
} fla_cmd_t;

enum
{
    // Basic
    OP_INVAL = 0,
    OP_NOOP,

    OP_PUSH,
    OP_POP,
    OP_DUP,

    // Control
    OP_RET,
    OP_SLEEP,

    //
    OP_LABEL,
    OP_GOTO,
    
    // Simple arithmetics
    OP_ADD,
    OP_SUB,
    OP_NEG,

    OP_MUL,
    OP_DIV,
    OP_MOD,

    OP_ABS,
    OP_ROUND,
    OP_TRUNC,

    // More arithmetics
    OP_SQRT,
    OP_PW2,
    OP_PWR,
    OP_EXP,
    OP_LOG,

    //
    OP_ALLOW_W,
    OP_REFRESH,
    OP_CALCERR,
    OP_WEIRD,
    OP_GETOTHEROP,

    //
    OP_BOOLEANIZE,
    OP_BOOL_OR,
    OP_BOOL_AND,

    //
    OP_CASE,
    OP_TEST,
    OP_CMP_IF_LT,
    OP_CMP_IF_LE,
    OP_CMP_IF_EQ,
    OP_CMP_IF_GE,
    OP_CMP_IF_GT,
    OP_CMP_IF_NE,
    
    //
    OP_GETCHAN,
    OP_PUTCHAN,
    OP_GETVAR,
    OP_PUTVAR,

    //
    OP_QRYVAL,
    OP_GETTIME,

    //
    OP_PRINT_STR,
    OP_PRINT_DBL,

    //
    OP_LAPPROX,
};

enum
{
    FLAG_IMMED = 1 << 31,
    FLAG_NO_IM = 1 << 30,

    FLAG_ALLOW_WRITE  = 1 << 0,
    FLAG_SKIP_COMMAND = 1 << 1,
    FLAG_PRGLY        = 1 << 2,
};

enum
{
    CMD_RC_OK    = CDA_PROCESS_DONE,
};

struct _cda_f_fla_privrec_t_struct;

typedef int (*cmd_proc_t)(struct _cda_f_fla_privrec_t_struct *fla,
                          fla_val_t *stk, int *stk_idx_p);

enum
{
    ARG_NONE = 0,
    ARG_DOUBLE,
    ARG_STRING,
    ARG_CHANREF,
    ARG_VARPARM,
    ARG_LAPPROX,
};

typedef struct
{
    const char *name;
    cmd_proc_t  proc;
    int         nargs;    // # of consumed from stack
    int         nrslt;    // # of returned un stack
    int         flags;
    int         argtype;
} cmd_descr_t;

//////////////////////////////////////////////////////////////////////

enum
{
    RAW_COUNT_NONE = 0,
    RAW_COUNT_SNGL = 1,
    RAW_COUNT_MANY = 2,
};

typedef struct _cda_f_fla_privrec_t_struct
{
    /* Formula body */
    cda_dataref_t  dataref; // "Backreference" to corr.entry in the global table
    fla_cmd_t     *buf;
    int            fla_len; // Formula length in commands
    int            uniq;

    /* Execution context */
    double         userval;
////    int            is_wr;
    int            exec_options;
    int            cmd_idx; // "Instruction Pointer"
    rflags_t       rflags;
    cx_time_t      timestamp;

    int32          thisflags;
    int32          nextflags;

    sl_tid_t       sleep_tid;

    /* Results */
    double         retval;
    CxAnyVal_t     raw;
    cxdtype_t      raw_dtype;
    int            raw_count;
    int            do_refresh;
} cda_f_fla_privrec_t;

//////////////////////////////////////////////////////////////////////

static int _cda_debug_lapprox = 0;

static
int cda_do_linear_approximation(double *rv_p, double x,
                                int npts, double *matrix)
{
  int     ret = 0;
  int     lb;
  double  x0, y0, x1, y1;

    if (npts == 1)
    {
        *rv_p = matrix[1];
        return 1;
    }
    if (npts <  1)
    {
        *rv_p = x;
        return 1;
    }
  
    if      (x < matrix[0])
    {
        ret = 1;
        lb = 0;
    }
    else if (x > matrix[(npts - 1) * 2])
    {
        ret = 1;
        lb = npts - 2;
    }
    else
    {
        /*!!!Should replace with binary search!*/
        for (lb = 0;  lb < npts - 1/*!!!?*/;  lb++)
            if (matrix[lb*2] <= x  &&  x <= matrix[(lb + 1) * 2])
                break;
    }

    x0 = matrix[lb * 2];
    y0 = matrix[lb * 2 + 1];
    x1 = matrix[lb * 2 + 2];
    y1 = matrix[lb * 2 + 3];
    
    *rv_p = y0 + (x - x0) * (y1 - y0) / (x1 - x0);

    if (_cda_debug_lapprox)
    {
        fprintf(stderr, "LAPPROX: x=%f, npts=%d; lb=%d:[%f,%f] =>%f  r=%d\n",
                                    x,       npts,  lb, x0,x1,  *rv_p, ret);
        //    if (npts == 121) fprintf(stderr, "x=%f: lb=%d v=%f\n", x, lb, *rv_p);
    }

    return ret;
}

static int compare_doubles (const void *a, const void *b)
{
  const double *da = (const double *) a;
  const double *db = (const double *) b;
    
    return (*da > *db) - (*da < *db);
}
static lapprox_rec_t *load_lapprox_table(cda_dataref_t  ref,
                                         const char    *table_name,
                                         int xcol, int ycol)
{
  FILE              *fp;
  char               line[1000];

  int                maxcol;
  char              *p;
  double             v;
  double             a;
  double             b;
  char              *err;
  int                col;
  
  int                tab_size;
  int                tab_used;
  lapprox_rec_t *rec;
  lapprox_rec_t *nrp;

    if (table_name == NULL) return NULL;

    if (xcol < 1) xcol = 1;
    if (ycol < 1) ycol = 2;

    fp = findfilein(table_name,
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
                    program_invocation_name,
#else
                    NULL,
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */
                    NULL, ".lst",
                    "./"
                        FINDFILEIN_SEP "../common"
                        FINDFILEIN_SEP "!/"
                        FINDFILEIN_SEP "!/../settings/common"
                        FINDFILEIN_SEP "$PULT_ROOT/settings/common"
                        FINDFILEIN_SEP "~/4pult/settings/common");
    if (fp == NULL)
    {
        cda_ref_p_report(ref, "lapprox: couldn't find a table \"%s.lst\"", table_name);
        return NULL;
    }

    tab_size = 0;
    tab_used = 0;
    rec      = malloc(sizeof(*rec));
    bzero(rec, sizeof(*rec));

    maxcol = xcol;
    if (ycol > maxcol) maxcol = ycol;
    
    while (fgets(line, sizeof(line) - 1, fp) != NULL)
    {
        line[sizeof(line) - 1] = '\0';

        /* Skip leading whitespace and ignore empty/comment lines */
        p = line;
        while(*p != '\0'  &&  isspace(*p)) p++;
        if (*p == '\0'  ||  *p == '#')
            goto NEXT_LINE;

        /* Obtain "x" and "y" */
        /* Note: gcc barks
               "warning: `a' might be used uninitialized in this function"
               "warning: `b' might be used uninitialized in this function"
           here, but in fact either a/b are initialized, or we skip their usage.
         */
        for (col = 1;  col <= maxcol;  col++)
        {
            v = strtod(p, &err);
            if (err == p  ||  (*err != '\0'  &&  !isspace(*err)))
                goto NEXT_LINE;
            p = err;

            if (col == xcol) a = v;
            if (col == ycol) b = v;
        }

        /* Grow table if required */
        if (tab_used >= tab_size)
        {
            tab_size += 20;
            nrp = safe_realloc(rec, sizeof(*rec) + sizeof(double)*2*tab_size);
            if (nrp == NULL) goto END_OF_FILE;
            rec = nrp;
        }

        rec->data[tab_used * 2]     = a;
        rec->data[tab_used * 2 + 1] = b;
        tab_used++;
        
 NEXT_LINE:;
    }
 END_OF_FILE:
    
    fclose(fp);

    ////fprintf(stderr, "%d points read from %s\n", tab_used, fname);
    
    rec->numpts = tab_used;

    /* Sort the table in ascending order */
    qsort(rec->data, rec->numpts, sizeof(double)*2, compare_doubles);
    
    return rec;
}

//////////////////////////////////////////////////////////////////////

static int proc_POP(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    (*stk_idx_p)++;

    return CMD_RC_OK;
}

static int proc_DUP(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  fla_val_t  var;

    var = stk[*stk_idx_p];
    stk[--(*stk_idx_p)] = var;
    return CMD_RC_OK;
}

static void SleepExp(int      uniq,  void *privptr1,
                     sl_tid_t tid,   void *privptr2);
static int proc_SLEEP(cda_f_fla_privrec_t *fla,
                      fla_val_t *stk, int *stk_idx_p)
{
  double          dur;
  double          secs;
  struct timeval  now;
  struct timeval  add;

    dur = stk[(*stk_idx_p)++].number;
    if (dur < 0) dur = 0;
    add.tv_usec = modf (dur, &secs) * 1000000;
    add.tv_sec  = trunc(dur);
    gettimeofday(&now, NULL);
    timeval_add(&add, &add, &now);

    fla->sleep_tid = sl_enq_tout_at(fla->uniq, fla,
                                    &add, SleepExp, NULL);

    return CDA_PROCESS_FLAG_BUSY;
}

static int proc_GOTO(cda_f_fla_privrec_t *fla,
                     fla_val_t *stk, int *stk_idx_p)
{
  int  displacement;

    displacement = stk[(*stk_idx_p)++].displacement;

    if ((fla->thisflags & FLAG_SKIP_COMMAND) == 0)
        fla->cmd_idx = displacement;

    return CMD_RC_OK;
}

static int proc_CASE(cda_f_fla_privrec_t *fla,
                     fla_val_t *stk, int *stk_idx_p)
{
  double  a1, a2, a3;
  double  cs;
  double  val;

    cs = stk[(*stk_idx_p)++].number;
    a3 = stk[(*stk_idx_p)++].number;
    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    if      (cs < 0) val = a1;
    else if (cs > 0) val = a3;
    else             val = a2;
    stk[--(*stk_idx_p)].number = val;
    return CMD_RC_OK;
}

static int proc_TEST(cda_f_fla_privrec_t *fla,
                     fla_val_t *stk, int *stk_idx_p)
{
  double val;

    val = stk[(*stk_idx_p)++].number;
    fla->nextflags = (fla->thisflags &~ FLAG_SKIP_COMMAND) |
                     (val != 0? 0 : FLAG_SKIP_COMMAND);
    return CMD_RC_OK;
}

#define DEFINE_CMP_PROC(proc_NAME, op)                            \
    static int proc_NAME(cda_f_fla_privrec_t *fla,                \
                         fla_val_t *stk, int *stk_idx_p)          \
    {                                                             \
      double a1, a2;                                              \
                                                                  \
        a2 = stk[(*stk_idx_p)++].number;                          \
        a1 = stk[(*stk_idx_p)++].number;                          \
        fla->nextflags = (fla->thisflags &~ FLAG_SKIP_COMMAND) |  \
                         ((a1 op a2)? 0 : FLAG_SKIP_COMMAND);     \
        return CMD_RC_OK;                                         \
    }
DEFINE_CMP_PROC(proc_CMP_IF_LT, <)
DEFINE_CMP_PROC(proc_CMP_IF_LE, <=)
DEFINE_CMP_PROC(proc_CMP_IF_EQ, ==)
DEFINE_CMP_PROC(proc_CMP_IF_GE, >=)
DEFINE_CMP_PROC(proc_CMP_IF_GT, >)
DEFINE_CMP_PROC(proc_CMP_IF_NE, !=)


static int proc_ADD(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    stk[--(*stk_idx_p)].number = a1 + a2;
    return CMD_RC_OK;
}

static int proc_SUB(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    stk[--(*stk_idx_p)].number = a1 - a2;
    return CMD_RC_OK;
}

static int proc_NEG(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = -stk[*stk_idx_p].number;
    return CMD_RC_OK;
}

static int proc_MUL(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    stk[--(*stk_idx_p)].number = a1 * a2;
    return CMD_RC_OK;
}

static int proc_DIV(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    if (fabs(a2) < 0.00001) a2 = (a2 >= 0) ?  0.00001 : -0.00001;
    stk[--(*stk_idx_p)].number = a1 / a2;
    return CMD_RC_OK;
}

static int proc_MOD(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    if (fabs(a2) < 0.00001) a2 = (a2 >= 0) ?  0.00001 : -0.00001;
    stk[--(*stk_idx_p)].number = fmod(a1, a2);
    return CMD_RC_OK;
}

static int proc_ABS(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = fabs(stk[*stk_idx_p].number);
    return CMD_RC_OK;
}

static int proc_ROUND(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = round(stk[*stk_idx_p].number);
    return CMD_RC_OK;
}

static int proc_TRUNC(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = trunc(stk[*stk_idx_p].number);
    return CMD_RC_OK;
}

static int proc_SQRT(cda_f_fla_privrec_t *fla,
                     fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = sqrt(fabs(stk[*stk_idx_p].number));
    return CMD_RC_OK;
}

static int proc_PW2(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = stk[*stk_idx_p].number * stk[*stk_idx_p].number;
    return CMD_RC_OK;
}

static int proc_PWR(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    stk[--(*stk_idx_p)].number = pow(a1, a2);
    return CMD_RC_OK;
}

static int proc_EXP(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = exp(stk[*stk_idx_p].number);
    return CMD_RC_OK;
}

static int proc_LOG(cda_f_fla_privrec_t *fla,
                    fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = log(stk[*stk_idx_p].number);
    return CMD_RC_OK;
}

static int proc_ALLOW_W(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
    fla->nextflags = fla->thisflags | FLAG_ALLOW_WRITE;
    return CMD_RC_OK;
}

static int proc_REFRESH(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  double  val;

    val = stk[(*stk_idx_p)++].number;
    if (fabs(val) > 0.00001) fla->do_refresh = 1;
    return CMD_RC_OK;
}

static int proc_CALCERR(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  double  val;

    val = stk[(*stk_idx_p)++].number;
    if (fabs(val) > 0.00001) fla->rflags |= CXCF_FLAG_CALCERR;
    return CMD_RC_OK;
}

static int proc_WEIRD(cda_f_fla_privrec_t *fla,
                      fla_val_t *stk, int *stk_idx_p)
{
  double  val;

    val = stk[(*stk_idx_p)++].number;
    if (fabs(val) > 0.00001) fla->rflags |= CXCF_FLAG_COLOR_WEIRD;
    return CMD_RC_OK;
}

static int proc_GETOTHEROP(cda_f_fla_privrec_t *fla,
                           fla_val_t *stk, int *stk_idx_p)
{
    stk[--(*stk_idx_p)].number = (fla->rflags & CXCF_FLAG_OTHEROP)? 1.0 : 0.0;
    return CMD_RC_OK;
}

static int proc_BOOLEANIZE(cda_f_fla_privrec_t *fla,
                           fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = fabs(stk[*stk_idx_p].number) > 0.00001;
    return CMD_RC_OK;
}

static int proc_BOOL_OR(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    stk[--(*stk_idx_p)].number = fabs(a1) > 0.00001 || fabs(a2) > 0.00001;
    return CMD_RC_OK;
}

static int proc_BOOL_AND(cda_f_fla_privrec_t *fla,
                         fla_val_t *stk, int *stk_idx_p)
{
  double a1, a2;

    a2 = stk[(*stk_idx_p)++].number;
    a1 = stk[(*stk_idx_p)++].number;
    stk[--(*stk_idx_p)].number = fabs(a1) > 0.00001 && fabs(a2) > 0.00001;
    return CMD_RC_OK;
}

static int proc_GETCHAN(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  int            r;

  cda_dataref_t  ref;
  double         val;
  CxAnyVal_t     raw;
  cxdtype_t      raw_dtype;
  rflags_t       rflags;
  cx_time_t      timestamp;

    ref = stk[*stk_idx_p].chanref;

    r = cda_get_ref_dval(ref,
                         &val,
                         &raw, &raw_dtype,
                         &rflags, &timestamp);
    if (r != 0)
    {
        val           = NAN;
        raw_dtype     = CXDTYPE_UNKNOWN;
        rflags        = CXCF_FLAG_CALCERR;
        timestamp.sec = timestamp.nsec = 0;
    }

    stk[*stk_idx_p].number = val;
    if (raw_dtype != CXDTYPE_UNKNOWN)
    {
        if (fla->raw_count == RAW_COUNT_NONE)
        {
            fla->raw       = raw;
            fla->raw_dtype = raw_dtype;
            fla->raw_count = RAW_COUNT_SNGL;
        }
        else
            fla->raw_count = RAW_COUNT_MANY;
    }
    fla->rflags |= rflags;
    if (timestamp.sec != 0)
    {
        if (fla->timestamp.sec == 0                    // Missing is always replaced
            ||
             timestamp.sec  <  fla->timestamp.sec      // Much older?
            ||
            (timestamp.sec  == fla->timestamp.sec  &&  // A bit older?
             timestamp.nsec <  fla->timestamp.nsec))
            fla->timestamp = timestamp;
    }

    return CMD_RC_OK;
}

static int proc_PUTCHAN(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  cda_dataref_t  ref;
  double         val;

    ref = stk[(*stk_idx_p)++].chanref;
    val = stk[(*stk_idx_p)++].number;
    if ((fla->thisflags & FLAG_SKIP_COMMAND) == 0)
    {
        if ((fla->exec_options & CDA_OPT_IS_W)     != 0  ||
            (fla->thisflags    & FLAG_ALLOW_WRITE) != 0)
        {
            if ((fla->exec_options & CDA_OPT_READONLY) == 0)
                cda_set_dcval(ref, val);
        }
        else
        {
            /*!!! Bark about SETP in read-type formula */
        }
    }

    return CMD_RC_OK;
}

static int proc_GETVAR(cda_f_fla_privrec_t *fla,
                       fla_val_t *stk, int *stk_idx_p)
{
    stk[*stk_idx_p].number = 9.8765;
    // initializedness?

    return CMD_RC_OK;
}

static int proc_PUTVAR(cda_f_fla_privrec_t *fla,
                       fla_val_t *stk, int *stk_idx_p)
{
  cda_varparm_t  vpn;
  double         val;

    vpn = stk[(*stk_idx_p)++].varparm;
    val = stk[(*stk_idx_p)++].number;

    if ((fla->thisflags & FLAG_SKIP_COMMAND) == 0)
    {
        /*!!! write value */
    }

    return CMD_RC_OK;
}

static int proc_QRYVAL(cda_f_fla_privrec_t *fla,
                       fla_val_t *stk, int *stk_idx_p)
{
    if ((fla->exec_options & CDA_OPT_HAS_PARAM) != 0)
        stk[--(*stk_idx_p)].number = fla->userval;
    else
    {
        stk[--(*stk_idx_p)].number = NAN;
        fla->rflags |= CXCF_FLAG_CALCERR;
    }
    return CMD_RC_OK;
}

static int proc_GETTIME(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  struct timeval  now;

    gettimeofday(&now, NULL);
    stk[--(*stk_idx_p)].number = now.tv_sec + ((double)now.tv_usec)/1000000;
    return CMD_RC_OK;
}

static int proc_PRINT_STR(cda_f_fla_privrec_t *fla,
                          fla_val_t *stk, int *stk_idx_p)
{
  const char *str;

    str = stk[(*stk_idx_p)++].str;
    if (str != NULL  &&
        (fla->thisflags & FLAG_SKIP_COMMAND) == 0)
        fprintf(stderr, "%s: %s%sPRINT_STR: %s\n", strcurtime(),
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
                program_invocation_short_name, ": ",
#else
                "", "",
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */
                str);

    return CMD_RC_OK;
}

static int proc_PRINT_DBL(cda_f_fla_privrec_t *fla,
                          fla_val_t *stk, int *stk_idx_p)
{
  const char *str;
  double      val;

    str = stk[(*stk_idx_p)++].str;
    val = stk[(*stk_idx_p)++].number;
    if ((fla->thisflags & FLAG_SKIP_COMMAND) == 0)
    {
        fprintf(stderr, "%s: %s%sCMD_DEBUGPV_I: %s", strcurtime(),
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
                program_invocation_short_name, ": ",
#else
                "", "",
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */
                "" // This is just to allow previous lines end with ','
               );
        fprintf(stderr, str, val);
        fprintf(stderr, "\n");
    }

    return CMD_RC_OK;
}

static int proc_LAPPROX(cda_f_fla_privrec_t *fla,
                        fla_val_t *stk, int *stk_idx_p)
{
  lapprox_rec_t *lapprp;
  double         val;

    lapprp = stk[(*stk_idx_p)++].lapprox_rp;
    val    = stk[(*stk_idx_p)++].number;

    if (cda_do_linear_approximation(&val, val, lapprp->numpts, lapprp->data) != 0)
        fla->rflags |= CXCF_FLAG_CALCERR;

    stk[--(*stk_idx_p)].number = val;

    return CMD_RC_OK;
}


//--------------------------------------------------------------------

static cmd_descr_t  command_set[] =
{
    [OP_INVAL]      = {"INVAL=0",    NULL,            0, 0, 0,          ARG_NONE},
    [OP_NOOP]       = {"NOOP",       NULL,            0, 0, 0,          ARG_NONE},
    [OP_PUSH]       = {"PUSH",       NULL,            0, 1, 0,          ARG_DOUBLE},
    [OP_POP]        = {"POP",        proc_POP,        1, 0, 0,          ARG_NONE},
    [OP_DUP]        = {"DUP",        proc_DUP,        1, 2, 0,          ARG_NONE},
    [OP_ADD]        = {"ADD",        proc_ADD,        2, 1, 0,          ARG_DOUBLE},
    [OP_SUB]        = {"SUB",        proc_SUB,        2, 1, 0,          ARG_DOUBLE},
    [OP_NEG]        = {"NEG",        proc_NEG,        1, 1, 0,          ARG_DOUBLE},
    [OP_MUL]        = {"MUL",        proc_MUL,        2, 1, 0,          ARG_DOUBLE},
    [OP_DIV]        = {"DIV",        proc_DIV,        2, 1, 0,          ARG_DOUBLE},
    [OP_MOD]        = {"MOD",        proc_MOD,        1, 1, 0,          ARG_DOUBLE},
    [OP_ABS]        = {"ABS",        proc_ABS,        1, 1, 0,          ARG_DOUBLE},
    [OP_ROUND]      = {"ROUND",      proc_ROUND,      1, 1, 0,          ARG_DOUBLE},
    [OP_TRUNC]      = {"TRUNC",      proc_TRUNC,      1, 1, 0,          ARG_DOUBLE},
    [OP_SQRT]       = {"SQRT",       proc_SQRT,       1, 1, 0,          ARG_DOUBLE},
    [OP_PW2]        = {"PW2",        proc_PW2,        1, 1, 0,          ARG_DOUBLE},
    [OP_PWR]        = {"PWR",        proc_PWR,        2, 1, 0,          ARG_DOUBLE},
    [OP_EXP]        = {"EXP",        proc_EXP,        1, 1, 0,          ARG_DOUBLE},
    [OP_LOG]        = {"LOG",        proc_LOG,        1, 1, 0,          ARG_DOUBLE},
    [OP_ALLOW_W]    = {"ALLOW_W",    proc_ALLOW_W,    0, 0, FLAG_NO_IM, ARG_NONE},
    [OP_REFRESH]    = {"REFRESH",    proc_REFRESH,    1, 0, 0,          ARG_DOUBLE},
    [OP_CALCERR]    = {"CALCERR",    proc_CALCERR,    1, 0, 0,          ARG_DOUBLE},
    [OP_WEIRD]      = {"WEIRD",      proc_WEIRD,      1, 0, 0,          ARG_DOUBLE},
    [OP_GETOTHEROP] = {"GETOTHEROP", proc_GETOTHEROP, 0, 1, FLAG_NO_IM, ARG_NONE},
    [OP_BOOLEANIZE] = {"BOOLEANIZE", proc_BOOLEANIZE, 1, 1, 0,          ARG_DOUBLE},
    [OP_BOOL_OR]    = {"BOOL_OR",    proc_BOOL_OR,    2, 1, 0,          ARG_DOUBLE},
    [OP_BOOL_AND]   = {"BOOL_AND",   proc_BOOL_AND,   2, 1, 0,          ARG_DOUBLE},
    [OP_RET]        = {"RET",        NULL,            0, 0, 0,          ARG_DOUBLE},
    [OP_SLEEP]      = {"SLEEP",      proc_SLEEP,      1, 0, 0,          ARG_DOUBLE},
    [OP_LABEL]      = {"LABEL",      NULL,            0, 0, FLAG_IMMED, ARG_STRING},
    [OP_GOTO]       = {"GOTO",       proc_GOTO,       0, 0, FLAG_IMMED, ARG_STRING},
    [OP_CASE]       = {"CASE",       proc_CASE,       4, 1, 0,          ARG_DOUBLE},
    [OP_TEST]       = {"TEST",       proc_TEST,       1, 0, 0,          ARG_DOUBLE},
    [OP_CMP_IF_LT]  = {"CMP_IF_LT",  proc_CMP_IF_LT,  2, 0, 0,          ARG_DOUBLE},
    [OP_CMP_IF_LE]  = {"CMP_IF_LE",  proc_CMP_IF_LE,  2, 0, 0,          ARG_DOUBLE},
    [OP_CMP_IF_EQ]  = {"CMP_IF_EQ",  proc_CMP_IF_EQ,  2, 0, 0,          ARG_DOUBLE},
    [OP_CMP_IF_GE]  = {"CMP_IF_GE",  proc_CMP_IF_GE,  2, 0, 0,          ARG_DOUBLE},
    [OP_CMP_IF_GT]  = {"CMP_IF_GT",  proc_CMP_IF_GT,  2, 0, 0,          ARG_DOUBLE},
    [OP_CMP_IF_NE]  = {"CMP_IF_NE",  proc_CMP_IF_NE,  2, 0, 0,          ARG_DOUBLE},
    [OP_GETCHAN]    = {"GETCHAN",    proc_GETCHAN,    1, 1, FLAG_IMMED, ARG_CHANREF},
    [OP_PUTCHAN]    = {"PUTCHAN",    proc_PUTCHAN,    2, 0, FLAG_IMMED, ARG_CHANREF},
    [OP_GETVAR]     = {"GETVAR",     proc_GETVAR,     1, 1, FLAG_IMMED, ARG_VARPARM},
    [OP_PUTVAR]     = {"PUTVAR",     proc_PUTVAR,     2, 0, FLAG_IMMED, ARG_VARPARM},
    [OP_QRYVAL]     = {"QRYVAL",     proc_QRYVAL,     0, 1, FLAG_NO_IM, ARG_NONE},
    [OP_GETTIME]    = {"GETTIME",    proc_GETTIME,    0, 1, FLAG_NO_IM, ARG_NONE},
    [OP_PRINT_STR]  = {"PRINT_STR",  proc_PRINT_STR,  1, 0, FLAG_IMMED, ARG_STRING},
    [OP_PRINT_DBL]  = {"PRINT_DBL",  proc_PRINT_DBL,  1, 0, FLAG_IMMED, ARG_STRING},
    [OP_LAPPROX]    = {"LAPPROX",    proc_LAPPROX,    2, 1, FLAG_IMMED, ARG_LAPPROX},
};

//////////////////////////////////////////////////////////////////////

static int process_commands(cda_f_fla_privrec_t *fla)
{

  enum         {STKSIZE = 1000};
  fla_val_t    argstk[STKSIZE];
  int          stkidx = STKSIZE;

  fla_cmd_t   *cp;
  int32        opcode;
  cmd_descr_t *descr;
  int          r;

    if (fla->sleep_tid >= 0)
    {
        errno = EBUSY;
        return CDA_PROCESS_ERR;
    }

    while (1)
    {
        fla->thisflags = fla->nextflags;
        fla->nextflags = 0;

        if (fla->cmd_idx >= fla->fla_len)
        {
            /*!!!*/
            errno = EFAULT;
            return CDA_PROCESS_ERR;
        }

        cp = fla->buf + fla->cmd_idx++;
        opcode = cp->cmd;

        if (opcode < 0  ||  opcode >= countof(command_set))
        {
            /*!!!*/
            errno = ENOEXEC;
            return CDA_PROCESS_ERR;
        }

        descr = command_set + opcode;

        if ((cp->flags & FLAG_IMMED) != 0)
        {
            if (stkidx <= 0)
            {
                /* "Stack OVERFLOW in OP_%s at idx=%d", descr->name, cmd_idx */
                errno = EFAULT;
                return CDA_PROCESS_ERR;
            }
            argstk[--stkidx] = cp->arg;
        }

//fprintf(stderr, "OP=%d\n", opcode);
        if (opcode == OP_RET)
        {
            if ((fla->exec_options & CDA_OPT_RETVAL_RQD) == 0)
            {
                fla->retval = 0.0;
            }
            else
            {
                if (stkidx < STKSIZE)
                    fla->retval = argstk[stkidx++].number;
                else
                {
                    fla->retval = NAN;
                    /*!!! Stack UNDERFLOW */
                }
            }

            if (stkidx < STKSIZE)
            {
                /* Warn somehow? */
            }

            if ((fla->thisflags & FLAG_SKIP_COMMAND) == 0)
                return
                    CDA_PROCESS_DONE
                    |
                    (fla->do_refresh? CDA_PROCESS_FLAG_REFRESH : 0);
            else goto NEXT_CMD;
        }

        if (descr->nargs > (STKSIZE - stkidx))
        {
            /*!!! Stack UNDERFLOW */
            errno = EFAULT;
            return CDA_PROCESS_ERR;
        }
        /*!!! Check for stack OVERFLOW after cmd execution */
        if (0)
        {
            errno = EFAULT;
            return CDA_PROCESS_ERR;
        }

        r = CMD_RC_OK;
        if (descr->proc != NULL) r = descr->proc(fla, argstk, &stkidx);

        if (r < 0) return r;
        if (r & CDA_PROCESS_FLAG_BUSY)
        {
            if (stkidx < STKSIZE)
            {
                /* Warn somehow? */
            }
            return r;
        }
        /* else just continue */
 NEXT_CMD:;
    }
}

static void SleepExp(int      uniq,  void *privptr1,
                     sl_tid_t tid,   void *privptr2)
{
  cda_f_fla_privrec_t *fla = privptr1;
  int                  r;

    fla->sleep_tid = -1;

    r = process_commands(fla);
    if (
        r >= 0  &&  (r & CDA_PROCESS_FLAG_BUSY)  == 0 // Has fla script completed?
        &&
        (fla->exec_options & CDA_OPT_RETVAL_RQD) != 0 // And is it READING?
       )
    {
        if (fla->raw_count != RAW_COUNT_SNGL)
            fla->raw_dtype = CXDTYPE_UNKNOWN;
        /*!!! Push value to dataref */
        cda_fla_p_update_fla_result(fla->dataref,
                                    fla->retval,
                                    fla->raw,
                                    fla->raw_dtype,
                                    fla->rflags,
                                    fla->timestamp);
    }
}

//////////////////////////////////////////////////////////////////////

static inline int isletter(int c)
{
    return isalpha(c)  ||  c == '_';
}

static int DO_ERR(const char *format, ...)
    __attribute__((format (printf, 1, 2)));
static int DO_ERR(const char *format, ...)
{
  va_list  ap;
  
    va_start(ap, format);
#if 1 /* 22.12.2015: immediate fprintf(stderr) was usefule before, when error from fla_p->create() wasn't passed up to the client */
    cda_vset_err(format, ap);
#else
    vfprintf(stderr, format, ap);
    fprintf (stderr, "\n");
#endif
    va_end(ap);

    return CDA_DAT_P_ERROR;
}

static int  find_label(cda_f_fla_privrec_t *fla, const char *name)
{
  int  idx;

    for (idx = 0;  idx < fla->fla_len;  idx++)
        if (fla->buf[idx].cmd == OP_LABEL  &&
            fla->buf[idx].arg.str != NULL  &&
            strcasecmp(fla->buf[idx].arg.str, name) == 0)
            return idx;

    return -1;
}

static int  cda_f_fla_p_create (cda_dataref_t  ref,
                                void          *fla_privptr,
                                int            uniq,
                                const char    *base,
                                const char    *spec,
                                int            options,
                                CxKnobParam_t *params,
                                int            num_params)
{
  cda_f_fla_privrec_t *fla = fla_privptr;

  const char          *src;
  const char          *endp;

  int                  stage;
  int                  cmd_count;
  uint8               *auxdatabuf;
  int                  auxdatasize;

  uint8               *fla_buf;
  size_t               fla_bufsize;
  size_t               cmdssize;

  int                  all_code;
  
  int32                opcode;
  cmd_descr_t         *descr;

  fla_val_t            arg;
  int32                cmd_flags;

  char                 end_ch;
  char                 ch;
  char                 buf[1000];
  int                  buf_used;

  int                  cmd_idx;
  fla_cmd_t           *cp;
  int                  displacement;

  const char          *buf_p;
  int                  xcol;
  int                  ycol;

#define SKIP_SPACES()                                               \
    do {                                                            \
        while (*src != '\0'  &&  *src != '\n'  &&  *src != '\r'  && \
               isspace(*src)) src++;                                \
    } while (0)

//fprintf(stderr, "\t%s('%s':\"%s\")\n", __FUNCTION__, base, spec);

    fla->dataref   = ref;
    fla->uniq      = uniq;
    fla->sleep_tid = -1;

    for (stage = 0;  stage <= 1;  stage++)
    {
        src = spec;
        while (*src != '\0'  &&  isspace(*src)) src++;

        if (*src == '=')
            return DO_ERR("=FORMULA format isn't supported yet");

        for (cmd_count = 0, auxdatasize = 0, all_code = 0;
             *src != '\0';
            )
        {
            while (*src != '\0'  &&
                   (
                    *src == ';'  ||
                    isspace(*src)  ||  *src == '\n'  ||  *src == '\r'
                   )
                  ) src++;
            if (*src == '\0') goto END_SPEC;

            if (*src == '#')
            {
                while (*src != '\0'  &&  *src != '\n'  &&  *src != '\r') src++;
                goto NEXT_LINE;
            }

            if (!isletter(*src))
                return DO_ERR("Syntax error: non-letter at the beginning of command");

            if (all_code)
                endp = src;
            else
                /* Get word */
                for (endp = src;
                     isletter(*endp);
                     endp++);

            if      (all_code  ||
                     cx_strmemcasecmp("_code", src, endp - src) == 0)
            {
                src = endp;
                SKIP_SPACES();
                
                if (!isletter(*src))
                    return DO_ERR("Syntax error: non-letter after _code, '%c'", *src);

                /* Get word */
                for (endp = src;
                     isletter(*endp);
                     endp++);

                /* Find in table */
                for (opcode = 0;
                     opcode < countof(command_set)         &&
                         (command_set[opcode].name == NULL
                          ||
                          cx_strmemcasecmp(command_set[opcode].name,
                                           src, endp - src) != 0
                         );
                     opcode++);
                if (opcode >= countof(command_set))
                    return DO_ERR("Unknown opcode '%.*s'", (int)(endp-src), src);
                descr = command_set + opcode;
                src = endp;

                SKIP_SPACES();

                cmd_flags = 0;
                bzero(&arg, sizeof(arg));
                // Is parameter present?
                if (*src == '\0'  ||
                    *src == '\n'  ||  *src == '\r'  ||
                    *src == ';'   ||
                    *src == '#')
                {
                    if (descr->flags & FLAG_IMMED)
                        return DO_ERR("parameter is mandatory in %s", descr->name);
                }
                else
                {
                    if (descr->flags & FLAG_NO_IM)
                        return DO_ERR("parameter is forbidden in %s", descr->name);

                    cmd_flags |= FLAG_IMMED;

                    if      (descr->argtype == ARG_NONE)
                        ;
                    else if (descr->argtype == ARG_DOUBLE)
                    {
                        arg.number = strtod(src, &endp);
                        if (endp == src)
                            return DO_ERR("number expected after %s", descr->name);
                        src = endp;
                    }
                    else if (descr->argtype == ARG_STRING   ||
                             descr->argtype == ARG_CHANREF  ||
                             descr->argtype == ARG_VARPARM  ||
                             descr->argtype == ARG_LAPPROX)
                    {
                        /* A common part -- string parsing */
                        end_ch = '\0';
                        if (*src == '\''  ||  *src == '\"')
                            end_ch = *src++;
                        buf_used = 0;
                        while (1)
                        {
                            ch = *src;
                            if (end_ch != '\0'  &&  ch == end_ch)
                            {
                                src++;
                                goto END_PARSE_STR;
                            }
                            if (ch == '\0'  ||  ch == '\n'  ||  ch == '\r')
                            {
                                if (end_ch != '\0')
                                    return DO_ERR("unterminated %s-quoted string",
                                                  end_ch=='\''? "single" : "double");
                                else
                                    goto END_PARSE_STR;
                            }
                            if (end_ch == '\0'  &&
                                !isletter(ch)  &&  !isdigit(ch)  &&
                                ch != '-'                        &&
                                ch != '@'  &&  ch != '.'  &&  ch != ':'  &&
                                (
                                 descr->argtype != ARG_CHANREF  ||
                                 buf_used       != 0            ||
                                 ch             != '%'
                                )
                               )
                                goto END_PARSE_STR;
                            if (buf_used >= sizeof(buf) - 1)
                                return DO_ERR("string too long");
                            buf[buf_used++] = ch;
                            src++;
                        }
                    END_PARSE_STR:
                        buf[buf_used++] = '\0';

                        /* Type-dependent storing */
                        if      (descr->argtype == ARG_STRING)
                        {
                            if (stage == 1)
                            {
                                arg.str = auxdatabuf + auxdatasize;
                                memcpy(arg.str, buf, buf_used);
                            }
                            auxdatasize += buf_used;
                        }
                        else if (descr->argtype == ARG_CHANREF)
                        {
                            if (stage == 1)
                                arg.chanref =
                                    buf[0] == '%'
                                    ?
                                    cda_add_varchan(cda_dat_p_get_cid(ref),
                                                    buf + 1)
                                    :
                                    cda_add_chan(cda_dat_p_get_cid(ref),
                                                 base,
                                                 buf, CDA_OPT_NONE, CXDTYPE_DOUBLE, 1,
                                                 0, NULL, NULL);
                        }
                        else if (descr->argtype == ARG_VARPARM)
                        {
                            if (stage == 1)
                                arg.varparm =
                                    cda_add_varparm(cda_dat_p_get_cid(ref),
                                                    buf);
                        }
                        else if (descr->argtype == ARG_LAPPROX)
                        {
                            if (buf_used == 0)
                                return DO_ERR("\'lapprox\' requires an Xcol:Ycol:FILEREF parameter");

                            buf_p = buf;
                            if (*buf_p == '='  &&  0/* In beamweld-table columns are 2:1, so no-specification (1:2) is useless */)
                            {
                                xcol = 1;
                                ycol = 2;
                                buf_p++;
                            }
                            else
                            {
                                /* xcol */
                                xcol = strtol(buf_p, &endp, 10);
                                if (endp == buf_p)
                                    return DO_ERR("Xcol-number expected after \"%s\"", descr->name);
                                buf_p = endp;
                                /* ':' */
                                if (*buf_p != ':')
                                    return DO_ERR(":Ycol expected after \"%s %d\"", descr->name, xcol);
                                buf_p++;
                                /* ycol */
                                ycol = strtol(buf_p, &endp, 10);
                                if (endp == buf_p)
                                    return DO_ERR("Ycol-number expected after \"%s %d:\"", descr->name, xcol);
                                buf_p = endp;
                                /* ':' */
                                if (*buf_p != ':')
                                    return DO_ERR(":FILEREF expected after \"%s %d:%d\"", descr->name, xcol, ycol);
                                buf_p++;
                            }

                            if (*buf_p == '\0')
                                return DO_ERR("FILEREF expected in %s", descr->name);

                            if (stage == 1)
                            {
                                arg.lapprox_rp = load_lapprox_table(ref, 
                                                                    buf_p,
                                                                    xcol, ycol);
                                if (arg.lapprox_rp == NULL)
                                {
                                    opcode     = OP_CALCERR;
                                    arg.number = 1.0;
                                }
                            }
                        }
                    }

                    SKIP_SPACES();
                }

                if (stage == 1)
                {
                    fla->buf[cmd_count].cmd   = opcode;
                    fla->buf[cmd_count].flags = cmd_flags;
                    fla->buf[cmd_count].arg   = arg;
                }
                cmd_count++;

                //////
                //if (stage == 0) fprintf(stderr, "\tOP_%s\n", descr->name);

            }
            else if (cx_strmemcasecmp("_all_code", src, endp - src) == 0)
            {
                src = endp;

                all_code = 1;
            }
            else
                return DO_ERR("Unknown command '%.*s'", (int)(endp-src), src);

            if      (*src == '\0'  ||
                     *src == '\n'  ||  *src == '\r'  ||
                     *src == '#')
                ;
            else if (*src == ';')
            {
                src++;
            }
            else
                return DO_ERR("either ';' or newline expected after command, %s", src);
 NEXT_LINE:;
        }
 END_SPEC:;

        /* Allocate buffer at end of stage 0 */
        if (stage == 0)
        {
            cmdssize = cmd_count * sizeof(fla_cmd_t);
            fla_bufsize  = cmdssize + auxdatasize;
            fla_buf = malloc(fla_bufsize);
            if (fla_buf == NULL) return CDA_DAT_P_ERROR;
            bzero(fla_buf, fla_bufsize);
            fla->buf = fla_buf;
            auxdatabuf = fla_buf + cmdssize;
            fla->fla_len = cmd_count;
        }

    }

    /* Post-stage: resolve jumps */
    for (cmd_idx = 0, cp = fla->buf;  cmd_idx < cmd_count;  cmd_idx++, cp++)
    {
        /* Drop IMMED flag from LABEL opcodes -- it isn't used during execution */
        if      (cp->cmd == OP_LABEL) cp->flags &=~ FLAG_IMMED;
        else if (cp->cmd == OP_GOTO)
        {
            displacement = find_label(fla, cp->arg.str);
            if (displacement < 0)
            {
                cp->cmd   = OP_NOOP;
                cp->flags = 0;
            }
            else
                cp->arg.displacement = displacement;
        }
    }
    
    return CDA_DAT_P_OPERATING;
}

static void cda_f_fla_p_destroy(void          *fla_privptr)
{
  cda_f_fla_privrec_t *fla = fla_privptr;
  int                  idx;
  fla_cmd_t           *cp;

    /* Go through all buf[fla_len] to free command-specific data.
       (Note: ARG_STRING's arg.str are placed in the same fla->buf and should NOT be free()d.) */
    for (idx = 0, cp = fla->buf;  idx < fla->fla_len;  idx++, cp++)
        if (cp->cmd == OP_LAPPROX)
        {
            safe_free(cp->arg.lapprox_rp);
            cp->arg.lapprox_rp = NULL;
        }

    safe_free(fla->buf); fla->buf = NULL;
}

static int  cda_f_fla_p_add_evp(void          *fla_privptr,
                                int                   evmask,
                                cda_dataref_evproc_t  evproc,
                                void                 *privptr2)
{
  cda_f_fla_privrec_t *fla = fla_privptr;
  int                  idx;
  fla_cmd_t           *cp;

    for (idx = 0, cp = fla->buf;  idx < fla->fla_len;  idx++, cp++)
        if (cp->cmd == OP_GETCHAN)
            cda_add_dataref_evproc(cp->arg.chanref, evmask, evproc, privptr2);

    return 0;
}

static int  cda_f_fla_p_del_evp(void          *fla_privptr,
                                int                   evmask,
                                cda_dataref_evproc_t  evproc,
                                void                 *privptr2)
{
  cda_f_fla_privrec_t *fla = fla_privptr;
  int                  idx;
  fla_cmd_t           *cp;

    for (idx = 0, cp = fla->buf;  idx < fla->fla_len;  idx++, cp++)
        if (cp->cmd == OP_GETCHAN)
            cda_del_dataref_evproc(cp->arg.chanref, evmask, evproc, privptr2);

    return 0;
}

static int  cda_f_fla_p_execute(void          *fla_privptr,
                                int            options,
                                CxKnobParam_t *params,
                                int            num_params,
                                double         userval,
                                double        *result_p,
                                CxAnyVal_t    *rv_p, cxdtype_t *rv_dtype_p,
                                rflags_t      *rflags_p,
                                cx_time_t     *timestamp_p)
{
  cda_f_fla_privrec_t *fla = fla_privptr;
  int                  r;

    /* Check if this formula is ready to start */
    if (fla->sleep_tid >= 0)
    {
        /*!!! describe error somehow?  */
        errno = EBUSY;
        return CDA_PROCESS_ERR;
    }

    /* Initialize */
    fla->userval      = userval;
    fla->exec_options = options;
    fla->cmd_idx      = 0;
    fla->rflags       = 0;
    bzero(&(fla->timestamp), sizeof(fla->timestamp));

    fla->nextflags  = 0;

    fla->retval     = NAN;
    fla->raw_dtype  = CXDTYPE_UNKNOWN;
    fla->raw_count  = RAW_COUNT_NONE;
    fla->do_refresh = 0;

    /* Start */
    r = process_commands(fla);
    if (
        r >= 0  &&  (r & CDA_PROCESS_FLAG_BUSY)  == 0 // Has fla script completed?
        &&
        (fla->exec_options & CDA_OPT_RETVAL_RQD) != 0 // And is it READING?
       )
    {
        if (fla->raw_count != RAW_COUNT_SNGL)
            fla->raw_dtype = CXDTYPE_UNKNOWN;

        if (result_p    != NULL) *result_p    = fla->retval;
        if (rv_p        != NULL) *rv_p        = fla->raw;
        if (rv_dtype_p  != NULL) *rv_dtype_p  = fla->raw_dtype;
        if (rflags_p    != NULL) *rflags_p    = fla->rflags;
        if (timestamp_p != NULL) *timestamp_p = fla->timestamp;
    }
    return r;
}

static void cda_f_fla_p_stop   (void          *fla_privptr)
{
  cda_f_fla_privrec_t *fla = fla_privptr;

    /* Check if this formula is ready to start */
    if (fla->sleep_tid >= 0)
    {
        sl_deq_tout(fla->sleep_tid);
        fla->sleep_tid = -1;
    }
}

static int  cda_f_fla_p_source (void          *fla_privptr,
                                char          *buf,
                                size_t         bufsize)
{
    strzcpy(buf, "UNSUPPORTED op", bufsize);
    
    return 0;
}

static int  cda_f_fla_p_srvs_of(void          *fla_privptr,
                                uint8 *conns_u, int conns_u_size)
{
  cda_f_fla_privrec_t *fla = fla_privptr;
  int                  idx;

    for (idx = 0;  idx < fla->fla_len;  idx++)
        if (fla->buf[idx].cmd == OP_GETCHAN)
            cda_srvs_of_ref(fla->buf[idx].arg.chanref, conns_u, conns_u_size);

    return 0;
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_FLA_PLUGIN(fla, "CDA-CX Formulae plugin",
                      NULL, NULL,
                      sizeof(cda_f_fla_privrec_t),
                      CDA_FLA_P_FLAG_NONE,
                      cda_f_fla_p_create,  cda_f_fla_p_destroy,
                      cda_f_fla_p_add_evp, cda_f_fla_p_del_evp,
                      cda_f_fla_p_execute, cda_f_fla_p_stop,
                      cda_f_fla_p_source,  cda_f_fla_p_srvs_of);
