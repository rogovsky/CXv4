#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cxscheduler.h" /*!!! Should use DRIVELET API!!! */

#include "bivme2_vme_io.h"

#include "libvmedirect.h"


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
