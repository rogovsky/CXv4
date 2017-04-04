#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cx.h"
#include "cxlib.h"


/*
 *  _cx_errlist
 *      A list of cx-specific error descriptions for cx_strerror().
 */

static char *_cx_errlist[] = {
    "Internal cxlib problem or data corrupt",
    "Unknown host",
    "Unknown CX response error code",
    "Handshake timed out",
    "Too many clients",
    "Access to server denied",
    "Bad CX request code",
    "Packet is too big for server",
    "Invalid channel number",
    "Server ran out of memory",
    "Too many connections",
    "Request was successfully sent",
    "Connection is closed",
    "Bad connection number",
    "Connection is busy",
    "Connection socket is closed by server",
    "Connection is killed by server",
    "Received packet is too big",
    "Set request in subscription",
    "Invalid CX argument",
    "Request packet will be too big",
    "One of the forks has error code set",
    "Client-supplied buffer is too small",
    "Invalid connection type",
    "Internal server error",
    "Wrong usage",
    "Server speaks different CX-protocol version",
    "Server-side handshake timeout",
};


/*
 *  cx_strerror
 *      The same as strerror(3), but additionally understands CExxx codes
 *      (which are negative).
 *
 *  Behaves exactly as strerror().
 */

char *cx_strerror(int errnum)
{
  static char buf[100];
    
    if (errnum >= 0) return strerror(errnum);

    if (errnum < -(signed)(sizeof(_cx_errlist) / sizeof(_cx_errlist[0])))
    {
        sprintf(buf, "Unknown CX error %d", errnum);
        return buf;
    }
    
    return _cx_errlist[-errnum - 1];
}


/*
 *  cx_perror
 *      The same as perror(3), but additionally understands CExxx codes
 *      (which are negative).
 *
 *  Behaves exactly as perror().
 */

void  cx_perror  (const char *s)
{
  int         errnum = errno;
  const char *colon;

    if (s == NULL || *s == '\0')
        s = colon = "";
    else
        colon = ": ";

    fprintf(stderr, "%s%s%s\n", s, colon, cx_strerror(errnum));
}

/*
 *  cx_perror2
 *      Similar to perror(3)/cx_perror, but prepends a timestamp and
 *      program name.
 */

void  cx_perror2 (const char *s, const char *argv0)
{
  int         errnum = errno;

    if (s     == NULL  ||  *s     == '\0') s     = NULL;
    if (argv0 == NULL  ||  *argv0 == '\0') argv0 = NULL;
    
    fprintf(stderr, "%s %s%s%s%s%s\n",
            strcurtime(),
            argv0 != NULL? argv0 : "", argv0 != NULL? ": " : "",
            s     != NULL? s     : "", s     != NULL? ": " : "",
            cx_strerror(errnum));
}


/*
 *  Note: this list should be kept in sync with
 *  include/cxlib.h::CAR_XXX enum.
 */
static char *_cx_carlist[] = {
    "Connection succeeded",
    "Connection failed",
    "Connection was closed on error",
    "Server cycle notification had arrived",

    "Data chunk had arrived",
    "RSLV result",
    "FRESH_AGE",
    "STRS",
    "RDS",
    "QUANT",
    
    "Echo packet",
    "Connection was killed by server",
};


/*
 *  cx_strreason
 *      Returns string describing asynchronous call reason.
 *
 *  Behaves like strerror()/cx_strerror()
 */

char *cx_strreason(int reason)
{
  static char buf[100];
  
    if (reason < 0  ||  reason >= (signed)(sizeof(_cx_carlist) / sizeof(_cx_carlist[0])))
    {
        sprintf(buf, "Unknown reason code %d", reason);
        return buf;
    }

    return _cx_carlist[reason];
}


/*
 *  Note: this list should be kept in sync with
 *  include/cx.h::CXRF_XXX/CXCF_XXX enum.
 */

