#ifndef __CANKOZ_LYR_H
#define __CANKOZ_LYR_H


#include "cx_version.h"
#include "sendqlib.h"

#include "cxsd_driver.h"
#include "cankoz_numbers.h"

#include "drv_i/cankoz_common_drv_i.h"


#define CANKOZ_LYR_NAME "cankoz"
enum
{
    CANKOZ_LYR_VERSION_MAJOR = 3,
    CANKOZ_LYR_VERSION_MINOR = 0,
    CANKOZ_LYR_VERSION = CX_ENCODE_VERSION(CANKOZ_LYR_VERSION_MAJOR,
                                           CANKOZ_LYR_VERSION_MINOR)
};


enum
{
    CANKOZ_LYR_OPTION_NONE                = 0,

    CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET = 1 << 31,
    CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET  = 1 << 30,
};


/* Driver-provided callbacks */

typedef void (*CanKozOnIdProc)   (int devid, void *devptr, int is_a_reset);
typedef void (*CanKozPktinProc)  (int devid, void *devptr,
                                  int desc,  size_t dlc, uint8 *data);
typedef void (*CanKozOnSendProc) (int devid, void *devptr,
                                  sq_try_n_t try_n, void *privptr);
typedef int  (*CanKozCheckerProc)(size_t dlc, uint8 *data, void *privptr);


/* Layer API for drivers */

typedef int  (*CanKozAddDevice) (int devid, void *devptr,
                                 int businfocount, int businfo[],
                                 int devcode,
                                 CanKozOnIdProc  ffproc,
                                 CanKozPktinProc inproc,
                                 int queue_size,
                                 int options);
typedef int  (*CanKozAddDevcode)(int handle, int devcode);
typedef int  (*CanKozGetDevVer) (int handle, 
                                 int *hw_ver_p, int *sw_ver_p,
                                 int *hw_code_p);

typedef void        (*CanKozQClear)     (int handle);

typedef sq_status_t (*CanKozQEnqTotal)  (int handle, sq_enqcond_t how,
                                         int tries, int timeout_us,
                                         CanKozOnSendProc on_send, void *privptr,
                                         int model_dlc, int dlc, uint8 *data);
typedef sq_status_t (*CanKozQEnqTotal_v)(int handle, sq_enqcond_t how,
                                         int tries, int timeout_us,
                                         CanKozOnSendProc on_send, void *privptr,
                                         int model_dlc, int dlc, ...);
typedef sq_status_t (*CanKozQEnq)       (int handle, sq_enqcond_t how,
                                         int dlc, uint8 *data);
typedef sq_status_t (*CanKozQEnq_v)     (int handle, sq_enqcond_t how,
                                         int dlc, ...);

typedef sq_status_t (*CanKozQErAndSNx)  (int handle,
                                         int dlc, uint8 *data);
typedef sq_status_t (*CanKozQErAndSNx_v)(int handle,
                                         int dlc, ...);

typedef sq_status_t (*CanKozQForeach)   (int handle,
                                         CanKozCheckerProc checker, void *privptr,
                                         sq_action_t action,
                                         int dlc, uint8 *data);
typedef sq_status_t (*CanKozQForeach_v) (int handle,
                                         sq_action_t action,
                                         int dlc, ...);


typedef int  (*CanKozHasRegs)  (int handle, int chan_base);
typedef void (*CanKozRegsRW)   (int handle, int action, int chan, int32 *wvp);

typedef void (*CanKozSetLogTo)  (int onoff);
typedef void (*CanKozSend0xFF)  (void);
typedef int  (*CanKozGetDevInfo)(int devid,
                                 int *line_p, int *kid_p, int *hw_code_p);


typedef struct
{
    /* General device management */
    CanKozAddDevice   add;
    CanKozAddDevcode  add_devcode;
    CanKozGetDevVer   get_dev_ver;

    /* Queue management */
    CanKozQClear      q_clear;
    
    CanKozQEnqTotal   q_enqueue;
    CanKozQEnqTotal_v q_enqueue_v;
    CanKozQEnq        q_enq;
    CanKozQEnq_v      q_enq_v;
    CanKozQEnq        q_enq_ons;
    CanKozQEnq_v      q_enq_ons_v;

    CanKozQErAndSNx   q_erase_and_send_next;
    CanKozQErAndSNx_v q_erase_and_send_next_v;

    CanKozQForeach    q_foreach;
    CanKozQForeach_v  q_foreach_v;

    /* Services */
    CanKozHasRegs     has_regs;
    CanKozRegsRW      regs_rw;

    /* Layer-level services */
    CanKozSetLogTo    set_log_to;
    CanKozSend0xFF    send_0xff;
    CanKozGetDevInfo  get_dev_info;
} cankoz_vmt_t;


#endif /* __CANKOZ_LYR_H */
