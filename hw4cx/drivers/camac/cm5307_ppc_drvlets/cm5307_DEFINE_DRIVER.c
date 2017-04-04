#include <signal.h>
#include <fcntl.h>

#include "misclib.h"

#define __CM5307_DEFINE_DRIVER_C
#include "cxsd_driver.h" // This will include cm5307__DEFINE_DRIVER.h


enum
{   /* this enumeration according to man(3) pipe */
    PIPE_RD_SIDE = 0,
    PIPE_WR_SIDE = 1,
};
static int lam_pipe[2] = {-1, -1};
static int lam_N = -1;

static void sigusr_handler(int sig __attribute__((unused)))
{
  static char snd_byte = 0;

    signal(SIGUSR1, sigusr_handler);
    write(lam_pipe[PIPE_WR_SIDE], &snd_byte, sizeof(snd_byte));
}

static void sig2pipe_proc(int devid, void *devptr,
                          sl_fdh_t fdh, int fd, int mask, void *privptr)
{
  char           rcv_byte;
  CAMAC_LAM_CB_T cb = privptr;

    while (read(fd, &rcv_byte, sizeof(rcv_byte)) > 0);
    cb(devid, devptr);
    /* LAM should be re-enabled only here, instead of sigusr_handler(),
       because otherwise SIGUSR will be sent infinitely, blocking regular
       operation. */
    camac_setlam(CAMAC_REF, lam_N, 1);
}

const char * WATCH_FOR_LAM(int devid, void *devptr, int N, CAMAC_LAM_CB_T cb)
{
    if (pipe(lam_pipe) < 0)
        return "pipe()";
    if (set_fd_flags(lam_pipe[PIPE_RD_SIDE], O_NONBLOCK, 1) < 0  ||
        set_fd_flags(lam_pipe[PIPE_WR_SIDE], O_NONBLOCK, 1) < 0)
        return "set_fd_flags()";

    if (sl_add_fd(devid, devptr,
                  lam_pipe[PIPE_RD_SIDE], SL_RD,
                  sig2pipe_proc, cb) < 0)
        return "sl_add_fd()";

    lam_N = N;
    signal(SIGUSR1, sigusr_handler);
    camac_setlam(CAMAC_REF, lam_N, 1);

    return NULL;
}
