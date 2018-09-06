#ifndef __CDR_H
#define __CDR_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <time.h>

#include "datatree.h"


enum
{
    CDR_OPT_SYNTHETIC = 1 << 0,
    CDR_OPT_READONLY  = 1 << 1,
};

enum
{
    CDR_MODE_OPT_PART_DATA       = 1 << 0,
    CDR_MODE_OPT_PART_PARAMS     = 1 << 1,
    CDR_MODE_OPT_PART_EVERYTHING =
        CDR_MODE_OPT_PART_DATA | CDR_MODE_OPT_PART_PARAMS,
};


DataSubsys  CdrLoadSubsystem(const char *def_scheme,
                             const char *reference,
                             const char *argv0);

void     *CdrFindSection    (DataSubsys subsys, const char *type, const char *name);
DataKnob  CdrGetMainGrouping(DataSubsys subsys);

void  CdrDestroySubsystem(DataSubsys subsys);
void  CdrDestroyKnobs(DataKnob list, int count);

void  CdrSetSubsystemRO  (DataSubsys  subsys, int ro);

int   CdrRealizeSubsystem(DataSubsys  subsys,
                          int         cda_ctx_options,
                          const char *defserver,
                          const char *argv0);
int   CdrRealizeKnobs    (DataSubsys  subsys,
                          const char *baseref,
                          DataKnob list, int count);

void  CdrProcessSubsystem(DataSubsys subsys, int cause_conn_n,  int options,
                                                    rflags_t *rflags_p);
void  CdrProcessKnobs    (DataSubsys subsys, int cause_conn_n,  int options,
                          DataKnob list, int count, rflags_t *rflags_p);

int   CdrSetKnobValue(DataSubsys subsys, DataKnob  k, double      v, int options);
int   CdrSetKnobText (DataSubsys subsys, DataKnob  k, const char *s, int options);
int   CdrSetKnobVect (DataSubsys subsys, DataKnob  k, const double *data, int nelems,
                                                                     int options);

int   CdrAddHistory  (DataKnob  k, int histring_len);
void  CdrShiftHistory(DataSubsys subsys);

int   CdrBroadcastCmd(DataKnob  k, const char *cmd, int info_int);

int   CdrSaveSubsystemMode(DataSubsys subsys,
                           const char *filespec, int         options,
                           const char *comment);
int   CdrLoadSubsystemMode(DataSubsys subsys,
                           const char *filespec, int         options,
                           char   *commentbuf, size_t commentbuf_size);
int   CdrStatSubsystemMode(const char *filespec,
                           time_t *cr_time,
                           char   *commentbuf, size_t commentbuf_size);


/* Error descriptions */
char *CdrLastErr(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CDR_H */
