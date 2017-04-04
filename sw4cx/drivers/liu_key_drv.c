#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cxsd_driver.h"
#include "cda.h"

#include "drv_i/liu_key_drv_i.h"


typedef struct {
    int            devid;
    int            fd;
    sl_fdh_t       fd_handle;
    sl_tid_t       tid;

    int            prev_val;

    const char    *dev_path;
    const char    *on_off_action;
    const char    *on_on_action;

    cda_context_t  cid;
    cda_dataref_t  on_off_ref;
    cda_dataref_t  on_on_ref;
} liu_key_privrec_t;

static psp_paramdescr_t liu_key_params[] = 
{
    PSP_P_MSTRING("dev",    liu_key_privrec_t, dev_path,      NULL, 1000),
    PSP_P_MSTRING("on_off", liu_key_privrec_t, on_off_action, NULL, 10000),
    PSP_P_MSTRING("on_on",  liu_key_privrec_t, on_on_action,  NULL, 10000),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

static void SetKeyVal(liu_key_privrec_t *me, int val, int reflect_in_hw)
{
    if (val == me->prev_val) return;
    me->prev_val = val;

    if (reflect_in_hw)
    {
        if (val == LIU_KEY_STATE_ON)
        {
            if (cda_ref_is_sensible(me->on_off_ref))
                cda_process_ref(me->on_off_ref, CDA_OPT_IS_W | CDA_OPT_DO_EXEC,
                                0.0,
                                NULL, 0);
        }
        else
        {
            if (cda_ref_is_sensible(me->on_on_ref))
                cda_process_ref(me->on_on_ref,  CDA_OPT_IS_W | CDA_OPT_DO_EXEC,
                                0.0,
                                NULL, 0);
        }
    }

    ReturnInt32Datum(me->devid, LIU_KEY_CHAN_STATE, val, 0);
}

static void ScheduleReOpen(liu_key_privrec_t *me);

static void liu_key_fd_p(int devid, void *devptr,
                         sl_fdh_t fdhandle, int fd, int mask,
                         void *privptr)
{
  liu_key_privrec_t *me = devptr;
  int                r;
  unsigned char      c;

    r = read(me->fd, &c, 1);
    if      (r <= 0)
    {
        SetKeyVal(me, LIU_KEY_STATE_ABSENT, 1);
        ScheduleReOpen(me);
    }
    else if (c == 0xaa)
    {
        SetKeyVal(me, LIU_KEY_STATE_OFF,    1);
    }
    else if (c == 0xbb)
    {
        SetKeyVal(me, LIU_KEY_STATE_ON,     1);
    }
}
                                              
static int OpenRS232(const char *devpath)
{
  int             fd;
  int             r;
  struct termios  newtio;

    /* Open a descriptor to COM port */
    fd = open(devpath, O_RDONLY | O_NOCTTY | O_NONBLOCK | O_EXCL);
    if (fd < 0)
        return fd;

    /* Perform serial setup wizardry */
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag     = B9600 | CRTSCTS*1 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag     = IGNPAR;
    newtio.c_oflag     = 0;
    newtio.c_lflag     = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN]  = 1;
    tcflush(fd, TCIFLUSH);
    errno = 0;
    r = tcsetattr(fd,TCSANOW,&newtio);

    return fd;
}
static void OpenKey(liu_key_privrec_t *me)
{
    me->fd = OpenRS232(me->dev_path);
    if (me->fd < 0)
    {
        ScheduleReOpen(me);
        return;
    }

    me->fd_handle = sl_add_fd(me->devid, me, me->fd, SL_RD, liu_key_fd_p, NULL);
}

static void tout_proc   (int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  liu_key_privrec_t *me = devptr;

    me->tid = -1;
    OpenKey(me);
}
static void ScheduleReOpen(liu_key_privrec_t *me)
{
    if (me->fd >= 0)
    {
        close(me->fd);            me->fd        = -1;
        sl_del_fd(me->fd_handle); me->fd_handle = -1;
    }

    me->tid = sl_enq_tout_after(me->devid, me, 1*1000000, tout_proc, NULL);
}

static int liu_key_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  liu_key_privrec_t *me = devptr;

    me->devid     = devid;
    me->fd        = -1;
    me->fd_handle = -1;
    me->tid       = -1;
    me->prev_val  = -1;

    // Initialize actions
    if ((me->cid = cda_new_context(devid, devptr,
                                   NULL, 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }

    if (me->on_off_action    != NULL  &&
        me->on_off_action[0] != '\0')
    {
        me->on_off_ref = cda_add_formula(me->cid, "",
                                         me->on_off_action, CDA_OPT_IS_W,
                                         NULL, 0,
                                         0, NULL, NULL);
        if (me->on_off_ref < 0)
        {
            DoDriverLog(devid, 0, "cda_add_formula(on_off): %s", cda_last_err());
            return -CXRF_DRV_PROBL;
        }
    }
    if (me->on_on_action     != NULL  &&
        me->on_on_action[0]  != '\0')
    {
        me->on_on_ref  = cda_add_formula(me->cid, "",
                                         me->on_on_action,  CDA_OPT_IS_W,
                                         NULL, 0,
                                         0, NULL, NULL);
        if (me->on_on_ref < 0)
        {
            DoDriverLog(devid, 0, "cda_add_formula(on_on): %s", cda_last_err());
            return -CXRF_DRV_PROBL;
        }
    }

    OpenKey(me);
    SetKeyVal(me, LIU_KEY_STATE_ABSENT, 0);

    return 0;
}


DEFINE_CXSD_DRIVER(liu_key,  "LIU pult key driver",
                   NULL, NULL,
                   sizeof(liu_key_privrec_t), liu_key_params,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   liu_key_init_d, NULL, NULL);
