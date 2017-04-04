#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "misc_macros.h"
#include "misclib.h"


int check_fd_state(int fd, int rw)
{
  int             r;
  fd_set          fds;
  struct timeval  timeout;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = timeout.tv_usec = 0;
        
        r = select(fd + 1,
                   rw==O_RDONLY?&fds:NULL, rw==O_RDONLY?NULL:&fds, NULL,
                   &timeout);
        if (r < 0)
        {
            if (SHOULD_RESTART_SYSCALL()) continue;
            return -1;
        }
        break;
    }

    return r;
}
