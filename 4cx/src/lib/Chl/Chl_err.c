#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "Chl.h"
#include "ChlP.h"


static char Chl_lasterr_str[200] = "";


char *ChlLastErr(void)
{
    return Chl_lasterr_str[0] != '\0'? Chl_lasterr_str : strerror(errno);
}

void ChlClearErr(void)
{
    Chl_lasterr_str[0] = '\0';
}

void ChlSetErr  (const char *format, ...)
{
  va_list        ap;
  
    va_start(ap, format);
    ChlVSetErr(format, ap);
    va_end(ap);
}

void ChlVSetErr (const char *format, va_list ap)
{
    vsnprintf(Chl_lasterr_str, sizeof(Chl_lasterr_str), format, ap);
    Chl_lasterr_str[sizeof(Chl_lasterr_str) - 1] = '\0';
}
