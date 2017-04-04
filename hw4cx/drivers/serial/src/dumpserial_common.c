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

static struct
{
    const char *name;
    int         val;
} baudrate_table[] =
{
    {"600",   B600},
    {"1200",  B1200},
    {"2400",  B2400},
    {"4800",  B4800},
    {"9600",  B9600},
    {"19200", B19200},
    {"38400", B38400},
    {"57600", B57600},
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
  int    baudrate;
  int    n;

  int    r;
  int    fd;
  uint8  c;

    if (argc < 2  ||  argc > 3)
    {
        fprintf(stderr, "Usage: %s LINE [SPEED]\n",
                argv[0]);
        exit(1);
    }

    line     = atoi(argv[1]);
    baudrate = B9600;
    if (argc >= 3)
    {
        for (n = 0;  n < countof(baudrate_table);  n++)
            if (strcmp(argv[2], baudrate_table[n].name) == 0)
            {
                baudrate = baudrate_table[n].val;
                break;
            }
        if (n >= countof(baudrate_table))
        {
            fprintf(stderr, "%s: unknown baudrate \"%s\"\n",
                    argv[0], argv[2]);
            exit(1);
        }
    }

    fd = serialhal_opendev(line, baudrate, errreport, argv[0]);
    if (fd < 0) exit(2);
    set_fd_flags(fd, O_NONBLOCK, 0);

    while (1)
    {
        r = read(fd, &c, 1);
        if (r == 1)
            printf("0x%02x =%d\n", c, c);
        else
        {
            if (r == 0)
            {
                fprintf(stderr, "%s: EOF\n", argv[0]);
                exit(2);
            }
            else
            {
                fprintf(stderr, "%s: read()=%d: %s\n", argv[0],
                        r, strerror(errno));
            }
        }
    }
}
