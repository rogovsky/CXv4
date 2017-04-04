#include <unistd.h>
#include <errno.h>

#include "misc_macros.h"
#include "misclib.h"


ssize_t n_read (int fd, void       *buf, size_t count)
{
  ssize_t  result;
  uint8   *ptr  = buf;
  size_t   rest = count;

    while (rest)
    {
        result = read(fd, ptr, rest);
        if (result < 0)
        {  
            if (SHOULD_RESTART_SYSCALL())  continue;
            return -1;
        }

        /* Nothing to read, but read() returns -- it is an EOF */
        if (result == 0)
        {
          if (rest == count) return 0;   /* Just a close() by peer? */
          errno = EPIPE;     return -1;  /* Data+EOF -- peer died */
        }

        rest -= result;
        ptr  += result;
    }

    return count;
}
