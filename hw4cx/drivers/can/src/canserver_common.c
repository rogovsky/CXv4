#include <stdio.h>

#include "cankoz_lyr.h"
#include "remsrv.h"


#ifndef CANSERVER_ISSUE
  #define CANSERVER_ISSUE "CX canserver"
#endif


remcxsd_dev_t  devinfo[1 + 200]; // An almost arbitrary limit -- >64*2

remcxsd_dev_t *remcxsd_devices = devinfo;
int            remcxsd_maxdevs = countof(devinfo);

static void disable_log_frame(int               nargs     __attribute__((unused)),
                              const char       *argp[]    __attribute__((unused)),
                              void             *context,
                              remsrv_cprintf_t  do_printf)
{
  int  lyr_n;

    for (lyr_n = 1;  lyr_n < remcxsd_numlyrs;  lyr_n++)
        if (remcxsd_layers[lyr_n]          != NULL  &&
            remcxsd_layers[lyr_n]->vmtlink != NULL  &&
            ((cankoz_vmt_t*)(remcxsd_layers[lyr_n]->vmtlink))->set_log_to != NULL)
            ((cankoz_vmt_t*)(remcxsd_layers[lyr_n]->vmtlink))->set_log_to(0);
    do_printf(context, "Frame logging is disabled\n");
}

static void enable_log_frame (int               nargs     __attribute__((unused)),
                              const char       *argp[]    __attribute__((unused)),
                              void             *context,
                              remsrv_cprintf_t  do_printf)
{
  int  lyr_n;

    for (lyr_n = 1;  lyr_n < remcxsd_numlyrs;  lyr_n++)
        if (remcxsd_layers[lyr_n]          != NULL  &&
            remcxsd_layers[lyr_n]->vmtlink != NULL  &&
            ((cankoz_vmt_t*)(remcxsd_layers[lyr_n]->vmtlink))->set_log_to != NULL)
            ((cankoz_vmt_t*)(remcxsd_layers[lyr_n]->vmtlink))->set_log_to(1);
    do_printf(context, "Frame logging is enabled\n");
}

static void exec_ff(int               nargs     __attribute__((unused)),
                    const char       *argp[]    __attribute__((unused)),
                    void             *context   __attribute__((unused)),
                    remsrv_cprintf_t  do_printf __attribute__((unused)))
{
  int  lyr_n;

    for (lyr_n = 1;  lyr_n < remcxsd_numlyrs;  lyr_n++)
        if (remcxsd_layers[lyr_n]          != NULL  &&
            remcxsd_layers[lyr_n]->vmtlink != NULL  &&
            ((cankoz_vmt_t*)(remcxsd_layers[lyr_n]->vmtlink))->send_0xff != NULL)
            ((cankoz_vmt_t*)(remcxsd_layers[lyr_n]->vmtlink))->send_0xff();
}

static remsrv_conscmd_descr_t canserver_commands[] =
{
    {"disable_log_frame", disable_log_frame, ""},
    {"enable_log_frame",  enable_log_frame,  ""},
    {"ff",                exec_ff,           ""},
    {NULL, NULL, NULL}
};

static const char * canserver_clninfo(int devid)
{
  cankoz_vmt_t *vmt;
  int           dev_line;
  int           dev_kid;
  int           dev_code;

  char          devinfo_buf[100];
  static char   buf2return [100];

    if ((vmt = GetLayerVMT(devid)) != NULL  &&
        vmt->get_dev_info(devid, &dev_line, &dev_kid, &dev_code) >= 0)
        sprintf(devinfo_buf, "(%d,%-2d 0x%02X)",
                dev_line, dev_kid, dev_code);
    else
        sprintf(devinfo_buf, "-");

    sprintf(buf2return, "%12s", devinfo_buf);

    return buf2return;
}


int main(int argc, char *argv[])
{
    return remsrv_main(argc, argv,
                       "canserver", CANSERVER_ISSUE,
                       canserver_commands, canserver_clninfo);
}
