#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cx.h"
#include "cxlib.h" // For cx_strrflag_*()
#include "cda.h"


#include "console_cda_util.h"   


static int EC_USR_ERR_is_fatal = 1;

void console_cda_util_EC_USR_ERR_mode(int is_fatal)
{
    EC_USR_ERR_is_fatal = is_fatal;
}

#define RAISE_EC_USR_ERR()                         \
    do {                                           \
        if (EC_USR_ERR_is_fatal) exit(EC_USR_ERR); \
        else return -1;}                           \
    while (0)


static void PrintChar(FILE *fp, char32 c32, int unescaped)
{
    if      ((c32 & 0x000000FF) == c32)
    {
        if      (unescaped)
            fprintf(fp, "%c", c32);
        else if (c32 == '\\')
            fprintf(fp, "\\\\");
        else if (c32 == '\'')
            fprintf(fp, "\\'");
        else if (c32 == '\"')
            fprintf(fp, "\\\"");
        else if (isprint(c32))
            fprintf(fp, "%c", c32);
        else if (c32 == '\a')
            fprintf(fp, "\\a");
        else if (c32 == '\b')
            fprintf(fp, "\\b");
        else if (c32 == '\f')
            fprintf(fp, "\\f");
        else if (c32 == '\n')
            fprintf(fp, "\\n");
        else if (c32 == '\r')
            fprintf(fp, "\\r");
        else if (c32 == '\t')
            fprintf(fp, "\\t");
        else if (c32 == '\v')
            fprintf(fp, "\\v");
        else
            fprintf(fp, "\\x%02x", c32);
    }
    else if ((c32 & 0x0000FFFF) == c32)
    {
        fprintf(fp, "\\u%04x", c32);
    }
    else
    {
        fprintf(fp, "\\U%08x", c32);
    }
}

