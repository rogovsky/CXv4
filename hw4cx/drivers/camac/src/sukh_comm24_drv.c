#define ABSTRACT_NAME sukh_comm24
#include "camac_abstract_common.h"


ABSENT_INIT_FUNC

ABSTRACT_READ_FUNC()
{
  int  w;
    
    *rflags = status2rflags(DO_NAF(ref, N1, chan, 0, &w));
    *value = w;
    
    return 0;
}

ABSTRACT_FEED_FUNC()
{
  int  w;

    w = *value;
    if (w < 0  ||  w > 3) w = 4; // Treat everything outside [0..3] as "off"
    *rflags = status2rflags(DO_NAF(ref, N1, chan, 16, &w));
    *value = w;
    
    return 0;
}
