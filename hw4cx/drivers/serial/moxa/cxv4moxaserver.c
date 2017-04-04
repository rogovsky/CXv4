#include <stdio.h>

#include "remsrv.h"


remcxsd_dev_t  devinfo[1 + 100]; // An almost arbitrary limit -- >32*2 (32 RS485 devices per port, 2 ports)

remcxsd_dev_t *remcxsd_devices = devinfo;
int            remcxsd_maxdevs = countof(devinfo);


static remsrv_conscmd_descr_t cxv4moxaserver_commands[] =
{
    {NULL, NULL, NULL}
};

int main(int argc, char *argv[])
{
    return remsrv_main(argc, argv,
                       "cxv4moxaserver", "CXv4 MOXA server",
                       cxv4moxaserver_commands, NULL);
}