void PrintDatarefData(FILE *fp, util_refrec_t *urp, int parts)
{
  int            max_nelems;
  int            cur_nelems;
  cxdtype_t      dtype;
  size_t         size;
  CxAnyVal_t     val;
  char32         c32;
  int            n;
  const char    *src_p;
  cx_time_t      timestamp;

  rflags_t       rflags;
  int            shift;
  int            notfirst;

  char           buf[400];
  char          *p;
  int            len;

  static char    int_type_chars[] = {'X', 'b', 'h', 'X', 'i', 'X', 'X', 'X', 'q'};

    if (cda_src_of_ref(urp->ref, &src_p) < 0) src_p = "UNKNOWN";
    if (parts & UTIL_PRINT_RELATIVE)          src_p = urp->spec;

    if (parts & UTIL_PRINT_TIME) fprintf(fp, "@%s  ", strcurtime());
    if (parts & UTIL_PRINT_REFN) fprintf(fp, "%d:",   urp->ref);
    if (parts & UTIL_PRINT_DTYPE)
    {
        fprintf(fp, "@");

        if      (urp->dtype == CXDTYPE_TEXT)   fprintf(fp, "t");
        else if (urp->dtype == CXDTYPE_UCTEXT) fprintf(fp, "u");
        else if (urp->dtype == CXDTYPE_SINGLE) fprintf(fp, "s");
        else if (urp->dtype == CXDTYPE_DOUBLE) fprintf(fp, "d");
        else if (reprof_cxdtype(urp->dtype) == CXDTYPE_REPR_INT)
        {
            if (urp->dtype & CXDTYPE_USGN_MASK) fprintf(fp, "+");
            if (sizeof_cxdtype(urp->dtype) < sizeof(int_type_chars))
                fprintf(fp, "%c", int_type_chars[sizeof_cxdtype(urp->dtype)]);
            else
                fprintf(fp, "X");
        }
        else                                   fprintf(fp, "X");

        if (urp->n_items > 1) fprintf(fp, "%d", urp->n_items);
        fprintf(fp, ":");
    }
    if (parts & UTIL_PRINT_NAME) fprintf(fp, "%s",   src_p);
    //fprintf(fp, " ");

    max_nelems = cda_max_nelems_of_ref    (urp->ref);
    cur_nelems = cda_current_nelems_of_ref(urp->ref);
    dtype      = cda_dtype_of_ref         (urp->ref);
    size       = sizeof_cxdtype(dtype);

    if      (dtype == CXDTYPE_TEXT  ||  dtype == CXDTYPE_UCTEXT)
    {
        if      (parts & UTIL_PRINT_NELEMS) fprintf(fp, "[%d]=", cur_nelems);
        else if (parts & UTIL_PRINT_NAME)   fprintf(fp, " ");
        if (parts & UTIL_PRINT_QUOTES) fprintf(fp, "\"");
        for (n = 0;  n < cur_nelems;  n++)
        {
            cda_get_ref_data(urp->ref, n*size, size, &val);
            c32 = (dtype == CXDTYPE_TEXT)? val.c8 : val.c32;
            PrintChar(fp, c32, (parts & UTIL_PRINT_UNESCAPED) != 0);
        }
        if (parts & UTIL_PRINT_QUOTES) fprintf(fp, "\"");
    }
    else
    {
        if      (parts & UTIL_PRINT_NELEMS) fprintf(fp, "[%d]=", cur_nelems);
        else if (parts & UTIL_PRINT_NAME)   fprintf(fp, " ");
        if (max_nelems != 1  &&  parts & UTIL_PRINT_PARENS) fprintf(fp, "{");
        for (n = 0;  n < cur_nelems;  n++)
        {
            cda_get_ref_data(urp->ref, n*size, size, &val);
            if      (dtype == CXDTYPE_SINGLE)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.f32);
            else if (dtype == CXDTYPE_DOUBLE)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.f64);
            else if (dtype == CXDTYPE_INT32)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.i32);
            else if (dtype == CXDTYPE_UINT32)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.u32);
            else if (dtype == CXDTYPE_INT16)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.i16);
            else if (dtype == CXDTYPE_UINT16)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.u16);
            else if (dtype == CXDTYPE_INT8)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.i8);
            else if (dtype == CXDTYPE_UINT8)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.u8);
            else if (dtype == CXDTYPE_INT64)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.i64);
            else if (dtype == CXDTYPE_UINT64)
                len = snprintf(buf, sizeof(buf), urp->dpyfmt, val.u64);
            else
                len = snprintf(buf, sizeof(buf), "unknown_dtype_%d", dtype);

            /* Check for overflow */
            if (len < 0  ||  (size_t)len > sizeof(buf)-1)
                buf[len = sizeof(buf)-1] = '\0';
            /* Rtrim */
            p = buf + len - 1;
            while (p > buf  &&  isspace(*p)) *p-- = '\0';
            /* Ltrim */
            p = buf;
            while (*p == ' ') p++;

            if (n > 0) fprintf(fp, ",");
            fprintf(fp, "%s", p);
        }
        if (max_nelems != 1  &&  parts & UTIL_PRINT_PARENS) fprintf(fp, "}");
    }
    if (parts & UTIL_PRINT_TIMESTAMP)
    {
        if (cda_get_ref_stat(urp->ref, NULL, &timestamp) >= 0)
        {
            fprintf(fp, " @%ld.%06ld", timestamp.sec, timestamp.nsec/1000);
        }
        else
            fprintf(fp, " @UNKNOWN.TIME");
    }
    if (parts & UTIL_PRINT_RFLAGS)
    {
        if (cda_get_ref_stat(urp->ref, &rflags, NULL) < 0) rflags = 0xFFFFFFFF;
        fprintf(fp, " <");
        for (shift = 31, notfirst = 0;
             shift >= 0;
             shift--)
            if ((rflags & (1 << shift)) != 0)
            {
                if (notfirst) printf(",");
                fprintf(fp, "%s", cx_strrflag_short(shift));
                notfirst = 1;
            }
        fprintf(fp, ">");
    }

    if (parts & UTIL_PRINT_NEWLINE) fprintf(fp, "\n");
}

