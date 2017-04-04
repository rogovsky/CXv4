/*********************************************************************
  c4lcanhal.h
      This file implements CAN Hardware Abstraction Layer
      via can4linux (c4l) /dev/canX API,
      and is intended to be included by cankoz_lyr*
*********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


#include "canhal.h"

#include "can4linux.h"


static int  canhal_open_and_setup_line(int line_n, int speed_n, char **errstr)
{
  int              fd;
  char             devpath[100];
  int              saved_errno;
  volatile Command_par_t cmd;
  Config_par_t     cfg;
  
  static int       baud_ids  [4] = {125, 250, 500, 1000};
  
    speed_n &= 3;

    sprintf(devpath, "/dev/can%d", line_n);
    fd = open(devpath, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        *errstr = "open()";
        return -1;
    }

    cmd.cmd = CMD_STOP;
    if (ioctl(fd, COMMAND, &cmd) < 0)
    {
        *errstr = "SetBitRate/CMD_STOP";
        goto CLEANUP_ON_ERROR;
    }

    cfg.target = CONF_TIMING;
    cfg.val1   = baud_ids[speed_n];
    if (ioctl(fd, CONFIG, &cfg) < 0)
    {
        *errstr = "SetBitRate/CONF_TIMING";
        goto CLEANUP_ON_ERROR;
    }

    cmd.cmd = CMD_START;
    if (ioctl(fd, COMMAND, &cmd) < 0)
    {
        *errstr = "SetBitRate/CMD_START";
        goto CLEANUP_ON_ERROR;
    }

    return fd;
    
 CLEANUP_ON_ERROR:
    saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return -1;
}

static void canhal_close_line         (int  fd)
{
    close(fd);
}

static int  canhal_send_frame         (int  fd,
                                       int  id,   int  dlc,   uint8 *data)
{
  canmsg_t  frame;
  int       r;
  
    if (dlc > 8) dlc = 8;
    if (dlc < 0) dlc = 0; /*!!!???*/
  
    bzero(&frame, sizeof(frame));
    frame.id     = id;
    frame.length = dlc;
    if (dlc != 0) memcpy(frame.data, data, dlc);
    
    errno = 0;
    r = write(fd, &frame, 1);
    if      (r == 1) return CANHAL_OK;
    else if (r == 0) return CANHAL_ZERO;
    else             return CANHAL_ERR;
}

static int  canhal_recv_frame         (int  fd,
                                       int *id_p, int *dlc_p, uint8 *data)
{
  canmsg_t  frame;
  int       r;

    r = read(fd, &frame, 1);
    if      (r == 1) ;
    else if (r == 0) return CANHAL_ZERO;
    else             return CANHAL_ERR;
        
    *id_p  = frame.id;
    *dlc_p = frame.length;
    if (frame.length > 0) memcpy(data, frame.data, frame.length);

    return CANHAL_OK;
}
