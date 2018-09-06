#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "cx_sysdeps.h"
#include "misc_macros.h"
#include "misclib.h"
#include "ppf4td.h"

#include "datatreeP.h"
#include "CdrP.h"
#include "Cdr.h"


typedef struct
{
  DataSubsys    sys;
  ppf4td_ctx_t *ctx;
} parse_rec_t;


#define BARK(format, params...)                                    \
    (CdrSetErr("%s() %s:%d: "format,  __FUNCTION__,                                  \
     ppf4td_cur_ref(rp->ctx), ppf4td_cur_line(rp->ctx), ##params), \
     -1)

static int  IsAtEOL(parse_rec_t *rp)
{
  int  r;
  int  ch;

    r = ppf4td_is_at_eol(rp->ctx);
    if (r != 0) return r;
    r = ppf4td_peekc    (rp->ctx, &ch);
    if (r < 0)  return r;
    if (ch == EOF) return -1; /*!!! -2?*/
    return ch == '#';

#if 0
    return
      ppf4td_is_at_eol(rp->ctx)  ||
      (ppf4td_peekc(rp->ctx, &ch) > 0  &&  ch == '#');
#endif
}

static int PeekCh(parse_rec_t *rp, const char *name, int *ch_p)
{
  int  r;
  int  ch;

    r = ppf4td_peekc(rp->ctx, &ch);
    if (r < 0)
        return BARK("unexpected error parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (r == 0)
        return BARK("unexpected EOF parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (IsAtEOL(rp))
        return BARK("unexpected EOL parsing %s",
                    name);

    *ch_p = ch;
    return 0;
}

static int NextCh(parse_rec_t *rp, const char *name, int *ch_p)
{
  int  r;
  int  ch;

    r = ppf4td_peekc(rp->ctx, &ch);
    if (r < 0)
        return BARK("unexpected error parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (r == 0)
        return BARK("unexpected EOF parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (IsAtEOL(rp))
        return BARK("unexpected EOL parsing %s",
                    name);

    r = ppf4td_nextc(rp->ctx, &ch);

    *ch_p = ch;
    return 0;
}

static void SkipToNextLine(parse_rec_t *rp)
{
  int  ch;

    /* 1. Skip non-CR/LF */
    while (ppf4td_peekc(rp->ctx, &ch) > 0  &&
           (ch != '\r'  &&  ch != '\n')) ppf4td_nextc(rp->ctx, &ch);
    /* 2. Skip all CR/LF (this skips multiple empty lines too) */
    while (ppf4td_peekc(rp->ctx, &ch) > 0  &&
           (ch == '\r'  ||  ch == '\n')) ppf4td_nextc(rp->ctx, &ch);
}

static int ParseXName(parse_rec_t *rp,
                      int flags,
                      const char *name, char *buf, size_t bufsize)
{
  int  r;

    r = ppf4td_get_ident(rp->ctx, flags, buf, bufsize);
    if (r < 0)
        return BARK("%s expected; %s\n", name, ppf4td_strerror(errno));

    return 0;
}

static int ParseAName(parse_rec_t *rp,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(rp, PPF4TD_FLAG_NONE, name, buf, bufsize);
}

static int ParseDName(parse_rec_t *rp,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(rp, PPF4TD_FLAG_DASH, name, buf, bufsize);
}

static inline int isletnum(int c)
{
    return isalnum(c)  ||  c == '_'  ||  c == '-';
}

//////////////////////////////////////////////////////////////////////

// "ft" -- FromText
typedef int (*ft_proc_t)(parse_rec_t *rp, const char *name);
typedef struct
{
    const char *dirv;
    ft_proc_t   processer;
} ft_cmd_t;

static ft_cmd_t * FindSubsysCommand(const char *name);

//--------------------------------------------------------------------

static int ParseKnobDescr(parse_rec_t *rp,
                          DataKnob k, int type,
                          int kind, int behaviour,
                          DataKnob uplink);

typedef int (*kfieldparser_t)(parse_rec_t *rp, const char *name, void *kfp);

typedef struct
{
    const char     *name;
    int             offset;
    kfieldparser_t  parser;
} kfielddescr_t;

#define KFD_LINE(name, field, TYPE) \
    {name, offsetof(data_knob_t, field), __CX_CONCATENATE(TYPE,_fparser)}
#define KFD_END \
    {NULL, 0, NULL}
#define KFD_BRK \
    {"-",  0, NULL}


typedef struct
{
    double         dummy;   // To occupy offset=0
    double         stdparams[DATAKNOB_NUM_STD_PARAMS];
    int            noparams;
    CxKnobParam_t *auxparams;
    int            num_auxparams;
} kparamsdescr_t;

#define KPD_LINE(name, field, TYPE) \
    {name, -(int)offsetof(kparamsdescr_t, field), __CX_CONCATENATE(TYPE,_fparser)}

static void CleanKparamsDescr(kparamsdescr_t *dsc)
{
  int             pn;

    for (pn = 0;  pn < dsc->num_auxparams;  pn++)
    {
        safe_free(dsc->auxparams[pn].ident);  dsc->auxparams[pn].ident = NULL;
        safe_free(dsc->auxparams[pn].label);  dsc->auxparams[pn].label = NULL;
    }
    safe_free(dsc->auxparams);
    dsc->auxparams     = NULL;
    dsc->num_auxparams = 0;
}

// ----

enum
{
    ENTITY_PARSE_FLAGS =
        PPF4TD_FLAG_NOQUOTES |
        PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM |
        PPF4TD_FLAG_BRCTERM,
    STRING_PARSE_FLAGS =
        PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM |
        PPF4TD_FLAG_BRCTERM
};

static int INT_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  int    *dp = kfp;
  char    buf[100];
  int     r;
  int     v;
  char   *err;

    r = ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, buf, sizeof(buf));
    if (r < 0)
        return BARK("%s (integer) expected; %s\n", name, ppf4td_strerror(errno));

    v = strtol(buf, &err, 0);
    if (err == buf  ||  *err != '\0')
        return BARK("error in %s specification \"%s\"", name, buf);

    *dp = v;
    
    return 0;
}

static int DBL_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  double *dp = kfp;
  char    buf[100];
  int     r;
  double  v;
  char   *err;

    r = ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, buf, sizeof(buf));
    if (r < 0)
        return BARK("%s (double) expected; %s\n", name, ppf4td_strerror(errno));

    v = strtod(buf, &err);
    if (err == buf  ||  *err != '\0')
        return BARK("error in %s specification \"%s\"", name, buf);

    *dp = v;
    
    return 0;
}

static int RNG_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  double *dp = kfp;
  char    buf[100];
  int     r;
  double  v1;
  double  v2;
  char   *p;
  char   *err;

    r = ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, buf, sizeof(buf));
    if (r < 0)
        return BARK("%s (double-double range) expected; %s\n", name, ppf4td_strerror(errno));

    /* "Min" part */
    p = buf;
    v1 = strtod(p, &err);
    if (err == p  ||  *err != '-')
        return BARK("error in %s.min specification in \"%s\"", name, buf);

    /* "Max" part */
    p = err + 1; // Skip '-'
    v2 = strtod(p, &err);
    if (err == p  ||  *err != '\0')
        return BARK("error in %s.max specification in \"%s\"", name, buf);

    dp[0] = v1;
    dp[1] = v2;
    
    return 0;
}

