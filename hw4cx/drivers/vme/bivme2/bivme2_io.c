#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cxscheduler.h" /*!!! Should use DRIVELET API!!! */

#include "bivme2_io.h"

#include "libvmedirect.h"


enum
{
    MAXDEVS = 100
};

typedef struct
{
    int               is_used;

    int               devid;
    void             *devptr;

    uint32            base_addr;
    uint32            space_size; // As of 15.09.2011 -- unused; for future checks in layer

    int               irq_fd;
    sl_fdh_t          irq_fd_fdh;
    int               irq_n;
    bivme2_irq_proc   irq_proc;
} bivme2_io_devinfo_t;


static bivme2_io_devinfo_t devs[MAXDEVS];


static void irq_callback(int uniq, void *unsdptr,
                         sl_fdh_t fdh, int fd, int mask, void *privptr)
{
  bivme2_io_devinfo_t *dp = privptr;
  int                  r;
  int32                vector;

    r = read(fd, &vector, sizeof(vector));
fprintf(stderr, "%s() r=%d vector=%d\n", __FUNCTION__, r, vector);
    if (r == sizeof(vector))
        dp->irq_proc(dp->devid, dp->devptr, dp->irq_n, vector);
    else
        fprintf(stderr, "%s(devid=%d irq_n=%d): read()=%d\n",
                __FUNCTION__, dp->devid, dp->irq_n, r);
}

int bivme2_io_open    (int devid, void *devptr,
                       uint32 base_addr, uint32 space_size, int am,
                       int irq_n, bivme2_irq_proc irq_proc)
{
  int                  handle;
  bivme2_io_devinfo_t *dp;

  char                 dev_vmeiN[100];

  int  r;
  int  v;

  static int libvmedirect_inited = 0;

    if (!libvmedirect_inited)
    {
        r = libvme_init();
        if (r < 0) return r;

        libvmedirect_inited = 1;
    }

    v = 1;
    r = libvme_ctl(VME_SYSTEM_W, &v);

    v = am;
    r = libvme_ctl(VME_AM_W, &v);

    handle = 1;
    dp = devs + handle;
    dp->is_used = 1;

    /* Remember info */
    dp->devid      = devid;
    dp->devptr     = devptr;
    dp->base_addr  = base_addr;
    dp->space_size = space_size;
    dp->irq_fd     = -1;
    dp->irq_n      = irq_n;
    dp->irq_proc   = irq_proc;

    if (dp->irq_n > 0  &&  dp->irq_n <= 7  &&  irq_proc != NULL  &&  1)
    {
        sprintf(dev_vmeiN, "/dev/vmei%d", dp->irq_n);
        dp->irq_fd = open(dev_vmeiN, O_RDONLY | O_NONBLOCK);
        if (dp->irq_fd >= 0)
            dp->irq_fd_fdh = sl_add_fd(devid, devptr,
                                       dp->irq_fd, SL_EX, irq_callback, dp);
        fprintf(stderr, "irq_fd=%d, irq_fd_fdh=%d\n", dp->irq_fd, dp->irq_fd_fdh);
    }

    return handle;
}

int  bivme2_io_close  (int handle)
{
  bivme2_io_devinfo_t *dp;

    return 0;
}


#if 1
#define DEFINE_IO(AS, name, TS)                                       \
int  bivme2_io_a##AS##wr##TS(int handle, uint32 ofs, uint##TS  value) \
{                                                                     \
  bivme2_io_devinfo_t *dp = devs + handle;                            \
                                                                      \
    return libvme_write_a##AS##_##name(dp->base_addr + ofs, value);   \
}                                                                     \
                                                                      \
int  bivme2_io_a##AS##rd##TS(int handle, uint32 ofs, uint##TS *val_p) \
{                                                                     \
  bivme2_io_devinfo_t *dp = devs + handle;                            \
  uint##TS             w;                                             \
  int                  r;                                             \
                                                                      \
    r = libvme_read_a##AS##_##name(dp->base_addr + ofs, &w);          \
    *val_p = w;                                                       \
    return r;                                                         \
}

DEFINE_IO(16, byte,  8)
DEFINE_IO(16, word,  16)
DEFINE_IO(16, dword, 32)

DEFINE_IO(24, byte,  8)
DEFINE_IO(24, word,  16)
DEFINE_IO(24, dword, 32)

DEFINE_IO(32, byte,  8)
DEFINE_IO(32, word,  16)
DEFINE_IO(32, dword, 32)
#else
int  bivme2_io_a16wr16(int handle, uint32 ofs, uint16  value)
{
  bivme2_io_devinfo_t *dp = devs + handle;

    return libvme_write_a16_word(dp->base_addr + ofs, value);
}

int  bivme2_io_a16rd16(int handle, uint32 ofs, uint16 *val_p)
{
  bivme2_io_devinfo_t *dp = devs + handle;
  unsigned short       w;
  int                  r;

    r = libvme_read_a16_word(dp->base_addr + ofs, &w);
    *val_p = w;
    return r;
}
#endif
