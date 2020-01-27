#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libvmedirect.h"


//// misc_types.h ////////////////////////////////////////////////////

typedef   signed int    int32;
typedef unsigned int    uint32;
typedef   signed short  int16;
typedef unsigned short  uint16;
typedef   signed char   int8;
typedef unsigned char   uint8;

typedef unsigned char   char8;

typedef   signed long long int int64;
typedef unsigned long long int uint64;


//////////////////////////////////////////////////////////////////////

static uint32  base_addr;
static int     irq_fd;


//// bivme2_io.[ch] //////////////////////////////////////////////////

#define DEFINE_IO(AS, name, TS)                                       \
static int  bivme2_io_a##AS##wr##TS(uint32 ofs, uint##TS  value) \
{                                                                     \
    return libvme_write_a##AS##_##name(base_addr + ofs, value);   \
}                                                                     \
                                                                      \
static int  bivme2_io_a##AS##rd##TS(uint32 ofs, uint##TS *val_p) \
{                                                                     \
  uint##TS             w;                                             \
  int                  r;                                             \
                                                                      \
    r = libvme_read_a##AS##_##name(base_addr + ofs, &w);          \
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



//////////////////////////////////////////////////////////////////////
enum {ADDRESS_MODIFIER = 0x29};

enum
{
    KREG_IO  = 0,
    KREG_IRQ = 2,
};

enum
{
    KCMD_STOP       = 0,
    KCMD_START      = 1,
    KCMD_SET_ADTIME = 2,
    KCMD_SET_CHBEG  = 3,
    KCMD_SET_CHEND  = 4,
    KCMD_READ_MEM   = 5,
};

static inline uint16 KCMD(uint8 cmd, uint8 modifier)
{
    return (((uint16)cmd) << 8) | modifier;
}

enum
{
    KCMD_START_FLAG_MULTICHAN = 1 << 0,
    KCMD_START_FLAG_INFINITE  = 1 << 1,
    KCMD_START_FLAG_EACH_IRQ  = 1 << 2,
};

enum
{
    ADDR_FLAG0     = 33,
    ADDR_FLAG1     = 34,
    
    ADDR_CHBEG     = 37,
    ADDR_CHEND     = 38,
    ADDR_CHCUR     = 39,
    ADDR_ADTIME    = 40,

    ADDR_SWversion = 113,
    ADDR_HWversion = 114,
    
    ADDR_CHNNDATA_BASE = 128,
};


static uint8 KVRdByte(uint8 addr)
{
  uint16  w;

    bivme2_io_a16wr16(KREG_IO, KCMD(KCMD_READ_MEM, addr));
    bivme2_io_a16rd16(KREG_IO, &w);

    return w & 0xFF;
}

enum {DEFAULT_IDLE_USECS = 10*1000*1000};
static int                select_idle_usecs = DEFAULT_IDLE_USECS;

int main(int argc, char *argv[])
{
  int   jumpers;
  int   irq_n;

  int   r;
  int   v;
  char  dev_vmeiN[100];

  fd_set               sel_efds;
  struct timeval  timeout;      /* Timeout for select() */
  int32                vector;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s JUMPERS [IRQ_N]\n", argv[0]);
        exit(2);
    }

    jumpers =           strtol(argv[1], NULL, 0);
    irq_n   = argc > 2? strtol(argv[2], NULL, 0) : 0;
    base_addr = jumpers << 4;
    fprintf(stderr, "Using base_addr=%04x irq=%d\n", base_addr, irq_n);
    
    r = libvme_init();
    v = 1;
    r = libvme_ctl(VME_SYSTEM_W, &v);
    v = ADDRESS_MODIFIER;
    r = libvme_ctl(VME_AM_W,    &v);

    if (irq_n > 0  &&  irq_n <= 7)
    {
        sprintf(dev_vmeiN, "/dev/vmei%d", irq_n); 
        irq_fd = open(dev_vmeiN, O_RDONLY | O_NONBLOCK);
        fprintf(stderr, "irq_fd=%d\n", irq_fd);
    }
    else
        irq_fd = -1;

    {
        int  a;
        for (a = 0;  a <= 255; a++)
            fprintf(stderr, "%3d%c", KVRdByte(a), (a&15)==15? '\n' : ' ');
    }

    bivme2_io_a16wr16(KREG_IRQ,
                      (((uint16)(irq_n)) << 8) | 0xdb /* irq_vect */);

    bivme2_io_a16wr16(KREG_IO, KCMD(KCMD_STOP, 0));
    bivme2_io_a16wr16(KREG_IO, KCMD(KCMD_SET_CHBEG,  0  /* ch_beg */));
    bivme2_io_a16wr16(KREG_IO, KCMD(KCMD_SET_CHEND,  24 /* ch_end */));
    bivme2_io_a16wr16(KREG_IO, KCMD(KCMD_SET_ADTIME, 4  /* adtime */));
    bivme2_io_a16wr16(KREG_IO,
                      KCMD(KCMD_START,
                           KCMD_START_FLAG_MULTICHAN |
                           KCMD_START_FLAG_INFINITE |
                           KCMD_START_FLAG_EACH_IRQ));

    if (irq_fd < 0) return 0;
    
    while (1)
    {
        timeout.tv_sec  = select_idle_usecs / 1000000;
        timeout.tv_usec = select_idle_usecs % 1000000;
        FD_ZERO       (&sel_efds);
        FD_SET(irq_fd, &sel_efds);
        r = select(irq_fd + 1, NULL, NULL, &sel_efds, &timeout);
        fprintf(stderr, "select: r=%d ", r);
        if      (r < 0)
        {
            fprintf(stderr, "err=%d/\"%s\"\n", errno, strerror(errno));
            exit(1);
        }
        else if (r == 0)
            fprintf(stderr, "timeout\n");
        else
        {
            r = read(irq_fd, &vector, sizeof(vector));
            fprintf(stderr, "read=%d vector=%x\n", r, vector);
        }
            
    }

    return 0;
}