static int STR_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  char  **dp = kfp;
  char    buf[4000]; // 21.08.2018: was 1000, but too little for long formulas
  int     r;

    r = ppf4td_get_string(rp->ctx, STRING_PARSE_FLAGS, buf, sizeof(buf));
    if (r < 0)
        return BARK("%s (string) expected; %s\n", name, ppf4td_strerror(errno));

    if (*dp != NULL) free(*dp);
    *dp = strdup(buf);
    if (*dp == NULL)
        return BARK("strdup() %s fail", name);
    
    return 0;
}

static int MSTR_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  char **dp = kfp;
  int    ch1;
  int    ch2;
  char   c4u;

    /* Check for "-" and set *dp=NULL if so */
    if (ppf4td_peekc(rp->ctx, &ch1) > 0  &&  ch1 == '-')
    {
        ppf4td_nextc(rp->ctx, &ch1);
        /* "-EOL"? */
        if (IsAtEOL(rp))
        {
            if (*dp != NULL) free(*dp);
            *dp = NULL;
            return 0;
        }
        /* "-WHITESPACE"? */
        if (ppf4td_peekc(rp->ctx, &ch2) > 0  &&  isspace(ch2))
        {
            ppf4td_nextc(rp->ctx, &ch1);
            if (*dp != NULL) free(*dp);
            *dp = NULL;
            return 0;
        }
        /* No, unget that char and proceed to STR */
        c4u = ch1;
        ppf4td_ungetchars(rp->ctx, &c4u, 1);
    }

    return STR_fparser(rp, name, kfp);
}

static int DPYFMT_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  char   *dp = kfp;
  char    buf[100];
  int     r;

  int     flags;
  int     width;
  int     precision;
  char    conv_c;

    r = ppf4td_get_string(rp->ctx, STRING_PARSE_FLAGS, buf, sizeof(buf));
    if (r < 0)
        return BARK("%s expected; %s\n", name, ppf4td_strerror(errno));

    /* Clever "duplication" of dpyfmt */
    if (ParseDoubleFormat(GetTextFormat(buf),
                          &flags, &width, &precision, &conv_c) < 0)
        return BARK("invalid %s specification : %s", name, printffmt_lasterr());
    CreateDoubleFormat(dp, sizeof(((dataknob_knob_data_t*)NULL)->dpyfmt), 20, 10,
                       flags, width, precision, conv_c);

    return 0;
}

static int PARAM_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  kparamsdescr_t *pr = (kparamsdescr_t *)(((int8 *)kfp) - offsetof(kparamsdescr_t, auxparams));
  int             r;
  int             ch;

  int             readonly = 0;
  double          defval   = 0.0;
  double          minval   = 0.0;
  double          maxval   = 0.0;

  char            ident_buf [100];
  char            label_buf [100] = "";
  char            rorw_buf  [5];
  char            defval_buf[50];
  char            range_buf [100];

  char           *p;
  char           *err;

  CxKnobParam_t  *new_auxparams;
  CxKnobParam_t  *np;