int  ParseDatarefSpec(const char    *argv0, 
                      const char    *spec,
                      char         **endptr_p,
                      char          *namebuf, size_t namebufsize,
                      util_refrec_t *urp)
{
  const char    *cp;
  const char    *cl_p;  // ColoN ptr
  const char    *eq_p;  // EQuals ptr
  size_t         slen;
  char           ut_char;
  char          *endptr;

  int            options;
  cxdtype_t      dtype;
  cxdtype_t      is_unsigned_mask;
  int            n_items;
  char           dpyfmt[20+2];
  int            dpy_rpr;
  const char    *forced_dpyfmt;
  size_t         dpyfmtlen;

  char    buf[100];
  int     r;

  int     flags;
  int     width;
  int     precision;
  char    conv_c;

    options = CDA_DATAREF_OPT_NONE | CDA_DATAREF_OPT_ON_UPDATE;
    dtype   = CXDTYPE_DOUBLE;
    n_items = 1;
    dpyfmt[0] = '\0';
    dpy_rpr = CXDTYPE_REPR_UNKNOWN;

    cp = spec;

    while (1)
    {
        if (*cp == '@')
        {
            cp++;

            is_unsigned_mask = 0;
            while (1)
            {
                if      (*cp == '-') options |= CDA_DATAREF_OPT_PRIVATE;
                else if (*cp == '.') options |= CDA_DATAREF_OPT_NO_RD_CONV;
                else if (*cp == '/') options |= CDA_DATAREF_OPT_SHY;
                else if (*cp == '+') is_unsigned_mask = CXDTYPE_USGN_MASK;
                else if (*cp == ':') {cp++; goto NEXT_FLAG;}
                else break;
                cp++;
            }

            ut_char = tolower(*cp);
            if      (ut_char == 'b') dtype = CXDTYPE_INT8  | is_unsigned_mask;
            else if (ut_char == 'h') dtype = CXDTYPE_INT16 | is_unsigned_mask;
            else if (ut_char == 'i') dtype = CXDTYPE_INT32 | is_unsigned_mask;
            else if (ut_char == 'q') dtype = CXDTYPE_INT64 | is_unsigned_mask;
            else if (ut_char == 's') dtype = CXDTYPE_SINGLE;
            else if (ut_char == 'd') dtype = CXDTYPE_DOUBLE;
            else if (ut_char == 't') dtype = CXDTYPE_TEXT;
            else if (ut_char == 'u') dtype = CXDTYPE_UCTEXT;
            /*
            Unfortunately, support for variable-typed references (CXDTYPE_UNKNOWN)
            would require too much effort in various places of code:
              1. dpyfmt determination becomes impossible.
              2. PrintDatarefData():
                a. Use cda_current_dtype_of_ref()
                   instead of cda_dtype_of_ref()
                b. Move this code upwards, to the very beginning, for ...
                c. UTIL_PRINT_DTYPE: switch from urp->dtype to a value
                   returned from cda_current_dtype_of_ref().
              3. Forbid =VALUE cpecifications for 'x'-typed channels
            So, this alternative is present here only for notice and
            is commented out.
            else if (ut_char == 'x') dtype = CXDTYPE_UNKNOWN;
            */
            else if (ut_char == '\0')
            {
                fprintf(stderr, "%s %s: data-type expected after '@' in \"%s\"\n",
                        strcurtime(), argv0, spec);
                RAISE_EC_USR_ERR();
            }
            else
            {
                fprintf(stderr, "%s %s: invalid channel-data-type after '@' in \"%s\"\n",
                        strcurtime(), argv0, spec);
                RAISE_EC_USR_ERR();
            }
            // Optional COUNT
            cp++;
            if (isdigit(*cp))
            {
                n_items = strtol(cp, &endptr, 10);
                if (endptr == cp)
                {
                    fprintf(stderr, "%s %s: invalid channel-n_items after '@%c' in \"%s\"\n",
                            strcurtime(), argv0, ut_char, spec);
                    RAISE_EC_USR_ERR();
                }
                cp = endptr;
            }
            // Mandatory ':' separator
            if (*cp != ':')
            {
                fprintf(stderr, "%s %s: ':' expected after '@%c[N]' in \"%s\"\n",
                        strcurtime(), argv0, ut_char, spec);
                RAISE_EC_USR_ERR();
            }
            else
                cp++;
        }
        else if (*cp == '%')
        {
            cl_p = strchr(cp, ':');
            if (cl_p == NULL)
            {
                fprintf(stderr, "%s %s: ':' expected after %%DPYFMT spec in \"%s\"\n",
                        strcurtime(), argv0, spec);
                RAISE_EC_USR_ERR();
            }
            slen = cl_p - cp;
            if (slen > sizeof(buf) - 1)
            {
                fprintf(stderr, "%s %s: %%DPYFMT spec too long in \"%s\"\n",
                        strcurtime(), argv0, spec);
                RAISE_EC_USR_ERR();
            }
            memcpy(buf, cp, slen);
            buf[slen] = '\0';

            if (reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT)
            {
                r = ParseDoubleFormat(GetTextFormat(buf),
                                      &flags, &width, &precision, &conv_c);
                dpy_rpr = CXDTYPE_REPR_FLOAT;
            }
            else
            {
                r = ParseIntFormat   (GetIntFormat(buf),
                                      &flags, &width, &precision, &conv_c);
                dpy_rpr = CXDTYPE_REPR_INT;
            }
            if (r < 0)
            {
                fprintf(stderr, "%s %s: invalid %%DPYFMT spec in \"%s\": %s\n",
                        strcurtime(), argv0, spec, printffmt_lasterr());
                RAISE_EC_USR_ERR();
            }
            if (reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT)
                CreateDoubleFormat(dpyfmt, sizeof(dpyfmt), 20, 10,
                                   flags, width, precision, conv_c);
            else
            {
                if (conv_c == 'o'  ||  conv_c == 'x'  ||  conv_c == 'X')
                    flags |= FMT_ALTFORM;
                CreateIntFormat   (dpyfmt, sizeof(dpyfmt), 20, 10,
                                   flags, width, precision, conv_c);
            }

            cp = cl_p + 1;
        }
        else if (*cp == '(')
        {
            cl_p = strchr(cp, ')');
            if (cl_p == NULL)
            {
                fprintf(stderr, "%s %s: ')' expected after (knobname... spec in \"%s\"\n",
                        strcurtime(), argv0, spec);
                RAISE_EC_USR_ERR();
            }

            cp = cl_p + 1;
        }
        else break;
 NEXT_FLAG:;
    }

    /* Isn't dpyfmt specified? */
    if (dpy_rpr == CXDTYPE_REPR_UNKNOWN)
    {
        /* No, it isn't, should set default */
        if (reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT)
            strzcpy(dpyfmt, GetTextFormat(NULL), sizeof(dpyfmt));
        else
            strzcpy(dpyfmt, GetIntFormat (NULL), sizeof(dpyfmt));
    }
    else
    {
        /* Yes, it is, should check compatibility */
        if      (reprof_cxdtype(dtype) == CXDTYPE_REPR_FLOAT  &&
                 dpy_rpr != CXDTYPE_REPR_FLOAT)
        {
            forced_dpyfmt = GetTextFormat(NULL);
            fprintf(stderr,
                    "%s %s: invalid non-float DPYFMT \"%s\", forcing \"%s\" in \"%s\"\n",
                    strcurtime(), argv0, dpyfmt, forced_dpyfmt, spec);
            strzcpy(dpyfmt, forced_dpyfmt, sizeof(dpyfmt));
        }
        else if (reprof_cxdtype(dtype) == CXDTYPE_REPR_INT    &&
                 dpy_rpr != CXDTYPE_REPR_INT)
        {
            forced_dpyfmt = GetIntFormat (NULL);
            fprintf(stderr,
                    "%s %s: invalid non-int DPYFMT \"%s\", forcing \"%s\" in \"%s\"\n",
                    strcurtime(), argv0, dpyfmt, forced_dpyfmt, spec);
            strzcpy(dpyfmt, forced_dpyfmt, sizeof(dpyfmt));
        }
    }

    if (dtype == CXDTYPE_INT64  ||  dtype == CXDTYPE_UINT64)
    { 
        dpyfmtlen = strlen(dpyfmt);
        dpyfmt[dpyfmtlen+2] = '\0';
        dpyfmt[dpyfmtlen+1] = dpyfmt[dpyfmtlen-1];
        dpyfmt[dpyfmtlen]   = 'l';
        dpyfmt[dpyfmtlen-1] = 'l';
    }

    /* Find a terminator */
    for (eq_p = cp;
         *eq_p != '\0'  &&  *eq_p != '='  &&  !isspace(*eq_p);
         eq_p++);

    if (endptr_p != NULL) *endptr_p = eq_p;

    slen = eq_p - cp;
    if (slen > namebufsize - 1)
        slen = namebufsize - 1;
    memcpy(namebuf, cp, slen); namebuf[slen] = '\0';

    urp->options = options;
    urp->dtype   = dtype;
    urp->n_items = n_items;
    strzcpy(urp->dpyfmt, dpyfmt, sizeof(urp->dpyfmt));

    return 0;
}

