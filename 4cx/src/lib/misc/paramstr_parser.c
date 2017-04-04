#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "misc_types.h"
#include "memcasecmp.h"
#include "paramstr_parser.h"


#if 1 /* To get rid of floats on FPU-less platforms  */
#define MAY_USE_REAL 1
#endif

#ifndef MAY_USE_INT64
  #define MAY_USE_INT64 1
#endif
  

/*********************************************************************
  Note: this library isn't optimized for speed; although it can gain
  some speedup by replacing strchr() with memchr().
  Additionally, it allows weird specifications like
    "par1=val1,,,par2=val2"
*********************************************************************/


static char  psp_errdescr[100] = "";
static char *psp_errp          = NULL;

#define RESET_ERR() do { psp_errp = NULL; } while (0)

#define RET_ERR(code, format, args...)               \
    do {                                             \
        snprintf(psp_errdescr, sizeof(psp_errdescr), \
                 format, ##args);                    \
        psp_errp = psp_errdescr;                     \
        return code;                                 \
    } while (0)

static int PspSetParameter(psp_paramdescr_t *descr, void *rec, void *vp, int init);


static int PspDoInit(psp_paramdescr_t *table, void *rec)
{
  psp_paramdescr_t *dp;
  int               r;

    if (table == NULL) return PSP_R_OK; // Empty table -- do nothing
    if (rec   == NULL) return PSP_R_OK; // Black hole -- nothing to init

    for (dp = table;  dp->name != NULL;  dp++)
        if ((r = PspSetParameter(dp, rec, NULL, 1) != PSP_R_OK)) return r;
    
    return PSP_R_OK;
}

static int PspSetParameter(psp_paramdescr_t *descr, void *rec, void *vp, int init)
{
  char *dst;
#define SSRV *((char **)vp)
#define MSLV *((char **)dst)
  int   r;
  char *plugin_errstr;
    
    if (init) vp = &(descr->var); /*!!! This uses the fact that the "defval" is always at the beginning of the union-parts*/

    if (rec == NULL) return PSP_R_OK;
    dst = ((char *)rec) + descr->offset;
    
    switch (descr->t)
    {
        case PSP_T_NOP:
        case PSP_T_VOID:
            break;

        case PSP_T_INT:
        case PSP_T_SELECTOR:
        case PSP_T_LOOKUP:
        case PSP_T_FLAG:
            if (init  &&  descr->t == PSP_T_FLAG  &&  !descr->var.p_flag.is_defval)
                break; /* Not all FLAG parameters take part in initialization */
            switch (descr->size)
            {
                case 1:
                    *((int8 *)dst)  = *((int *)vp);
                    break;

                case 2:
                    *((int16 *)dst) = *((int *)vp);
                    break;

                case 4:
                    *((int32 *)dst) = *((int *)vp);
                    break;

#if MAY_USE_INT64
                case 8:
                    /* !!! Uint64, for size_t's to be converted correctly;
                           however, regular int64s will be mangled */
                    *((uint64 *)dst) = *((int *)vp);
                    break;
#endif

                default:
                    RET_ERR(PSP_R_APPERR,
                            "Invalid size=%zu in '%s' parameter",
                            descr->size, descr->name);
            }
            break;

        case PSP_T_STRING:
            if (SSRV != NULL)
                strncpy(dst, SSRV, descr->size);
            else
                dst[0] = '\0';
            dst[descr->size - 1] = '\0';
            break;
        
        case PSP_T_MSTRING:
            if (MSLV != NULL)
            {
                free(MSLV);
                MSLV = NULL;
            }
            if (vp != NULL  &&  SSRV != NULL)
            {
                if ((MSLV = malloc(strlen(SSRV) + 1)) == NULL)
                    RET_ERR(PSP_R_FAILURE, "malloc() failure");
                strcpy(MSLV, SSRV);
            }
            break;

#ifdef MAY_USE_REAL
        case PSP_T_REAL:
            switch (descr->size)
            {
                case sizeof(float):
                    *((float *) dst) = *((double *)vp);
                    break;
                    
                case sizeof(double):
                    *((double *)dst) = *((double *)vp);
                    break;
                    
                default:
                    RET_ERR(PSP_R_APPERR,
                            "Invalid size=%zu in REAL '%s' parameter",
                            descr->size, descr->name);
            }
            break;
#endif /* MAY_USE_REAL */
        
        case PSP_T_PSP:
            if (init)
            {
                r = PspDoInit(descr->var.p_psp.descr, dst);
                if (r < 0) return r;
            }
            break;

        case PSP_T_PLUGIN:
            if (init)
            {
                plugin_errstr = NULL;
                r = descr->var.p_plugin.parser
                    (NULL, NULL,
                     ((char *)rec) + descr->offset, descr->size,
                     "", "",
                     descr->var.p_plugin.privptr, &plugin_errstr);
                if (r < 0)
                    RET_ERR(r,
                            "Error initializing '%s' parameter%s%s",
                            descr->name,
                            plugin_errstr == NULL? "" : ":",
                            plugin_errstr == NULL? "" : plugin_errstr);
            }
            break;

        case PSP_T_INCLUDE:
            if (init)
            {
                r = PspDoInit(descr->var.p_include.table,
                              ((char *)rec) + descr->offset);
                if (r < 0) return r;
            }
            break;
            
        default:
            RET_ERR(PSP_R_APPERR,
                    "Invalid type=%d in '%s' parameter",
                    descr->t, descr->name);
    }
    
    return PSP_R_OK;

#undef SSRV
#undef MSLV
}


int psp_parse   (const char *str, const char **endptr,
                 void *rec,
                 char equals_c, const char *separators, const char *terminators,
                 psp_paramdescr_t *table)
{
    return psp_parse_as(str, endptr,
                        rec,
                        equals_c, separators, terminators,
                        table,
                        0);
}

int psp_parse_as(const char *str, const char **endptr,
                 void *rec,
                 char equals_c, const char *separators, const char *terminators,
                 psp_paramdescr_t *table,
                 int mode_flags)
{
    return psp_parse_v(str, endptr,
                       &rec, &table, 1,
                       equals_c, separators, terminators,
                       mode_flags);
}

static psp_paramdescr_t * FindItem(psp_paramdescr_t *table,
                                   const char       *name_b,
                                   size_t            namelen,
                                   int              *aux_offset_p)
{
  psp_paramdescr_t *item;
  psp_paramdescr_t *r;
  int               nested_aux_offset;

    *aux_offset_p = 0;

    if (table == NULL) return NULL;

    for (item = table; item->name != NULL; item++)
    {
        if      (item->t == PSP_T_NOP);
        else if (item->t == PSP_T_INCLUDE)
        {
            r = FindItem(item->var.p_include.table, name_b, namelen, &nested_aux_offset);
            if (r != NULL)
            {
                *aux_offset_p = item->offset + nested_aux_offset;
                return r;
            }
        }
        else if (cx_strmemcasecmp(item->name, name_b, namelen) == 0)
            return item;
    }

    return NULL;
}

static inline int isletnum(int c)
{
    return isalnum(c)  ||  c == '_'  ||  c == '-';
}

int psp_parse_v (const char *str, const char **endptr,
                 void *recs[], psp_paramdescr_t *tables[], int pair_count,
                 char equals_c, const char *separators, const char *terminators,
                 int mode_flags)
{
  int               r;
  const char       *srcp;
  const char       *name_b;
  size_t            namelen;
  int               is_term, is_sepr, is_eqc, is_nlet;

  int               npair;
  psp_paramdescr_t *item;
  int               known;
  psp_paramdescr_t  to_skip;
  int               aux_offset;
  char             *base_addr;

  char              nested_terms[255];
  int               nested_r;
  const char       *nested_endptr;
  
  char              valbuf[2000]; /*!!!*/
  char             *vbp;
  char              cur_q;

  char             *errp;
  
  int               v_int;
  char             *v_str;
#ifdef MAY_USE_REAL
  double            v_real;
#endif /* MAY_USE_REAL */

  char            **sli;  /* Selector List Item */
  psp_lkp_t        *lti;  /* Lookup Table Item */

  char *plugin_errstr;

    RESET_ERR();
  
    if ((mode_flags & PSP_MF_NOINIT) == 0)
        for (npair = 0;  npair < pair_count;  npair++)
        {
            r = PspDoInit(tables[npair], recs[npair]);
            if (r < 0) return r;
        }

    if (separators  == NULL) separators  = "";
    if (terminators == NULL) terminators = "";
    
    srcp = str;

    if (srcp != NULL)
        while (1)
        {
            /* Skip spaces (or whatever separators are) */
            is_term = 0;
            while (!(is_term = (*srcp == '\0'  ||  strchr(terminators, *srcp) != NULL))  &&
                   strchr(separators,  *srcp) != NULL)
                srcp++;

            /* Did we hit the terminator? */
            if (is_term) goto END_PARSE;

            /* Fine, let's extract param name */
            name_b = srcp;

            is_term = is_sepr = is_eqc = is_nlet = 0;
            while (!(is_term = (*srcp == '\0'  ||  strchr(terminators, *srcp) != NULL))  &&
                   !(is_sepr = (                   strchr(separators,  *srcp) != NULL))  &&
                   !(is_eqc  = (*srcp == equals_c))                                      &&
                   !(is_nlet = !isletnum(*srcp)))
                srcp++;

            namelen = srcp - name_b;
            
            if (namelen == 0  ||  is_nlet)
                RET_ERR(PSP_R_USRERR,
                        "Junk at position %zu", srcp - str + 1);

            /* And find it in the table */
#if 1
            for (npair = 0,              item = NULL, base_addr = NULL;
                 npair < pair_count  &&  item == NULL;
                 npair++)
            {
                item = FindItem(tables[npair], name_b, namelen, &aux_offset);
                if (item != NULL
                    &&
                    recs[npair]!= NULL /* Non-black-hole */)
                    base_addr = ((char *)(recs[npair])) + aux_offset;
            }
#else
            item = FindItem(table, name_b, namelen, &aux_offset);
            base_addr = ((char *)rec) + aux_offset;
#endif

            /* Was something found? */
            known = item != NULL;
            if (!known)
            {
                if ((mode_flags & PSP_MF_SKIP_UNKNOWN) == 0)
                    RET_ERR(PSP_R_USRERR,
                            "Unrecognized parameter '%.*s'", /*!!! -- non-portable*/
                            (int)namelen, name_b);
                
                bzero(&to_skip, sizeof(to_skip));
                to_skip.name = "-UNKNOWN-";
                to_skip.t    = -1;
                item = &to_skip;
            }

            /* Ok-ka-a-ay, what type of parameter is it, does it require '='? */
            if      (known  &&  item->t == PSP_T_FLAG  &&  is_eqc)
                RET_ERR(PSP_R_USRERR,
                        "Superfluous '%c' with '%s' flag parameter",
                        equals_c, item->name);
            else if (known  &&  item->t != PSP_T_FLAG  &&  !is_eqc)
                RET_ERR(PSP_R_USRERR,
                        "Parameter '%s' requires a value",
                        item->name);

            if (is_eqc) srcp++;
            
            if      (item->t == PSP_T_PSP)
            {
                /* Nested PSP... */
                snprintf(nested_terms, sizeof(nested_terms),
                         "%s%s", terminators, separators);
                if (base_addr != NULL) base_addr += item->offset;
                nested_r = psp_parse_as
                    (srcp, &nested_endptr,
                     base_addr,
                     item->var.p_psp.equals_c, item->var.p_psp.separators, nested_terms,
                     item->var.p_psp.descr,
                     mode_flags);
                if (nested_r < 0) return nested_r;

                srcp = nested_endptr;
            }
            else if (item->t == PSP_T_PLUGIN)
            {
                /* Plugin */
                plugin_errstr = NULL;
                if (base_addr != NULL) base_addr += item->offset;
                nested_r = item->var.p_plugin.parser
                    (srcp, &nested_endptr,
                     base_addr, item->size,
                     separators, terminators,
                     item->var.p_plugin.privptr, &plugin_errstr);
                if (nested_r < 0)
                    RET_ERR(nested_r,
                            "Error in '%s' parameter%s%s",
                            item->name,
                            plugin_errstr == NULL? "" : ":",
                            plugin_errstr == NULL? "" : plugin_errstr);

                srcp = nested_endptr;
            }
            else
            {
                /* Okay -- let's parse as a shell-string */
                vbp   = valbuf;
                cur_q = '\0';
                if (is_eqc)
                    while (*srcp != '\0')
                    {
                        /* Should check for string terminators first */
                        if (cur_q == '\0')
                        {
                            if (strchr(terminators, *srcp) != NULL  ||
                                strchr(separators,  *srcp) != NULL)
                                goto END_OF_PARAMVAL;
                        }
                        
                        /* An closing quote (same char as was opening quote)? */
                        if (*srcp == cur_q)
                        {
                            cur_q = '\0';
                            srcp++;
                            goto NEXT_CHAR;
                        }
                        
                        /* A quote char -- opening one? */
                        if (cur_q == '\0'  &&  (*srcp == '"'  ||  *srcp == '\''))
                        {
                            cur_q = *srcp;
                            srcp++;
                            goto NEXT_CHAR;
                        }
                        
                        /* Okay, it must be a character, so we must check for buffer overflow */
                        if (vbp - valbuf >= sizeof(valbuf) - 1)
                            RET_ERR(PSP_R_USRERR,
                                    "Value too long in '%s%c' specification",
                                    item->name, equals_c);
                        
                        /* A backslashed something? */
                        if (*srcp == '\\')
                        {
                            srcp++;
                            switch (*srcp)
                            {
                                case '\0': RET_ERR(PSP_R_USRERR,
                                                   "Backslash at end of line in '%s%c' specification",
                                                   item->name, equals_c);
                                case 'a':  *vbp = '\a';  break;
                                case 'b':  *vbp = '\b';  break;
                                case 'f':  *vbp = '\f';  break;
                                case 'n':  *vbp = '\n';  break;
                                case 'r':  *vbp = '\r';  break;
                                case 't':  *vbp = '\t';  break;
                                case 'v':  *vbp = '\v';  break;
                                default:   *vbp = *srcp;
                            }
                            srcp++;
                            vbp++;
                        }
                        /* No, just a regular character */
                        else
                        {
                            *vbp++ = *srcp++;
                        }
                        
 NEXT_CHAR:;
                    }

                if (cur_q != '\0')
                    RET_ERR(PSP_R_USRERR,
                            "Unterminated <%c>-string in '%s' value",
                            cur_q, item->name);

 END_OF_PARAMVAL:
                
                /* Terminate the string */
                *vbp++ = '\0';
                
                /* Okay, what should we do with obtained string? */
                switch (item->t)
                {
                    case PSP_T_INT:
                        /* Convert to int */
                        v_int = strtol(valbuf, &errp, 0);
                        if (errp == valbuf  ||  *errp != '\0')
                            RET_ERR(PSP_R_USRERR,
                                     "The '%s' is an invalid value for '%s%c' (integer expected)",
                                     valbuf, item->name, equals_c);
                        /* Check range */
                        if (item->var.p_int.minval < item->var.p_int.maxval  &&
                            (v_int < item->var.p_int.minval  ||
                             v_int > item->var.p_int.maxval))
                            RET_ERR(PSP_R_USRERR,
                                    "The '%s%c%s' is out of range [%d..%d]",
                                    item->name, equals_c, valbuf,
                                    item->var.p_int.minval, item->var.p_int.maxval);
                        /* Use */
                        PspSetParameter(item, base_addr, &v_int, 0);
                        break;

                    case PSP_T_SELECTOR:
                        /* Find integer */
                        for (sli = item->var.p_selector.list;
                             *sli != NULL;
                             sli++)
                            if (strcasecmp(valbuf, *sli) == 0) break;
                        if (*sli == NULL)
                            RET_ERR(PSP_R_USRERR,
                                    "Unrecognized value '%s' for '%s' parameter",
                                    valbuf, item->name);
                        v_int = sli - item->var.p_selector.list;
                        /* Use */
                        PspSetParameter(item, base_addr, &v_int, 0);
                        break;

                    case PSP_T_LOOKUP:
                        /* Find integer */
                        for (lti = item->var.p_lookup.table;
                             lti->label != NULL;
                             lti++)
                            if (strcasecmp(valbuf, lti->label) == 0) break;
                        if (lti->label == NULL)
                            RET_ERR(PSP_R_USRERR,
                                    "Unrecognized value '%s' for '%s' parameter",
                                    valbuf, item->name);
                        v_int = lti->val;
                        /* Use */
                        PspSetParameter(item, base_addr, &v_int, 0);
                        break;

                    case PSP_T_FLAG:
                        PspSetParameter(item, base_addr, &(item->var.p_flag.theval), 0);
                        break;
                        
                    case PSP_T_STRING:
                    case PSP_T_MSTRING:
                        v_str = valbuf;
                        PspSetParameter(item, base_addr, &v_str, 0);
                        break;
                        
#ifdef MAY_USE_REAL
                    case PSP_T_REAL:
                        /* Convert to double */
                        v_real = strtod(valbuf, &errp);
                        if (errp == valbuf  ||  *errp != '\0')
                            RET_ERR(PSP_R_USRERR,
                                     "The '%s' is an invalid value for '%s%c' (float expected)",
                                     valbuf, item->name, equals_c);
                        /* Check range */
                        if (item->var.p_real.minval < item->var.p_real.maxval  &&
                            (v_real < item->var.p_real.minval  ||
                             v_real > item->var.p_real.maxval))
                            RET_ERR(PSP_R_USRERR,
                                    "The '%s%c%s' is out of range [%f..%f]",
                                    item->name, equals_c, valbuf,
                                    item->var.p_real.minval, item->var.p_real.maxval);
                        /* Use */
                        PspSetParameter(item, base_addr, &v_real, 0);
                        break;
#endif /* MAY_USE_REAL */

                    case PSP_T_VOID:
                        /* Do nothing */
                        break;

                    /* Just to make GCC happy */
                    case PSP_T_NULL:
                    case PSP_T_NOP:
                    case PSP_T_PSP: 
                    case PSP_T_PLUGIN:
                    case PSP_T_INCLUDE:
                        ;
                }
            }
        }

 END_PARSE:
    
    if (endptr != NULL) *endptr = srcp;

    return PSP_R_OK;
}


void psp_free(void *rec, psp_paramdescr_t *table)
{
  psp_paramdescr_t  *item;

    if (rec == NULL) return;

    for (item = table; item->name != NULL; item++)
        if      (item->t == PSP_T_MSTRING  ||
                 item->t == PSP_T_PLUGIN)
            PspSetParameter(item, rec, NULL, 0);
        else if (item->t == PSP_T_PSP)
            psp_free(((char *)rec) + item->offset, item->var.p_psp.descr);
        else if (item->t == PSP_T_INCLUDE)
            psp_free(((char *)rec) + item->offset, item->var.p_include.table);
}


int  psp_set_include_table(psp_paramdescr_t *main_table,
                           const char *name, psp_paramdescr_t *include_table)
{
  psp_paramdescr_t  *item;

    if (name == NULL) return -1;

    for (item = main_table;  item->name != NULL;  item++)
        if (item->t == PSP_T_INCLUDE  &&
            item->name[0] == '#'      &&
            strcasecmp(item->name + 1, name) == 0)
        {
            item->var.p_include.table = include_table;
            return 0;
        }

    return -1;
}


const char *psp_error(void)
{
  const char *ret = psp_errp;
  
    RESET_ERR();
    return ret;
}
