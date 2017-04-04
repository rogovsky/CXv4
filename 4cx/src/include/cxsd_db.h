#ifndef __CXSD_DB_H
#define __CXSD_DB_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <unistd.h> // For ssize_t


typedef struct _CxsdDbInfo_t_struct *CxsdDb;


CxsdDb  CxsdDbLoadDb(const char *argv0,
                     const char *def_scheme, const char *reference);
int     CxsdDbDestroy(CxsdDb db);

int     CxsdDbFindDevice (CxsdDb  db, const char *devname, ssize_t devnamelen);
int     CxsdDbResolveName(CxsdDb      db,
                          const char *name,
                          int        *devid_p,
                          int        *chann_p);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_DB_H */
