#include <stdio.h>
#include <stdarg.h>

#include "Knobs.h"
#include "KnobsP.h"


static char Knobs_lasterr_str[200] = "";


char *KnobsLastErr(void)
{
    return Knobs_lasterr_str;
}

void KnobsClearErr(void)
{
    Knobs_lasterr_str[0] = '\0';
}

void KnobsSetErr  (const char *format, ...)
{
  va_list        ap;
  
    va_start(ap, format);
    KnobsVSetErr(format, ap);
    va_end(ap);
}

void KnobsVSetErr (const char *format, va_list ap)
{
    vsnprintf(Knobs_lasterr_str, sizeof(Knobs_lasterr_str), format, ap);
    Knobs_lasterr_str[sizeof(Knobs_lasterr_str) - 1] = '\0';
}
