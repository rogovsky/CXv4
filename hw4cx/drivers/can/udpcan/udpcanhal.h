/*********************************************************************
  udpcanhal.h
      This file implements CAN Hardware Abstraction Layer
      via UDP-CAN (CAN over UDP by Panov/Bolkhovityanov)
      and is intended to be included by cankoz_lyr*
*********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "misclib.h"

#include "canhal.h"

#include "udpcan_proto.h"


static int  canhal_open_and_setup_line(int line_n, int speed_n, char **errstr)
{
  int                 fd;
  int                 r;
  int                 saved_errno;

  unsigned long       host_ip;
  struct sockaddr_in  srv_addr;
  size_t              bsize;    // Parameter for setsockopt

    host_ip = inet_addr("127.0.0.1");
    /*!!! If use something else, look in layerinfo(?!) and do gethostbyname() */

    /* Record server's ip:port */
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port   = htons(UDP_CAN_PORT_BASE + line_n);
    memcpy(&(srv_addr.sin_addr), &host_ip, sizeof(host_ip));

    /* Create a UDP socket */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        *errstr = "socket(AF_INET,SOCK_DGRAM)";
        return -1;
    }
    /* Turn it into nonblocking mode */
    set_fd_flags(fd, O_NONBLOCK, 1);
    /* And set RCV buffer size */
    bsize = 1500*1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *) &bsize, sizeof(bsize));
    /* Specify the other endpoint */
    r = connect(fd, &srv_addr, sizeof(srv_addr));
    if (r != 0)
    {
        *errstr = "connect()";
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
  udp_can_frame_b_t  frame;
  int                r;

    if (dlc > 8) dlc = 8;
    if (dlc < 0) dlc = 0; /*!!!???*/
  
    bzero(&frame, sizeof(frame));

    frame.can_id_b[0] =  id        & 0xFF;
    frame.can_id_b[1] = (id >>  8) & 0xFF;
    frame.can_id_b[2] = (id >> 16) & 0xFF;
    frame.can_id_b[3] = (id >> 24) & 0xFF;
    frame.can_dlc_b[0] = dlc;
    if (dlc != 0) memcpy(frame.can_data, data, dlc);

    errno = 0;
    r = write(fd, &frame, sizeof(frame));
    if      (r >  0/*!!!*/) return CANHAL_OK;
    else if (r == 0) return CANHAL_ZERO;
    else             return CANHAL_ERR;
}

static int  canhal_recv_frame         (int  fd,
                                       int *id_p, int *dlc_p, uint8 *data)
{
  udp_can_frame_b_t  frame;
  int                r;

    r = read(fd, &frame, sizeof(frame));
    if      (r <  0)            return CANHAL_ERR;
    else if (r < sizeof(frame)) return CANHAL_ZERO;

    *id_p = frame.can_id_b[0]        |
           (frame.can_id_b[1] <<  8) |
           (frame.can_id_b[2] << 16) |
           (frame.can_id_b[3] << 24);
    *dlc_p = frame.can_dlc_b[0];
    if (*dlc_p > 8)
    {
        /* Log this somehow? */
        errno = EFBIG;
        return CANHAL_ERR;
    }
    if (*dlc_p > 0) memcpy(data, frame.can_data, *dlc_p);

    return CANHAL_OK;
}