static int hexdigit2num(char digit)
{
    if (digit >= '0'  &&  digit <= '9') return digit - '0';
    else                                return 10 + tolower(digit) - 'a';
}
static int ParseOneChar (const char    *argv0,
                         const char   **p_p,
                         CxAnyVal_t    *val_p,
                         util_refrec_t *urp)
{
  const char *cp = *p_p;
  char        ch;
  char32      c32;

    if (*cp == '\\')
    {
        cp++;
        if      (*cp == '\0')
        {
            fprintf(stderr, "%s %s: unexpected end of line parsing %s value\n",
                    strcurtime(), argv0, urp->spec);
            RAISE_EC_USR_ERR();
        }

        ch = *cp++;
        if      (ch == '\\') c32 = '\\';
        else if (ch == '\'') c32 = '\'';
        else if (ch == '\"') c32 = '\"';
        else if (ch == 'a')  c32 = '\a';
        else if (ch == 'b')  c32 = '\b';
        else if (ch == 'f')  c32 = '\f';
        else if (ch == 'n')  c32 = '\n';
        else if (ch == 'r')  c32 = '\r';
        else if (ch == 't')  c32 = '\t';
        else if (ch == 'v')  c32 = '\v';
        else if (ch == 'x')
        {
            if (!isxdigit(cp[0])  ||
                !isxdigit(cp[1]))
            {
                fprintf(stderr, "%s %s: 2 hex-digits expected after \\x (\\xXX) in %s value\n",
                        strcurtime(), argv0, urp->spec);
                RAISE_EC_USR_ERR();
            }
            c32 =
                (hexdigit2num(cp[0]) <<  4) |
                (hexdigit2num(cp[1]));
            cp += 2;
        }
        else if (ch == 'u')
        {
            if (!isxdigit(cp[0])  ||
                !isxdigit(cp[1])  ||
                !isxdigit(cp[2])  ||
                !isxdigit(cp[3]))
            {
                fprintf(stderr, "%s %s: 4 hex-digits expected after \\u (\\uXXXX) in %s value\n",
                        strcurtime(), argv0, urp->spec);
                RAISE_EC_USR_ERR();
            }
            c32 =
                (hexdigit2num(cp[0]) << 12) |
                (hexdigit2num(cp[1]) <<  8) |
                (hexdigit2num(cp[2]) <<  4) |
                (hexdigit2num(cp[3]));
            cp += 4;
        }
        else if (ch == 'U')
        {
            if (!isxdigit(cp[0])  ||
                !isxdigit(cp[1])  ||
                !isxdigit(cp[2])  ||
                !isxdigit(cp[3])  ||
                !isxdigit(cp[4])  ||
                !isxdigit(cp[5])  ||
                !isxdigit(cp[6])  ||
                !isxdigit(cp[7]))
            {
                fprintf(stderr, "%s %s: 8 hex-digits expected after \\U (\\UXXXXXXXX) in %s value\n",
                        strcurtime(), argv0, urp->spec);
                RAISE_EC_USR_ERR();
            }
            c32 =
                (hexdigit2num(cp[0]) << 28) |
                (hexdigit2num(cp[1]) << 24) |
                (hexdigit2num(cp[2]) << 20) |
                (hexdigit2num(cp[3]) << 16) |
                (hexdigit2num(cp[4]) << 12) |
                (hexdigit2num(cp[5]) <<  8) |
                (hexdigit2num(cp[6]) <<  4) |
                (hexdigit2num(cp[7]));
            cp += 4;
        }
        else
            c32 = (unsigned char)ch;
    }
    else
    {
        c32 = *((unsigned char *)cp);
        cp++;
    }

    if (urp->dtype == CXDTYPE_UCTEXT)
        val_p->c32 = c32;
    else
    {
        if ((c32 & 0xFF) != c32)
        {
            fprintf(stderr, "%s %s: character \\U%08x is out of char8 range in %s value\n",
                    strcurtime(), argv0, c32, urp->spec);
            RAISE_EC_USR_ERR();
        }
        val_p->c8 = c32;
    }

    *p_p = cp;
    return 0;
}

