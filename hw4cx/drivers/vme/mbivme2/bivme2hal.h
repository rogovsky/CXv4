/*********************************************************************
  bivme2hal.h
      This file implements VME Hardware Abstraction Layer
      via BIVME2 libvmedirect API,
      and is intended to be included by *vme_lyr*
*********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "vmehal.h"

#include "libvmedirect.h"


static int vmehal_last_used_am = -1;
static int vmehal_param_var;

typedef struct
{
    int              fd;
    sl_fdh_t         fd_fdh;
    vmehal_irq_cb_t  cb;
} vmehal_irq_info_t;
VMEHAL_STORAGE_CLASS vmehal_irq_info_t vmehal_irq_info[8] = {[0 ... 7] = {.fd = -1, .fd_fdh = -1}};

VMEHAL_STORAGE_CLASS int libvmedirect_inited = 0;

static int  vmehal_init     (void)
{
  int  r;
  int  v;

    if (!libvmedirect_inited)
    {
        r = libvme_init();
        if (r < 0) return r;

        libvmedirect_inited = 1;
    }

    v = 1;
    r = libvme_ctl(VME_SYSTEM_W, &v);

    return 0;
}

static void vmehal_irq_cb(int uniq, void *unsdptr,
                          sl_fdh_t fdh, int fd, int mask, void *privptr)
{
  int                irq_n = ptr2lint(privptr);
  vmehal_irq_info_t *ii    = vmehal_irq_info + irq_n;
  int                r;
  int32              vector;

    errno = 0;
    r = read(fd, &vector, sizeof(vector));
    if (r == sizeof(vector))
        ii->cb(irq_n, vector);
    else
        fprintf(stderr, "%s::%s(irq_n=%d): read()=%d, !=%d, errno=%d/%s\n",
                __FILE__, __FUNCTION__,
                irq_n, r, sizeof(vector),
                errno, strerror(errno));
                
}
static int  vmehal_open_irq (int irq_n, vmehal_irq_cb_t cb, int uniq)
{
  char               dev_vmeiN[100];
  vmehal_irq_info_t *ii = vmehal_irq_info + irq_n;

    sprintf(dev_vmeiN, "/dev/vmei%d", irq_n);
    if ((ii->fd = open(dev_vmeiN, O_RDONLY | O_NONBLOCK)) < 0)
    {
        fprintf(stderr, "%s::%s(): open(\"%s\"): %s\n",
                __FILE__, __FUNCTION__,
                dev_vmeiN, strerror(errno));
        return -1;
    }
    if ((ii->fd_fdh = sl_add_fd(uniq, NULL,
                                ii->fd, SL_EX,
                                vmehal_irq_cb, lint2ptr(irq_n))) < 0)
    {
        fprintf(stderr, "%s::%s(): sl_add_fd(fd=%d): %s\n",
                __FILE__, __FUNCTION__,
                ii->fd,    strerror(errno));
        close(ii->fd); ii->fd     = -1;
        return -1;
    }
    ii->cb = cb;

    return 0;
}

static int  vmehal_close_irq(int irq_n)
{
  vmehal_irq_info_t *ii = vmehal_irq_info + irq_n;

    sl_del_fd(ii->fd_fdh); ii->fd_fdh = -1;
    close    (ii->fd);     ii->fd     = -1;

    return 0;
}


#define VMEHAL_DEFINE_IO(AS, name, TS)                           \
VMEHAL_STORAGE_CLASS int vmehal_a##AS##wr##TS(int am,            \
                                              uint32 addr, uint##TS  value)  \
{                                                                \
    if (am != vmehal_last_used_am)                               \
    {                                                            \
        vmehal_param_var = am;                                   \
        libvme_ctl(VME_AM_W, &vmehal_param_var);                 \
        vmehal_last_used_am = am;                                \
    }                                                            \
                                                                 \
    return libvme_write_a##AS##_##name(addr, value);  \
}                                                                \
                                                                 \
VMEHAL_STORAGE_CLASS int vmehal_a##AS##rd##TS(int am,            \
                                              uint32 addr, uint##TS *val_p)  \
{                                                                \
  uint##TS  w;                                                   \
  int       r;                                                   \
                                                                 \
    if (am != vmehal_last_used_am)                               \
    {                                                            \
        vmehal_param_var = am;                                   \
        libvme_ctl(VME_AM_W, &vmehal_param_var);                 \
        vmehal_last_used_am = am;                                \
    }                                                            \
                                                                 \
    r = libvme_read_a##AS##_##name(addr, &w);         \
    *val_p = w;                                                  \
    return r;                                                    \
}

VMEHAL_DEFINE_IO(16, byte,  8)
VMEHAL_DEFINE_IO(16, word,  16)
VMEHAL_DEFINE_IO(16, dword, 32)

VMEHAL_DEFINE_IO(24, byte,  8)
VMEHAL_DEFINE_IO(24, word,  16)
VMEHAL_DEFINE_IO(24, dword, 32)

VMEHAL_DEFINE_IO(32, byte,  8)
VMEHAL_DEFINE_IO(32, word,  16)
VMEHAL_DEFINE_IO(32, dword, 32)