#define PARAM_BRACE_OR_EOL()                  \
    ppf4td_skip_white(rp->ctx);               \
    if (PeekCh(rp, name, &ch) < 0) return -1; \
    if (ch == '}')                            \
    {                                         \
        NextCh(rp, name, &ch);                \
        goto END_OF_SPEC;                     \
    }

    /* An opening brace */
    if (NextCh(rp, name, &ch) < 0) return -1;
    if (ch != '{')
        return BARK("'{' expected in %s specification", name);

    /* An identifier */
    ppf4td_skip_white(rp->ctx);
    if (ParseAName(rp, "param-ident", ident_buf, sizeof(ident_buf)) < 0)
        return -1;

    /* A label */
    PARAM_BRACE_OR_EOL();
    if(ppf4td_get_string(rp->ctx, STRING_PARSE_FLAGS, label_buf, sizeof(label_buf)) < 0)
        return BARK("param-label expected; %s\n", ppf4td_strerror(errno));

    /* The "rw"/"ro" switch */
    PARAM_BRACE_OR_EOL();
    if (ParseAName(rp, "ro/rw", rorw_buf, sizeof(rorw_buf)) < 0)
        return -1;
    if      (strcasecmp("ro", rorw_buf) == 0)
        readonly = 1;
    else if (strcasecmp("rw", rorw_buf) == 0)
        readonly = 0;
    else
        return BARK("\"ro\" or \"rw\" expected in %s spec, not \"%s\"",
                    name, rorw_buf);

    /* A defval */
    PARAM_BRACE_OR_EOL();
    if (PeekCh(rp, name, &ch) < 0) return -1;
    if (ch == '=')
        NextCh(rp, name, &ch);
    if(ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, defval_buf, sizeof(defval_buf)) < 0)
        return -1;
    defval = strtod(defval_buf, &err);
    if (err == defval_buf  ||  *err != '\0')
        return BARK("error in %s-defval specification", name);

    /* And, finally, a range */
    PARAM_BRACE_OR_EOL();
    r = ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, range_buf, sizeof(range_buf));
    if (r < 0)
        return BARK("double-double range expected in %s spec; %s\n", name, ppf4td_strerror(errno));
    /* "Min" part */
    p = range_buf;
    minval = strtod(p, &err);
    if (err == p  ||  *err != '-')
        return BARK("error in %s-min specification in \"%s\"", name, range_buf);
    /* "Max" part */
    p = err + 1; // Skip '-'
    maxval = strtod(p, &err);
    if (err == p  ||  *err != '\0')
        return BARK("error in %s-max specification in \"%s\"", name, range_buf);

    /* ...and a closing brace */
    if (NextCh(rp, name, &ch) < 0) return -1;
    if (ch != '}')
        return BARK("'}' expected in %s specification", name);

 END_OF_SPEC:
    /* Allocate a new cell and store parsed values there */
    new_auxparams = safe_realloc(pr->auxparams,
                                 sizeof(CxKnobParam_t)
                                 *
                                 (pr->num_auxparams + 1));
    if (new_auxparams == NULL)
        return BARK("%s: realloc(auxparams) fail", name);
    pr->auxparams = new_auxparams;
    np = pr->auxparams + pr->num_auxparams;
    pr->num_auxparams += 1;

    bzero(np, sizeof(*np));
    np->ident    = strdup(ident_buf);
    if (np->ident == NULL)
        return BARK("%s: strdup(ident) fail", name);
    if (label_buf[0] != '\0')
    {
        np->label = strdup(label_buf);
        if (np->label == NULL)
            return BARK("%s: strdup(label) fail", name);
    }
    np->readonly = readonly;
    np->value    = defval;
    np->minalwd  = minval;
    np->maxalwd  = maxval;

    return 0;
}

static int REF_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
    /* For now, use MSTR parser for references */
    return MSTR_fparser(rp, name, kfp);
}

static int VECTREF_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  dataknob_vect_src_t *dp = kfp;
  int                  r;
  int                  ch;

    r = ppf4td_get_int(rp->ctx, 0, &(dp->max_nelems), 10, NULL);
    if (r < 0)
        return BARK("MAX_NELEMS (integer) expected in %s specification; %s",
                    name, ppf4td_strerror(errno));
    if (dp->max_nelems < 1)
        return BARK("max_nelems should be >=1 (not %d) in %s specification",
                    dp->max_nelems, name);

    /* An opening brace */
    if (NextCh(rp, name, &ch) < 0) return -1;
    if (ch != ':')
        return BARK("':' expected in %s specification", name);

    return REF_fparser(rp, name, &(dp->src));
}

