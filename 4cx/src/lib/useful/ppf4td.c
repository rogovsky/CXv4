#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "misclib.h"

#include "ppf4td.h"
#include "ppf4td_plaintext.h"
#include "ppf4td_m4.h"
#include "ppf4td_cpp.h"
#include "ppf4td_mem.h"


static int           ppf4td_plugmgr_initialized = 0;
static ppf4td_vmt_t *first_ppf4td_plugin_metric = NULL;


static int initialize_ppf4td_plugmgr(void)
{
    //if (CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(string))) < 0  ||
    //    CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(newf)))   < 0  ||
    //    CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(file)))   < 0)
    //    return -1;

    if (ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(plaintext))) < 0  ||
        ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(m4)))        < 0  ||
        ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(cpp)))       < 0  ||
        ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(mem)))       < 0)
        return -1;

    ppf4td_plugmgr_initialized = 1;

    return 0;
}

int ppf4td_register_plugin  (ppf4td_vmt_t *metric)
{
  ppf4td_vmt_t *mp;

    /* First, check plugin credentials */
    if (metric->mr.magicnumber != PPF4TD_MODREC_MAGIC)
    {
        //CdrSetErr("subsys_ldr-plugin \"%s\".magicnumber mismatch", metric->mr.name);
        return -1;
    }
    if (!CX_VERSION_IS_COMPATIBLE(metric->mr.version, PPF4TD_MODREC_VERSION))
    {
        //CdrSetErr("subsys_ldr-plugin \"%s\".version=%d.%d.%d.%d, incompatible with %d.%d.%d.%d",
        //          metric->mr.name,
        //          CX_MAJOR_OF(metric->mr.version),
        //          CX_MINOR_OF(metric->mr.version),
        //          CX_PATCH_OF(metric->mr.version),
        //          CX_SNAP_OF (metric->mr.version),
        //          CX_MAJOR_OF(CDR_SUBSYS_LDR_MODREC_VERSION),
        //          CX_MINOR_OF(CDR_SUBSYS_LDR_MODREC_VERSION),
        //          CX_PATCH_OF(CDR_SUBSYS_LDR_MODREC_VERSION),
        //          CX_SNAP_OF (CDR_SUBSYS_LDR_MODREC_VERSION));
        return -1;
    }

    /* ...just a guard: check if this exact plugin is already registered */
    for (mp = first_ppf4td_plugin_metric;  mp != NULL;  mp = mp->next)
        if (mp == metric) return +1;
    
    /* Then, ensure that this scheme isn't already registered */
    for (mp = first_ppf4td_plugin_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(metric->mr.name, mp->mr.name) == 0)
        {
            //CdrSetErr("subsys_ldr scheme \"%s\" already registered", metric->mr.name);
            return -1;
        }

    /* Insert at beginning of the list */
    metric->next = first_ppf4td_plugin_metric;
    first_ppf4td_plugin_metric = metric;
    
    return 0;
}

int ppf4td_deregister_plugin(ppf4td_vmt_t *metric)
{
}

