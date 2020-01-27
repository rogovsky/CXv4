#include <ctype.h>

#include "cxsd_core_commons.h"
#include "cxsd_db_via_ppf4td.h"
/*!!! vvv a temporary for CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN */
#include "cxsd_hwP.h"


#ifndef MAY_USE_INT64
  #define MAY_USE_INT64 1
#endif
#ifndef MAY_USE_FLOAT
  #define MAY_USE_FLOAT 1
#endif


static void CxsdDbList(FILE *fp, CxsdDb db)
{
  int  i,j;
  const char *options;
  const char *auxinfo;
    
    fprintf(fp, "devlist[%d]={\n", db->numdevs);
    for (i = 0; i < db->numdevs;  i++)
    {
        options = CxsdDbGetStr(db, db->devlist[i].options_ofs);
        auxinfo = CxsdDbGetStr(db, db->devlist[i].auxinfo_ofs);

        fprintf(fp, "\tdev %s", CxsdDbGetStr(db, db->devlist[i].instname_ofs));
        if (options != NULL) fprintf(fp, ":%s", options);
        fprintf(fp, " %s", CxsdDbGetStr(db, db->devlist[i].typename_ofs));
        if (db->devlist[i].drvname_ofs > 0) fprintf(fp, "/%s", CxsdDbGetStr(db, db->devlist[i].drvname_ofs));
        if (db->devlist[i].lyrname_ofs > 0) fprintf(fp, "@%s", CxsdDbGetStr(db, db->devlist[i].lyrname_ofs));
        fprintf(fp, " ");
        if (db->devlist[i].changrpcount == 0)
            fprintf(fp, "-");
        else
            for (j = 0;  j < db->devlist[i].changrpcount; j++)
                fprintf(fp, "%s%c%d%c%d",
                        j == 0? "" : ",",
                        db->devlist[i].changroups[j].rw?'w':'r',
                        db->devlist[i].changroups[j].count,
                        '.', db->devlist[i].changroups[j].max_nelems
                        );
        fprintf(fp, " ");
        if (db->devlist[i].businfocount == 0)
            fprintf(fp, "-");
        else
            for (j = 0;  j < db->devlist[i].businfocount; j++)
                fprintf(fp, "%s%d",
                        j == 0? "" : ",",
                        db->devlist[i].businfo[j]
                        );
        if (auxinfo != NULL) fprintf(fp, " %s", auxinfo);
        fprintf(fp, "\n");
    }
    fprintf(fp, "}\n");

    fprintf(fp, "layerinfos[%d]={\n", db->numlios);
    for (i = 0; i < db->numlios;  i++)
        fprintf(fp, "\tlayerinfo %s:%d <%s>\n",
                CxsdDbGetStr(db, db->liolist[i].lyrname_ofs), 
                db->liolist[i].bus_n,
                CxsdDbGetStr(db, db->liolist[i].lyrinfo_ofs));
    fprintf(fp, "}\n");
}

//////////////////////////////////////////////////////////////////////

