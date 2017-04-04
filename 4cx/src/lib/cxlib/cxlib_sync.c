#include "cxlib_wait_procs.h"
#include "cxscheduler.h"


int _cxlib_begin_wait(void)
{
    return sl_main_loop();
}

int _cxlib_break_wait(void)
{
    return sl_break();
}
