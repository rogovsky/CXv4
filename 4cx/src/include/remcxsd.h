#ifndef __REMCXSD_H
#define __REMCXSD_H


#include <netinet/in.h>

#include "cxsd_driver.h"
#include "cxscheduler.h"
#include "fdiolib.h"


/*##################################################################*/

typedef struct
{
    int                 in_use;       // Is this cell currently used
    int                 being_processed;
    int                 being_destroyed;

    int                 dlref;        // ==0 -- builtin, >0 -- index in dlopen()'ed-table

    /* I/O stuff */
    int                 s;            // Host communication socket
    sl_fdh_t            s_fdh;        // ...its cxscheduler descriptor is used upon handshake only
    fdio_handle_t       fhandle;      // The fdiolib handle for .s, used after handshake

    time_t              when;
    struct sockaddr_in  cln_addr;
    char                drvname[32];  // Name of this driver
    size_t              drvnamelen;   // ...and its length (used only  when readin
    sl_tid_t            tid;          // Cleanup tid for semi-connected drivers (is active during handshake)

    /* Device's vital activity */
    int                 inited;       // Was the driver inited?
    CxsdDriverModRec   *metric;       // Pointer to the driver's methods-table
    int                 lyrid;        // ID of layer serving this driver
    int                 businfocount;
    int                 businfo[20];  // Bus ID, as supplied by host /*!!!==CXSD_DB_MAX_BUSINFOCOUNT*/
    void               *devptr;       // Device's private pointer

    int                 loglevel;
    int                 logmask;
} remcxsd_dev_t;


// These two should be provided by "user"
extern remcxsd_dev_t    *remcxsd_devices;
extern int               remcxsd_maxdevs;
extern CxsdLayerModRec **remcxsd_layers;
extern int               remcxsd_numlyrs;

/*##################################################################*/

/* Note:
       report_TO_FD  is used on unfinished connections w/o working devid, and
                     such connections are always closed immediately afterwards.
                     So, there's no need to check for sending error.
       report_TO_DEV is mainly used prior to closing the connection,
                     but it also implements DoDriverLog(), so that there IS
                     need to check for sending error(s).
                     (However (as of 18.07.2013), that is unused.)
*/

void remcxsd_report_to_fd (int fd,    int code,            const char *format, ...)
                           __attribute__ ((format (printf, 3, 4)));
int  remcxsd_report_to_dev(int devid, int code, int level, const char *format, ...)
                           __attribute__ ((format (printf, 4, 5)));

int  AllocateDevID(void);
void FreeDevID    (int devid);

void ProcessPacket(int uniq, void *unsdptr,
                   fdio_handle_t handle, int reason,
                   void *inpkt,  size_t inpktsize,
                   void *privptr);

/*** Utilities ******************************************************/

void InitRemDevRec(remcxsd_dev_t *dp);

void remcxsd_debug (const char *format, ...)
                   __attribute__ ((format (printf, 1, 2)));

int  cxsd_uniq_checker(const char *func_name, int uniq);


#endif /* __REMCXSD_H */