static int CONTENT_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  DataKnob  k  = (DataKnob)(((int8 *)kfp) - offsetof(data_knob_t, u.c.content));
  int       r;
  int       count;
  char     *err;
  int       ch;
  int       n;

  char      count_buf[20];
  char      type_buf [30];
  char      opt_buf  [30];

  int       type;
  int       kind;
  int       behaviour;

  enum {COUNT_MAX = 100000};

    /* Check for duplicate specification */
    if (k->u.c.count != 0)
        return BARK("duplicate %s specification", name);

    /* Parse the count first */
    r = ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, count_buf, sizeof(count_buf));
    if (r < 0)
        return BARK("%s (integer) expected; %s\n", "content-count", ppf4td_strerror(errno));

    count = strtol(count_buf, &err, 0);
    if (err == count_buf  ||  *err != '\0')
        return BARK("error in %s specification \"%s\"", "content-count", count_buf);

    /* Next, find an opening '{'... */
    ppf4td_skip_white(rp->ctx);
    if (NextCh(rp, "content-'{'", &ch) < 0)
        return -1;
    if (ch != '{')
        return BARK("'{' expected");

    ppf4td_skip_white(rp->ctx);
    k->u.c.count = 0;
    if (count == 0)
    {
        k->u.c.content = NULL;
        /* Is it an empty element, so that "content:0 { }" spec is valid? */
        /*!!! And is this exception a really good idea? */
        /* (Note: NOT PeekCh(), since EOL is perfectly legal here) */
        if (ppf4td_peekc(rp->ctx, &ch) > 0  &&
            ch == '}')
        {
            ppf4td_nextc(rp->ctx, &ch);
            goto END_PARSE;
        }
    }
    else if (count > COUNT_MAX)
        return BARK("too large count=%d, max=%d", count, COUNT_MAX);
    else
    {
        /* Allocate space... */
        k->u.c.content = malloc(sizeof(data_knob_t) * count);
        if (k->u.c.content == NULL)
             return BARK("malloc(%s) fail", name);
        bzero(k->u.c.content, sizeof(data_knob_t) * count);
        k->u.c.count = count;
    }

    ppf4td_skip_white(rp->ctx);
    if (!IsAtEOL(rp))
        return BARK("EOL expected after '{'");
    SkipToNextLine(rp);

    /* Now, finally, get count knobs... */
    for (n = 0;  /* "n<count" checks are inside loop */;  n++)
    {
 RE_DO_LINE:
        /* Skip whitespace... */
        ppf4td_skip_white(rp->ctx);
        /* ...and empty lines, ... */
        r = IsAtEOL(rp);
        /*!!!(check for ==0?) ...but carefully, not to loop infinitely upon EOF */
        if (r < 0)
            return BARK("error while expecting knob #%d", n);
        if (r > 0)
        {
            SkipToNextLine(rp);
            goto RE_DO_LINE;
        }

        /* Okay, what's than: a '}' or a keyword? */
        if (PeekCh(rp, "content-'}'", &ch) < 0)
            return -1;
        if (ch == '}')
        {
            if (n < count)
                return BARK("closing '}' too early: only %d of %d sub-knobs listed",
                            n, count);
                
            ppf4td_nextc(rp->ctx, &ch);
            goto END_PARSE;
        }

        /* Get knob type */
        if (ParseAName(rp, "knob-type", type_buf, sizeof(type_buf)) < 0)
            return -1;

        /* Type+kind+behaviour one-word specification */
        type      = DATAKNOB_KNOB;
        kind      = DATAKNOB_KIND_WRITE;
        behaviour = 0;
        if      (strcasecmp(type_buf, "knob")      == 0)
            ;
        else if (strcasecmp(type_buf, "disp")      == 0)
            kind      = DATAKNOB_KIND_READ;
        else if (strcasecmp(type_buf, "devn")      == 0)
            kind      = DATAKNOB_KIND_DEVN;
        else if (strcasecmp(type_buf, "minmax")    == 0)
            kind      = DATAKNOB_KIND_MINMAX;
        else if (strcasecmp(type_buf, "selector")  == 0)
            behaviour = DATAKNOB_B_STEP_FXD | DATAKNOB_B_IS_SELECTOR;
        else if (strcasecmp(type_buf, "light")     == 0)
        {
            kind      = DATAKNOB_KIND_READ;
            behaviour = DATAKNOB_B_STEP_FXD | DATAKNOB_B_IS_LIGHT;
        }
        else if (strcasecmp(type_buf, "alarm")     == 0)
        {
            kind      = DATAKNOB_KIND_READ;
            behaviour = DATAKNOB_B_STEP_FXD | DATAKNOB_B_IS_LIGHT | DATAKNOB_B_IS_ALARM;
        }
        else if (strcasecmp(type_buf, "button")    == 0)
            behaviour = DATAKNOB_B_STEP_FXD | DATAKNOB_B_IS_LIGHT | DATAKNOB_B_IS_BUTTON | DATAKNOB_B_IGN_OTHEROP;
        else if (strcasecmp(type_buf, "container") == 0)
            type      = DATAKNOB_CONT;
        else if (strcasecmp(type_buf, "noop")      == 0)
            type      = DATAKNOB_NOOP;
        else if (strcasecmp(type_buf, "bigc")      == 0)
            type      = DATAKNOB_BIGC;
        else if (strcasecmp(type_buf, "text")      == 0)
        {
            type      = DATAKNOB_TEXT;
            kind      = DATAKNOB_KIND_WRITE;
        }
        else if (strcasecmp(type_buf, "mesg")      == 0)
        {
            type      = DATAKNOB_TEXT;
            kind      = DATAKNOB_KIND_READ;
        }
        else if (strcasecmp(type_buf, "users")     == 0)
            type      = DATAKNOB_USER;
        else if (strcasecmp(type_buf, "pzframe")   == 0)
            type      = DATAKNOB_PZFR;
        else if (strcasecmp(type_buf, "vector")    == 0)
        {
            type      = DATAKNOB_VECT;
            kind      = DATAKNOB_KIND_WRITE;
        }
        else if (strcasecmp(type_buf, "column")    == 0)
        {
            type      = DATAKNOB_VECT;
            kind      = DATAKNOB_KIND_READ;
        }
        /* To allow inclusion of other .subsys files: */
        /* 1. Treat grouping as "container", additionally swallowing extra parameter */
        else if (strcasecmp(type_buf, "grouping")  == 0)
        {
            ppf4td_skip_white(rp->ctx);
            if (ParseDName(rp, "grouping-sect", type_buf, sizeof(type_buf)) < 0)
                return -1;
            type      = DATAKNOB_CONT;
        }
        /* 2. Ignore top-level directives */
        else if (FindSubsysCommand(type_buf) != NULL)
        {
            SkipToNextLine(rp);
            goto RE_DO_LINE;
        }
        else
            return BARK("unknown knob-type \"%s\"", type_buf);

        if (n >= count)
            return BARK("'}' expected after all of %d sub-knobs specified", count);

        /* Optional "/BEHAVIOUR" specifiers? */
        ppf4td_skip_white(rp->ctx);
        if (/*type == DATAKNOB_KNOB           &&*/
            ppf4td_peekc(rp->ctx, &ch) > 0  &&
            ch == '/')
            while (1)
            {
                ppf4td_nextc(rp->ctx, &ch);
                if (ParseAName(rp, "behaviour-option", opt_buf, sizeof(opt_buf)) < 0)
                    return -1;

                if      (strcasecmp(opt_buf, "button")      == 0)
                    behaviour |= DATAKNOB_B_IS_BUTTON;
                else if (strcasecmp(opt_buf, "ign_otherop") == 0)
                    behaviour |= DATAKNOB_B_IGN_OTHEROP;
                else if (strcasecmp(opt_buf, "light")       == 0)
                    behaviour |= DATAKNOB_B_IS_LIGHT;
                else if (strcasecmp(opt_buf, "alarm")       == 0)
                    behaviour |= DATAKNOB_B_IS_ALARM;
                else if (strcasecmp(opt_buf, "selector")    == 0)
                    behaviour |= DATAKNOB_B_IS_SELECTOR;
                else if (strcasecmp(opt_buf, "groupable")   == 0)
                    behaviour |= DATAKNOB_B_IS_GROUPABLE;
                else if (strcasecmp(opt_buf, "fixedstep")   == 0)
                    behaviour |= DATAKNOB_B_STEP_FXD;
                else if (strcasecmp(opt_buf, "fixedranges") == 0)
                    behaviour |= DATAKNOB_B_RANGES_FXD;
                else if (strcasecmp(opt_buf, "unsavable")   == 0)
                    behaviour |= DATAKNOB_B_UNSAVABLE;
                else if (strcasecmp(opt_buf, "on_update")   == 0)
                    behaviour |= DATAKNOB_B_ON_UPDATE;
                else
                    return BARK("unknown behaviour-option \"%s\"", opt_buf);

                if (ppf4td_peekc(rp->ctx, &ch) <= 0  ||
                    (ch != '/'  &&  ch != ','))
                    break;
            }


        ppf4td_skip_white(rp->ctx);
        if (ParseKnobDescr(rp,
                           k->u.c.content + n, type,
                           kind, behaviour,
                           k) < 0)
            return -1;
////fprintf(stderr, "#%d/%d %s %s\n", n, count, type_buf, k->u.c.content[n].ident);

        SkipToNextLine(rp);
    }

 END_PARSE:
    return 0;
}

