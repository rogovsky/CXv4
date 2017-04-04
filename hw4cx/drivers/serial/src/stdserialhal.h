#ifndef SERIALHAL_PFX
  #error The "SERIALHAL_PFX" macro is undefined
#endif

#ifdef MOXA_KSHD485
  #include <sys/ioctl.h>
  #include <moxadevice.h>
#endif

#include "serialhal.h"


static int serialhal_opendev (int line, int baudrate,
                              serialhal_errreport_t errreport,
                              void * opaqueptr)
{
  int             fd;
  char            devpath[PATH_MAX];
  int             r;
  struct termios  newtio;

    sprintf(devpath, "/dev/tty%s%d", SERIALHAL_PFX, line);

    /* Open a descriptor to COM port... */
    fd = open(devpath, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        errreport(opaqueptr, "open(\"%s\")", devpath);
        return -1;
    }

    /* ...setup... */
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag     = baudrate | CRTSCTS*1 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag     = IGNPAR;
    newtio.c_oflag     = 0;
    newtio.c_lflag     = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN]  = 1;
    tcflush(fd, TCIFLUSH);
    errno = 0;
    r = tcsetattr(fd,TCSANOW,&newtio);
    if (r < 0)
    {
        errreport(opaqueptr, "tcsetattr()=%d", r);
        return -1;
    }
#ifdef MOXA_KSHD485
    {
      int  mode = RS485_2WIRE_MODE;

        r = ioctl(fd, MOXA_SET_OP_MODE, &mode);
        if (r < 0)
        {
            errreport(opaqueptr, "ioctl(MOXA_SET_OP_MODE, RS485_2WIRE_MODE)=%d", r);
            return -1;
        }
    }
#endif

    /* ...and turn it to non-blocking mode */
    if (set_fd_flags(fd, O_NONBLOCK, 1) != 0)
    {
        errreport(opaqueptr, "set_fd_flags()");
        close(fd);
        return -1;
    }

    return fd;
}
