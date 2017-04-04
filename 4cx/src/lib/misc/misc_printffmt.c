#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "misc_macros.h"

#include "misclib.h"


static char _printffmt_lasterr_str[160] = "";
#define CLEAR_ERR() _printffmt_lasterr_str[0] = '\0'


int ParseDoubleFormat (const char *fmt,
                       int  *flags_p,
                       int  *width_p,
                       int  *precision_p,
                       char *conv_c_p)
{
  int         ret       = -1;
  const char *p         = fmt;
  int         flags     = 0;
  int         width     = -1;
  int         precision = -1;
  char        conv_c    = 'f';
  char       *endptr;

    if (fmt == NULL)
        goto RETURN_VALUES;
  
    CLEAR_ERR();
  
    /* Does the string start with '%'? */
    if (*p++ != '%')
    {
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: \"%s\" doesn't start with a '%%'",
                       __FUNCTION__, fmt);
        errno = EINVAL;
        goto RETURN_VALUES;
    }

    /* I. Check for '#','0','-',' ','+' flags */
    while (1)
    {
        if      (*p == '#')  flags |= FMT_ALTFORM;
        else if (*p == '0')  flags |= FMT_ZEROPAD;
        else if (*p == '-')  flags |= FMT_LEFTADJUST;
        else if (*p == ' ')  flags |= FMT_SPACE;
        else if (*p == '+')  flags |= FMT_SIGN;
        else if (*p == '\'') flags |= FMT_GROUPTHNDS;
        else break;

        p++;
    }
    /* And take care of flags' precedence */
    if (flags & FMT_LEFTADJUST) flags &=~ FMT_ZEROPAD;
    if (flags & FMT_SIGN)       flags &=~ FMT_SPACE;

    /* II. Is there a "width" field? */
    if (isdigit(*p))
    {
        width = strtol(p, &endptr, 10);
        p = endptr;
    }

    /* III. A ".precision" field? */
    if (*p == '.')
    {
        p++;
        precision = 0;
        if (isdigit(*p))
        {
            precision = strtol(p, &endptr, 10);
            p = endptr;
        }
    }

    /* IV. A conversion character... */
    if (*p == '\0'  ||  strchr("eEfFgG", *p) == NULL)
    {
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: \"%s\" doesn't end with a valid conversion specifier",
                       __FUNCTION__, fmt);
        errno = EINVAL;
        goto RETURN_VALUES;
    }
    conv_c = *p;

    /* Should we check for a required NUL after conversion character? */

    ret = 0;
    
    /* Return the values */
 RETURN_VALUES:
    *flags_p     = flags;
    *width_p     = width;
    *precision_p = precision;
    *conv_c_p    = conv_c;
    
    return ret;
}

int CreateDoubleFormat(char *buf, size_t bufsize,
                       int   max_width, int max_precision,
                       int   flags,
                       int   width,
                       int   precision,
                       char  conv_c)
{
  char  width_buf    [10];  //  Hopefully 9 digits will be enough
  char  precision_buf[10];  //  for width & precision...

    CLEAR_ERR();
  
    if (max_width     >  0  &&  width     > max_width)     width     = max_width;
    if (max_precision >= 0  &&  precision > max_precision) precision = max_precision;

    if (width >= 0)
        sprintf(width_buf,     "%d",  width);
    else
        width_buf[0] = '\0';
    if (precision >= 0)
        sprintf(precision_buf, ".%d", precision);
    else
        precision_buf[0] = '\0';

    if (check_snprintf(buf, bufsize, "%%%s%s%s%s%s%s%s%s%c",
                       flags & FMT_ALTFORM?    "#":"",
                       flags & FMT_ZEROPAD?    "0":"",
                       flags & FMT_LEFTADJUST? "-":"",
                       flags & FMT_SPACE?      " ":"",
                       flags & FMT_SIGN?       "+":"",
                       flags & FMT_GROUPTHNDS? "\'":"",
                       width_buf, precision_buf, conv_c) != 0)
    {
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: buffer[%zu] is too small for format",
                       __FUNCTION__, bufsize);
        return -1;
    }

    return 0;
}

int         GetTextColumns(const char *dpyfmt, const char *units)
{
  int   cols;
    
  int   flags;
  int   width;
  int   precision;
  char  conv_c;
    
    ParseDoubleFormat(dpyfmt, &flags, &width, &precision, &conv_c);

    if (width < 0)
    {
        if (precision < 0)
            precision = 6;
        
        switch (tolower(conv_c))
        {
            case 'e':
                width = 1/*sign*/ + 1/*before_point*/ +
                    (precision==0? 0:precision+1)/*point+after_point*/ +
                    4/*e+EE*/;
                break;

            case 'f':
                width = 5/*Just guess how many digits before point*/ +
                    (precision==0? 0:precision+1)/*point+after_point*/;
                break;

            case 'g':
                width = 8 /*Just guess -- %g is completely unpredictable*/;
                break;
        }
    }
    
    cols = width;
    if (units != NULL) cols += strlen(units);
    
    return cols;
}

typedef struct
{
    const char *label;
    const char *format;
} fmtdesc_t;

static fmtdesc_t std_fmts[] =
{
    {"degcw", "%5.2f"},
    {"zad",   "%015.4f"},
    {NULL, NULL}
};