static int COLZ_fparser(parse_rec_t *rp, const char *name, void *kfp)
{
  int    *dp = kfp;
  char    buf[100];
  int     r;

    r = ppf4td_get_string(rp->ctx, ENTITY_PARSE_FLAGS, buf, sizeof(buf));
    if (r < 0)
        return BARK("%s (colz-style) expected; %s\n", name, ppf4td_strerror(errno));

    if      (strcasecmp(buf, "normal")    == 0) *dp = DATAKNOB_COLZ_NORMAL;
    else if (strcasecmp(buf, "hilited")   == 0) *dp = DATAKNOB_COLZ_HILITED;
    else if (strcasecmp(buf, "important") == 0) *dp = DATAKNOB_COLZ_IMPORTANT;
    else if (strcasecmp(buf, "vic")       == 0) *dp = DATAKNOB_COLZ_VIC;
    else if (strcasecmp(buf, "dim")       == 0) *dp = DATAKNOB_COLZ_DIM;
    else if (strcasecmp(buf, "heading")   == 0) *dp = DATAKNOB_COLZ_HEADING;
    else
        return BARK("unknown colz-style \"%s\"", buf);

    return 0;
}


static kfielddescr_t GRPG_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),

    KFD_LINE("param1",    u.c.param1,   INT),
    KFD_LINE("param2",    u.c.param2,   INT),
    KFD_LINE("param3",    u.c.param3,   INT),
    KFD_LINE("str1",      u.c.str1,     MSTR),
    KFD_LINE("str2",      u.c.str2,     MSTR),
    KFD_LINE("str3",      u.c.str3,     MSTR),
    
    KFD_LINE("content",   u.c.content,  CONTENT),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),

    KFD_LINE("base",      u.c.refbase,  MSTR),
    KFD_LINE("at_init",   u.c.at_init_src, REF),
    KFD_LINE("ap",        u.c.ap_src,   REF),

    // Aliases
    KFD_LINE("ncols",     u.c.param1,   INT),
    KFD_LINE("nflrs",     u.c.param2,   INT),
    KFD_LINE("nattl",     u.c.param3,   INT),
    KFD_LINE("coltitles", u.c.str1,     MSTR),
    KFD_LINE("rowtitles", u.c.str2,     MSTR),
    KFD_LINE("subwintitle",u.c.str3,    MSTR),
    KFD_END
};

