#include <signal.h>

#include "misclib.h"
#include "cx_sysdeps.h"


int set_signal(int signum, void (*handler)(int))
{
  struct sigaction  act;
  
    bzero(&act, sizeof(act));
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags   = OPTION_SA_FLAGS;
    return sigaction(signum, &act, NULL);
}
