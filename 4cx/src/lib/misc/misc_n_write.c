#include <unistd.h>
#include <errno.h>

#include "cx_sysdeps.h"

#include "misc_macros.h"
#include "misclib.h"


ssize_t n_write(int fd, const void *buf, size_t count)
{
  ssize_t      result;
  const uint8 *ptr  = buf;
  size_t       rest = count;

    while (rest)
    {
        result = write(fd, ptr,
#if OPTION_CYGWIN_BROKEN_WRITE_SIZE != 0
                       rest >= OPTION_CYGWIN_BROKEN_WRITE_SIZE ? OPTION_CYGWIN_BROKEN_WRITE_SIZE
                                                               :
#endif
                       rest
                      );
        if (result < 0)
        {
            if (SHOULD_RESTART_SYSCALL())  continue;
            return -1;
        }

        rest -= result;
        ptr  += result;
    }

    return count;
}