static int ParseOneDatum(const char    *argv0,
                         const char   **p_p,
                         CxAnyVal_t    *val_p,
                         util_refrec_t *urp)
{
  const char *cp = *p_p;
  char       *endptr;
  size_t      size = sizeof_cxdtype(urp->dtype);
  double      f64;
  int32       i32;
  int64       i64;

    if      (reprof_cxdtype(urp->dtype) == CXDTYPE_REPR_INT)
    {
        if (size == sizeof(int64)) i64 = strtoll(cp, &endptr, 0)/*, fprintf(stderr, "i64=%lld\n", i64)*/;
        else                       i32 = strtol (cp, &endptr, 0)/*, fprintf(stderr, "i32=%d\n",   i32)*/;

        if (endptr == cp)
        {
            fprintf(stderr, "%s %s: syntax error in %s int-value\n",
                    strcurtime(), argv0, urp->spec);
            RAISE_EC_USR_ERR();
        }
        cp = endptr;

        if      (size == sizeof(int64))        val_p->i64 = i64;
        else if (urp->dtype == CXDTYPE_INT32)  val_p->i32 = i32;
        else if (urp->dtype == CXDTYPE_UINT32) val_p->u32 = i32;
        else if (urp->dtype == CXDTYPE_INT16)  val_p->i16 = i32;
        else if (urp->dtype == CXDTYPE_UINT16) val_p->u16 = i32;
        else if (urp->dtype == CXDTYPE_INT8)   val_p->i8  = i32;
        else if (urp->dtype == CXDTYPE_UINT8)  val_p->u8  = i32;
    }
    else if (reprof_cxdtype(urp->dtype) == CXDTYPE_REPR_FLOAT)
    {
        if      (urp->dtype == CXDTYPE_SINGLE) val_p->f32 = strtof(cp, &endptr);
        else if (urp->dtype == CXDTYPE_DOUBLE) val_p->f64 = strtod(cp, &endptr);

        if (endptr == cp)
        {
            fprintf(stderr, "%s %s: syntax error in %s float-value\n",
                    strcurtime(), argv0, urp->spec);
            RAISE_EC_USR_ERR();
        }
        cp = endptr;
    }

    *p_p = cp;
    return 0;
}