static kfielddescr_t CONT_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),
    
    KFD_LINE("param1",    u.c.param1,   INT),
    KFD_LINE("param2",    u.c.param2,   INT),
    KFD_LINE("param3",    u.c.param3,   INT),
    KFD_LINE("str1",      u.c.str1,     MSTR),
    KFD_LINE("str2",      u.c.str2,     MSTR),
    KFD_LINE("str3",      u.c.str3,     MSTR),
    
    KFD_LINE("content",   u.c.content,  CONTENT),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),

    KFD_LINE("base",      u.c.refbase,  MSTR),
    KFD_LINE("at_init",   u.c.at_init_src, REF),
    KFD_LINE("ap",        u.c.ap_src,   REF),

    // Aliases
    KFD_LINE("ncols",     u.c.param1,   INT),
    KFD_LINE("nflrs",     u.c.param2,   INT),
    KFD_LINE("nattl",     u.c.param3,   INT),
    KFD_LINE("coltitles", u.c.str1,     MSTR),
    KFD_LINE("rowtitles", u.c.str2,     MSTR),
    KFD_LINE("subwintitle",u.c.str3,    MSTR),
    KFD_END
};

static kfielddescr_t NOOP_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),
    KFD_END
};

static kfielddescr_t KNOB_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),

    KFD_LINE("units",     u.k.units,    MSTR),
    KFD_LINE("dpyfmt",    u.k.dpyfmt,   DPYFMT),

    KFD_LINE("r",         u.k.rd_src,   REF),
    KFD_LINE("w",         u.k.wr_src,   REF),
    KFD_LINE("c",         u.k.cl_src,   REF),
    
    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),

    KFD_LINE("items",     u.k.items,    MSTR),
    
    KPD_LINE("noparams",  noparams,     INT),
    KPD_LINE("step",      stdparams[DATAKNOB_PARAM_STEP],     DBL),
    KPD_LINE("safeval",   stdparams[DATAKNOB_PARAM_SAFEVAL],  DBL),
    KPD_LINE("grpcoeff",  stdparams[DATAKNOB_PARAM_GRPCOEFF], DBL),
    KPD_LINE("alwdrange", stdparams[DATAKNOB_PARAM_ALWD_MIN], RNG),
    KPD_LINE("normrange", stdparams[DATAKNOB_PARAM_NORM_MIN], RNG),
    KPD_LINE("yelwrange", stdparams[DATAKNOB_PARAM_YELW_MIN], RNG),
    KPD_LINE("disprange", stdparams[DATAKNOB_PARAM_DISP_MIN], RNG),
    KPD_LINE("param",     auxparams,                          PARAM),
    KFD_END
};

static kfielddescr_t BIGC_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),
    KFD_END
};

static kfielddescr_t TEXT_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),
    KFD_LINE("r",         u.t.src,      REF),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),
    KFD_END
};

static kfielddescr_t USER_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),
    KFD_END
};

static kfielddescr_t PZFR_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),
    KFD_LINE("r",         u.z.src,      REF),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),
    KFD_END
};

static kfielddescr_t VECT_fields[] =
{
    KFD_LINE("ident",     ident,        MSTR),
    KFD_LINE("label",     label,        STR),
    KFD_LINE("look",      look,         MSTR),
    KFD_LINE("options",   options,      MSTR),
    KFD_LINE("dpyfmt",    u.v.dpyfmt,   DPYFMT),
    KFD_LINE("r",         u.v.src,      VECTREF),

    KFD_BRK,
    KFD_LINE("colz",      colz_style,   COLZ),
    KFD_LINE("tip",       tip,          MSTR),
    KFD_LINE("comment",   comment,      MSTR),
    KFD_LINE("style",     style,        MSTR),
    KFD_LINE("layinfo",   layinfo,      MSTR),
    KFD_LINE("geoinfo",   geoinfo,      MSTR),
    KFD_END
};

// ----

