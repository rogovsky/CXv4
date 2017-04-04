#define ABSTRACT_NAME r0601
#include "camac_abstract_common.h"


ABSTRACT_INIT_FUNC()
{
  int  v;
  
    DO_NAF(ref, N1, 1, 0, &v);
    //DO_LOG("mask=%d", v);
    v = 0;
    DO_NAF(ref, N1, 0, 16, &v);
    
    return 0;
}

ABSTRACT_READ_FUNC()
{
  int  v;
  
    DO_NAF(ref, N1, 0, 2, &v);
    *rflags = status2rflags(DO_NAF(ref, N1, 0, 0, &v));
    //DO_LOG("do_read(@%d::%d): v=%d", N1, chan, v);
    *value = (v >> chan) & 1;

    return 0;
}

ABSENT_FEED_FUNC