/* An important notice regarding ParseDatarefVal() operation:

   1. Any SINGLE datum (either a char or numeric)
      is ALWAYS parsed into urp->val2wr.

      For vectors, it is later copied to urp->databuf[],
      but initially parsing is always performed into val2wr,
      regardless of databuf[] presence.

   2. Vectors (n_items != 1) ALWAYS use databuf, even
      if "sizeof_cxdtype(dtype) * n_items < sizeof(val2wr)".

   Thus, ParseDatarefVal() behaviour differs from cda_core's,
   where
   1) imm_val2snd is used if sndbuf==NULL, but
      is never used when sndbuf!=NULL.
   2) imm_val2snd is used as a sizeof(CxAnyVal_t)-sized byte buffer,
      regardless of the number of elements
      (i.e., both a 1x float64 (8 bytes) and 16x uint8 (16 bytes)
      will fit into imm_val2snd).
 */
int  ParseDatarefVal (const char    *argv0,
                      const char    *start,
                      char         **endptr_p,
                      util_refrec_t *urp)
{
  const char  *cp;
  char         qc;
  CxAnyVal_t   val;
  int          count_exp;
  char        *endptr;

  size_t       usize;

    while (*start != '\0'  &&  isspace(*start)) start++;
    cp = start;
    if (*cp == '\0') goto DO_RET;

    urp->num2wr = 0;

    count_exp = -1;
    /* An optional [COUNT] prefix */
    if (*cp == '[')
    {
        cp++;
        count_exp = strtol(cp, &endptr, 10);
        if (endptr == cp)
        {
            fprintf(stderr,
                    "%s %s: invalid [NUMBER] in %s value\n",
                    strcurtime(), argv0, urp->spec);
            RAISE_EC_USR_ERR();
        }
        cp = endptr;
        if ((urp->n_items == 1  &&  count_exp != 1)  ||
            count_exp > urp->n_items)
        {
            fprintf(stderr,
                    "%s %s: count [%d] is invalid (n_items=%d) for %s value\n",
                    strcurtime(), argv0, count_exp, urp->n_items, urp->spec);
            RAISE_EC_USR_ERR();
        }
        if (*cp != ']')
        {
            fprintf(stderr,
                    "%s %s: closing ']' expected in %s value\n",
                    strcurtime(), argv0, urp->spec);
            RAISE_EC_USR_ERR();
        }
        cp++;
    }

    /* Spaces or '=' sign */
    while (*cp != '\0'  &&
           (isspace(*cp)  ||  *cp == '='))
        cp++;

    if     (urp->dtype == CXDTYPE_TEXT  ||  urp->dtype == CXDTYPE_UCTEXT)
    {
        qc = 0;
        if (*cp == '\"'  ||  *cp == '\'')
        {
            qc = *cp;
            cp++;
        }

        if (urp->n_items == 1)
        {
            if (ParseOneChar(argv0, &cp, &(urp->val2wr), urp) < 0) return -1;
            urp->num2wr++;
        }
        else
        {
            /* Do NOT alloc space for:
                   0 elements (no use)
                   1 element (use urp->val2wr) */
            if (count_exp != 0  &&  count_exp != 1  &&  urp->databuf == NULL)
            {
                urp->databuf_allocd =
                    sizeof_cxdtype(urp->dtype)
                    *
                    (count_exp >= 0? count_exp : urp->n_items);
                if ((urp->databuf = malloc(urp->databuf_allocd)) == NULL)
                {
                    fprintf(stderr, "%s %s: malloc(%zd) failure\n",
                            strcurtime(), argv0, urp->databuf_allocd);
                    exit(EC_ERR);
                }
            }

            for (urp->num2wr = 0;
                 urp->num2wr < (count_exp >= 0? count_exp : urp->n_items);
                 urp->num2wr++)
            {
                if (*cp == '\0'                  ||
                    (qc != '\0'  &&  *cp == qc)  ||
                    (qc == '\0'  &&  isspace(*cp)))
                {
                    goto END_TEXT_LIST;
                }
                if (ParseOneChar(argv0, &cp, &(urp->val2wr), urp) < 0) return -1;
                if (urp->databuf != NULL)
                    memcpy((uint8*)(urp->databuf) +
                           sizeof_cxdtype(urp->dtype) * urp->num2wr,
                           &(urp->val2wr),
                           sizeof_cxdtype(urp->dtype));
            }
        END_TEXT_LIST:;
        }

        if (qc != 0)
        {
            if (*cp != qc)
            {
                fprintf(stderr,
                        "%s %s: closing quote (%c) expected in %s value\n",
                        strcurtime(), argv0, qc, urp->spec);
                RAISE_EC_USR_ERR();
            }
            cp++;
        }
    }
    else if (reprof_cxdtype(urp->dtype) == CXDTYPE_REPR_INT  ||
             reprof_cxdtype(urp->dtype) == CXDTYPE_REPR_FLOAT)
    {
        qc = 0;
        if (*cp == '{')
        {
            qc = '}';
            cp++;
        }

        if (urp->n_items == 1)
        {
            if (ParseOneDatum(argv0, &cp, &(urp->val2wr), urp) < 0) return -1;
            urp->num2wr++;
            ////fprintf(stderr, "val.i32=%d databuf=%p\n", urp->val2wr.i32, urp->databuf);
        }
        else
        {
            usize = sizeof_cxdtype(urp->dtype);
            if (urp->n_items > 0  &&  urp->databuf == NULL)
            {
                urp->databuf = malloc(urp->n_items * usize);
                if (urp->databuf == NULL)
                {
                    fprintf(stderr, "%s %s: failed to malloc() buffer for %s value: %s\n",
                            strcurtime(), argv0, urp->spec, strerror(errno));
                    exit(EC_ERR);
                }
            }
            while (1)
            {
                while (*cp != '\0'  &&  isspace(*cp)) cp++;
                if (*cp == '\0'  ||  (qc != 0  &&  *cp == qc))
                    goto BREAK_PARSE_COMMA_LIST;
                /* Parse into static buffer... */
                if (ParseOneDatum(argv0, &cp, &(urp->val2wr), urp) < 0) return -1;
                /* any space left? */
                if (urp->num2wr >= urp->n_items)
                {
                    fprintf(stderr, "%s %s: too many elements in %s value (>%d)\n",
                            strcurtime(), argv0, urp->spec, urp->n_items);
                    RAISE_EC_USR_ERR();
                }
                /* ...store in allocated mem */
                memcpy((uint8*)(urp->databuf) + urp->num2wr * usize, 
                       &(urp->val2wr),
                       usize);
                urp->num2wr++;

                /* Skip optional comma */
                if (*cp == ',') cp++;
            }
 BREAK_PARSE_COMMA_LIST:;
        }

        while (*cp != '\0'  &&  isspace(*cp)) cp++;

        if (qc != 0)
        {
            if (*cp != qc)
            {
                fprintf(stderr,
                        "%s %s: closing bracket \"%c\" expected in %s value\n",
                        strcurtime(), argv0, qc, urp->spec);
                RAISE_EC_USR_ERR();
            }
            cp++;
        }
    }
    else
    {
    }

 DO_RET:
    if (endptr_p != NULL) *endptr_p = cp;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static inline int isletnumdot(int c)
{
    return isalnum(c)  ||  c == '_'  ||  c == '-'  ||  c == '.';
}

static const char *FindCloseBr(const char *p)
{
    while (1)
    {
        if      (isletnumdot(*p)  ||  *p == ',') p++;
        else if (*p == '}')       return p;
        else                      return NULL;
    }
}

static const char * do_one_stage
    (const char         *argv0,
     char               *path_buf, size_t path_buf_size, size_t path_len,
     const char         *seg, size_t seg_len,
     const char         *curp,
     util_refrec_t      *urp,
     process_one_name_t  processer, void *privptr)
{
  const char *ret;
  const char *srcp = curp;

  const char *clsp;
  const char *begp;

  char       *endptr;
  int         range_min;
  int         range_max;
  int         range_i;
  char        intbuf[100];

    /* Append next segment */
    if (seg_len > path_buf_size - 1 - path_len)
        seg_len = path_buf_size - 1 - path_len;
    if (seg_len > 0)
    {
        memcpy(path_buf + path_len, seg, seg_len);
        path_len += seg_len;
    }
  
    while (1)
    {
        if      (*srcp == '{')
        {
            /*!!! Note: now it is a simple, non-nestable, comma-list-only  */
            srcp++;

            /* Find a closing '}' first */
            clsp = FindCloseBr(srcp);
            if (clsp == NULL)
                return "missing closing '}'";

            /* Go through list */
            for (begp = srcp; ;)
            {
                if (*srcp == ','  ||  *srcp == '}')
                {
                    ret = do_one_stage(argv0,
                                       path_buf, path_buf_size, path_len,
                                       begp, srcp - begp,
                                       clsp + 1,
                                       urp,
                                       processer, privptr);
                    if (ret != NULL) return ret;

                    if (*srcp == '}') return NULL;

                    srcp++; // Skip ','
                    begp = srcp;
                }
                else
                    srcp++;
            }
        }
        else if (*srcp == '<')
        {
            srcp++;

            range_min = strtol(srcp, &endptr, 10);
            if (endptr == srcp)
                return "invalid range-min after '<'";
            if (range_min < 0)
                return "negative range-min";
            srcp = endptr;

            if (*srcp != '-')
                return "'-' expected after range-min";
            srcp++;

            range_max = strtol(srcp, &endptr, 10);
            if (endptr == srcp)
                return "invalid range-max after '<'";
            if (range_max < range_min)
                return "range-max<range-min";
            srcp = endptr;

            if (*srcp != '>')
                return "'>' expected after range-max";
            srcp++;

            for (range_i = range_min;  range_i <= range_max;  range_i++)
            {
                sprintf(intbuf, "%d", range_i);
                ret = do_one_stage(argv0,
                                   path_buf, path_buf_size, path_len,
                                   intbuf, strlen(intbuf),
                                   srcp,
                                   urp,
                                   processer, privptr);
                if (ret != NULL) return ret;
            }
            return NULL;
        }
        else if (*srcp == '\0')
        {
            path_buf[path_len] = '\0';
            if (processer != NULL)
                processer(argv0, path_buf, urp, privptr);
            return NULL;
        }
        else
        {
            if (path_len < path_buf_size - 1)
                path_buf[path_len++] = *srcp;
            srcp++;
        }
    }
}

const char * ExpandName(const char         *argv0,
                        const char         *chanref,
                        util_refrec_t      *urp,
                        process_one_name_t  processer, void *privptr)
{
#if 1
  char    path_buf[CDA_PATH_MAX];

    if (chanref == NULL) return "NULL chanref";

    return do_one_stage(argv0,
                        path_buf, sizeof(path_buf), 0,
                        "", 0,
                        chanref,
                        urp,
                        processer, privptr);
#else
    processer(argv0, chanref, urp);
    return NULL;
#endif
}