static int ParseKnobDescr(parse_rec_t *rp,
                          DataKnob k, int type,
                          int kind, int behaviour,
                          DataKnob uplink)
{
  kfielddescr_t  *table;
  int             fn;  // >= 0 -- index in the table; <0 => switched to "fieldname:"-keyed specification
  kparamsdescr_t  pdescr;
  void           *ea;  // Executive address
  int             idx;

  int             r;
  int             ch;
  char            keybuf[30];
  int             keylen;

  int             num_params;
  int             pn;

    switch (type)
    {
        case DATAKNOB_GRPG: table = GRPG_fields; break;
        case DATAKNOB_CONT: table = CONT_fields; break;
        case DATAKNOB_NOOP: table = NOOP_fields; break;
        case DATAKNOB_KNOB: table = KNOB_fields; break;
        case DATAKNOB_BIGC: table = BIGC_fields; break;
        case DATAKNOB_TEXT: table = TEXT_fields; break;
        case DATAKNOB_USER: table = USER_fields; break;
        case DATAKNOB_PZFR: table = PZFR_fields; break;
        case DATAKNOB_VECT: table = VECT_fields; break;
        default:
            return BARK("INTERNAL ERROR: parsing of unknown knob type %d requested",
                        type);
    }

    k->type = type;
    k->behaviour = behaviour;
    if (type == DATAKNOB_KNOB)
    {
        k->u.k.kind  = kind;
        k->is_rw     = (k->u.k.kind == DATAKNOB_KIND_WRITE);
    }
    if (type == DATAKNOB_TEXT)
    {
        k->is_rw     = kind == DATAKNOB_KIND_WRITE;
    }
    if (type == DATAKNOB_VECT)
    {
        k->is_rw     = kind == DATAKNOB_KIND_WRITE;
    }
    k->uplink = uplink;
    
    bzero(&pdescr, sizeof(pdescr));
    pdescr.stdparams[DATAKNOB_PARAM_STEP] = 1.0;
    
    fn = 0;
    while (1)
    {
        ppf4td_skip_white(rp->ctx);
        if (IsAtEOL(rp)) break;

        idx = -1;

        /* Is following a "fieldname:..."? */
        r = ppf4td_peekc(rp->ctx, &ch);
        if (r < 0)
        {
            BARK("unexpected error; %s\n", ppf4td_strerror(errno));
            goto CLEANUP;
        }
        if (isalpha(ch))
        {
            for (keylen = 0;  keylen < sizeof(keybuf) - 1;  keylen++)
            {
                r = ppf4td_peekc(rp->ctx, &ch);
                if (r < 0)
                {
                    BARK("unexpected error; %s\n", ppf4td_strerror(errno));
                    goto CLEANUP;
                }
                if (r > 0  &&  ch == ':')
                {
                    /* Yeah, that's a "fieldname:..." */
                    fn = -1;

                    /*Let's perform the search */
                    keybuf[keylen] = '\0';
                    for (idx = 0;  table[idx].name != NULL;  idx++)
                        if (strcasecmp(table[idx].name, keybuf) == 0)
                            break;
                    
                    /* Did we find such a field? */
                    if (table[idx].name == NULL)
                    {
                        BARK("unknown field \"%s\"", keybuf);
                        goto CLEANUP;
                    }

                    /* Skip the ':' */
                    ppf4td_nextc(rp->ctx, &ch);
                    keylen = 0;

                    goto END_KEYGET;
                }
                if (r == 0  ||  !isletnum(ch)) goto END_KEYGET;
                /* Consume character */
                ppf4td_nextc(rp->ctx, &ch);
                keybuf[keylen] = ch;
            }
        END_KEYGET:
            if (keylen > 0) ppf4td_ungetchars(rp->ctx, keybuf, keylen);
        }

        /* Wasn't "fieldname:" specified? */
        if (idx < 0)
        {
            /* Are we in random mode, but no "fieldname:" specified? */
            if (fn < 0)
            {
                BARK("unlabeled value after a labeled one");
                goto CLEANUP;
            }

            /* Have we reached end of table? */
            if (table[fn].name    == NULL  ||  // End-of-table?
                table[fn].name[0] == '-')      // Or an optional-params separator?
            {
                BARK("all fields are specified, but something left in the line");
                goto CLEANUP;
            }

            /* Okay, just pick the next field */
            idx = fn;
        }

        ////fprintf(stderr, "PRE='%s'\n", p);
        if (table[idx].offset >= 0)
            ea = ((int8 *)k)       + table[idx].offset;
        else
            ea = ((int8 *)&pdescr) - table[idx].offset;
        if (table[idx].parser(rp, table[idx].name, ea) < 0)
            goto CLEANUP;
        ////fprintf(stderr, "POST='%s'\n", p);

        /* Advance field # if not keyed-parsing */
        if (fn >= 0) fn++;
    }

    /* Post-processing */
    k->curstate = KNOBSTATE_JUSTCREATED;

    if (type == DATAKNOB_KNOB)
    {
        if (k->u.k.dpyfmt[0] == '\0')
        {
            strcpy(k->u.k.dpyfmt, "%8.3f");
            k->strsbhvr |= DATAKNOB_STRSBHVR_DPYFMT;
        }

#if 1
        /* Parameters... */
        if (!pdescr.noparams)
        {
            num_params = DATAKNOB_NUM_STD_PARAMS + pdescr.num_auxparams;
            k->u.k.params = malloc(sizeof(CxKnobParam_t) * num_params);
            if (k->u.k.params == NULL)
            {
                BARK("malloc(params) fail");
                goto CLEANUP;
            }
            k->u.k.num_params = num_params;

            /* Zero-init, since std.params have only values present */
            bzero(k->u.k.params, sizeof(CxKnobParam_t) * num_params);
            /* Copy std.params values */
            for (pn = 0;  pn < DATAKNOB_NUM_STD_PARAMS; pn++)
                k->u.k.params[pn].value = pdescr.stdparams[pn];
            /* Move aux.params data: */
            if (pdescr.num_auxparams > 0)
            {
                /* copy... */
                memcpy(k->u.k.params + DATAKNOB_NUM_STD_PARAMS,
                       pdescr.auxparams,
                       sizeof(CxKnobParam_t) * pdescr.num_auxparams);
                /* ...and zero-fill sources */
                bzero (pdescr.auxparams,
                       sizeof(CxKnobParam_t) * pdescr.num_auxparams);
            }

            for (pn = DATAKNOB_NUM_STD_PARAMS;  pn < k->u.k.num_params; pn++)
                fprintf(stderr, "[%d]:'%s':'%s'\n",
                        pn, k->u.k.params[pn].ident, k->u.k.params[pn].label);
        }
        /* Free memory if not referenced */
        CleanKparamsDescr(&pdescr);
#endif
    }

    if (type == DATAKNOB_GRPG  ||  type == DATAKNOB_CONT)
    {
        k->u.c.subsyslink = rp->sys;
    }
    
    return 0;

 CLEANUP:
    CleanKparamsDescr(&pdescr);
    return -1;
}

