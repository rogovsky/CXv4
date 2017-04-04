/*********************************************************************
  marcanhal.h
      This file implements CAN Hardware Abstraction Layer
      for Marathon CAN-bus-PCI adapter, using kernel-2.4.x interface,
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

#include "candrv.h"


static int  canhal_open_and_setup_line(int line_n, int speed_n, char **errstr)
{
  int              fd;
  char             devpath[100];
  int              saved_errno;
  struct can_baud  br;
  
  static int       baud_ids  [4] = {BAUD_125K, BAUD_250K, BAUD_500K, BAUD_1M};
  
    speed_n &= 3;

    sprintf(devpath, "/dev/can%d", line_n);
    fd = open(devpath, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        *errstr = "open()";
        return -1;
    }

    br.baud = baud_ids[speed_n];
    if (ioctl(fd, CAN_SET_BAUDRATE, &br) < 0)
    {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        *errstr = "ioctl(,CAN_SET_BAUDRATE,)";
        return -1;
    }

    return fd;
}

static void canhal_close_line         (int  fd)
{
    close(fd);
}

static int  canhal_send_frame         (int  fd,
                                       int  id,   int  dlc,   uint8 *data)
{
  can_msg  frame;
  int      r;
  
    if (dlc > 8) dlc = 8;
    if (dlc < 0) dlc = 0; /*!!!???*/
  
    bzero(&frame, sizeof(frame));
    frame.id  = id;
    frame.len = dlc;
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
  can_msg  frame;
  int      r;

    r = read(fd, &frame, 1);
    if      (r == 1) ;
    else if (r == 0) return CANHAL_ZERO;
    else             return CANHAL_ERR;
        
    *id_p  = frame.id;
    *dlc_p = frame.len;
    if (frame.len > 0) memcpy(data, frame.data, frame.len);

    return CANHAL_OK;
}