static ppf4td_vmt_t * get_plugin_by_scheme(const char *scheme)
{
  ppf4td_vmt_t *mp;

    if (!ppf4td_plugmgr_initialized)
    {
        if (initialize_ppf4td_plugmgr() < 0) return NULL;
    }

    for (mp = first_ppf4td_plugin_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    return NULL;
}


//////////////////////////////////////////////////////////////////////

static void SetCurRef(ppf4td_ctx_t *ctx, const char *ref)
{
  size_t len;

    if (ref == NULL)
    {
        strcpy(ctx->_curref, "<UNKNOWN>");
        return;
    }

    /* Note: should better use TAIL, but taking 'len' from end can
       cause problems in case of multibyte locales */
    /* Note2: can we truncate additional 3 chars and add "..." to resulting _curref? */
    len = strlen(ref);
    if (len > sizeof(ctx->_curref) - 1)
        len = sizeof(ctx->_curref) - 1;
    memcpy(ctx->_curref, ref, len); ctx->_curref[len] = '\0';
}

static void UnGetStr(ppf4td_ctx_t *ctx, const char *buf, int len)
{
    if(0){
        int x;
        if (len == 0) return;
        fprintf(stdout, "%s(\"", __FUNCTION__);
        for (x = 0;  x < len;  x++)
            fprintf(stdout, "%c", buf[x]);
        fprintf(stdout, "\")");
    }
    while (len > 0  &&  ctx->_ucbuf_used < countof(ctx->_ucbuf))
    {
        ctx->_ucbuf[ctx->_ucbuf_used++] = buf[--len];
    }
    if (len > 0)
    {
        fprintf(stderr, "\n%s %s::%s(),%s:%d: WARNING: _ucbuf[] overflow\n",
                strcurtime(), __FILE__, __FUNCTION__,
                ctx->_curref, ctx->_curline);
    }
    if(0){
        int x;
        fprintf(stdout, "-><");
        for (x = 0;  x < ctx->_ucbuf_used;  x++)
            fprintf(stdout, "%c", ctx->_ucbuf[x]);
        fprintf(stdout, ">\n");
    }
}

static void UnGetChr(ppf4td_ctx_t *ctx, int ch)
{
    if (ctx->_ucbuf_used < countof(ctx->_ucbuf))
    {
        ctx->_ucbuf[ctx->_ucbuf_used++] = ch;
    }
    else
    {
        fprintf(stderr, "\n%s %s::%s(),%s:%d: WARNING: _ucbuf[] overflow\n",
                strcurtime(), __FILE__, __FUNCTION__,
                ctx->_curref, ctx->_curline);
    }
}

static void UnGetL0s(ppf4td_ctx_t *ctx, const char *buf, int len)
{
    if(0){
        int x;
        if (len == 0) return;
        fprintf(stdout, "%s(\"", __FUNCTION__);
        for (x = 0;  x < len;  x++)
            fprintf(stdout, "%c", buf[x]);
        fprintf(stdout, "\")");
    }
    while (len > 0  &&  ctx->_l0buf_used < countof(ctx->_l0buf))
    {
        ctx->_l0buf[ctx->_l0buf_used++] = buf[--len];
    }
    if (len > 0)
    {
        fprintf(stderr, "\n%s %s::%s(),%s:%d: WARNING: _l0buf[] overflow\n",
                strcurtime(), __FILE__, __FUNCTION__,
                ctx->_curref, ctx->_curline);
    }
    if(0){
        int x;
        fprintf(stdout, "-><");
        for (x = 0;  x < ctx->_l0buf_used;  x++)
            fprintf(stdout, "%c", ctx->_l0buf[x]);
        fprintf(stdout, ">\n");
    }
}

static void UnGetL0c(ppf4td_ctx_t *ctx, int ch)
{
    if (ctx->_l0buf_used < countof(ctx->_l0buf))
    {
        ctx->_l0buf[ctx->_l0buf_used++] = ch;
    }
    else
    {
        fprintf(stderr, "\n%s %s::%s(),%s:%d: WARNING: _l0buf[] overflow\n",
                strcurtime(), __FILE__, __FUNCTION__,
                ctx->_curref, ctx->_curline);
    }
}

//////////////////////////////////////////////////////////////////////


int ppf4td_open (ppf4td_ctx_t *ctx, const char *def_scheme, const char *reference)
{
  char                scheme[20];
  const char         *location;
  ppf4td_vmt_t       *metric;
  int                 r;

    if (def_scheme != NULL  &&  *def_scheme == '!')
    {
        strzcpy(scheme, def_scheme + 1, sizeof(scheme));
        location = reference;
    }
    else
        split_url(def_scheme, "::",
                  reference, scheme, sizeof(scheme),
                  &location);

    metric = get_plugin_by_scheme(scheme);
    if (metric == NULL)
    {
        //
        errno = ENOENT;
        return -1;
    }

    bzero(ctx, sizeof(*ctx));
    SetCurRef(ctx, location);
    ctx->_curline   = 0; // Will be incremented upon 1st getc
    ctx->_is_at_bol = 1;

    r = metric->open(ctx, location);
    if (r != 0) return -1;

    ctx->vmt = metric;

    return 0;
}

int ppf4td_close(ppf4td_ctx_t *ctx)
{
  int  r;

    if (ctx == NULL  ||  ctx->vmt == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    r = (ctx->vmt->close != NULL)? ctx->vmt->close(ctx) : 0;
    ctx->vmt = NULL;
    return r;
}


int         ppf4td_peekc     (ppf4td_ctx_t *ctx, int *ch_p)
{
  int         r;

    if (ctx == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (ctx->_ucbuf_used)
    {
        *ch_p = ctx->_ucbuf[ctx->_ucbuf_used - 1];
        return +1;
    }

    r = ppf4td_nextc(ctx, ch_p);
    if (r > 0  &&  *ch_p != EOF) UnGetChr(ctx, *ch_p);
    ////fprintf(stderr, "peekc: ch=%d, r=%d\n", *ch_p, r);
    return r;
}

static int ReadNextCh(ppf4td_ctx_t *ctx, int *ch_p)
{
    if (ctx->_l0buf_used)
    {
        *ch_p = ctx->_l0buf[ctx->_l0buf_used - 1]; ctx->_l0buf_used--; // Yes, [--_l0buf_used] is the same, but less vivid
        return +1;
    }

    return ctx->vmt->nextc(ctx, ch_p); /*!!! In case of EOF -- store it? */
}

static int PeekNextCh(ppf4td_ctx_t *ctx, int *ch_p)
{
  int         r;
  int         ch;

    if (ctx->_l0buf_used)
    {
        *ch_p = ctx->_l0buf[ctx->_l0buf_used - 1];
        return +1;
    }

    r = ctx->vmt->nextc(ctx, &ch);
    if (r > 0) UnGetL0c(ctx, ch);
    *ch_p = ch;
    return r;
}

int         ppf4td_nextc     (ppf4td_ctx_t *ctx, int *ch_p)
{
  int         r;
  int         ch;

  char        sniffbuf[1000];
  int         sniffgot;
  int         sniffchr;
  const char *model;

  int         line_no;

  static const char *hash_model = " ";
  static const char *hlin_model = "line ";

    if (ctx == NULL  ||  ctx->vmt == NULL  ||  ctx->vmt->nextc == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (ctx->_ucbuf_used)
    {
        *ch_p = ctx->_ucbuf[ctx->_ucbuf_used - 1]; ctx->_ucbuf_used--; // Yes, [--_ucbuf_used] is the same, but less vivid
        return +1;
    }

 DO_NEXTC:
    do
    {
        r = ReadNextCh(ctx, &ch);
    }
    while (r > 0  &&
           // Ignore LF after CR (to recognize CR,LF as a SINGLE newline)
           ch == '\n'  &&  ctx->_prev_ch == '\r');

    /* Check for backslash-newline */
    if (r > 0  &&  ch == '\\'           &&
        PeekNextCh(ctx, &sniffchr) > 0  &&
        (sniffchr == '\r'  ||  sniffchr == '\n'))
    {
        /* Consume the "newline" itself... */
        ReadNextCh(ctx, &ch);
        /* ...and check if that was a CR,LF combo... */
        if (ch == '\r'                      &&
            PeekNextCh(ctx, &sniffchr) > 0  &&
            sniffchr == '\n')
            /* then consume LF too */
            ReadNextCh(ctx, &ch);

        /* Note: since this backslashed newline is deleted completely
           and wouldn't reach accounting code below, the _curline should
           be promoted here explicitly. */
        ctx->_curline++;

        /* Following was added 04.08.2014 for correct processing of
           m4 "feature" injecting "#line..." right inside macro expansion
           (cpp does NOT insert "# NNN" inside string or char constants) */
        ctx->_is_at_bol = 1;

        /* 28.12.2015: consume all spaces after "\NL", to enable splitting strings into multiple lines */
        while (PeekNextCh(ctx, &sniffchr) > 0          &&
               sniffchr != '\r'  &&  sniffchr != '\n'  &&
               isspace(sniffchr))
            ReadNextCh(ctx, &ch);

        goto DO_NEXTC;
    }

    /* Note: that is bol from PREVIOUS nextc */
    if (ctx->_is_at_bol)
    {
        ctx->_curline++;
        if      ((ctx->vmt->linesync_type == PPF4TD_LINESYNC_HASH  ||
                  ctx->vmt->linesync_type == PPF4TD_LINESYNC_HLIN)  &&
                 ch == '#'  &&  1)
        {
            model = ctx->vmt->linesync_type == PPF4TD_LINESYNC_HASH? hash_model : hlin_model;

            sniffgot = 0;
            while (sniffgot < strlen(model)             &&
                   PeekNextCh(ctx, &sniffchr) > 0       &&
                   (sniffbuf[sniffgot] = sniffchr,
                    sniffbuf[sniffgot] == model[sniffgot]))
            {
                ReadNextCh(ctx, &sniffchr);
                sniffgot++;
            }
            if (sniffgot < strlen(model))
            {
                /* No, that's not what expected */
                UnGetStr(ctx, sniffbuf, sniffgot);
            }
            else
            {
                /* Yes, that's a required prefix */

                /* Get the LINENUM */
                line_no = 0;
                while ((r = ReadNextCh(ctx, &ch)) > 0)
                {
                    if (isdigit(ch))
                    {
                        line_no = line_no * 10 + (ch - '0');
                    }
                    else break;
                }
                ////fprintf(stdout, "#%s%d\n", model, line_no);
                ctx->_curline = line_no - 1; // "-1" because it concerns the NEXT line, and "++" will be done upon next read

                /* Any "FILENAME"? */
                /* Skip whitespace, if any */
                while (r > 0  &&  ch != '\r'  &&  ch != '\n'  &&  isspace(ch))
                    r = ReadNextCh(ctx, &ch);
                /* Is it really a quote? */
                if (r > 0  &&  ch == '"')
                {
                    sniffgot = 0;
                    while (sniffgot < countof(sniffbuf) - 1     &&
                           (r = ReadNextCh(ctx, &ch)) > 0  &&
                           ch != '"')
                        sniffbuf[sniffgot++] = ch;

                    if (sniffgot > 0)
                    {
                        sniffbuf[sniffgot] = '\0';
                        SetCurRef(ctx, sniffbuf);
                    }
                }

                /* Go till EOL */
                while (r > 0  &&  ch != '\r'  &&  ch != '\n')
                    r = ReadNextCh(ctx, &ch)/*, fprintf(stdout, "_%c", ch)*/;

                /* Finally, re-start nextc */
                /* _prev_ch and _is_at_bol are kept intact and valid for next line */
                goto DO_NEXTC;
            }
        }
    }

    ctx->_is_at_bol = (ch == '\r'  ||  ch == '\n');
    ctx->_prev_ch   = ch;

    *ch_p = ch;
    return r;
}

int         ppf4td_ungetchars(ppf4td_ctx_t *ctx, const char *buf, int len)
{
    UnGetStr(ctx, buf, len);

    return 0; /*!!! Should better get "overflow" result from UnGetStr() */
}

int         ppf4td_is_at_eol (ppf4td_ctx_t *ctx)
{
  int  ch;

    if (ppf4td_peekc(ctx, &ch) < 0) return -1;
    return ch == EOF  ||  ch == '\r'  ||  ch == '\n';
}

int         ppf4td_skip_white(ppf4td_ctx_t *ctx)
{
  int  ch;

    while (!ppf4td_is_at_eol(ctx)      &&
           ppf4td_peekc(ctx, &ch) > 0  &&
           isspace(ch))
        ppf4td_nextc(ctx, &ch)/*, fprintf(stderr, "\t<%c>\n", ch)*/;

    return 0;
}

int         ppf4td_get_ident (ppf4td_ctx_t *ctx, int flags, char *buf, size_t bufsize)
{
  int  ch;
  int  x;

    bufsize--; // To leave space for '\0', and avoid other further +1/-1

    for (x = 0;;)
    {
        if (ppf4td_peekc(ctx, &ch) < 0) return -1;
        if (!isalnum(ch)  &&  ch != '_'                       &&
            ((flags & PPF4TD_FLAG_DASH) == 0  ||  ch != '-')  &&
            ((flags & PPF4TD_FLAG_DOT)  == 0  ||  ch != '.')) break;

        if ((flags & PPF4TD_FLAG_JUST_SKIP) == 0)
        {
            if (x >= bufsize)
            {
                errno = PPF4TD_E2LONG;
                return -1;
            }
            buf[x++] = ch;
        }
        ppf4td_nextc(ctx, &ch);
    }
    if (x == 0)
    {
        errno = PPF4TD_EIDENT;
        return -1;
    }
    if ((flags & PPF4TD_FLAG_JUST_SKIP) == 0) buf[x++] = '\0';

    //fprintf(stderr, ":%d[%d]=<%s>\n", ctx->_curline, x, buf);
    return 0;
}

int         ppf4td_get_int   (ppf4td_ctx_t *ctx, int flags, int  *vp,  int defbase, int *base_p)
{
  int  r;
  int  ch;
  int  digit;

  int  base;
  int  pos;
  int  rqd;
  int  val;
  int  neg;

    if (defbase < 0)
    {
        errno = EINVAL;
        return -1;
    }

    base = defbase > 0? defbase : 10;
    for (pos = 0, rqd = 1, val = 0, neg = 0;
         ;
         pos++)
    {
        r = ppf4td_peekc(ctx, &ch);
        if (r <= 0) goto END_PARSE;

        if      (ch >= '0'  &&  ch <= '9') digit = ch - '0';
        else if (ch >= 'a'  &&  ch <= 'z') digit = ch - 'a' + 10;
        else if (ch >= 'A'  &&  ch <= 'Z') digit = ch - 'A' + 10;
        else                               digit = -1;

        if      (ch == '-'  &&  pos == 0  && (flags & PPF4TD_FLAG_UNSIGNED) == 0)
        {
            neg = 1;
            goto NEXT_CH;
        }
        else if (digit >= 0  &&  digit < base)
        {
            val = (val * base) + digit;
            rqd = 0;
            if (pos == 0  &&  digit == 0  &&  defbase <= 0) base = 8;
        }
        else if (pos == 1  &&  val == 0  &&
                 (
                  ((ch == 'b'  ||  ch == 'B')  &&  (defbase <= 0  ||  defbase == 2))
                  ||
                  ((ch == 'x'  ||  ch == 'X')  &&  (defbase <= 0  ||  defbase == 16))
                 )
                )
        {
            base = (toupper(ch) == 'B')? 2 : 16;
            rqd = 1;
        }
        else goto END_PARSE;

 NEXT_CH:
        ppf4td_nextc(ctx, &ch);
    }
 END_PARSE:
    if      (r < 0)
        return -1;
    else if (r == 0  &&  rqd)
    {
        errno = PPF4TD_EEOF;
        return -1;
    }
    else if (rqd)
    {
        errno = ppf4td_is_at_eol(ctx)? PPF4TD_EEOL : PPF4TD_EINT;
        return -1;
    }

    if (neg) val = -val;
    *vp = val;
    if (base_p != NULL) *base_p = base;
    return 0;
}

int         ppf4td_get_double(ppf4td_ctx_t *ctx, int flags, double *vp)
{
}

static int getxdigit(ppf4td_ctx_t *ctx, int *xdigit_p)
{
  int  ch;

    if (ppf4td_peekc(ctx, &ch) < 0) return -1;
    if      (ch == EOF)  {errno = PPF4TD_EEOF; return -1;}
    else if (ch == '\r'  ||
             ch == '\n') {errno = PPF4TD_EEOL; return -1;}

    if      (ch >= '0'  &&  ch <= '9') *xdigit_p = ch - '0';
    else if (ch >= 'A'  &&  ch <= 'F') *xdigit_p = ch - 'A' + 10;
    else if (ch >= 'a'  &&  ch <= 'f') *xdigit_p = ch - 'a' + 10;
    else
    {
        errno = PPF4TD_EXDIG;
        return -1;
    }

    ppf4td_nextc(ctx, &ch); // Remove character from input stream

    return 0;
}
int         ppf4td_get_string(ppf4td_ctx_t *ctx, int flags, char *buf, size_t bufsize)
{
  int   ch;
  int   x;
  char  cur_q;
  int   xdigit1, xdigit2;

    bufsize--; // To leave space for '\0', and avoid other further +1/-1

#define SHELLSTYLE 1
#if SHELLSTYLE
    cur_q = '\0';
#else
    /* Check if we should switch to quoted-string mode */
    if (ppf4td_peekc(ctx, &ch) < 0) return -1;
    if ((flags & PPF4TD_FLAG_IGNQUOTES) == 0  &&
        (ch == '\''  ||  ch == '\"'))
    {
        if (flags & PPF4TD_FLAG_NOQUOTES)
        {
            errno = PPF4TD_EIDENT;
            return -1;
        }
        cur_q = ch;
        ppf4td_nextc(ctx, &ch);
    }
    else
        cur_q = '\0';
#endif

    flags |= PPF4TD_FLAG_EOLTERM;
    for (x = 0;;)
    {
        if (ppf4td_peekc(ctx, &ch) < 0) return -1;

        /* Check for terminator first... */
        if (cur_q != '\0'  &&  ch == cur_q)
        {
            ppf4td_nextc(ctx, &ch);
            cur_q = '\0';
#if SHELLSTYLE
            goto NEXT_CHAR;
#else
            goto END_READ;
#endif
        }
        if (cur_q == '\0'  &&
            (
             ((flags & PPF4TD_FLAG_EOLTERM) != 0  &&  ppf4td_is_at_eol(ctx))  ||
             ((flags & PPF4TD_FLAG_HSHTERM) != 0  &&  ch == '#')              ||
             ((flags & PPF4TD_FLAG_COMTERM) != 0  &&  ch == ',')              ||
             ((flags & PPF4TD_FLAG_SEMTERM) != 0  &&  ch == ';')              ||
             ((flags & PPF4TD_FLAG_BRCTERM) != 0  &&  ch == '}')              ||
             ((flags & PPF4TD_FLAG_SPCTERM) != 0  &&  isspace(ch)  &&
              ch != '\n'  &&  ch != '\r')
            ))
            goto END_READ;

        /* Okay, may READ the character */
        ppf4td_nextc(ctx, &ch);

#if SHELLSTYLE
        /* An opening quote? */
        if (cur_q == '\0'                         &&
            (flags & PPF4TD_FLAG_IGNQUOTES) == 0  &&
            (ch == '\''  ||  ch == '\"'))
        {
            if (flags & PPF4TD_FLAG_NOQUOTES)
            {
                errno = PPF4TD_EIDENT;
                return -1;
            }
            cur_q = ch;
            goto NEXT_CHAR;
        }
#endif   

        /* A backslashed something? */
        if (ch == '\\')
        {
            if (ppf4td_nextc(ctx, &ch) < 0) return -1;
            switch (ch)
            {
                case EOF:
                    errno = PPF4TD_EEOF;
                    return -1;

                /* "\EOL" is thrown away */
                case '\r':
                case '\n':
                    goto NEXT_CHAR;

                case 'a': ch = '\a'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'v': ch = '\v'; break;

                case 'x':
                    if (getxdigit(ctx, &xdigit1) < 0) return -1;
                    if (getxdigit(ctx, &xdigit2) < 0) ch = xdigit1;
                    else                              ch = xdigit1*16 + xdigit2;
                    if (ch == '\0') goto NEXT_CHAR;
                    break;

                /* Otherwise retain backslashed character as-is */
            }
        }

        /* Okay, finally store the character */
        if ((flags & PPF4TD_FLAG_JUST_SKIP) == 0)
        {
            if (x >= bufsize)
            {
                errno = PPF4TD_E2LONG;
                return -1;
            }
            buf[x++] = ch;
        }
 NEXT_CHAR:;
    }
 END_READ:
    if ((flags & PPF4TD_FLAG_JUST_SKIP) == 0) buf[x++] = '\0';

    //fprintf(stderr, ".%d[%d]=<%s>\n", ctx->_curline, x, buf);
    return 0;
}

int         ppf4td_read_line (ppf4td_ctx_t *ctx, int flags, char *buf, size_t bufsize)
{
  int  ch;
  int  x;

    bufsize--; // To leave space for '\0', and avoid other further +1/-1

    for (x = 0;;)
    {
        //!!! A very simplistic approach, not dealing with "\n\r" and "\r"
        if (ppf4td_nextc(ctx, &ch) < 0) return -1;
        if (ch == EOF  ||  ch == '\n') break;

        if (x >= bufsize)
        {
            errno = PPF4TD_E2LONG;
            return -1;
        }
        buf[x++] = ch;
    }
    buf[x++] = '\0';

    return 0;
}

const char *ppf4td_cur_ref   (ppf4td_ctx_t *ctx)
{
    return ctx->_curref;
}

int         ppf4td_cur_line  (ppf4td_ctx_t *ctx)
{
    return ctx->_curline;
}


static char *_ppf4td_errlist[] =
{
    "Unexpected end of file",
    "Unexpected end of line",
    "Identifier expected",
    "Integer expected",
    "String too long",
    "Underminated quote",
    "Hex-digit expected",
};

char       *ppf4td_strerror  (int errnum)
{
  static char buf[100];
    
    if (errnum > 0) return strerror(errnum);

    if (errnum < -(signed)(sizeof(_ppf4td_errlist) / sizeof(_ppf4td_errlist[0])))
    {
        sprintf(buf, "Unknown PPF4TD error %d", errnum);
        return buf;
    }
    
    return _ppf4td_errlist[-errnum];
}
