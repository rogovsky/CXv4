#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "misclib.h"
#include "timeval_utils.h"


void SleepBySelect(int usecs)
{
  struct timeval  deadline;
  struct timeval  now;
  struct timeval  timeout;
    
    gettimeofday(&deadline, NULL);
    timeval_add_usecs(&deadline, &deadline, usecs);

    while (1)
    {
        gettimeofday(&now, NULL);
        if (timeval_subtract(&timeout, &deadline, &now) != 0) return;
        if (select(0, NULL, NULL, NULL, &timeout) == 0)       return;
    }
}
