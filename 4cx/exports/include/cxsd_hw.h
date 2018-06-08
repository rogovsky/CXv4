#ifndef __CXSD_HW_H
#define __CXSD_HW_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cxsd_db.h"


typedef void (*cxsd_hw_cleanup_proc)(int uniq);


int  CxsdHwSetDb   (CxsdDb db);
int  CxsdHwActivate(const char *argv0);

int  CxsdHwSetSimulate(int state);
int  CxsdHwSetCacheRFW(int state);
int  CxsdHwSetDefDrvLogMask(int mask);
int  CxsdHwSetCycleDur(int cyclesize_us);
int  CxsdHwGetCurCycle(void);
int  CxsdHwTimeChgBack(void);
int  CxsdHwSetCleanup (cxsd_hw_cleanup_proc proc);


//////////////////////////////////////////////////////////////////////

int  cxsd_uniq_checker(const char *func_name, int uniq);

void cxsd_hw_do_cleanup(int uniq);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_HW_H */