#define DEF_FORMAT "%8.3f"
const char *GetTextFormat (const char *dpyfmt)
{
  fmtdesc_t *p;

    CLEAR_ERR();
  
    if (dpyfmt == NULL  ||  dpyfmt[0] == '\0')
        return DEF_FORMAT;

    if (dpyfmt[0] == '%')
        return dpyfmt;
    else if (dpyfmt[0] == '=')
    {
        for (p = std_fmts;  p->label != NULL;  p++)
            if (strcasecmp(dpyfmt + 1, p->label) == 0)
                return p->format;

        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: unknown format \"%s\"",
                       __FUNCTION__, dpyfmt);
    }
    else
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: \"%s\" doesn't start with a '%%'",
                       __FUNCTION__, dpyfmt);
    
    return NULL;
}


int ParseIntFormat    (const char *fmt,
                       int  *flags_p,
                       int  *width_p,
                       int  *precision_p,
                       char *conv_c_p)
{
  int         ret       = -1;
  const char *p         = fmt;
  int         flags     = 0;
  int         width     = -1;
  int         precision = -1;
  char        conv_c    = 'd';
  char       *endptr;

    if (fmt == NULL)
        goto RETURN_VALUES;
  
    CLEAR_ERR();
  
    /* Does the string start with '%'? */
    if (*p++ != '%')
    {
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: \"%s\" doesn't start with a '%%'",
                       __FUNCTION__, fmt);
        errno = EINVAL;
        goto RETURN_VALUES;
    }

    /* I. Check for '#','0','-',' ','+' flags */
    while (1)
    {
        if      (*p == '#')  flags |= FMT_ALTFORM;
        else if (*p == '0')  flags |= FMT_ZEROPAD;
        else if (*p == '-')  flags |= FMT_LEFTADJUST;
        else if (*p == ' ')  flags |= FMT_SPACE;
        else if (*p == '+')  flags |= FMT_SIGN;
        else if (*p == '\'') flags |= FMT_GROUPTHNDS;
        else break;

        p++;
    }
    /* And take care of flags' precedence */
    if (flags & FMT_LEFTADJUST) flags &=~ FMT_ZEROPAD;
    if (flags & FMT_SIGN)       flags &=~ FMT_SPACE;

    /* II. Is there a "width" field? */
    if (isdigit(*p))
    {
        width = strtol(p, &endptr, 10);
        p = endptr;
    }

    /* III. A ".precision" field? */
    if (*p == '.')
    {
        p++;
        precision = 0;
        if (isdigit(*p))
        {
            precision = strtol(p, &endptr, 10);
            p = endptr;
        }
    }

    /* IV. A conversion character... */
    if (*p == '\0'  ||  strchr("diouxX", *p) == NULL)
    {
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: \"%s\" doesn't end with a valid conversion specifier",
                       __FUNCTION__, fmt);
        errno = EINVAL;
        goto RETURN_VALUES;
    }
    conv_c = *p;

    /* Should we check for a required NUL after conversion character? */

    ret = 0;
    
    /* Return the values */
 RETURN_VALUES:
    *flags_p     = flags;
    *width_p     = width;
    *precision_p = precision;
    *conv_c_p    = conv_c;
    
    return ret;
}

int CreateIntFormat   (char *buf, size_t bufsize,
                       int   max_width, int max_precision,
                       int   flags,
                       int   width,
                       int   precision,
                       char  conv_c)
{
    return CreateDoubleFormat(buf, bufsize,
                              max_width, max_precision,
                              flags, width, precision, conv_c);
}

int         GetIntColumns (const char *dpyfmt, const char *units)
{
  int   cols;
    
  int   flags;
  int   width;
  int   precision;
  char  conv_c;
    
    ParseIntFormat(dpyfmt, &flags, &width, &precision, &conv_c);

    if (width < 0)
    {
        if (precision < 0)
            precision = 0;
        
        switch (tolower(conv_c))
        {
            case 'd':
            case 'i':
                width = strlen("-2147483648"); // signed 1<<31
                break;

            case 'u':
                width = strlen( "4294967295"); // 0xFFFFFFFFU
                break;

            case 'o':
                width = strlen("037777777777"); // 0xFFFFFFFFU
                break;

            case 'x':
                width = strlen("0xFFFFFFFF");
                break;
        }
    }
    
    cols = width;
    if (units != NULL) cols += strlen(units);
    
    return cols;
}

static fmtdesc_t std_int_fmts[] =
{
    {NULL, NULL}
};

#define DEF_INT_FORMAT "%5d"
const char *GetIntFormat  (const char *dpyfmt)
{
  fmtdesc_t *p;

    CLEAR_ERR();
  
    if (dpyfmt == NULL  ||  dpyfmt[0] == '\0')
        return DEF_INT_FORMAT;

    if (dpyfmt[0] == '%')
        return dpyfmt;
    else if (dpyfmt[0] == '=')
    {
        for (p = std_int_fmts;  p->label != NULL;  p++)
            if (strcasecmp(dpyfmt + 1, p->label) == 0)
                return p->format;

        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: unknown format \"%s\"",
                       __FUNCTION__, dpyfmt);
    }
    else
        check_snprintf(_printffmt_lasterr_str, sizeof(_printffmt_lasterr_str),
                       "%s: \"%s\" doesn't start with a '%%'",
                       __FUNCTION__, dpyfmt);
    
    return NULL;
}


char *printffmt_lasterr(void)
{
    return _printffmt_lasterr_str;
}
