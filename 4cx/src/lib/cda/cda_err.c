#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "misclib.h"

#include "cda.h"
#include "cdaP.h"


static char cda_lasterr_str[200] = "";


char *cda_last_err(void)
{
    return cda_lasterr_str[0] != '\0'? cda_lasterr_str : strerror(errno);
}

void cda_clear_err(void)
{
    cda_lasterr_str[0] = '\0';
}

void cda_set_err  (const char *format, ...)
{
  va_list  ap;
  
    va_start(ap, format);
    cda_vset_err(format, ap);
    va_end(ap);
}

void cda_vset_err (const char *format, va_list ap)
{
    vsnprintf(cda_lasterr_str, sizeof(cda_lasterr_str), format, ap);
    cda_lasterr_str[sizeof(cda_lasterr_str) - 1] = '\0';
}
