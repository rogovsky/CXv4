#ifndef __CXSD_MODMGR_H
#define __CXSD_MODMGR_H


#include "cxsd_driver.h"
#include "cxsd_frontend.h"
#include "cxsd_extension.h"


int  CxsdSetDrvLyrPath    (const char *path);
int  CxsdSetFndPath       (const char *path);
int  CxsdSetExtPath       (const char *path);
int  CxsdSetLibPath       (const char *path);

int  CxsdLoadDriver  (const char          *argv0,
                      const char          *typename, const char *drvname,
                      CxsdDriverModRec   **metric_p);
int  CxsdLoadLayer   (const char          *argv0,
                      const char          *lyrname,
                      CxsdLayerModRec    **metric_p);
int  CxsdLoadFrontend(const char          *argv0,
                      const char          *name,
                      CxsdFrontendModRec **metric_p);
int  CxsdLoadExt     (const char *argv0, const char *name);
int  CxsdLoadLib     (const char *argv0, const char *name);


int  CxsdRegisterFrontend  (CxsdFrontendModRec *metric);
int  CxsdDeregisterFrontend(CxsdFrontendModRec *metric);

int  CxsdActivateFrontends (int instance);


#endif /* __CXSD_MODMGR_H */

