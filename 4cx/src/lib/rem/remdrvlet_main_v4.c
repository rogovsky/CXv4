#include <signal.h>

#include "remdrv_proto_v4.h"
#include "remdrvlet.h"
#include "remcxsdP.h"


enum
{
    DRIVELET_INPUT_FD  = 0,
    DRIVELET_OUTPUT_FD = 1
};


/*### devid management #############################################*/

enum {FAKE_DEVID = 1};
remcxsd_dev_t  devinfo[1 + FAKE_DEVID];

remcxsd_dev_t    *remcxsd_devices = devinfo;
int               remcxsd_maxdevs = countof(devinfo);
CxsdLayerModRec **remcxsd_layers  = NULL;
int               remcxsd_numlyrs = 0;


int  AllocateDevID(void)
{
    return -1;
}

void FreeDevID    (int devid)
{
    sl_break();
}


/*### main() #######################################################*/

int remdrvlet_main(CxsdDriverModRec *metric, void *devptr)
{
  int            r;
  int            on = 1;

  int            devid = FAKE_DEVID;
  remcxsd_dev_t *dev   = remcxsd_devices + devid;
  int            s_devid;

    signal(SIGPIPE, SIG_IGN);
    sl_set_uniq_checker  (cxsd_uniq_checker);
    fdio_set_uniq_checker(cxsd_uniq_checker);

    InitRemDevRec(dev);
    dev->metric = metric;
    dev->devptr = devptr;

    if (devptr != NULL  &&  metric->privrecsize != 0)
        bzero(devptr, metric->privrecsize);

    /* Should also check that both stdin and stdout are sockets */
  
    r = setsockopt(DRIVELET_INPUT_FD,  SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    r = setsockopt(DRIVELET_OUTPUT_FD, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    dev->s       = DRIVELET_INPUT_FD;
    dev->fhandle = fdio_register_fd(devid, devptr,
                                    dev->s,  FDIO_STREAM,
                                    ProcessPacket, NULL,
                                    REMDRV_MAXPKTSIZE,
                                    sizeof(remdrv_pkt_header_t),
                                    FDIO_OFFSET_OF(remdrv_pkt_header_t, pktsize),
                                    FDIO_SIZEOF   (remdrv_pkt_header_t, pktsize),
#if   BYTE_ORDER == LITTLE_ENDIAN
                                    FDIO_LEN_LITTLE_ENDIAN,
#elif BYTE_ORDER == BIG_ENDIAN
                                    FDIO_LEN_BIG_ENDIAN,
#else
#error Sorry, the "BYTE_ORDER" is neither "LITTLE_ENDIAN" nor "BIG_ENDIAN"
#endif
                                    1,
                                    0);
    /*!!! Check dev->fhandle < 0? */

    if (metric->mr.init_mod != NULL  &&
        metric->mr.init_mod() < 0) return -1;

    r = sl_main_loop();
    
    ENTER_DRIVER_S(devid, s_devid);
    if (metric->term_dev != NULL)
        metric->term_dev(devid, dev->devptr);
    LEAVE_DRIVER_S(s_devid);
    if (metric->mr.term_mod != NULL)
        metric->mr.term_mod();

    return r == 0? 0 : 1;
}
