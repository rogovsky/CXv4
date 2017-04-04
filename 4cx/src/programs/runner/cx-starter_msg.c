#include <stdio.h>
#include <stdarg.h>

#include "misclib.h"

#include "cx-starter_msg.h"


void reportinfo(const char *format, ...)
{
  va_list ap;

    va_start(ap, format);
    fprintf (stdout, "%s cx-starter: ",
             strcurtime());
    vfprintf(stdout, format, ap);
    fprintf (stdout, "\n");
    va_end(ap);
}