#define BARK(format, params...)         \
    (fprintf(stderr, "%s %s: %s:%d: ",  \
             strcurtime(), argv0, ppf4td_cur_ref(ctx), ppf4td_cur_line(ctx)), \
     fprintf(stderr, format, ##params), \
     fprintf(stderr, "\n"),             \
     -1)

static int  IsAtEOL(ppf4td_ctx_t *ctx)
{
  int  r;
  int  ch;

    r = ppf4td_is_at_eol(ctx);
    if (r != 0) return r;
    r = ppf4td_peekc    (ctx, &ch);
    if (r < 0)  return r;
    if (ch == EOF) return -1; /*!!! -2?*/
    return ch == '#';

#if 0
    return
      ppf4td_is_at_eol(ctx)  ||
      (ppf4td_peekc(ctx, &ch) > 0  &&  ch == '#');
#endif
}

static int PeekCh(const char *argv0, ppf4td_ctx_t *ctx,
                  const char *name, int *ch_p)
{
  int  r;
  int  ch;

    r = ppf4td_peekc(ctx, &ch);
    if (r < 0)
        return BARK("unexpected error parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (r == 0)
        return BARK("unexpected EOF parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (IsAtEOL(ctx))
        return BARK("unexpected EOL parsing %s",
                    name);

    *ch_p = ch;
    return 0;
}

static int NextCh(const char *argv0, ppf4td_ctx_t *ctx,
                  const char *name, int *ch_p)
{
  int  r;
  int  ch;

    r = ppf4td_peekc(ctx, &ch);
    if (r < 0)
        return BARK("unexpected error parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (r == 0)
        return BARK("unexpected EOF parsing %s; %s",
                    name, ppf4td_strerror(errno));
    if (IsAtEOL(ctx))
        return BARK("unexpected EOL parsing %s",
                    name);

    r = ppf4td_nextc(ctx, &ch);

    *ch_p = ch;
    return 0;
}

static void SkipToNextLine(ppf4td_ctx_t *ctx)
{
  int  ch;

    /* 1. Skip non-CR/LF */
    while (ppf4td_peekc(ctx, &ch) > 0  &&
           (ch != '\r'  &&  ch != '\n')) ppf4td_nextc(ctx, &ch);
    /* 2. Skip all CR/LF (this skips multiple empty lines too) */
    while (ppf4td_peekc(ctx, &ch) > 0  &&
           (ch == '\r'  ||  ch == '\n')) ppf4td_nextc(ctx, &ch);
}

static int ParseXName(const char *argv0, ppf4td_ctx_t *ctx,
                      int flags,
                      const char *name, char *buf, size_t bufsize)
{
  int  r;

    r = ppf4td_get_ident(ctx, flags, buf, bufsize);
    if (r < 0)
    {
        fprintf(stderr, "%s %s: %s:%d: %s expected, %s\n",
                strcurtime(), argv0,
                ppf4td_cur_ref(ctx), ppf4td_cur_line(ctx), name, ppf4td_strerror(errno));
        return -1;
    }

    return 0;
}

static int ParseAName(const char *argv0, ppf4td_ctx_t *ctx,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(argv0, ctx, PPF4TD_FLAG_NONE, name, buf, bufsize);
}

static int ParseDName(const char *argv0, ppf4td_ctx_t *ctx,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(argv0, ctx, PPF4TD_FLAG_DASH, name, buf, bufsize);
}

/*!!!DEVTYPE-DOT-HACK*/
static int ParseMName(const char *argv0, ppf4td_ctx_t *ctx,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(argv0, ctx, PPF4TD_FLAG_DOT,  name, buf, bufsize);
}


//////////////////////////////////////////////////////////////////////

enum
{
    SFT_END = 0,
    SFT_DBSTR,
    SFT_DOUBLE,
    SFT_INT,
    SFT_FLAG,
    SFT_RANGE,
    SFT_ANY,
    SFT_TIME
};

typedef struct
{
    int         type;       // One of SFT_nnn
    const char *name;       // Key
    int         offset;     // Offset in data structure
    int         spc_f_ofs;  // If >0 -- offset of an int to be =1 if this field is specified
    int         var_int;    // SFT_FLAG: value to set to appropriate int; SFT_RANGE: offset of dtype
} somefielddescr_t;

#define SFD_DBSTR(   n, sname, field)        {SFT_DBSTR,  n, offsetof(sname, field), -1,                     0}
#define SFD_DOUBLE  (n, sname, field)        {SFT_DOUBLE, n, offsetof(sname, field), -1,                     0}
#define SFD_DOUBLE_S(n, sname, field, spc_f) {SFT_DOUBLE, n, offsetof(sname, field), offsetof(sname, spc_f), 0}
#define SFD_FLAG(    n, sname, field, val)   {SFT_FLAG,   n, offsetof(sname, field), -1,                     val}
#define SFD_RANGE(   n, sname, field, dt_f)  {SFT_RANGE,  n, offsetof(sname, field), -1,                     offsetof(sname, dt_f)}
#define SFD_ANY(     n, sname, field, dt_f)  {SFT_ANY,    n, offsetof(sname, field), -1,                     offsetof(sname, dt_f)}
#define SFD_END()                            {SFT_END,    NULL, 0,                   -1,                     0}

enum
{
    ENTITY_PARSE_FLAGS =
        PPF4TD_FLAG_NOQUOTES |
        PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM |
        PPF4TD_FLAG_BRCTERM*0,
    STRING_PARSE_FLAGS =
        PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM |
        PPF4TD_FLAG_BRCTERM*0
};
static inline int isletnum(int c)
{
    return isalnum(c)  ||  c == '_'  ||  c == '-';
}
static int ParseSomeProps(const char *argv0,  ppf4td_ctx_t *ctx, CxsdDb db,
                          cxdtype_t dtype,
                          somefielddescr_t *table, void *data_p)
{
  int             fn;  // >= 0 -- index in the table; <0 => switched to "fieldname:"-keyed specification
  void           *ea;  // Executive address
  int             idx;

  int             r;
  int             ch;
  char            keybuf[30];
  int             keylen;
  char            strbuf[1000];
  int             strofs;

  int             is_a_range;
  CxAnyVal_t     *a_p;
  int             subfield;
  const char     *subfield_name;
  char           *p;
  char           *err;
  long            l_v;
  long long       q_v;
  double          d_v;

    fn = 0;
    while (1)
    {
        ppf4td_skip_white(ctx);
        if (IsAtEOL(ctx)) break;

        idx = -1;

        /* Is following a "fieldname:..."? */
        if (PeekCh(argv0, ctx, "fieldname", &ch) < 0) return -1;
        if (isletnum(ch))
        {
            for (keylen = 0;  keylen < sizeof(keybuf) - 1;  keylen++)
            {
                r = ppf4td_peekc(ctx, &ch);
                if (r < 0)
                    return BARK("unexpected error; %s", ppf4td_strerror(errno));
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
                        return BARK("unknown field \"%s\"", keybuf);

                    /* Skip the ':' */
                    ppf4td_nextc(ctx, &ch);
                    keylen = 0;

                    goto END_KEYGET;
                }
                if (r == 0  ||  !isletnum(ch)) goto END_KEYGET;
                /* Consume character */
                ppf4td_nextc(ctx, &ch);
                keybuf[keylen] = ch;
            }
 END_KEYGET:
            if (keylen > 0) ppf4td_ungetchars(ctx, keybuf, keylen);
        }

        /* Wasn't "fieldname:" specified? */
        if (idx < 0)
        {
            /* Are we in random mode, but no "fieldname:" specified? */
            if (fn < 0)
                return BARK("unlabeled value after a labeled one");

            /* Have we reached end of table? */
            if (table[fn].name == NULL)
                return BARK("all fields are specified, but something left in the line");

            /* Okay, just pick the next field */
            idx = fn;
        }

        /* Okay, do parse */
        ea = ((int8 *)data_p) + table[idx].offset;
        if (table[idx].type == SFT_DBSTR)
        {
            r = ppf4td_get_string(ctx, STRING_PARSE_FLAGS, strbuf, sizeof(strbuf));
            if (r < 0)
                return BARK("%s (string) expected; %s",
                            table[idx].name, ppf4td_strerror(errno));

            strofs = CxsdDbAddStr(db, strbuf);
            if (strofs < 0)
                return BARK("can't CxsdDbAddStr(%s)", table[idx].name);
            *((int *)ea) = strofs;
        }
        else if (table[idx].type == SFT_DOUBLE)
        {
#if MAY_USE_FLOAT
            r = ppf4td_get_string(ctx, ENTITY_PARSE_FLAGS, strbuf, sizeof(strbuf));
            if (r < 0)
                return BARK("\"%s\" value (float) expected; %s\n",
                            table[idx].name, ppf4td_strerror(errno));

            d_v = strtod(strbuf, &err);
            if (err == strbuf  ||  *err != '\0')
                return BARK("error in \"%s\" float specification \"%s\"",
                            table[idx].name, strbuf);

            *((double *)ea) = d_v;
#else
            return BARK("floats are disabled at compile-time");
#endif
        }
        else if (table[idx].type == SFT_ANY  ||
                 table[idx].type == SFT_RANGE)
        {
            if (reprof_cxdtype(dtype) != CXDTYPE_REPR_INT  &&
                reprof_cxdtype(dtype) != CXDTYPE_REPR_FLOAT)
                return BARK("\"%s\" specified for a non-numeric channel", table[idx].name);

            is_a_range = table[idx].type == SFT_RANGE;

            a_p = ea;

            r = ppf4td_get_string(ctx, ENTITY_PARSE_FLAGS, strbuf, sizeof(strbuf));
            if (r < 0)
                return BARK("\"%s\" value%s expected; %s\n",
                            table[idx].name, is_a_range? " (MIN-MAX)" : "", ppf4td_strerror(errno));

            for (subfield = 0, p = strbuf;
                 subfield <= (is_a_range? 1 : 0);
                 subfield++)
            {
                /* Prepare a name for errors */
                if      (!is_a_range)     subfield_name = "";
                else if (subfield == 0)   subfield_name = "-min";
                else  /* subfield == 1 */ subfield_name = "-max";

                /* A '-' range separator */
                if (subfield > 0)
                {
                    if (*p != '-')
                        return BARK("'-' expected before \"%s\"%s value",
                                    table[idx].name, subfield_name);
                    p++;
                }

                /* Get a value */
                if (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT)
                {
#if MAY_USE_INT64
                    if (sizeof_cxdtype(dtype) == sizeof(int64))
                        q_v = strtoull(p, &err, 0);
                    else
#endif /* MAY_USE_INT64 */
                        l_v = strtoul (p, &err, 0);
                    if (err == p)
                        return BARK("error in \"%s\"%s int specification \"%s\"",
                                    table[idx].name, subfield_name, p);

                    if      (dtype == CXDTYPE_INT8)   a_p[subfield].i8  = l_v;
                    else if (dtype == CXDTYPE_UINT8)  a_p[subfield].u8  = l_v;
                    else if (dtype == CXDTYPE_INT16)  a_p[subfield].i16 = l_v;
                    else if (dtype == CXDTYPE_UINT16) a_p[subfield].u16 = l_v;
                    else if (dtype == CXDTYPE_INT32)  a_p[subfield].i32 = l_v;
                    else if (dtype == CXDTYPE_UINT32) a_p[subfield].u32 = l_v;
#if MAY_USE_INT64
                    else if (dtype == CXDTYPE_INT64)  a_p[subfield].i64 = q_v;
                    else if (dtype == CXDTYPE_UINT64) a_p[subfield].u64 = q_v;
#endif /* MAY_USE_INT64 */
                    else
                        return BARK("\"%s\" is %zd-byte int, which is unsupported size",
                                    table[idx].name, sizeof_cxdtype(dtype));
                }
                else
                {
#if MAY_USE_FLOAT
                    d_v = strtod(p, &err);
                    if (err == p)
                        return BARK("error in \"%s\"%s float specification \"%s\"",
                                    table[idx].name, subfield_name, p);

                    if (dtype == CXDTYPE_SINGLE)
                        a_p[subfield].f32 = d_v;
                    else
                        a_p[subfield].f64 = d_v;
#else
                    return BARK("floats are disabled at compile-time");
#endif
                }

                /* Advance to next subfield */
                p = err;
            }

            /* Additional check */
            if (*p != '\0')
                return BARK("junk after \"%s\" value in \"%s\"",
                            table[idx].name, strbuf);

            /* Store dtype */
            ea = ((int8 *)data_p) + table[idx].var_int;
            *((cxdtype_t *)ea) = dtype;
        }
        else
            return BARK("%s(): field \"%s\" is of unsupported type %d", __FUNCTION__, table[idx].name, table[idx].type);

        if (table[idx].spc_f_ofs > 0)
        {
            ea = ((int8 *)data_p) + table[idx].spc_f_ofs;
            *((int *)ea) = 1;
        }

        /* Advance field # if not keyed-parsing */
        if (fn >= 0) fn++;
    }

    return 0;
}
//////////////////////////////////////////////////////////////////////

typedef struct
{
    const char  *name;
    int        (*parser)(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db);
} keyworddef_t;

static int GetToken(const char *argv0, ppf4td_ctx_t *ctx, char *buf, size_t bufsize, const char *location)
{
  int  r;

    r = ppf4td_get_string(ctx,
                          PPF4TD_FLAG_NOQUOTES |
                          PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM |
                          PPF4TD_FLAG_COMTERM | PPF4TD_FLAG_SEMTERM,
                          buf, bufsize);
    if (r < 0)
        return BARK("%s; %s", location, ppf4td_strerror(errno));
    if (buf[0] == '0'  &&  0) /*!!! What for?!?!?! */
        return BARK("%s", location);

    return 0;
}

static int ParseChangroup(const char *argv0, ppf4td_ctx_t *ctx,
                          const char *buf,
                          CxsdChanInfoRec *rec, int *n_p)
{
  const char *p = buf;
  char       *endptr;
  char        rw_char;
  char        ut_char;
  char        sg_char;

    rec += *n_p;

    /* 1. R/W */
    rw_char = tolower(*p++);
    if      (rw_char == 'r') rec->rw = 0;
    else if (rw_char == 'w') rec->rw = 1;
    else
        return BARK("chan-group-%d should start with either 'r' or 'w'", *n_p);

    /* 2. # of channels in this segment */
    rec->count = strtol(p, &endptr, 10);
    if (endptr == p)
        return BARK("invalid channel-count spec in chan-group-%d", *n_p);
    if (rec->count < 0)
        return BARK("negative channel-count spec %d in chan-group-%d",
                    rec->count, *n_p);
    p = endptr;

    /* 3. Data type */
    sg_char = '\0';
    ut_char = tolower(*p++);
    // An optional '-' or '+' prefix?  Okay, just store and get next char
    if (ut_char == '-'  ||  ut_char == '+')
    {
        sg_char = ut_char;
        ut_char = tolower(*p++);
    }

    if      (ut_char == 'b') rec->dtype = CXDTYPE_INT8;
    else if (ut_char == 'h') rec->dtype = CXDTYPE_INT16;
    else if (ut_char == 'i') rec->dtype = CXDTYPE_INT32;
    else if (ut_char == 'q') rec->dtype = CXDTYPE_INT64;
    else if (ut_char == 's') rec->dtype = CXDTYPE_SINGLE;
    else if (ut_char == 'd') rec->dtype = CXDTYPE_DOUBLE;
    else if (ut_char == 't') rec->dtype = CXDTYPE_TEXT;
    else if (ut_char == 'u') rec->dtype = CXDTYPE_UCTEXT;
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
    else if (ut_char == 'x') rec->dtype = CXDTYPE_UNKNOWN;
#endif
    else if (ut_char == '\0')
        return BARK("channel-data-type expected after \"%d\" in chan-group-%d",
                    rec->count, *n_p);
    else
        return BARK("invalid channel-data-type in chan-group-%d", *n_p);

    if (sg_char != '\0')
    {
        if (reprof_cxdtype(rec->dtype) != CXDTYPE_REPR_INT)
            return BARK("invalid \"%c%c\" type spec ([un]signedness is for ints only)",
                        sg_char, ut_char);
        if (sg_char == '+') rec->dtype |= CXDTYPE_USGN_MASK;
    }

    /* 4. Optional "Max # of units" */
    if (*p == '\0')
    {
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
        rec->max_nelems = (rec->dtype != CXDTYPE_UNKNOWN)? 1
                                                         : 16/*!!!sizeof(CxAnyVal_t)*/;
#else
        rec->max_nelems = 1;
#endif
        return 0;
    }
    rec->max_nelems = strtol(p, &endptr, 10);
    if (endptr == p)
        return BARK("invalid max-units-count spec in chan-group-%d", *n_p);
    if (rec->max_nelems <= 0)
        return BARK("invalid max-units-count %d in chan-group-%d",
                    rec->max_nelems, *n_p);
    p = endptr;

    /* Finally, that must be end... */
    if (*p != '\0')
        return BARK("junk after max-units-count %d in chan-group-%d",
                    rec->max_nelems, *n_p);

    return +1;
}

static int ParseChanGroupList(const char *argv0, ppf4td_ctx_t *ctx,
                              CxsdChanInfoRec *changroups,
                              int              changroups_allocd,
                              int             *changrpcount_p,
                              int             *chancount_p)
{
  int                r;
  int                ch;
  char               tokbuf[30];
  const char        *loc;

    for (*changrpcount_p = 0, *chancount_p = 0, loc = "chan-group";
         ;
                                                loc = "chan-group after ','")
    {
        if (*changrpcount_p >= changroups_allocd)
            return BARK("too many chan-group-components (>%d)",
                        changroups_allocd);

        if (GetToken(argv0, ctx, tokbuf, sizeof(tokbuf), loc) < 0)
            return -1;

        /* "-" means "no channels" */
        if (*changrpcount_p == 0  &&  strcmp(tokbuf, "-") == 0) break;

        r = ParseChangroup(argv0, ctx, tokbuf,
                           changroups, changrpcount_p);
        if (r < 0) return -1;
        // if (r  == 0) break;

        (*chancount_p) += changroups[*changrpcount_p].count;
        (*changrpcount_p)++;

        r = ppf4td_peekc(ctx, &ch);
        if (r < 0) return -1;
        if (r > 0  &&  ch == ',')
        {
            ppf4td_nextc(ctx, &ch);
        }
        else
            break;
    }

    return 0;
}

static int dev_parser(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db)
{
  CxsdDbDevLine_t    dline;
  int                r;
  int                ch;
  char               instname_buf[50];
  char               typename_buf[50];
  char               drvname_buf [50];
  char               lyrname_buf [50];
  char               options_buf [1000];
  char               auxinfo_buf [4000];
  char               tokbuf[30];
  char              *endptr;
  const char        *loc;
  CxsdDbDcNsp_t     *nsp;
    
    bzero(&dline, sizeof(dline));
    drvname_buf[0] = lyrname_buf[0] = options_buf[0] = auxinfo_buf[0] = '\0';

    /* INSTANCE_NAME */
    if (ParseAName(argv0, ctx, "device-instance-name",
                   instname_buf, sizeof(instname_buf)) < 0) return -1;
    /* Check for duplicate! */
    if (CxsdDbFindDevice(db, instname_buf, -1) >= 0)
        return BARK("duplicate device-name \"%s\"", instname_buf);

    /* [:OPTIONS] */
    r = ppf4td_peekc(ctx, &ch);
    if (r < 0) return -1;
    if (r > 0  &&  ch == ':')
    {
        ppf4td_nextc(ctx, &ch);
        r = ppf4td_get_string(ctx,
                              PPF4TD_FLAG_SPCTERM | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM,
                              options_buf, sizeof(options_buf));
        if (r < 0)
            return BARK("device-options expected after ':'; %s", ppf4td_strerror(errno));
    }

    /* TYPE */
    ppf4td_skip_white(ctx);
    /* Optional '-'/'+' prefix (per-device simulation mode) */
    r = ppf4td_peekc(ctx, &ch);
    if (r < 0) return -1;
    if (r > 0  &&  (ch == '-'  ||  ch == '+'))
    {
        ppf4td_nextc(ctx, &ch);
        dline.is_simulated = (ch == '-')? CXSD_SIMULATE_YES : CXSD_SIMULATE_SUP;
    }
    /* The type itself */
    if (ParseAName(argv0, ctx, "device-type",
                   typename_buf, sizeof(typename_buf)) < 0) return -1;
    dline.type_nsp_id = CxsdDbFindNsp(db, typename_buf);

#if 1
    while (1)
    {
        r = ppf4td_peekc(ctx, &ch);
        if (r < 0) return -1;
        if (r == 0) break;
        if      (ch == '/')
        {
            ppf4td_nextc(ctx, &ch);
            if (ParseAName(argv0, ctx, "driver-name (after '/')",
                           drvname_buf, sizeof(drvname_buf)) < 0) return -1;
        }
        else if (ch == '@')
        {
            ppf4td_nextc(ctx, &ch);
            if (ParseAName(argv0, ctx, "layer-name (after '@')",
                           lyrname_buf, sizeof(lyrname_buf)) < 0) return -1;
        }
        else break;
    }
#else
    /* [/DRIVER] */
    r = ppf4td_peekc(ctx, &ch);
    if (r < 0) return -1;
    if (r > 0  &&  ch == '/')
    {
        ppf4td_nextc(ctx, &ch);
        if (ParseAName(argv0, ctx, "driver-name (after '/')",
                       drvname_buf, sizeof(drvname_buf)) < 0) return -1;
    }

    /* [@LAYER] */
    r = ppf4td_peekc(ctx, &ch);
    if (r < 0) return -1;
    if (r > 0  &&  ch == '@')
    {
        ppf4td_nextc(ctx, &ch);
        if (ParseAName(argv0, ctx, "layer-name (after '@')",
                       lyrname_buf, sizeof(lyrname_buf)) < 0) return -1;
    }
#endif

    /* CHANINFO */
    ppf4td_skip_white(ctx);
    r = ppf4td_peekc(ctx, &ch);
    if (r < 0) return -1;
    if (r > 0  &&  ch == '~')
    {
        ppf4td_nextc(ctx, &ch);
        if (dline.type_nsp_id < 0)
            return BARK("'~' requires a previously defined devtype, but no \"%s\" devtype found",
                        typename_buf);

        nsp = db->nsps_list[dline.type_nsp_id];
        dline.changrpcount = nsp->changrpcount;
        memcpy(dline.changroups, nsp->changroups, sizeof(dline.changroups));
    }
    else
    {
        if (ParseChanGroupList(argv0, ctx,
                               dline.changroups, countof(dline.changroups),
                               &(dline.changrpcount), &(dline.chancount)) < 0)
            return -1;
    }

    /* BUSINFO */
    ppf4td_skip_white(ctx);
    for (dline.businfocount = 0, loc = "bus-id";
         ;
                                 loc = "bus-id after ','")
    {
        if (dline.businfocount >= countof(dline.businfo))
            return BARK("too many bus-info-components (>%d)",
                        countof(dline.businfo));

        if (GetToken(argv0, ctx, tokbuf, sizeof(tokbuf), loc) < 0)
            return -1;
        
        /* "-" means "no bus info" */
        if (dline.businfocount == 0  &&  strcmp(tokbuf, "-") == 0) break;
        
        dline.businfo[dline.businfocount] = strtol(tokbuf, &endptr, 0);
        if (endptr == tokbuf  ||  *endptr != '\0')
            return BARK("invalid bus-id-%d specification \"%s\"",
                        dline.businfocount, tokbuf);

        dline.businfocount++;

        r = ppf4td_peekc(ctx, &ch);
        if (r < 0) return -1;
        if (r > 0  &&  ch == ',')
        {
            ppf4td_nextc(ctx, &ch);
        }
        else
            break;
    }

    /* AUXINFO */
    ppf4td_skip_white(ctx);
    r = ppf4td_get_string(ctx,
                          PPF4TD_FLAG_IGNQUOTES | PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM,
                          auxinfo_buf, sizeof(auxinfo_buf));
    if (r < 0)
        return BARK("auxinfo-string expected; %s", ppf4td_strerror(errno));

    if (CxsdDbAddDev(db, &dline,
                     instname_buf, typename_buf, drvname_buf, lyrname_buf,
                     options_buf, auxinfo_buf))
        return BARK("unable to CxsdDbAddDev(%s): %s",
                    instname_buf, strerror(errno));

    return 0;
}

static somefielddescr_t dcpr_fields[] =
{
    SFD_DOUBLE_S("r",       CxsdDbDcPrInfo_t, phys_rds[0], phys_rd_specified),
    SFD_DOUBLE_S("d",       CxsdDbDcPrInfo_t, phys_rds[1], phys_rd_specified),
    SFD_DBSTR   ("ident",   CxsdDbDcPrInfo_t, ident_ofs),
    SFD_DBSTR   ("label",   CxsdDbDcPrInfo_t, label_ofs),
    SFD_DBSTR   ("tip",     CxsdDbDcPrInfo_t, tip_ofs),
    SFD_DBSTR   ("comment", CxsdDbDcPrInfo_t, comment_ofs),
    SFD_DBSTR   ("geoinfo", CxsdDbDcPrInfo_t, geoinfo_ofs),
    SFD_DBSTR   ("rsrvd6",  CxsdDbDcPrInfo_t, rsrvd6_ofs),
    SFD_DBSTR   ("units",   CxsdDbDcPrInfo_t, units_ofs),
    SFD_DBSTR   ("dpyfmt",  CxsdDbDcPrInfo_t, dpyfmt_ofs),
    SFD_RANGE   ("range",   CxsdDbDcPrInfo_t, range,       range_dtype),
    SFD_ANY     ("quant",   CxsdDbDcPrInfo_t, q,           q_dtype),
    SFD_FLAG    ("autoupdated_not",     CxsdDbDcPrInfo_t, return_type, IS_AUTOUPDATED_NOT),
    SFD_FLAG    ("autoupdated_yes",     CxsdDbDcPrInfo_t, return_type, IS_AUTOUPDATED_YES),
    SFD_FLAG    ("autoupdated_trusted", CxsdDbDcPrInfo_t, return_type, IS_AUTOUPDATED_TRUSTED),
    SFD_FLAG    ("ignore_upd_cycle",    CxsdDbDcPrInfo_t, return_type, DO_IGNORE_UPD_CYCLE),
    SFD_END()
};
static int ParseChanList(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db,
                         CxsdDbDcNsp_t **nsp_p, int type_nsp_id,
                         int chan_n_limit,
                         int changrpcount, CxsdChanInfoRec changroups[])
{
  CxsdDbDcNsp_t     *type_nsp = CxsdDbGetNsp(db, type_nsp_id);
////  int                chan_n_limit;
  int                r;
  int                ch;
  char               namebuf[50];
  char               refcbuf[50];
  int                refid;
  int                refval;
  char              *endptr;
  int                nsline;

  int                range_min;
  int                range_max;
  int                range_i;
  char               intbuf[20];
  char               prefix[50];
  char               suffix[50];

  CxsdDbDcPrInfo_t   dcpr_data;
  int                dcpr_id;
  int                changrp_n;
  int                changrp_base;
  cxdtype_t          dtype;

    /* Get names line-by-line */
    while (1)
    {
 RE_DO_LINE:
        /* Skip whitespace... */
        ppf4td_skip_white(ctx);
        /* ...and empty lines, ... */
        r = IsAtEOL(ctx);
        /*!!!(check for ==0?) ...but carefully, not to loop infinitely upon EOF */
        if (r < 0)
            return BARK("error while expecting something");
        if (r > 0)
        {
            SkipToNextLine(ctx);
            goto RE_DO_LINE;
        }

        /* Okay, what's than: a '}' or a name? */
        if (PeekCh(argv0, ctx, "devtype-'}'", &ch) < 0)
            return -1;
        if (ch == '}')
        {
            ppf4td_nextc(ctx, &ch);
            /* ...and an EOL */
            ppf4td_skip_white(ctx);
            if (!IsAtEOL(ctx))
                return BARK("EOL expected after '}'");
            /* Note: SHOULD NOT call SkipToNextLine(ctx), since that is
               done in main read-loop */

            goto END_PARSE;
        }

        range_min = range_max = -1;
        /* Get a name */
        /*!!!DEVTYPE-DOT-HACK*/
        if (ParseMName(argv0, ctx, "channel-name",
                       namebuf, sizeof(namebuf)) < 0) return -1;
        if (PeekCh(argv0, ctx, "range-'<'", &ch) < 0)
            return -1;
        if (ch == '<')
        {
            ppf4td_nextc(ctx, &ch);
            // min
            if (ParseAName(argv0, ctx, "name range-min after '<'",
                           intbuf, sizeof(intbuf)) < 0) return -1;
            range_min = strtol(intbuf, &endptr, 10);
            if (endptr == refcbuf  ||  *endptr != '\0')
                return BARK("invalid range-min \"%s\"", intbuf);
            if (range_min < 0) // In fact, an impossible condition -- ParseAName() doesn't yeild idents with dashes
                return BARK("negative range-min \"%d\"", range_min);
            // '-' separator
            if (NextCh(argv0, ctx, "range-'-'", &ch) < 0)
                return -1;
            if (ch != '-')
                return BARK("'-' expected after name range-min");

            // max
            if (ParseAName(argv0, ctx, "name range-max",
                           intbuf, sizeof(intbuf)) < 0) return -1;
            range_max = strtol(intbuf, &endptr, 10);
            if (endptr == refcbuf  ||  *endptr != '\0')
                return BARK("invalid range-max \"%s\"", intbuf);
            if (range_max < range_min)
                return BARK("range-max<range-min (%d<%d)", range_max, range_min);
            // closing '>'
            if (NextCh(argv0, ctx, "range-'>'", &ch) < 0)
                return -1;
            if (ch != '>')
                return BARK("'>' expected after name range-min");

            // suffix
            if (PeekCh(argv0, ctx, "something after '>'", &ch) < 0)
                return -1;
            if (isalnum(ch)  ||  ch == '_'  ||  ch == '.'/*!!!DEVTYPE-DOT-HACK*/)
            {
                /*!!!DEVTYPE-DOT-HACK*/
                if (ParseMName(argv0, ctx, "name-suffix after '>'",
                               suffix, sizeof(suffix)) < 0) return -1;
            }
            else
                suffix[0] = '\0';
            strzcpy(prefix, namebuf, sizeof(prefix));
        }
        else
        {
            /* Check it isn't a duplicate */
            r = CxsdDbNspSrch(db, *nsp_p, namebuf);
            if (r >= 0)
                return BARK("duplicate channel-name \"%s\"", namebuf);
        }
//if (range_min >= 0) fprintf(stderr, "'%s'<%d,%d>'%s'\n", prefix, range_min, range_max, suffix);
//else                fprintf(stderr, "'%s'\n", namebuf);
        
        /* Get a reference */
        ppf4td_skip_white(ctx);
        /*!!!DEVTYPE-DOT-HACK*/
        if (ParseMName(argv0, ctx, "channel-reference",
                       refcbuf, sizeof(refcbuf)) < 0) return -1;
        /* Resolve reference */
        refval = -1;
        /* First try by name... */
        if (range_min < 0 /* NO aliases for range specs */)
        {
            /* a) In current namespace */
            if ((refid = CxsdDbNspSrch(db, *nsp_p, refcbuf)) >= 0)
                refval = (*nsp_p)->items[refid].devchan_n;
            /* b) In devtype namespace, if it is present */
            if (refval < 0  &&  type_nsp != NULL  &&
                (refid = CxsdDbNspSrch(db, type_nsp, refcbuf)) >= 0)
                refval = type_nsp->items[refid].devchan_n;
        }
        /* ...else as a number */
        if (refval < 0)
        {
            refval = strtol(refcbuf, &endptr, 10);
            if (endptr == refcbuf  ||  *endptr != '\0')
                return BARK("invalid channel-reference \"%s\"", refcbuf);
            if (refval < 0  ||  refval >= chan_n_limit)
                return BARK("channel number %d is out of range [0-%d)",
                            refval, chan_n_limit);
        }

        /* Any properties? */
        ppf4td_skip_white(ctx);
        dcpr_id = -1;
        if (!IsAtEOL(ctx))
        {
            bzero(&dcpr_data, sizeof(dcpr_data));
            dcpr_data.fresh_age.sec = -1;
            dcpr_data.ident_ofs   = -1;
            dcpr_data.label_ofs   = -1;
            dcpr_data.tip_ofs     = -1;
            dcpr_data.comment_ofs = -1;
            dcpr_data.geoinfo_ofs = -1;
            dcpr_data.rsrvd6_ofs  = -1;
            dcpr_data.units_ofs   = -1;
            dcpr_data.dpyfmt_ofs  = -1;

            for (changrp_base = 0,          changrp_n = 0, dtype = CXDTYPE_UNKNOWN;
                 changrp_n < changrpcount;
                 changrp_base += changroups[changrp_n++].count)
                if (refval >= changrp_base  /* <- a redundant cond */ &&
                    refval <  changrp_base + changroups[changrp_n].count)
                {
                    dtype = changroups[changrp_n++].dtype;
                    goto DTYPE_FOUND;
                }
 DTYPE_FOUND:;

            if (ParseSomeProps(argv0, ctx, db,
                               dtype,
                               dcpr_fields, &dcpr_data)) return -1;
            if ((dcpr_id = CxsdDbAddDcPr(db, dcpr_data)) < 0)
                return BARK("unable to CxsdDbAddDcPr(): %s", strerror(errno));
        }

        /* Everything got -- okay, record data... */
        if (range_min < 0)
        {
            nsline = CxsdDbNspAddL(db, nsp_p, namebuf);
            if (nsline < 0)
                return BARK("unable to CxsdDbNspAddL(%s): %s", namebuf, strerror(errno));
            (*nsp_p)->items[nsline].devchan_n = refval;
            (*nsp_p)->items[nsline].dcpr_id   = dcpr_id;
        }
        else
        {
            /* Note: no reason to check range_min, since it is >=0 and range_max>=range_min */
            if (refval + range_max-range_min >= chan_n_limit)
                return BARK("range <%d-%d> -> [%d,%d] is out of device range [0-%d]",
                            range_min, range_max,
                            refval, refval + range_max-range_min,
                            chan_n_limit-1);
            for (range_i = range_min;  range_i <= range_max;  range_i++)
            {
                r = snprintf(namebuf, sizeof(namebuf), "%s%d%s",
                             prefix, range_i, suffix);
                if (r < 0  ||  (size_t)r > sizeof(namebuf)-1)
                    namebuf[sizeof(namebuf)-1] = '\0';

                /* Check it isn't a duplicate */
                r = CxsdDbNspSrch(db, *nsp_p, namebuf);
                if (r >= 0)
                    return BARK("duplicate channel-name \"%s\"", namebuf);

                nsline = CxsdDbNspAddL(db, nsp_p, namebuf);
                if (nsline < 0)
                    return BARK("unable to CxsdDbNspAddL(%s): %s", namebuf, strerror(errno));
                (*nsp_p)->items[nsline].devchan_n = refval + range_i-range_min;
                (*nsp_p)->items[nsline].dcpr_id   = dcpr_id;
            }
        }

        /* ...and go to next line */
        ppf4td_skip_white(ctx);
        if (!IsAtEOL(ctx))
            return BARK("EOL expected after channel-reference"); /*!!! name!!!*/
        SkipToNextLine(ctx);
    }
 END_PARSE:;

    return 0;
}

static int channels_parser(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db)
{
  char               instname_buf[50];
  const char        *dev_instname;
  int                r;
  int                ch;
  CxsdDbDcNsp_t     *nsp;
  int                devid;

    /* TYPE */
    if (ParseAName(argv0, ctx, "device-instance-name",
                   instname_buf, sizeof(instname_buf)) < 0) return -1;

    /* Find specified device */
    for (devid = 0;  devid < db->numdevs;  devid++)
        if ((dev_instname = CxsdDbGetStr(db, db->devlist[devid].instname_ofs)) != NULL  &&
            strcasecmp(instname_buf, dev_instname) == 0)
            break;
    if (devid >= db->numdevs)
        return BARK("no such device \"%s\"", instname_buf);

    /* A required opening '{' */
    ppf4td_skip_white(ctx);
    if (NextCh(argv0, ctx, "channels-'{'", &ch) < 0) return -1;
    if (ch != '{')
        return BARK("'{' expected");
    /* ...and an EOL */
    ppf4td_skip_white(ctx);
    if (!IsAtEOL(ctx))
        return BARK("EOL expected after '{'");
    SkipToNextLine(ctx);

    /* Okay, at this point we can allocate nsp */
    nsp = malloc(sizeof(*nsp));
    if (nsp == NULL)
        return BARK("unable to malloc() nsp");
    bzero(nsp, sizeof(*nsp));

    r = ParseChanList(argv0, ctx, db, &nsp, db->devlist[devid].type_nsp_id,
                      db->devlist[devid].chancount, db->devlist[devid].changrpcount, db->devlist[devid].changroups);
    if (r < 0) goto CLEANUP;

    if ((db->devlist[devid].chan_nsp_id = CxsdDbAddNsp(db, nsp)) < 0)
    {
        BARK("unable to CxsdDbAddNsp(channels %s): %s",
             instname_buf, strerror(errno));
        goto CLEANUP;
    }

    return 0;

 CLEANUP:
    free(nsp);
    return -1;
}

static int devtype_parser(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db)
{
  CxsdDbDcNsp_t      spc;
  char               typename_buf[100];
  int                r;
  int                ch;
  CxsdDbDcNsp_t     *nsp;

    bzero(&spc, sizeof(spc));

    /* TYPE */
    if (ParseAName(argv0, ctx, "device-type",
                   typename_buf, sizeof(typename_buf)) < 0) return -1;
    /* Check for duplicate! */
    r = CxsdDbFindNsp(db, typename_buf);
    if (r > 0)
        return BARK("duplicate device-type \"%s\"", typename_buf);

    /* CHANINFO */
    ppf4td_skip_white(ctx);
    if (ParseChanGroupList(argv0, ctx,
                           spc.changroups, countof(spc.changroups),
                           &(spc.changrpcount), &(spc.chancount)) < 0)
        return -1;

    /* A required opening '{' */
    ppf4td_skip_white(ctx);
    if (NextCh(argv0, ctx, "devtype-'{'", &ch) < 0) return -1;
    if (ch != '{')
        return BARK("'{' expected");
    /* ...and an EOL */
    ppf4td_skip_white(ctx);
    if (!IsAtEOL(ctx))
        return BARK("EOL expected after '{'");
    SkipToNextLine(ctx);

    /* Okay, at this point we can allocate nsp */
    spc.typename_ofs = CxsdDbAddStr(db, typename_buf);
    if (spc.typename_ofs <= 0)
        return BARK("unable to CxsdDbAddStr(devtype)");
    nsp = malloc(sizeof(*nsp));
    if (nsp == NULL)
        return BARK("unable to malloc() nsp");
    /* ...and fill initial info */
    *nsp = spc;

    r = ParseChanList(argv0, ctx, db, &nsp, -1,
                      spc.chancount, spc.changrpcount, spc.changroups);
    if (r < 0) goto CLEANUP;

#if 0
    fprintf(stderr, "after devtype %s strbuf_used=%zd\n", nsp->typename, db->strbuf_used);
    {
      int  x;
      for (x = 0; x < db->strbuf_used;  x += strlen(db->strbuf + x) + 1)
          fprintf(stderr, "<%s>", db->strbuf + x);
      fprintf(stderr, "\n");
    }
#endif

    if (CxsdDbAddNsp(db, nsp) < 0)
    {
        BARK("unable to CxsdDbAddNsp(%s): %s",
             typename_buf, strerror(errno));
        goto CLEANUP;
    }

    return 0;

 CLEANUP:
    free(nsp);
    return -1;
}


static somefielddescr_t cpnt_fields[] =
{
    SFD_DOUBLE_S("r",       CxsdDbCpntInfo_t, phys_rds[0], phys_rd_specified),
    SFD_DOUBLE_S("d",       CxsdDbCpntInfo_t, phys_rds[1], phys_rd_specified),
    SFD_DBSTR   ("ident",   CxsdDbCpntInfo_t, ident_ofs),
    SFD_DBSTR   ("label",   CxsdDbCpntInfo_t, label_ofs),
    SFD_DBSTR   ("tip",     CxsdDbCpntInfo_t, tip_ofs),
    SFD_DBSTR   ("comment", CxsdDbCpntInfo_t, comment_ofs),
    SFD_DBSTR   ("geoinfo", CxsdDbCpntInfo_t, geoinfo_ofs),
    SFD_DBSTR   ("rsrvd6",  CxsdDbCpntInfo_t, rsrvd6_ofs),
    SFD_DBSTR   ("units",   CxsdDbCpntInfo_t, units_ofs),
    SFD_DBSTR   ("dpyfmt",  CxsdDbCpntInfo_t, dpyfmt_ofs),
    SFD_END()
};
static int ParseCpointProps(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db,
                            CxsdDbCpntInfo_t *cpnt_data_p)
{
    return ParseSomeProps(argv0, ctx, db,
                          CXDTYPE_UNKNOWN,
                          cpnt_fields, cpnt_data_p);
}

static void DumpStrs(CxsdDb db)
{
  int  ofs;

    fprintf(stderr, "strbuf[%zd/%zd]=", db->strbuf_used, db->strbuf_allocd);
    for (ofs = 0;
         ofs < db->strbuf_used;
         ofs += strlen(db->strbuf + ofs) + 1)
        fprintf(stderr, "%s[%d]=\"%s\"", ofs==0?"":" ", ofs, db->strbuf + ofs);
    fprintf(stderr, "\n");
}
static int ParseOneCpoint  (const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db,
                            char *basebuf, size_t basebufsize,
                            size_t basebufused);
static int ParseCpointGroup(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db,
                            char *basebuf, size_t basebufsize,
                            size_t basebufused)
{
  int     r;
  int     ch;

    while (1)
    {
 RE_DO_LINE:
        /* Skip whitespace... */
        ppf4td_skip_white(ctx);
        /* ...and empty lines, ... */
        r = IsAtEOL(ctx);
        /*!!!(check for ==0?) ...but carefully, not to loop infinitely upon EOF */
        if (r < 0)
            return BARK("error while expecting something");
        if (r > 0)
        {
            SkipToNextLine(ctx);
            goto RE_DO_LINE;
        }

        /* Okay, what's than: a '}' or something else? */
        if (PeekCh(argv0, ctx, "devtype-'}'", &ch) < 0)
            return -1;
        if (ch == '}')
        {
            ppf4td_nextc(ctx, &ch);
            /* ...and an EOL */
            ppf4td_skip_white(ctx);
            if (!IsAtEOL(ctx))
                return BARK("EOL expected after '}'");
            /* Note: SHOULD NOT call SkipToNextLine(ctx), since that is
               done bu caller */

            return 0;
        }

        /* Do work... */
        r = ParseOneCpoint(argv0, ctx, db, basebuf, basebufsize, basebufused);
        if (r < 0) return r;

        /* ...and go to next line */
        ppf4td_skip_white(ctx);
        if (!IsAtEOL(ctx))
            return BARK("EOL expected after channel-reference");
        SkipToNextLine(ctx);
    }
}
static int ParseOneCpoint  (const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db,
                            char *basebuf, size_t basebufsize,
                            size_t basebufused)
{
  size_t            size_left       = basebufsize - basebufused;
  size_t            basebufused_ini = basebufused;
  int               xtra_cpoint_fbd = 0;
  int               r;
  int               is_eol;
  int               ch;
  size_t            len;
  char              targetbuf[500];
  size_t            targ_used;
  size_t            targ_left;

  int               targ_type;
  int               targ_devid;
  int               targ_chann;

  int               parent_clvl_id;
  char             *p;
  char             *dot_p;
  int               last;
  CxsdDbClvlItem_t  item_data;
  CxsdDbCpntInfo_t  cpnt_data;

    /* Get name (appending it to previously buffered) */
    while (1)
    {
        if (ParseAName(argv0, ctx, "cpoint-name-component",
                       basebuf + basebufused, size_left) < 0) return -1;

        /* Allow optional "cpoint" at the beginning of line */
        if (strcasecmp(basebuf + basebufused, "cpoint") == 0)
        {
            if (xtra_cpoint_fbd)
                return BARK("too many consecutive \"cpoint\" keywords");
            xtra_cpoint_fbd = 1;
            ppf4td_skip_white(ctx);
            continue;
        }
        xtra_cpoint_fbd = 1;

        len = strlen(basebuf + basebufused);
        basebufused += len;
        size_left   -= len;
        if (PeekCh(argv0, ctx, "cpoint-name-'.'", &ch) < 0) return -1;
        if (ch == '.')
        {
            if (size_left < 3 /* 3: '.x\0' */)
                return BARK("cpoint name too long (buffer size is %zd characters)",
                            basebufsize - 1);
            ppf4td_nextc(ctx, &ch);
            basebuf[basebufused]   = '.';
            basebuf[basebufused+1] = '\0';
            basebufused++;
            size_left  --;
        }
        else break;
    }

    ppf4td_skip_white(ctx);
    if (PeekCh(argv0, ctx, "cpoint-'{'", &ch) < 0) return -1;
    if (ch == '{')
    {
        if (size_left < 3 /* 3: '.x\0' */)
            return BARK("cpoint name too long (buffer size is %zd characters)",
                        basebufsize - 1);
        ppf4td_nextc(ctx, &ch);
        basebuf[basebufused]   = '.';
        basebuf[basebufused+1] = '\0';
        basebufused++;
        size_left  --;

        /* ...and an EOL */
        ppf4td_skip_white(ctx);
        if (!IsAtEOL(ctx))
            return BARK("EOL expected after '{'");
        SkipToNextLine(ctx);

        return ParseCpointGroup(argv0, ctx, db,
                                basebuf, basebufsize, basebufused);
    }
    else
    {
        /* Parse target spec */
        targ_used = 0;
        targ_left = sizeof(targetbuf) - targ_used;

#if 0
        /* An optional '.' (from-root) prefix */
        if (ch == '.')
        {
            targetbuf[targ_used] = '.';
            targ_used++;
            targ_left--;
        }
#endif

        for (is_eol = 0;;)
        {
            if (ParseAName(argv0, ctx, "target-name-component",
                           targetbuf + targ_used, targ_left) < 0) return -1;
            len = strlen(targetbuf + targ_used);
            targ_used += len;
            targ_left -= len;
            is_eol = IsAtEOL(ctx);
            if (is_eol < 0) return BARK("error while expecting something");
            if (is_eol > 0) break;
            if (PeekCh(argv0, ctx, "cpoint-name-'.'", &ch) < 0) return -1;
            if (ch == '.')
            {
                if (targ_left < 3 /* 3: '.x\0' */)
                    return BARK("cpoint target name too long (buffer size is %zd characters)",
                                sizeof(targetbuf) - 1);
                ppf4td_nextc(ctx, &ch);
                targetbuf[targ_used]   = '.';
                targetbuf[targ_used+1] = '\0';
                targ_used++;
                targ_left--;
            }
            else break;
        }
        while (!is_eol)
        {
            is_eol = IsAtEOL(ctx);
            if (is_eol < 0) return BARK("error while expecting something");
            if (is_eol > 0) break;
            if (PeekCh(argv0, ctx, "something", &ch) < 0) return -1;
            if (isspace(ch)) NextCh(argv0, ctx, "something", &ch);
            else break;
        }

        /* And register cpoint */

	/* First, resolve the target and determine its type */
	targ_type = CxsdDbResolveName(db, targetbuf, &targ_devid, &targ_chann);
////fprintf(stderr, "(%s)->(%s), targ_type=%d\n", basebuf, targetbuf, targ_type);
        if (targ_type < 0  &&  0)
            DumpStrs(db);
        if (targ_type < 0)
            return BARK("cpoint target \"%s\" not found", targetbuf);
        if (targ_type == CXSD_DB_RESOLVE_GLOBAL)
            return BARK("global cpoint targets are forbidden");

        /* Second, go through cpoint name component-by-component,
           to check if it is possible (i.e. some of its components
           isn't a clevel but a cpoint) */
        for (p = basebuf,   parent_clvl_id = -1,         last = 0;
             !last;
             p = dot_p + 1, parent_clvl_id = item_data.ref_n)
        {
            dot_p = strchr(p, '.');
            if (dot_p == NULL)
            {
                dot_p = p + strlen(p);
                last = 1;
            }
            len = dot_p - p;

            r = CxsdDbClvlFindItem(db, parent_clvl_id, p, len, &item_data);
            if (r < 0) break; // "Level not found" is OK -- will be created

            if (last)
                return BARK("cpoint \"%s\" already exists", basebuf);
            if (item_data.type != CXSD_DB_CLVL_ITEM_TYPE_CLEVEL)
                return BARK("intermediate component isn't a container");
        }

        /* Third, create missing intermediate components (if any) */
        for (;
             !last;
             p = dot_p + 1, parent_clvl_id = item_data.ref_n)
        {
            dot_p = strchr(p, '.');
            if (dot_p == NULL)
            {
                dot_p = p + strlen(p);
                last = 1;
                goto END_CREATE_INTERMEDIATE;
            }
            len = dot_p - p;

            bzero(&item_data, sizeof(item_data));
            // Name
            if ((item_data.name_ofs = CxsdDbAddMem(db, p, len)) < 0)
                return BARK("can't CxsdDbAddMem() intermediate component");
            item_data.type = CXSD_DB_CLVL_ITEM_TYPE_CLEVEL;
            // Unnamed clvl inode
            if ((item_data.ref_n = CxsdDbAddClvl(db)) < 0)
                return BARK("can't CxsdDbAddClvl() intermediate component");
            // Named link to inode
            if (CxsdDbClvlAddItem(db, parent_clvl_id, item_data) < 0)
                return BARK("can't CxsdDbClvlAddItem() intermediate component");
        }
 END_CREATE_INTERMEDIATE:;

        /* Fourth, the node itself */
        bzero(&item_data, sizeof(item_data));
        if ((item_data.name_ofs = CxsdDbAddStr(db, p)) < 0)
            return BARK("can't CxsdDbAddStr() cpoint");

        if (targ_type == CXSD_DB_RESOLVE_DEVCHN  ||
            targ_type == CXSD_DB_RESOLVE_CPOINT)
        {
            bzero(&cpnt_data, sizeof(cpnt_data));
            cpnt_data.phys_rds[0] = 1.0;
            cpnt_data.phys_rds[1] = 0.0;
            if (targ_type == CXSD_DB_RESOLVE_DEVCHN)
            {
                cpnt_data.devid = targ_devid;
            }
            cpnt_data.ref_n = targ_chann;

            /* Here may parse additional parameters */
            if (!is_eol  &&
                ParseCpointProps(argv0, ctx, db, &cpnt_data) < 0) return -1;

            item_data.ref_n = CxsdDbAddCpnt(db, cpnt_data);
        }
        else
        {
            /* Mandatory EOL */
            if (!is_eol) return BARK("EOL expected after cpoint-target");
        }

        if      (targ_type == CXSD_DB_RESOLVE_DEVCHN)
        {
            item_data.type = CXSD_DB_CLVL_ITEM_TYPE_DEVCHN;
        }
        else if (targ_type == CXSD_DB_RESOLVE_CPOINT)
        {
            item_data.type  = CXSD_DB_CLVL_ITEM_TYPE_CPOINT;
        }
        else if (targ_type == CXSD_DB_RESOLVE_DEVICE)
        {
            item_data.type  = CXSD_DB_CLVL_ITEM_TYPE_DEVICE;
            item_data.ref_n = targ_devid;
        }
        else if (targ_type == CXSD_DB_RESOLVE_CLEVEL)
        {
            item_data.type  = CXSD_DB_CLVL_ITEM_TYPE_CLEVEL;
            item_data.ref_n = targ_chann;
        }
        else
            return BARK("don't know how to create cpoints to targets of type %d",
                        targ_type);

        if (CxsdDbClvlAddItem(db, parent_clvl_id, item_data) < 0)
            return BARK("can't CxsdDbClvlAddItem() cpoint");

        return 0;
    }
}
static int cpoint_parser(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db)
{
  char  namebuf[1000];

    return ParseOneCpoint(argv0, ctx, db,
                          namebuf, sizeof(namebuf), 0);
}

static int layerinfo_parser(const char *argv0, ppf4td_ctx_t *ctx, CxsdDb db)
{
  char               lyrname_buf[50];
  int                bus_n;
  char               lyrinfo_buf[1000];
  int                r;
  int                ch;
  char               numbuf[10];
  char              *endptr;

    /* Note: ParseDName() is used instead of ParseAName() to enable
       specification of HOSTNAME[:N] layerinfos (since hostnames may
       contain dashes). */
    if (ParseDName(argv0, ctx, "layer-name",
                   lyrname_buf, sizeof(lyrname_buf)) < 0) return -1;
    r = ppf4td_peekc(ctx, &ch);
    if (r < 0) return -1;
    if (r > 0  &&  ch == ':')
    {
        ppf4td_nextc(ctx, &ch);
        if (ParseAName(argv0, ctx, "bus-number",
                       numbuf, sizeof(numbuf)) < 0) return -1;
        bus_n = strtol(numbuf, &endptr, 0);
        if (endptr == numbuf  ||  *endptr != '\0')
            return BARK("invalid bus-number specification \"%s\"", numbuf);
    }
    else
        bus_n = -1;

    if (!IsAtEOL(ctx)  &&  ppf4td_peekc(ctx, &ch) > 0  &&  !isspace(ch))
        return BARK("junk after layer-name[:bus-number] spec");
    ppf4td_skip_white(ctx);

    r = ppf4td_get_string(ctx, PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_EOLTERM,
                          lyrinfo_buf, sizeof(lyrinfo_buf));
    if (r < 0)
        return BARK("layer-info-string expected; %s", ppf4td_strerror(errno));

    ////fprintf(stderr, "LAYERINFO <%s>:%d<%s>\n", lyrname_buf, bus_n, lyrinfo_buf);
    if (CxsdDbAddLyrI(db, lyrname_buf, bus_n, lyrinfo_buf))
        return BARK("unable to CxsdDbAddLyrI(%s:%d): %s",
                    lyrname_buf, bus_n, strerror(errno));

    return 0;
}

static keyworddef_t keyword_list[] =
{
    {"dev",       dev_parser},
    {"channels",  channels_parser},
    {"devtype",   devtype_parser},
    {"cpoint",    cpoint_parser},
    {"layerinfo", layerinfo_parser},
    {NULL, NULL}
};

CxsdDb CxsdDbLoadDbViaPPF4TD(const char *argv0, ppf4td_ctx_t *ctx)
{
  CxsdDb        db  = NULL;

  int           r;
  int           ch;

  char          keyword[50];
  keyworddef_t *kdp;

    db = CxsdDbCreate();
    if (db == NULL)
    {
        fprintf(stderr, "%s %s: unable to CxsdDbCreate()\n",
                strcurtime(), argv0);
        goto FATAL;
    }

    while (1)
    {
        ppf4td_skip_white(ctx);
        if (IsAtEOL(ctx)) goto SKIP_TO_NEXT_LINE;

        if (ParseAName(argv0, ctx, "keyword", keyword, sizeof(keyword)) < 0) goto FATAL;

        for (kdp = keyword_list;  kdp->name != NULL;  kdp++)
            if (strcasecmp(keyword, kdp->name) == 0) break;
        if (kdp->name == NULL)
        {
            fprintf(stderr, "%s %s: %s:%d: unknown keyword \"%s\"\n",
                    strcurtime(), argv0, ppf4td_cur_ref(ctx), ppf4td_cur_line(ctx),
                    keyword);
            goto FATAL;
        }

        ////fprintf(stderr, "->%s\n", kdp->name);
        ppf4td_skip_white(ctx);
        r = kdp->parser(argv0, ctx, db);
        if (r != 0) goto FATAL;

 SKIP_TO_NEXT_LINE:
        SkipToNextLine(ctx);
        /* Check for EOF */
        if (ppf4td_peekc(ctx, &ch) <= 0) goto END_PARSE_FILE;
    }
 END_PARSE_FILE:

    ppf4td_close(ctx);

    if (0) CxsdDbList(stderr, db);

    return db;

 FATAL:
    ppf4td_close(ctx);
    CxsdDbDestroy(db);
    return NULL;
}
