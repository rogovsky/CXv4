#include <unistd.h>
#include <fcntl.h>

#include "misclib.h"


int set_fd_flags  (int fd, int mask, int onoff)
{
  int  r;
  int  flags;

    /* Get current flags */
    flags = r = fcntl(fd, F_GETFL, 0);
    if (r < 0) return -1;

    /* Modify the value */
    flags &=~ mask;
    if (onoff != 0) flags |= mask;

    /* And try to set it */
    r = fcntl(fd, F_SETFL, flags);
    if (r < 0) return -1;

    return 0;
}
