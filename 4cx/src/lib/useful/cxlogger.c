#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cxlogger.h"


#define OPT_LOG_MSECS 1


static ll_catdesc_t  *cats           = NULL;
static unsigned int   LOGF_ERRNO     = 0;
static char           sysname[32]    = "(unset)";
static int            verbosity      = 0;
static int            is_on_console  = 0;
static ll_filedesc_t *fls            = NULL;

static int            cats_count     = 0;
static int            fls_count      = 0;

static char           thishost[64];

static int            is_initialized = 0;


static void ObtainHostName(void)
{
  struct utsname  buf;
  char           *p;
  size_t          namelen;

    uname(&buf);
    p = strchr(buf.nodename, '.');
    if (p == NULL)  namelen = strlen(buf.nodename);
    else            namelen = p - buf.nodename;
    
    if (namelen > sizeof(thishost) - 1) namelen = sizeof(thishost) - 1;

    memcpy(thishost, buf.nodename, namelen);
    thishost[namelen] = '\0';

    if (namelen == 0) strcpy(thishost, "?host?");
}


int   ll_init    (ll_catdesc_t *categories, int errno_mask,
                  const char *ident,
                  int verblevel, int consolity,
                  ll_filedesc_t *files)
{
  int  ret;

    cats          = categories;
    LOGF_ERRNO    = errno_mask;
    strzcpy(sysname, ident, sizeof(sysname));
    verbosity     = verblevel;
    is_on_console = consolity;
    fls           = files;

    for (cats_count = 0;
         cats != NULL  &&  cats[cats_count].name != NULL;
         cats_count++);
    for (fls_count = 0;
         fls != NULL   &&  fls [fls_count].path  != NULL;
         fls_count++) fls[fls_count].fd = -1;

    ObtainHostName();

    openlog(sysname, 0, LOG_PID);
    ret = ll_reopen(1);

    is_initialized = 1;

    return ret;
}

int   ll_reopen  (int initial)
{
  int  ret = 0;
  int  x;

    for (x = 0;  x < fls_count;  x++)
    {
        if (fls[x].fd >= 0)
            close(fls[x].fd);
        fls[x].fd = open(fls[x].path,
                         O_RDWR | O_CREAT | O_APPEND | O_NOCTTY,
                         0644);
        if (fls[x].fd < 0)
            if (ret >= 0) ret = -x - 1;
    }

    return ret;
}

void  ll_access  (void)
{
  int          x;
  int8         junk;
  struct stat  sinfo;

    for (x = 0;  x < fls_count;  x++)
        if (fls[x].fd >= 0  &&
            fstat(fls[x].fd, &sinfo) == 0  &&  S_ISREG(sinfo.st_mode))
            read (fls[x].fd, &junk, 1);
}


void  ll_logline (int category, int level,
                  const char *subaddress, const char *format, ...)
{
  va_list        ap;
  
    va_start(ap, format);
    ll_vlogline(category, level, subaddress, format, ap);
    va_end(ap);
}

void  ll_vlogline(int category, int level,
                  const char *subaddress, const char *format, va_list ap)
{
  int            saved_errno = errno;
  int            errno_present = (category & LOGF_ERRNO) != 0;
  int            unconfigured;
  int            fd;
  int            r;

  char           buf[2048];
  struct timeval timenow;
  char          *colon;

  int            curlen;
  int            msglen;
  int            newlinepos;

    /* 0. Check the level */
    if (level > verbosity) return;


    /* I. Was the logging subsystem initialized? */
    if (!is_initialized)
    {
        vsnprintf(buf, sizeof(buf) - 1, format, ap);
        syslog(LOG_ERR, errno_present ? "%s: %m" : "%s", buf);
        fprintf(stderr, "%s\n", buf);
        return;
    }
    

    /* II. Prepare the line */
    
    /* 1. Create line header */
    if (subaddress == NULL  ||  subaddress[0] == '\0')
    {
        subaddress = colon = "";
    }
    else
    {
        colon = ": ";
    }
    
    gettimeofday(&timenow, NULL);

    curlen = snprintf(buf, sizeof(buf) - 1, "%s %s %s: %s%s",
#if OPT_LOG_MSECS
                      stroftime_msc(&timenow,       " "),
#else
                      stroftime    (timenow.tv_sec, " "),
#endif
                      thishost, sysname,
                      subaddress, colon);

    /* 2. Add formatted message */
    if (curlen > 0  &&  ((unsigned int)curlen) < sizeof(buf) - 1)
    {
        msglen = vsnprintf(buf + curlen, sizeof(buf) - curlen - 1,
                           format, ap);

        if (msglen > 0) curlen += msglen;
        else            curlen  = msglen;
    }
    else
    {
        /* Hmmm...  how can we signal that <subaddress> had overflowed buf? */
    }

    /* 3. Add error description, if requested */
    if (errno_present  &&
        curlen > 0  &&  ((unsigned int)curlen) < sizeof(buf) - 1)
    {
        msglen = snprintf(buf + curlen, sizeof(buf) - curlen - 1,
                          ": %s", strerror(saved_errno));

        if (msglen > 0) curlen += msglen;
        else            curlen  = msglen;
    }
                      
    /* 4. And a terminating newline */
    /* "-2" since we need room for '\n' and terminating '\0' */
    if (curlen > 0  &&  ((unsigned int)curlen) < sizeof(buf) - 2)
    {
        newlinepos = curlen;
    }
    else
    {
        newlinepos = sizeof(buf) - 2;
    }

    buf[newlinepos]     = '\n';
    buf[newlinepos + 1] = '\0';

    /* III. Direct the resulting message to appropriate destinations */
    category &=~ LOGF_ERRNO;
    if (category < 0            ||
        category >= cats_count  ||
        cats[category].is_on == 0) category = 0;

    /* Additional check for case of uninitializedness */
    unconfigured = category >= cats_count  ||  cats[category].is_on == 0;

    if (!unconfigured)
    {
        fd = fls[cats[category].fref].fd;
        if (fd >= 0)
        {
            r = write(fd, buf, newlinepos + 1);
            if (r > 0)
            {
                if (cats[category].sync) fsync(fd);
            }
            else
                unconfigured = 1;
        }
        else
            unconfigured = 1;
    }
    if (unconfigured  &&  !is_on_console)
        syslog(LOG_LOCAL0, "%s", buf);

    if (is_on_console) fprintf(stderr, "%s", buf);
}
