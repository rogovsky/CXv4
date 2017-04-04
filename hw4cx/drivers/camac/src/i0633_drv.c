#define ABSTRACT_NAME i0633
#include "camac_abstract_common.h"


ABSENT_INIT_FUNC

ABSTRACT_READ_FUNC()
{
    *rflags = status2rflags(DO_NAF(ref, N1, 0,  0, value));
    return 0;
}

ABSTRACT_FEED_FUNC()
{
    *rflags = status2rflags(DO_NAF(ref, N1, 0, 16, value));
    return 0;
}
