#ifndef __VMEHAL_H
#define __VMEHAL_H


#ifndef VMEHAL_STORAGE_CLASS
  #define VMEHAL_STORAGE_CLASS static
#endif


#include "misc_types.h"


typedef void (*vmehal_irq_cb_t)(int irq_n, int irq_vect);

static int  vmehal_init     (void);
static int  vmehal_open_irq (int irq_n, vmehal_irq_cb_t cb, int uniq);
static int  vmehal_close_irq(int irq_n);

#define VMEHAL_DECLARE_IO(AS, TS)                                            \
VMEHAL_STORAGE_CLASS int vmehal_a##AS##wr##TS(int am,                        \
                                              uint32 addr, uint##TS  value); \
VMEHAL_STORAGE_CLASS int vmehal_a##AS##rd##TS(int am,                        \
                                              uint32 addr, uint##TS *val_p);

VMEHAL_DECLARE_IO(16, 8)
VMEHAL_DECLARE_IO(16, 16)
VMEHAL_DECLARE_IO(16, 32)

VMEHAL_DECLARE_IO(24, 8)
VMEHAL_DECLARE_IO(24, 16)
VMEHAL_DECLARE_IO(24, 32)

VMEHAL_DECLARE_IO(32, 8)
VMEHAL_DECLARE_IO(32, 16)
VMEHAL_DECLARE_IO(32, 32)


#endif /* __VMEHAL_H */