static int grouping_processer(parse_rec_t *rp, const char *dirv)
{
  char            namebuf[1000];
  data_section_t *nsp;

    if (ParseDName(rp, dirv, namebuf, sizeof(namebuf)) < 0)
        return -1;

    nsp = CdrCreateSection(rp->sys,
                           DSTN_GROUPING,
                           namebuf, strlen(namebuf),
                           NULL, sizeof(data_knob_t));
    if (nsp == NULL)
        return BARK("CdrCreateSection(grouping:%s) failed",
                    namebuf);

    ppf4td_skip_white(rp->ctx);
    return ParseKnobDescr(rp, nsp->data, DATAKNOB_GRPG, 0, 0, NULL);
}

//--------------------------------------------------------------------

static int parse_n_store_section_name_str(const char *type, int with_name,
                                          parse_rec_t *rp, const char *dirv)
{
  char            namebuf[1000];
  char            valbuf [1000];
  int             r;
  data_section_t *nsp;

    if (with_name)
    {
        if (ParseDName(rp, dirv, namebuf, sizeof(namebuf)) < 0)
            return -1;
    }
    else
        namebuf[0] = '\0';

    ppf4td_skip_white(rp->ctx);

    r = ppf4td_get_string(rp->ctx, PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM,
                          valbuf, sizeof(valbuf));
    if (r < 0)
        return BARK("unexpected error parsing %s name; %s",
                     type, ppf4td_strerror(errno));
    ppf4td_skip_white(rp->ctx);
    if (!IsAtEOL(rp))
        return BARK("EOL expected");

    nsp = CdrCreateSection(rp->sys,
                           type,
                           namebuf, strlen(namebuf),
                           valbuf, strlen(valbuf) + 1);
    if (nsp == NULL)
        return BARK("CdrCreateSection(%s%s%s) failed",
                    type, with_name? ":" : "", namebuf);

    return 0;
}

static int sysname_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_SYSNAME,    0, rp, dirv);
}

static int defserver_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_DEFSERVER,  0, rp, dirv);
}

static int treeplace_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_TREEPLACE,  0, rp, dirv);
}

static int wintitle_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_WINTITLE,   1, rp, dirv);
}

static int winname_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_WINNAME,    1, rp, dirv);
}

static int winclass_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_WINCLASS,   1, rp, dirv);
}

static int winopts_processer(parse_rec_t *rp, const char *dirv)
{
    return parse_n_store_section_name_str(DSTN_WINOPTIONS, 1, rp, dirv);
}


//////////////////////////////////////////////////////////////////////

static ft_cmd_t subsys_commands[] =
{
    {"grouping",  grouping_processer},
    {"sysname",   sysname_processer},
    {"defserver", defserver_processer},
    {"treeplace", treeplace_processer},
    {"wintitle",  wintitle_processer},
    {"winname",   winname_processer},
    {"winclass",  winclass_processer},
    {"winopts",   winopts_processer},
    {NULL, NULL}
};

static ft_cmd_t * FindSubsysCommand(const char *name)
{
  ft_cmd_t     *cmdp;

    for (cmdp = subsys_commands;
         cmdp->dirv != NULL;
         cmdp++)
        if (strcasecmp(cmdp->dirv, name) == 0) return cmdp;

    return NULL;
}

DataSubsys  CdrLoadSubsystemViaPpf4td(const char *scheme,
                                      const char *reference)
{
  parse_rec_t   rec;
  ppf4td_ctx_t  ctx;
  int           r;
  int           ch;
  char          keyword[50];
  char         *ch_p;
  ft_cmd_t     *cmdp;

    bzero(&rec, sizeof(rec));

    if ((rec.sys = malloc(sizeof(*rec.sys))) == NULL)
    {
        CdrSetErr("malloc(DataSubsys) failed");
        return NULL;
    }
    bzero(rec.sys, sizeof(*rec.sys));

    r = ppf4td_open(&ctx, scheme, reference);
    if (r != 0)
    {
        CdrSetErr("malloc(DataSubsys) failed");
        goto CLEANUP;
    }
    rec.ctx = &ctx;

    while (1)
    {
        ppf4td_skip_white(rec.ctx);
        if (IsAtEOL(&rec)) goto SKIP_TO_NEXT_LINE;

        if (ParseAName(&rec, "keyword", keyword, sizeof(keyword)) < 0)
            goto CLEANUP;
        for (ch_p = keyword;  *ch_p != '\0';  ch_p++) *ch_p = tolower(*ch_p);

        /* Okay, let's find a processer */
        for (cmdp = subsys_commands;
             cmdp->dirv != NULL  &&  strcasecmp(cmdp->dirv, keyword) != 0;
             cmdp++);

        /* None found? */
        if (cmdp->dirv == NULL)
        {
            CdrSetErr("%s:%d: unknown keyword \"%s\"",
                      ppf4td_cur_ref(rec.ctx), ppf4td_cur_line(rec.ctx),
                      keyword);
            goto CLEANUP;
        }

        /* Call it */
        ppf4td_skip_white(rec.ctx);
        r = cmdp->processer(&rec, keyword);
        if (r < 0) goto CLEANUP;

 SKIP_TO_NEXT_LINE:
        SkipToNextLine(&rec);
        /* Check for EOF */
        if (ppf4td_peekc(rec.ctx, &ch) <= 0) goto END_PARSE_FILE;
    }
 END_PARSE_FILE:

    ppf4td_close(rec.ctx);

    return rec.sys;

 CLEANUP:
    ppf4td_close(rec.ctx);
    CdrDestroySubsystem(rec.sys);
    return NULL;
}