static char *_cx_rflagslist[] = {
    /*  0 */ "CAMAC_NO_X",    "No 'X' from device",
    /*  1 */ "CAMAC_NO_Q",    "No 'Q' from device",
    /*  2 */ "IO_TIMEOUT",    "I/O timeout expired",
    /*  3 */ "REM_C_PROBL",   "Remote controller problem",
    /*  4 */ "OVERLOAD",      "Input channel overload",
    /*  5 */ "UNSUPPORTED",   "Unsupported feature/channel",
    /*  6 */ "", "",
    /*  7 */ "", "",
    /*  8 */ "", "",
    /*  9 */ "", "",
    /* 10 */ "INVAL",         "Invalid parameter",
    /* 11 */ "WRONG_DEV",     "Wrong device",
    /* 12 */ "CFG_PROBL",     "Configuration problem",
    /* 13 */ "DRV_PROBL",     "Driver internal problem",
    /* 14 */ "NO_DRV",        "Driver loading problem",
    /* 15 */ "OFFLINE",       "Device is offline",

    /* 16 */ "CALCERR",       "Formula calculation error",
    /* 17 */ "DEFUNCT",       "Defunct channel",
    /* 18 */ "OTHEROP",       "Other operator is active",
    /* 19 */ "PRGLYCHG",      "Channel was programmatically changed",
    /* 20 */ "NOTFOUND",      "Channel not found",
    /* 21 */ "COLOR_WEIRD",   "Value is weird",
    /* 22 */ "", "",
    /* 23 */ "", "",
    /* 24 */ "", "",
    /* 25 */ "", "",
    /* 26 */ "", "",
    /* 27 */ "", "",
    /* 28 */ "COLOR_YELLOW",  "Value in yellow zone",
    /* 29 */ "COLOR_RED",     "Value in red zone",
    /* 30 */ "ALARM_RELAX",   "Relaxing after alarm",
    /* 31 */ "ALARM_ALARM",   "Alarm!",
};


/*
 *  cx_strrflag_short, cx_strrflag_long
 *      Return string describing rflag.
 *
 *  Arg:
 *      shift  the shift-factor of the flag in question.
 *
 *  Return:
 *      A string describing the flag.
 *      cx_strrflag_short() returns a one-word flag name,
 *      cx_strrflag_long()  returns a user-readable description.
 */

char *cx_strrflag_short(int shift)
{
  static char buf[100];
  
    if (shift < 0  ||  shift >= (signed)(sizeof(_cx_rflagslist) / sizeof(_cx_rflagslist[0]) / 2)  ||
      _cx_rflagslist[shift * 2][0] == '\0')
    {
        sprintf(buf, "?EF_%d?", shift);
        return buf;
    }

    return _cx_rflagslist[shift * 2];
}

char *cx_strrflag_long (int shift)
{
  static char buf[100];
  
    if (shift < 0  ||  shift >= (signed)(sizeof(_cx_rflagslist) / sizeof(_cx_rflagslist[0]) / 2)  ||
      _cx_rflagslist[shift * 2 + 1][0] == '\0')
    {
        sprintf(buf, "Unknown error flag #%d", shift);
        return buf;
    }

    return _cx_rflagslist[shift * 2 + 1];
}


/*
 *  cx_parse_chanref
 *      Parses the server+channel specification in a form
 *          [serverref]!chan_n
 *      or
 *          [serverref]::chan_n
 *      or
 *          [host:N].chan_n
 *
 */

int   cx_parse_chanref(const char *spec,
                       char *srvnamebuf, size_t srvnamebufsize,
                       chanaddr_t  *chan_n,
                       char **channame_p)
{
  const char *sepr1 = "!";
  const char *sepr2 = "::";
  const char *sepr3 = ".";
    
  const char *sepr;
  size_t      seplen;
  char       *endptr;
  size_t      len;
  const char *colonp;
  const char *channame;

    sepr   = strstr(spec, sepr1);
    seplen = strlen(sepr1);
    if (sepr == NULL)
    {
        sepr   = strstr(spec, sepr2);
        seplen = strlen(sepr2);
    }
    if (sepr == NULL  &&
        (colonp = strchr(spec, ':')) != NULL)
    {
        sepr   = strstr(colonp, sepr3);
        seplen = strlen(sepr3);
    }
    
    if (sepr == NULL)
    {
        errno = CEINVAL;
        return -1;
    }

    len = sepr - spec;
    if (len > srvnamebufsize - 1) len = srvnamebufsize - 1;
    memcpy(srvnamebuf, spec, len);
    srvnamebuf[len] = '\0';

    channame = sepr + seplen;
    *channame_p = channame;

    if (chan_n != NULL)
    {
        *chan_n = strtol(channame, &endptr, 10);
        if (endptr == sepr + seplen  ||  *endptr != '\0')
        *chan_n = -1;
    }

    return 0;
}
