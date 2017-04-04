/*********************************************************************
  sktcanhal.h
      This file implements CAN Hardware Abstraction Layer
      via SocketCAN (Linux kernel ~= 2.6.25) API,
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
#include <sys/socket.h>


#include "canhal.h"

#include <net/if.h>
#include <linux/can.h>

/*!!! Damn!!! From FC10's /usr/include/linux/socket.h */
#ifndef AF_CAN
  #define AF_CAN              29      /* Controller Area Network      */
#endif
#ifndef PF_CAN
  #define PF_CAN              AF_CAN
#endif


static int  canhal_open_and_setup_line(int line_n, int speed_n, char **errstr)
{
  int                  fd;
  int                  saved_errno;
  struct sockaddr_can  addr;
  struct ifreq         ifr;
  
    speed_n &= 3;

    /* Create a CAN socket */
    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0)
    {
        *errstr = "socket(PF_CAN,SOCK_RAW,CAN_RAW)";
        return -1;
    }

    /* Bind it to specified CAN line */
    check_snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "can%d", line_n);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        *errstr = "ioctl(,SIOCGIFINDEX,)";
        goto CLEANUP_ON_ERROR;
    }
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        *errstr = "bind()";
        goto CLEANUP_ON_ERROR;
    }
    
    /*!!! No baud_rate setting as for now... */

    
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
  struct can_frame  frame;
  int               r;
  
    if (dlc > 8) dlc = 8;
    if (dlc < 0) dlc = 0; /*!!!???*/
  
    bzero(&frame, sizeof(frame));
    frame.can_id  = id;
    frame.can_dlc = dlc;
    if (dlc != 0) memcpy(frame.data, data, dlc);
    
    errno = 0;
    r = write(fd, &frame, sizeof(frame));
    if      (r >  0/*!!!*/) return CANHAL_OK;
    else if (r == 0) return CANHAL_ZERO;
    else             return CANHAL_ERR;
}

static int  canhal_recv_frame         (int  fd,
                                       int *id_p, int *dlc_p, uint8 *data)
{
  struct can_frame  frame;
  int               r;

    r = read(fd, &frame, sizeof(frame));
    if      (r >  0/*!!!*/) ;
    else if (r == 0) return CANHAL_ZERO;
    else             return CANHAL_ERR;
        
    *id_p  = frame.can_id;
    *dlc_p = frame.can_dlc;
    if (frame.can_dlc > 0) memcpy(data, frame.data, frame.can_dlc);

    return CANHAL_OK;
}
