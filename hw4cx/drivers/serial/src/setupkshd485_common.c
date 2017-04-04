#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/param.h> // For PATH_MAX, at least on ARM@MOXA

#include <termios.h>

#include "misc_types.h"
#include "misc_macros.h"
#include "misclib.h"

#ifndef SERIALHAL_FILE_H
  #error The "SERIALHAL_FILE_H" macro is undefined
#else
  #include SERIALHAL_FILE_H
#endif


static const char *baudrate_list[] =
{
    "1200",
    "2400",
    "4800",
    "9600",
    "19200",
    "38400",
    "57600"
};

static void errreport(void *opaqueptr,
                      const char *format, ...)
{
  char    *argv0       = opaqueptr;
  int      saved_errno = errno;

  va_list  ap;

    fprintf(stderr, "%s: ", argv0);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(saved_errno));
}

int main(int argc, char *argv[])
{
  int    line;
  int    serial;
  int    addr;
  int    baudrate_n;

  int    r;
  int    fd;

  uint8  seq[8];

    if (argc < 4  ||  argc > 5)
    {
        fprintf(stderr, "Usage: %s LINE SERIAL ADDR-TO-SET [BAUDRATE-TO-SET]\n",
                argv[0]);
        exit(1);
    }

    line   = atoi(argv[1]);
    serial = atoi(argv[2]);
    addr   = atoi(argv[3]);
    if (argc < 5) baudrate_n = 3;
    else
    {
        for (baudrate_n = 0;
             baudrate_n < countof(baudrate_list);
             baudrate_n++)
            if (strcmp(argv[4], baudrate_list[baudrate_n]) == 0)
                break;
        if (baudrate_n >= countof(baudrate_list))
        {
            fprintf(stderr, "%s: unknown baudrate \"%s\"\n",
                    argv[0], argv[4]);
            exit(1);
        }
    }

    seq[0] = 0x00;
    seq[1] = 0xAA;
    seq[2] = 'W';
    seq[3] = 'S';
    seq[4] = serial >> 8;
    seq[5] = serial & 0xFF;
    seq[6] = baudrate_n;
    seq[7] = addr;
    
    fd = serialhal_opendev(line, B600, errreport, argv[0]);
    if (fd < 0) exit(2);

    errno = 0;
    r = write(fd, seq, sizeof(seq));
    if (r != sizeof(seq))
    {
        fprintf(stderr, "%s: write()!=%zu, ==%d: %s\n",
                argv[0], sizeof(seq), r, strerror(errno));
        exit(2);
    }

    return 0;
}
