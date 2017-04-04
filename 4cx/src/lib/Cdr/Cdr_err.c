#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "Cdr.h"
#include "CdrP.h"


static char Cdr_lasterr_str[300] = "";


char *CdrLastErr(void)
{
    return Cdr_lasterr_str[0] != '\0'? Cdr_lasterr_str : strerror(errno);
}

void CdrClearErr(void)
{
    Cdr_lasterr_str[0] = '\0';
}

void CdrSetErr  (const char *format, ...)
{
  va_list        ap;
  
    va_start(ap, format);
    CdrVSetErr(format, ap);
    va_end(ap);
}

void CdrVSetErr (const char *format, va_list ap)
{
    vsnprintf(Cdr_lasterr_str, sizeof(Cdr_lasterr_str), format, ap);
    Cdr_lasterr_str[sizeof(Cdr_lasterr_str) - 1] = '\0';
}
