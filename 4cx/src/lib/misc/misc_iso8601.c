#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "misclib.h"


/*
 *  stroftime()
 *      Returns human-readable string for specified time,
 *      in ISO-8601-compliant format, YYYY-MM-DD?HH:MM:SS
 *  Args:
 *      when  time in question
 *      dts   string to separate date and time
 */

char *stroftime     (time_t when, char *dts)
{
  static char  buf[100];
  struct tm   *st      = localtime(&when);
  
    sprintf(buf, "%04d-%02d-%02d%s%02d:%02d:%02d",
            st->tm_year + 1900, st->tm_mon + 1, st->tm_mday,
            dts,
            st->tm_hour, st->tm_min, st->tm_sec);

    return buf;
}

char *stroftime_msc (struct timeval *when, char *dts)
{
  static char  buf[100];
  struct tm   *st      = localtime(&(when->tv_sec));
  
    sprintf(buf, "%04d-%02d-%02d%s%02d:%02d:%02d.%03d",
            st->tm_year + 1900, st->tm_mon + 1, st->tm_mday,
            dts,
            st->tm_hour, st->tm_min, st->tm_sec, (int)(when->tv_usec / 1000));

    return buf;
}

/*
 *  strcurtime()
 *      Returns human-readable string for current time, YYYY-MM-DD-HH:MM:SS
 */

char *strcurtime    (void)
{
    return stroftime(time(NULL), "-");
}

char *strcurtime_msc(void)
{
  struct timeval now;

    gettimeofday(&now, NULL);
    return stroftime_msc(&now, "-");
}
